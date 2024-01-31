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


#include "cxdref_math.h"
#include "cxdref_demod_dvbt2_monitor.h"

cxdref_result_t cxdref_demod_dvbt2_monitor_SyncStat (cxdref_demod_t * pDemod, uint8_t * pSyncStat, uint8_t * pTSLockStat, uint8_t * pUnlockDetected)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_SyncStat");

    if ((!pDemod) || (!pSyncStat) || (!pTSLockStat) || (!pUnlockDetected)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data;
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pSyncStat = data & 0x07;
        *pTSLockStat = ((data & 0x20) ? 1 : 0);
        *pUnlockDetected = ((data & 0x10) ? 1 : 0);
    }

    if (*pSyncStat == 0x07){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_CarrierOffset (cxdref_demod_t * pDemod, int32_t * pOffset)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_CarrierOffset");

    if ((!pDemod) || (!pOffset)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[4];
        uint32_t ctlVal = 0;
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (syncState != 6) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4C, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        ctlVal = ((data[0] & 0x0F) << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
        *pOffset = cxdref_Convert2SComplement (ctlVal, 28);

        switch (pDemod->bandwidth) {
        case CXDREF_DTV_BW_1_7_MHZ:
            *pOffset = -1 * ((*pOffset) / 582);
            break;
        case CXDREF_DTV_BW_5_MHZ:
        case CXDREF_DTV_BW_6_MHZ:
        case CXDREF_DTV_BW_7_MHZ:
        case CXDREF_DTV_BW_8_MHZ:
            *pOffset = -1 * ((*pOffset) * (uint8_t)pDemod->bandwidth / 940);
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }
    }

    {
        if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
            *pOffset *= -1;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_IFAGCOut (cxdref_demod_t * pDemod, uint32_t * pIFAGC)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_IFAGCOut");

    if ((!pDemod) || (!pIFAGC)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[2];
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x26, data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pIFAGC = ((data[0] & 0x0F) << 8) | (data[1]);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_L1Pre (cxdref_demod_t * pDemod, cxdref_dvbt2_l1pre_t * pL1Pre)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_L1Pre");

    if ((!pDemod) || (!pL1Pre)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[37];
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;
        uint8_t version = 0;
        cxdref_dvbt2_profile_t profile;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (syncState < 5) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        result = cxdref_demod_dvbt2_monitor_Profile(pDemod, &profile);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x20, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        SLVT_UnFreezeReg (pDemod);

        pL1Pre->type = (cxdref_dvbt2_l1pre_type_t) data[0];
        pL1Pre->bwExt = data[1] & 0x01;
        pL1Pre->s1 = (cxdref_dvbt2_s1_t) (data[2] & 0x07);
        pL1Pre->s2 = data[3] & 0x0F;
        pL1Pre->l1Rep = data[4] & 0x01;
        pL1Pre->gi = (cxdref_dvbt2_guard_t) (data[5] & 0x07);
        pL1Pre->papr = (cxdref_dvbt2_papr_t) (data[6] & 0x0F);
        pL1Pre->mod = (cxdref_dvbt2_l1post_constell_t) (data[7] & 0x0F);
        pL1Pre->cr = (cxdref_dvbt2_l1post_cr_t) (data[8] & 0x03);
        pL1Pre->fec = (cxdref_dvbt2_l1post_fec_type_t) (data[9] & 0x03);
        pL1Pre->l1PostSize = (data[10] & 0x03) << 16;
        pL1Pre->l1PostSize |= (data[11]) << 8;
        pL1Pre->l1PostSize |= (data[12]);
        pL1Pre->l1PostInfoSize = (data[13] & 0x03) << 16;
        pL1Pre->l1PostInfoSize |= (data[14]) << 8;
        pL1Pre->l1PostInfoSize |= (data[15]);
        pL1Pre->pp = (cxdref_dvbt2_pp_t) (data[16] & 0x0F);
        pL1Pre->txIdAvailability = data[17];
        pL1Pre->cellId = (data[18] << 8);
        pL1Pre->cellId |= (data[19]);
        pL1Pre->networkId = (data[20] << 8);
        pL1Pre->networkId |= (data[21]);
        pL1Pre->systemId = (data[22] << 8);
        pL1Pre->systemId |= (data[23]);
        pL1Pre->numFrames = data[24];
        pL1Pre->numSymbols = (data[25] & 0x0F) << 8;
        pL1Pre->numSymbols |= data[26];
        pL1Pre->regen = data[27] & 0x07;
        pL1Pre->postExt = data[28] & 0x01;
        pL1Pre->numRfFreqs = data[29] & 0x07;
        pL1Pre->rfIdx = data[30] & 0x07;
        version = (data[31] & 0x03) << 2;
        version |= (data[32] & 0xC0) >> 6;
        pL1Pre->t2Version = (cxdref_dvbt2_version_t) version;
        pL1Pre->l1PostScrambled = (data[32] & 0x20) >> 5;
        pL1Pre->t2BaseLite = (data[32] & 0x10) >> 4;
        pL1Pre->crc32 = (data[33] << 24);
        pL1Pre->crc32 |= (data[34] << 16);
        pL1Pre->crc32 |= (data[35] << 8);
        pL1Pre->crc32 |= data[36];

        if (profile == CXDREF_DVBT2_PROFILE_BASE) {
            switch ((pL1Pre->s2 >> 1)) {
            case CXDREF_DVBT2_BASE_S2_M1K_G_ANY:
                pL1Pre->fftMode = CXDREF_DVBT2_M1K;
                break;
            case CXDREF_DVBT2_BASE_S2_M2K_G_ANY:
                pL1Pre->fftMode = CXDREF_DVBT2_M2K;
                break;
            case CXDREF_DVBT2_BASE_S2_M4K_G_ANY:
                pL1Pre->fftMode = CXDREF_DVBT2_M4K;
                break;
            case CXDREF_DVBT2_BASE_S2_M8K_G_DVBT:
            case CXDREF_DVBT2_BASE_S2_M8K_G_DVBT2:
                pL1Pre->fftMode = CXDREF_DVBT2_M8K;
                break;
            case CXDREF_DVBT2_BASE_S2_M16K_G_ANY:
                pL1Pre->fftMode = CXDREF_DVBT2_M16K;
                break;
            case CXDREF_DVBT2_BASE_S2_M32K_G_DVBT:
            case CXDREF_DVBT2_BASE_S2_M32K_G_DVBT2:
                pL1Pre->fftMode = CXDREF_DVBT2_M32K;
                break;
            default:
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
        }
        else if (profile == CXDREF_DVBT2_PROFILE_LITE) {
            switch ((pL1Pre->s2 >> 1)) {
            case CXDREF_DVBT2_LITE_S2_M2K_G_ANY:
                pL1Pre->fftMode = CXDREF_DVBT2_M2K;
                break;
            case CXDREF_DVBT2_LITE_S2_M4K_G_ANY:
                pL1Pre->fftMode = CXDREF_DVBT2_M4K;
                break;
            case CXDREF_DVBT2_LITE_S2_M8K_G_DVBT:
            case CXDREF_DVBT2_LITE_S2_M8K_G_DVBT2:
                pL1Pre->fftMode = CXDREF_DVBT2_M8K;
                break;
            case CXDREF_DVBT2_LITE_S2_M16K_G_DVBT:
            case CXDREF_DVBT2_LITE_S2_M16K_G_DVBT2:
                pL1Pre->fftMode = CXDREF_DVBT2_M16K;
                break;
            default:
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
        }
        else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        pL1Pre->mixed = pL1Pre->s2 & 0x01;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_Version (cxdref_demod_t * pDemod, cxdref_dvbt2_version_t * pVersion)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t version = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_Version");

    if ((!pDemod) || (!pVersion)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[2];
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (syncState < 5) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3F, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        version = ((data[0] & 0x03) << 2);
        version |= ((data[1] & 0xC0) >> 6);
        *pVersion = (cxdref_dvbt2_version_t) version;
    }
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_OFDM (cxdref_demod_t * pDemod, cxdref_dvbt2_ofdm_t * pOfdm)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_OFDM");

    if ((!pDemod) || (!pOfdm)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[5];
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;
        cxdref_result_t result = CXDREF_RESULT_OK;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (syncState != 6) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        pOfdm->mixed = ((data[0] & 0x20) ? 1 : 0);
        pOfdm->isMiso = ((data[0] & 0x10) >> 4);
        pOfdm->mode = (cxdref_dvbt2_mode_t) (data[0] & 0x07);
        pOfdm->gi = (cxdref_dvbt2_guard_t) ((data[1] & 0x70) >> 4);
        pOfdm->pp = (cxdref_dvbt2_pp_t) (data[1] & 0x07);
        pOfdm->bwExt = (data[2] & 0x10) >> 4;
        pOfdm->papr = (cxdref_dvbt2_papr_t) (data[2] & 0x0F);
        pOfdm->numSymbols = (data[3] << 8) | data[4];
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_DataPLPs (cxdref_demod_t * pDemod, uint8_t * pPLPIds, uint8_t * pNumPLPs)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_DataPLPs");

    if ((!pDemod) || (!pNumPLPs)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t l1PostOK = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &l1PostOK, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(l1PostOK & 0x01)) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7F, pNumPLPs, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (*pNumPLPs == 0) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
        }

        if (!pPLPIds) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, pPLPIds, *pNumPLPs > 128 ? 128 : *pNumPLPs) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (*pNumPLPs > 128) {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x23) != CXDREF_RESULT_OK) {
                SLVT_UnFreezeReg (pDemod);
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, pPLPIds + 128, *pNumPLPs - 128) != CXDREF_RESULT_OK) {
                SLVT_UnFreezeReg (pDemod);
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        SLVT_UnFreezeReg (pDemod);
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_ActivePLP (cxdref_demod_t * pDemod,
                                                  cxdref_dvbt2_plp_btype_t type,
                                                  cxdref_dvbt2_plp_t * pPLPInfo)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_ActivePLP");

    if ((!pDemod) || (!pPLPInfo)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[20];
        uint8_t addr = 0;
        uint8_t index = 0;
        uint8_t l1PostOk = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &l1PostOk, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!l1PostOk) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (type == CXDREF_DVBT2_PLP_COMMON) {
            addr = 0x67;
        }
        else {
            addr = 0x54;
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, addr, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        if ((type == CXDREF_DVBT2_PLP_COMMON) && (data[13] == 0)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        pPLPInfo->id = data[index++];
        pPLPInfo->type = (cxdref_dvbt2_plp_type_t) (data[index++] & 0x07);
        pPLPInfo->payload = (cxdref_dvbt2_plp_payload_t) (data[index++] & 0x1F);
        pPLPInfo->ff = data[index++] & 0x01;
        pPLPInfo->firstRfIdx = data[index++] & 0x07;
        pPLPInfo->firstFrmIdx = data[index++];
        pPLPInfo->groupId = data[index++];
        pPLPInfo->plpCr = (cxdref_dvbt2_plp_code_rate_t) (data[index++] & 0x07);
        pPLPInfo->constell = (cxdref_dvbt2_plp_constell_t) (data[index++] & 0x07);
        pPLPInfo->rot = data[index++] & 0x01;
        pPLPInfo->fec = (cxdref_dvbt2_plp_fec_t) (data[index++] & 0x03);
        pPLPInfo->numBlocksMax = (uint16_t) ((data[index++] & 0x03)) << 8;
        pPLPInfo->numBlocksMax |= data[index++];
        pPLPInfo->frmInt = data[index++];
        pPLPInfo->tilLen = data[index++];
        pPLPInfo->tilType = data[index++] & 0x01;

        if (type == CXDREF_DVBT2_PLP_COMMON) {
            index++;
        }

        pPLPInfo->inBandAFlag = data[index++] & 0x01;
        pPLPInfo->rsvd = data[index++] << 8;
        pPLPInfo->rsvd |= data[index++];

        pPLPInfo->inBandBFlag = (uint8_t) ((pPLPInfo->rsvd & 0x8000) >> 15);
        pPLPInfo->plpMode = (cxdref_dvbt2_plp_mode_t) ((pPLPInfo->rsvd & 0x000C) >> 2);
        pPLPInfo->staticFlag = (uint8_t) ((pPLPInfo->rsvd & 0x0002) >> 1);
        pPLPInfo->staticPaddingFlag = (uint8_t) (pPLPInfo->rsvd & 0x0001);
        pPLPInfo->rsvd = (uint16_t) ((pPLPInfo->rsvd & 0x7FF0) >> 4);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_DataPLPError (cxdref_demod_t * pDemod, uint8_t * pPLPError)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_DataPLPError");

    if ((!pDemod) || (!pPLPError)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[2];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &data[0], 2) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((data[0] & 0x01) == 0x00) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        *pPLPError = data[1] & 0x01;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_L1Change (cxdref_demod_t * pDemod, uint8_t * pL1Change)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_L1Change");

    if ((!pDemod) || (!pL1Change)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data;
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;
        cxdref_result_t result = CXDREF_RESULT_OK;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (syncState < 5) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x15, &data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pL1Change = data & 0x01;
        if (*pL1Change) {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x16, 0x01) != CXDREF_RESULT_OK) {
                SLVT_UnFreezeReg (pDemod);
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
        SLVT_UnFreezeReg (pDemod);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_L1Post (cxdref_demod_t * pDemod, cxdref_dvbt2_l1post_t * pL1Post)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_L1Post");

    if ((!pDemod) || (!pL1Post)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[15];
        uint8_t l1PostOK = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &l1PostOK, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(l1PostOK & 0x01)) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x45, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        pL1Post->subSlicesPerFrame = (data[0] & 0x7F) << 8;
        pL1Post->subSlicesPerFrame |= data[1];
        pL1Post->numPLPs = data[2];
        pL1Post->numAux = data[3] & 0x0F;
        pL1Post->auxConfigRFU = data[4];
        pL1Post->rfIdx = data[5] & 0x07;
        pL1Post->freq = data[6] << 24;
        pL1Post->freq |= data[7] << 16;
        pL1Post->freq |= data[8] << 8;
        pL1Post->freq |= data[9];
        pL1Post->fefType = data[10] & 0x0F;
        pL1Post->fefLength = data[11] << 16;
        pL1Post->fefLength |= data[12] << 8;
        pL1Post->fefLength |= data[13];
        pL1Post->fefInterval = data[14];
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_BBHeader (cxdref_demod_t * pDemod,
                                                 cxdref_dvbt2_plp_btype_t type,
                                                 cxdref_dvbt2_bbheader_t * pBBHeader)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_BBHeader");

    if ((!pDemod) || (!pBBHeader)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        cxdref_result_t result = CXDREF_RESULT_OK;
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (!tsLock) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x24) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[17];
        uint8_t addr = 0;

        if (type == CXDREF_DVBT2_PLP_COMMON) {
            addr = 0xA8;
        } else {
            addr = 0x92;
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, addr, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        if ((data[0] & 0x01) == 0x00) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        pBBHeader->streamInput = (cxdref_dvbt2_stream_t)((data[1] >> 6) & 0x03);
        pBBHeader->isSingleInputStream = (uint8_t)((data[1] >> 5) & 0x01);
        pBBHeader->isConstantCodingModulation = (uint8_t)((data[1] >> 4) & 0x01);
        pBBHeader->issyIndicator = (uint8_t)((data[1] >> 3) & 0x01);
        pBBHeader->nullPacketDeletion = (uint8_t)((data[1] >> 2) & 0x01);
        pBBHeader->ext = (uint8_t)(data[1] & 0x03);

        pBBHeader->inputStreamIdentifier = data[2];
        pBBHeader->plpMode = (data[4] & 0x01) ? CXDREF_DVBT2_PLP_MODE_HEM : CXDREF_DVBT2_PLP_MODE_NM;
        pBBHeader->dataFieldLength = (uint16_t)((data[5] << 8) | data[6]);

        if(pBBHeader->plpMode == CXDREF_DVBT2_PLP_MODE_NM){
            pBBHeader->userPacketLength = (uint16_t)((data[7] << 8) | data[8]);
            pBBHeader->syncByte = data[9];
            pBBHeader->issy = 0;
        }else{
            pBBHeader->userPacketLength = 0;
            pBBHeader->syncByte = 0;
            pBBHeader->issy = (uint32_t)((data[14] << 16) | (data[15] << 8) | data[16]);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_InBandBTSRate (cxdref_demod_t * pDemod,
                                                      cxdref_dvbt2_plp_btype_t type,
                                                      uint32_t * pTSRateBps)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_InBandBTSRate");

    if ((!pDemod) || (!pTSRateBps)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        cxdref_result_t result = CXDREF_RESULT_OK;
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (!tsLock) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t l1PostOk = 0;
        uint8_t addr = 0;
        uint8_t data = 0;

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &l1PostOk, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(l1PostOk & 0x01)) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (type == CXDREF_DVBT2_PLP_COMMON) {
            addr = 0x79;
        } else {
            addr = 0x65;
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, addr, &data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((data & 0x80) == 0x00) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x25) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[4];
        uint8_t addr = 0;

        if (type == CXDREF_DVBT2_PLP_COMMON) {
            addr = 0xA6;
        } else {
            addr = 0xAA;
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, addr, &data[0], 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        *pTSRateBps = (uint32_t)(((data[0] & 0x07) << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_SpectrumSense (cxdref_demod_t * pDemod, cxdref_demod_terr_cable_spectrum_sense_t * pSense)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t syncState   = 0;
    uint8_t tsLock      = 0;
    uint8_t earlyUnlock = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_SpectrumSense");

    if ((!pDemod) || (!pSense)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyUnlock);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (syncState != 6) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint8_t data = 0;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x28) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE6, &data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
            *pSense = (data & 0x01) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV;
        } else {
            *pSense = (data & 0x01) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_SNR (cxdref_demod_t * pDemod, int32_t * pSNR)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_SNR");

    if ((!pDemod) || (!pSNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint16_t reg;
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;
        uint8_t data[2];

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (syncState != 6) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x28, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        SLVT_UnFreezeReg (pDemod);

        reg = ((data[0] & 0xFF) << 8) | data[1];

        if (reg == 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (reg > 10876) {
            reg = 10876;
        }

        *pSNR = 10 * 10 * ((int32_t) cxdref_math_log10 (reg) - (int32_t) cxdref_math_log10 (12600 - reg));
        *pSNR += 32000;
    }
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_PreLDPCBER (cxdref_demod_t * pDemod, uint32_t * pBER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t bitError = 0;
    uint32_t periodExp = 0;
    uint32_t n_ldpc = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_PreLDPCBER");

    if ((!pDemod) || (!pBER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[4];
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x39, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(data[0] & 0x10)) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        bitError = ((data[0] & 0x0F) << 24) | (data[1] << 16) | (data[2] << 8) | data[3];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6F, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp = data[0] & 0x0F;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5E, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        if (((cxdref_dvbt2_plp_fec_t) (data[0] & 0x03)) == CXDREF_DVBT2_FEC_LDPC_16K) {
            n_ldpc = 16200;
        }
        else {
            n_ldpc = 64800;
        }
    }

    if (bitError > ((1U << periodExp) * n_ldpc)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        if (periodExp >= 4) {
            div = (1U << (periodExp - 4)) * (n_ldpc / 200);

            Q = (bitError * 5) / div;
            R = (bitError * 5) % div;

            R *= 625;
            Q = Q * 625 + R / div;
            R = R % div;
        }
        else {
            div = (1U << periodExp) * (n_ldpc / 200);

            Q = (bitError * 10) / div;
            R = (bitError * 10) % div;

            R *= 5000;
            Q = Q * 5000 + R / div;
            R = R % div;
        }

        if (R >= div/2) {
            *pBER = Q + 1;
        }
        else {
            *pBER = Q;
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_PostBCHFER (cxdref_demod_t * pDemod, uint32_t * pFER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t fecError = 0;
    uint32_t period = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_PostBCHFER");

    if ((!pDemod) || (!pFER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[3];
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x75, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(data[0] & 0x01)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        fecError = ((data[1] & 0x7F) << 8) | (data[2]);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x72, data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = (1 << (data[0] & 0x0F));
    }

    if ((period == 0) || (fecError > period)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        div = period;

        Q = (fecError * 1000) / div;
        R = (fecError * 1000) % div;

        R *= 1000;
        Q = Q * 1000 + R / div;
        R = R % div;

        if ((div != 1) && (R >= div/2)) {
            *pFER = Q + 1;
        }
        else {
            *pFER = Q;
        }
    }

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_demod_dvbt2_monitor_PreBCHBER (cxdref_demod_t * pDemod, uint32_t * pBER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t bitError = 0;
    uint32_t periodExp = 0;
    uint32_t n_bch = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_PreBCHBER");

    if ((!pDemod) || (!pBER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[4];
        cxdref_dvbt2_plp_fec_t plpFecType = CXDREF_DVBT2_FEC_LDPC_16K;
        cxdref_dvbt2_plp_code_rate_t plpCr = CXDREF_DVBT2_R1_2;

        static const uint16_t nBCHBitsLookup[2][8] = {
            {7200,  9720,  10800, 11880, 12600, 13320, 5400,  6480},
            {32400, 38880, 43200, 48600, 51840, 54000, 21600, 25920}
        };

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x24) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x40, data, 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(data[0] & 0x01)) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        bitError = ((data[1] & 0x3F) << 16) | (data[2] << 8) | data[3];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5E, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        plpFecType = (cxdref_dvbt2_plp_fec_t) (data[0] & 0x03);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5B, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        plpCr = (cxdref_dvbt2_plp_code_rate_t) (data[0] & 0x07);

        SLVT_UnFreezeReg (pDemod);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x72, data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp = data[0] & 0x0F;

        if ((plpFecType > CXDREF_DVBT2_FEC_LDPC_64K) || (plpCr > CXDREF_DVBT2_R2_5)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        n_bch = nBCHBitsLookup[plpFecType][plpCr];
    }

    if (bitError > ((1U << periodExp) * n_bch)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        if (periodExp >= 6) {
            div = (1U << (periodExp - 6)) * (n_bch / 40);

            Q = (bitError * 625) / div;
            R = (bitError * 625) % div;

            R *= 625;
            Q = Q * 625 + R / div;
            R = R % div;
        }
        else {
            div = (1U << periodExp) * (n_bch / 40);

            Q = (bitError * 1000) / div;
            R = (bitError * 1000) % div;

            R *= 25000;
            Q = Q * 25000 + R / div;
            R = R % div;
        }

        if (R >= div/2) {
            *pBER = Q + 1;
        }
        else {
            *pBER = Q;
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_PacketErrorNumber (cxdref_demod_t * pDemod, uint32_t * pPEN)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    uint8_t data[3];

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_PacketErrorNumber");

    if ((!pDemod) || (!pPEN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x24) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFD, data, sizeof (data)) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data[0] & 0x01) ) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *pPEN =  ((data[1] << 0x08) | data[2]);

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_demod_dvbt2_monitor_SamplingOffset (cxdref_demod_t * pDemod, int32_t * pPPM)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_SamplingOffset");

    if ((!pDemod) || (!pPPM)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t ctlValReg[5];
        uint8_t nominalRateReg[5];
        uint32_t trlCtlVal = 0;
        uint32_t trcgNominalRate = 0;
        int32_t num;
        int32_t den;
        cxdref_result_t result = CXDREF_RESULT_OK;
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;
        int8_t diffUpper = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (syncState != 6) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x52, ctlValReg, sizeof (ctlValReg)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRateReg, sizeof (nominalRateReg)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        diffUpper = (ctlValReg[0] & 0x7F) - (nominalRateReg[0] & 0x7F);

        if ((diffUpper < -1) || (diffUpper > 1)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        trlCtlVal = ctlValReg[1] << 24;
        trlCtlVal |= ctlValReg[2] << 16;
        trlCtlVal |= ctlValReg[3] << 8;
        trlCtlVal |= ctlValReg[4];

        trcgNominalRate = nominalRateReg[1] << 24;
        trcgNominalRate |= nominalRateReg[2] << 16;
        trcgNominalRate |= nominalRateReg[3] << 8;
        trcgNominalRate |= nominalRateReg[4];

        trlCtlVal >>= 1;
        trcgNominalRate >>= 1;

        if (diffUpper == 1) {
            num = (int32_t)((trlCtlVal + 0x80000000u) - trcgNominalRate);
        } else if (diffUpper == -1) {
            num = -(int32_t)((trcgNominalRate + 0x80000000u) - trlCtlVal);
        } else {
            num = (int32_t)(trlCtlVal - trcgNominalRate);
        }

        den = (nominalRateReg[0] & 0x7F) << 24;
        den |= nominalRateReg[1] << 16;
        den |= nominalRateReg[2] << 8;
        den |= nominalRateReg[3];
        den = (den + (390625/2)) / 390625;

        den >>= 1;

        if (num >= 0) {
            *pPPM = (num + (den/2)) / den;
        }
        else {
            *pPPM = (num - (den/2)) / den;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t dvbt2_CalcQuality (cxdref_demod_t * pDemod,
                                        uint32_t ber, int32_t snr, uint8_t * pQuality)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    int32_t snrRel = 0;
    uint32_t berSQI = 0;
    cxdref_dvbt2_plp_constell_t qam;
    cxdref_dvbt2_plp_code_rate_t codeRate;

    static const int32_t snrNordigP1dB1000[4][8] = {
        {3500,  4700,   5600,   6600,   7200,   7700,  1300,  2200 },
        {8700,  10100,  11400,  12500,  13300,  13800, 6000,  7200 },
        {13000, 14800,  16200,  17700,  18700,  19400, 9800,  11100},
        {17000, 19400,  20800,  22900,  24300,  25100, 13200, 14800},
    };

    CXDREF_TRACE_ENTER ("dvbt2_CalcQuality");

    if ((!pDemod) || (!pQuality)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbt2_monitor_QAM (pDemod, CXDREF_DVBT2_PLP_DATA, &qam);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_monitor_CodeRate (pDemod, CXDREF_DVBT2_PLP_DATA, &codeRate);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((codeRate > CXDREF_DVBT2_R2_5) || (qam > CXDREF_DVBT2_QAM256)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    if (ber > 100000) {
        berSQI = 0;
    }
    else if (ber >= 100) {
        berSQI = 6667;
    }
    else {
        berSQI = 16667;
    }

    snrRel = snr - snrNordigP1dB1000[qam][codeRate];

    if (snrRel < -3000) {
        *pQuality = 0;
    }
    else if (snrRel <= 3000) {
        uint32_t tempSQI = (((snrRel + 3000) * berSQI) + 500000) / 1000000;
        *pQuality = (tempSQI > 100)? 100 : (uint8_t) tempSQI;
    }
    else {
        *pQuality = 100;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_Quality (cxdref_demod_t * pDemod,
                                                uint8_t * pQuality)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t ber = 0;
    int32_t snr = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_Quality");

    if ((!pDemod) || (!pQuality)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbt2_monitor_PreBCHBER (pDemod, &ber);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_monitor_SNR (pDemod, &snr);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = dvbt2_CalcQuality (pDemod, ber, snr, pQuality);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_TSRate (cxdref_demod_t * pDemod, uint32_t * pTSRateKbps)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t rd_smooth_dp = 0;
    uint32_t ep_ck_nume = 0;
    uint32_t ep_ck_deno = 0;
    uint8_t issy_on_data = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_TSRate");

    if ((!pDemod) || (!pTSRateKbps)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[8];
        uint8_t syncState = 0;
        uint8_t tsLock = 0;
        uint8_t earlyLock = 0;
        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_demod_dvbt2_monitor_SyncStat (pDemod, &syncState, &tsLock, &earlyLock);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!tsLock) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE5, &data[0], 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        rd_smooth_dp = (uint32_t) ((data[0] & 0x1F) << 24);
        rd_smooth_dp |= (uint32_t) (data[1] << 16);
        rd_smooth_dp |= (uint32_t) (data[2] << 8);
        rd_smooth_dp |= (uint32_t) data[3];

        if (rd_smooth_dp < 214958) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x24) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x90, &data[0], 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        issy_on_data = data[0] & 0x01;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x25) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB2, &data[0], 8) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        ep_ck_nume = (uint32_t) ((data[0] & 0x3F) << 24);
        ep_ck_nume |= (uint32_t) (data[1] << 16);
        ep_ck_nume |= (uint32_t) (data[2] << 8);
        ep_ck_nume |= (uint32_t) data[3];

        ep_ck_deno = (uint32_t) ((data[4] & 0x3F) << 24);
        ep_ck_deno |= (uint32_t) (data[5] << 16);
        ep_ck_deno |= (uint32_t) (data[6] << 8);
        ep_ck_deno |= (uint32_t) data[7];

        SLVT_UnFreezeReg (pDemod);
    }

    if (issy_on_data) {
        if ((ep_ck_deno == 0) || (ep_ck_nume == 0) || (ep_ck_deno >= ep_ck_nume)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    {
        uint32_t ick = 96;
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        div = rd_smooth_dp;

        Q = ick * 262144U * 100U / div;
        R = ick * 262144U * 100U % div;

        R *= 5U;
        Q = Q * 5 + R / div;
        R = R % div;

        R *= 2U;
        Q = Q * 2 + R / div;
        R = R % div;

        if (R >= div/2) {
            *pTSRateKbps = Q + 1;
        }
        else {
            *pTSRateKbps = Q;
        }
    }

    if (issy_on_data) {

        uint32_t diff = ep_ck_nume - ep_ck_deno;

        while(diff > 0x7FFF){
            diff >>= 1;
            ep_ck_nume >>= 1;
        }

        *pTSRateKbps -= (*pTSRateKbps * diff + ep_ck_nume/2) / ep_ck_nume;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_PER (cxdref_demod_t * pDemod, uint32_t * pPER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t packetError = 0;
    uint32_t period = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_PER");

    if (!pDemod || !pPER) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t rdata[3];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x24) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFA, rdata, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((rdata[0] & 0x01) == 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        packetError = (rdata[1] << 8) | rdata[2];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xDC, rdata, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = 1U << (rdata[0] & 0x0F);
    }

    if ((period == 0) || (packetError > period)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        div = period;

        Q = (packetError * 1000) / div;
        R = (packetError * 1000) % div;

        R *= 1000;
        Q = Q * 1000 + R / div;
        R = R % div;

        if ((div != 1) && (R >= div/2)) {
            *pPER = Q + 1;
        }
        else {
            *pPER = Q;
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_QAM (cxdref_demod_t * pDemod,
                                            cxdref_dvbt2_plp_btype_t type,
                                            cxdref_dvbt2_plp_constell_t * pQAM)
{
    uint8_t data;
    uint8_t l1PostOk = 0;
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_QAM");

    if ((!pDemod) || (!pQAM)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &l1PostOk, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(l1PostOk & 0x01)) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (type == CXDREF_DVBT2_PLP_COMMON) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x74, &data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (data == 0) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6F, &data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }
    else {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, &data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    SLVT_UnFreezeReg (pDemod);

    *pQAM = (cxdref_dvbt2_plp_constell_t) (data & 0x07);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_CodeRate (cxdref_demod_t * pDemod,
                                                 cxdref_dvbt2_plp_btype_t type,
                                                 cxdref_dvbt2_plp_code_rate_t * pCodeRate)
{
    uint8_t data;
    uint8_t l1PostOk = 0;
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_CodeRate");

    if ((!pDemod) || (!pCodeRate)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &l1PostOk, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(l1PostOk & 0x01)) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (type == CXDREF_DVBT2_PLP_COMMON) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x74, &data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (data == 0) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6E, &data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }
    else {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5B, &data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    SLVT_UnFreezeReg (pDemod);

    *pCodeRate = (cxdref_dvbt2_plp_code_rate_t) (data & 0x07);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_Profile (cxdref_demod_t * pDemod, cxdref_dvbt2_profile_t * pProfile)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_Profile");

    if ((!pDemod) || (!pProfile)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x2E) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data;
        
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x20, &data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (data & 0x02) {
            if (data & 0x01) {
                *pProfile = CXDREF_DVBT2_PROFILE_LITE;
            }
            else {
                *pProfile = CXDREF_DVBT2_PROFILE_BASE;
            }
        }
        else {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_AveragedPreBCHBER (cxdref_demod_t * pDemod,
                                                          uint32_t * pBER)
{
    uint8_t rdata[32];
    uint8_t periodType = 0;
    uint32_t bitError = 0;
    int hNum = 0;
    uint32_t periodExp = 0;
    uint32_t periodNum = 0;
    uint32_t n_bch = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_AveragedPreBCHBER");

    if ((!pDemod) || (!pBER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7C, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[1] & 0x03) != 0x00) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    periodType = rdata[0] & 0x01;

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, rdata, 32) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (hNum = 0; hNum < 8; hNum++) {
        if ((rdata[hNum * 4] & 0x80) == 0x00) {
            break;
        }

        bitError += ((rdata[hNum * 4] & 0x7F) << 16) + (rdata[hNum * 4 + 1] << 8) + rdata[hNum * 4 + 2];
    }

    if (hNum == 0) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        cxdref_dvbt2_plp_fec_t plpFecType = CXDREF_DVBT2_FEC_LDPC_16K;
        cxdref_dvbt2_plp_code_rate_t plpCr = CXDREF_DVBT2_R1_2;

        static const uint16_t nBCHBitsLookup[2][8] = {
            {7200,  9720,  10800, 11880, 12600, 13320, 5400,  6480},
            {32400, 38880, 43200, 48600, 51840, 54000, 21600, 25920}
        };

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5E, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        plpFecType = (cxdref_dvbt2_plp_fec_t) (rdata[0] & 0x03);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5B, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        plpCr = (cxdref_dvbt2_plp_code_rate_t) (rdata[0] & 0x07);

        SLVT_UnFreezeReg (pDemod);

        if ((plpFecType > CXDREF_DVBT2_FEC_LDPC_64K) || (plpCr > CXDREF_DVBT2_R2_5)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        n_bch = nBCHBitsLookup[plpFecType][plpCr];
    }

    if (periodType == 0) {

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x72, rdata, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp = (rdata[0] & 0x0F);
        periodNum = hNum;
    } else {
        int i = 0;

        periodExp = 0xFF;

        for (i = 0; i < hNum; i++) {
            uint8_t expTmp = rdata[i * 4 + 3];

            if (periodExp > expTmp) {
                periodExp = expTmp;
            }
        }

        for (i = 0; i < hNum; i++) {
            periodNum += 1 << (rdata[i * 4 + 3] - periodExp);
        }
    }

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        if (periodExp >= 6) {
            div = (1U << (periodExp - 6)) * (n_bch / 40) * periodNum;

            Q = (bitError * 125) / div;
            R = (bitError * 125) % div;

            R *= 125;
            Q = Q * 125 + R / div;
            R = R % div;

            R *= 25;
            Q = Q * 25 + R / div;
            R = R % div;
        }
        else {
            div = (1U << periodExp) * (n_bch / 40) * periodNum;

            Q = (bitError * 100) / div;
            R = (bitError * 100) % div;

            R *= 500;
            Q = Q * 500 + R / div;
            R = R % div;

            R *= 500;
            Q = Q * 500 + R / div;
            R = R % div;
        }

        if (R >= div/2) {
            *pBER = Q + 1;
        }
        else {
            *pBER = Q;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_AveragedSNR (cxdref_demod_t * pDemod,
                                                    int32_t * pSNR)
{
    uint8_t rdata[32];
    int hNum = 0;
    uint32_t reg = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_AveragedSNR");

    if ((!pDemod) || (!pSNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x30, rdata, 24) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        for (hNum = 0; hNum < 8; hNum++) {
            if ((rdata[hNum * 3] & 0x01) == 0x00) {
                break;
            }

            reg += (rdata[hNum * 3 + 1] << 8) + rdata[hNum * 3 + 2];
        }
    } else if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x30, rdata, 32) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        for (hNum = 0; hNum < 8; hNum++) {
            if ((rdata[hNum * 4] & 0x01) == 0x00) {
                break;
            }

            reg += (rdata[hNum * 4 + 2] << 8) + rdata[hNum * 4 + 3];
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    if (hNum == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    reg = (reg + hNum / 2) / hNum;

    if (reg > 10876) {
        reg = 10876;
    }

    *pSNR = 10 * 10 * ((int32_t) cxdref_math_log10 (reg) - (int32_t) cxdref_math_log10 (12600 - reg));
    *pSNR += 32000;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_AveragedQuality (cxdref_demod_t * pDemod,
                                                        uint8_t * pQuality)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t ber = 0;
    int32_t snr = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_AveragedQuality");

    if ((!pDemod) || (!pQuality)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbt2_monitor_AveragedPreBCHBER (pDemod, &ber);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_monitor_AveragedSNR (pDemod, &snr);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = dvbt2_CalcQuality (pDemod, ber, snr, pQuality);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
