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


#include "cxdref_demod_dvbt.h"
#include "cxdref_demod_dvbt_monitor.h"

static cxdref_result_t SLtoAT(cxdref_demod_t * pDemod);

static cxdref_result_t SLtoAT_BandSetting(cxdref_demod_t * pDemod);

static cxdref_result_t ATtoAT(cxdref_demod_t * pDemod);

static cxdref_result_t ATtoSL(cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbt_Tune (cxdref_demod_t * pDemod,
                                    cxdref_dvbt_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_Tune");

    if ((!pDemod) || (!pTuneParam)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->system == CXDREF_DTV_SYSTEM_DVBT)) {
        pDemod->bandwidth = pTuneParam->bandwidth;

        result = ATtoAT (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }
    else if((pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->system != CXDREF_DTV_SYSTEM_DVBT)){
        result = cxdref_demod_Sleep (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        pDemod->system = CXDREF_DTV_SYSTEM_DVBT;
        pDemod->bandwidth = pTuneParam->bandwidth;

        result = SLtoAT (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }
    else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP) {
        pDemod->system = CXDREF_DTV_SYSTEM_DVBT;
        pDemod->bandwidth = pTuneParam->bandwidth;

        result = SLtoAT (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->state = CXDREF_DEMOD_STATE_ACTIVE;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_Sleep (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_Sleep");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = ATtoSL (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_CheckDemodLock (cxdref_demod_t * pDemod,
                                              cxdref_demod_lock_result_t * pLock)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    uint8_t sync = 0;
    uint8_t tslock = 0;
    uint8_t unlockDetected = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_CheckDemodLock");

    if (!pDemod || !pLock) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;

    result = cxdref_demod_dvbt_monitor_SyncStat (pDemod, &sync, &tslock, &unlockDetected);
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

cxdref_result_t cxdref_demod_dvbt_CheckTSLock (cxdref_demod_t * pDemod,
                                           cxdref_demod_lock_result_t * pLock)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    uint8_t sync = 0;
    uint8_t tslock = 0;
    uint8_t unlockDetected = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_CheckTSLock");

    if (!pDemod || !pLock) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;

    result = cxdref_demod_dvbt_monitor_SyncStat (pDemod, &sync, &tslock, &unlockDetected);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (unlockDetected) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_UNLOCKED;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    if ((sync >= 6) && tslock) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_LOCKED;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_SetProfile (cxdref_demod_t * pDemod,
                                          cxdref_dvbt_profile_t profile)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_SetProfile");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x67, (profile == CXDREF_DVBT_PROFILE_HP)? 0x00 : 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_EchoOptimization (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dvbt_mode_t mode;
    uint8_t rdata = 0;
    uint8_t sync = 0;
    uint8_t tslock = 0;
    uint8_t earlyUnlock = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_EchoOptimization");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_dvbt_monitor_SyncStat (pDemod, &sync, &tslock, &earlyUnlock);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (sync < 2) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x40, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    mode = (cxdref_dvbt_mode_t) ((rdata >> 2) & 0x03);

    if (mode != CXDREF_DVBT_MODE_8K) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x13) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = { 0x00, 0x10 };
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9C, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_ClearBERSNRHistory (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_ClearBERSNRHistory");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4A, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x52, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t SLtoAT(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("SLtoAT");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetTSClockModeAndFreq (pDemod, CXDREF_DTV_SYSTEM_DVBT);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x01) != CXDREF_RESULT_OK) {
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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x11) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6A, 0x50) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA5, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }
    else {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA5, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x18) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x31, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        const uint8_t data[] = { 0x01, 0x01 };

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xCE, data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x11) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        const uint8_t data[3] = { 0x00, 0x03, 0x3B };
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    result = SLtoAT_BandSetting (pDemod);
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

static cxdref_result_t SLtoAT_BandSetting(cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("SLtoAT_BandSetting");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x13) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        const uint8_t data[] = { 0x01, 0x14 };
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9C, data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    switch (pDemod->bandwidth) {
    case CXDREF_DTV_BW_8_MHZ:
        {
            const uint8_t nominalRate[5] = {
                0x15, 0x00, 0x00, 0x00, 0x00
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRate, 5) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
            const uint8_t itbCoef[14] = {
                0x2F,  0xBA,  0x28,  0x9B,  0x28,  0x9D,  0x28,  0xA1,  0x29,  0xA5,  0x2A,  0xAC,  0x29,  0xB5
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA6, itbCoef, 14) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            uint8_t data[3];
            data[0] = (uint8_t) ((pDemod->iffreqConfig.configDVBT_8 >> 16) & 0xFF);
            data[1] = (uint8_t) ((pDemod->iffreqConfig.configDVBT_8 >> 8) & 0xFF);
            data[2] = (uint8_t) (pDemod->iffreqConfig.configDVBT_8 & 0xFF);
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = { 0x15, 0x28 };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD9, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            const uint8_t data[] = { 0x01, 0x02 };

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x17) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
        break;

    case CXDREF_DTV_BW_7_MHZ:
        {
            const uint8_t nominalRate[5] = {
                0x18, 0x00, 0x00, 0x00, 0x00
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRate, 5) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
            const uint8_t itbCoef[14] = {
                0x30,  0xB1,  0x29,  0x9A,  0x28,  0x9C,  0x28,  0xA0,  0x29,  0xA2,  0x2B,  0xA6,  0x2B,  0xAD
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA6, itbCoef, 14) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            uint8_t data[3];
            data[0] = (uint8_t) ((pDemod->iffreqConfig.configDVBT_7 >> 16) & 0xFF);
            data[1] = (uint8_t) ((pDemod->iffreqConfig.configDVBT_7 >> 8) & 0xFF);
            data[2] = (uint8_t) (pDemod->iffreqConfig.configDVBT_7 & 0xFF);
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x02) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = { 0x1F, 0xF8 };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD9, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            const uint8_t data[] = { 0x00, 0x03 };

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x17) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
        break;

    case CXDREF_DTV_BW_6_MHZ:
        {
            const uint8_t nominalRate[5] = {
                0x1C, 0x00, 0x00, 0x00, 0x00
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRate, 5) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
            const uint8_t itbCoef[14] = {
                0x31,  0xA8,  0x29,  0x9B,  0x27,  0x9C,  0x28,  0x9E,  0x29,  0xA4,  0x29,  0xA2,  0x29,  0xA8
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA6, itbCoef, 14) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            uint8_t data[3];
            data[0] = (uint8_t) ((pDemod->iffreqConfig.configDVBT_6 >> 16) & 0xFF);
            data[1] = (uint8_t) ((pDemod->iffreqConfig.configDVBT_6 >> 8) & 0xFF);
            data[2] = (uint8_t) (pDemod->iffreqConfig.configDVBT_6 & 0xFF);
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x04) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[2] = { 0x25, 0x4C };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD9, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            const uint8_t data[] = { 0x00, 0x03 };

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x17) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
        break;

    case CXDREF_DTV_BW_5_MHZ:
        {
            const uint8_t nominalRate[5] = {
                0x21, 0x99, 0x99, 0x99, 0x99
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRate, 5) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
            const uint8_t itbCoef[14] = {
                0x31,  0xA8,  0x29,  0x9B,  0x27,  0x9C,  0x28,  0x9E,  0x29,  0xA4,  0x29,  0xA2,  0x29,  0xA8
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA6, itbCoef, 14) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            uint8_t data[3];
            data[0] = (uint8_t) ((pDemod->iffreqConfig.configDVBT_5 >> 16) & 0xFF);
            data[1] = (uint8_t) ((pDemod->iffreqConfig.configDVBT_5 >> 8) & 0xFF);
            data[2] = (uint8_t) (pDemod->iffreqConfig.configDVBT_5 & 0xFF);
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x06) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[2] = { 0x2C, 0xC2 };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD9, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            const uint8_t data[] = { 0x00, 0x03 };

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x17) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t ATtoAT(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("ATtoAT");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = SLtoAT_BandSetting (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t ATtoSL(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("ATtoSL");

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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x17) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[] = { 0x01, 0x02 };

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, data, 2) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x18) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x31, 0x00) != CXDREF_RESULT_OK) {
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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
