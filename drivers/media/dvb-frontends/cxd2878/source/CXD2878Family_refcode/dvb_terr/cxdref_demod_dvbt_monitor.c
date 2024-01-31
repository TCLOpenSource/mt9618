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
#include "cxdref_math.h"

static cxdref_result_t IsTPSLocked (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbt_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                uint8_t * pSyncStat,
                                                uint8_t * pTSLockStat,
                                                uint8_t * pUnlockDetected)
{
    uint8_t rdata = 0x00;
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_SyncStat");

    if ((!pDemod) || (!pSyncStat) || (!pTSLockStat) || (!pUnlockDetected)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pUnlockDetected = (uint8_t)((rdata & 0x10)? 1 : 0);
    *pSyncStat = (uint8_t)(rdata & 0x07);
    *pTSLockStat = (uint8_t)((rdata & 0x20) ? 1 : 0);

    if (*pSyncStat == 0x07){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                uint32_t * pIFAGCOut)
{
    uint8_t rdata[2];

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_IFAGCOut");

    if ((!pDemod) || (!pIFAGCOut)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x26, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pIFAGCOut = ((rdata[0] & 0x0F) << 8) | rdata[1];

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_ModeGuard (cxdref_demod_t * pDemod,
                                                 cxdref_dvbt_mode_t * pMode,
                                                 cxdref_dvbt_guard_t * pGuard)
{
    uint8_t rdata = 0x00;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_ModeGuard");

    if ((!pDemod) || (!pMode) || (!pGuard)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsTPSLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
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

    *pMode = (cxdref_dvbt_mode_t) ((rdata >> 2) & 0x03);
    *pGuard = (cxdref_dvbt_guard_t) (rdata & 0x03);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                     int32_t * pOffset)
{
    uint8_t rdata[4];
    uint32_t ctlVal = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_CarrierOffset");

    if ((!pDemod) || (!pOffset)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsTPSLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4C, rdata, 4) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    ctlVal = ((rdata[0] & 0x1F) << 24) | (rdata[1] << 16) | (rdata[2] << 8) | (rdata[3]);
    *pOffset = cxdref_Convert2SComplement (ctlVal, 29);
    *pOffset = -1 * ((*pOffset) * (uint8_t)pDemod->bandwidth / 235);

    {
        if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
            *pOffset *= -1;
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_PreViterbiBER (cxdref_demod_t * pDemod,
                                                     uint32_t * pBER)
{
    uint8_t rdata[2];
    uint32_t bitError = 0;
    uint32_t period = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_PreViterbiBER");

    if ((!pDemod) || (!pBER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x39, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x01) == 0) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x22, rdata, 2) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    bitError = (rdata[0] << 8) | rdata[1];

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6F, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    period = ((rdata[0] & 0x07) == 0) ? 256 : (0x1000 << (rdata[0] & 0x07));

    if ((period == 0) || (bitError > period)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        div = period / 128;

        Q = (bitError * 3125) / div;
        R = (bitError * 3125) % div;

        R *= 25;
        Q = Q * 25 + R / div;
        R = R % div;

        if (R >= div/2) {
            *pBER = Q + 1;
        }
        else {
            *pBER = Q;
        }
    }
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                uint32_t * pBER)
{
    uint8_t rdata[3];
    uint32_t bitError = 0;
    uint32_t periodExp = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_PreRSBER");

    if ((!pDemod) || (!pBER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x62, rdata, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x80) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    bitError = ((rdata[0] & 0x3F) << 16) | (rdata[1] << 8) | rdata[2];

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    periodExp = (rdata[0] & 0x1F);

    if ((periodExp <= 11) && (bitError > (1U << periodExp) * 204 * 8)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        if (periodExp <= 8) {
            div = (1U << periodExp) * 51;
        }
        else {
            div = (1U << 8) * 51;
        }

        Q = (bitError * 250) / div;
        R = (bitError * 250) % div;

        R *= 250;
        Q = Q * 250 + R / div;
        R = R % div;

        if (periodExp > 8) {
            *pBER = (Q + (1 << (periodExp - 9))) >> (periodExp - 8);
        }
        else {
            if (R >= div/2) {
                *pBER = Q + 1;
            }
            else {
                *pBER = Q;
            }
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_TPSInfo (cxdref_demod_t * pDemod,
                                               cxdref_dvbt_tpsinfo_t * pInfo)
{
    uint8_t rdata[7];

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_TPSInfo");

    if ((!pDemod) || (!pInfo)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsTPSLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2F, rdata, 7) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    pInfo->constellation = (cxdref_dvbt_constellation_t) ((rdata[0] >> 6) & 0x03);
    pInfo->hierarchy = (cxdref_dvbt_hierarchy_t) ((rdata[0] >> 3) & 0x07);
    pInfo->rateHP = (cxdref_dvbt_coderate_t) (rdata[0] & 0x07);
    pInfo->rateLP = (cxdref_dvbt_coderate_t) ((rdata[1] >> 5) & 0x07);
    pInfo->guard = (cxdref_dvbt_guard_t) ((rdata[1] >> 3) & 0x03);
    pInfo->mode = (cxdref_dvbt_mode_t) ((rdata[1] >> 1) & 0x03);
    pInfo->fnum = (rdata[2] >> 6) & 0x03;
    pInfo->lengthIndicator = rdata[2] & 0x3F;
    pInfo->cellID = (uint16_t) ((rdata[3] << 8) | rdata[4]);
    pInfo->reservedEven = rdata[5] & 0x3F;
    pInfo->reservedOdd = rdata[6] & 0x3F;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_PacketErrorNumber (cxdref_demod_t * pDemod,
                                                         uint32_t * pPEN)
{
    uint8_t rdata[3];

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_PacketErrorNumber");

    if ((!pDemod) || (!pPEN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xEA, rdata, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(rdata[2] & 0x01)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *pPEN = (rdata[0] << 8) | rdata[1];

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                     cxdref_demod_terr_cable_spectrum_sense_t * pSense)
{
    uint8_t data = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_SpectrumSense");

    if ((!pDemod) || (!pSense)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsTPSLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3F, &data, sizeof (data)) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
        *pSense = (data & 0x01) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV;
    } else {
        *pSense = (data & 0x01) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_SNR (cxdref_demod_t * pDemod,
                                           int32_t * pSNR)
{
    uint16_t reg = 0;
    uint8_t rdata[2];

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_SNR");

    if ((!pDemod) || (!pSNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsTPSLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x28, rdata, 2) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    reg = (rdata[0] << 8) | rdata[1];

    if (reg == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (reg > 4996) {
        reg = 4996;
    }

    *pSNR = 10 * 10 * ((int32_t) cxdref_math_log10 (reg) - (int32_t) cxdref_math_log10 (5350 - reg));
    *pSNR += 28500;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pPPM)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_SamplingOffset");

    if ((!pDemod) || (!pPPM)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t ctlValReg[5];
        uint8_t nominalRateReg[5];
        uint32_t trlCtlVal = 0;
        uint32_t trcgNominalRate = 0;
        int32_t num;
        int32_t den;
        int8_t diffUpper = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = IsTPSLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
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

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t dvbt_CalcQuality (cxdref_demod_t * pDemod,
                                       uint32_t ber, int32_t snr, uint8_t * pQuality)
{
    cxdref_dvbt_tpsinfo_t tps;
    cxdref_dvbt_profile_t profile = CXDREF_DVBT_PROFILE_HP;
    int32_t snRel = 0;
    int32_t berSQI = 0;

    static const int32_t nordigNonHDVBTdB1000[3][5] = {
        {5100,  6900,  7900,  8900,  9700},
        {10800, 13100, 14600, 15600, 16000},
        {16500, 18700, 20200, 21600, 22500}
    };

    static const int32_t nordigHierHpDVBTdB1000[3][2][5] = {
        {
            {9100,   12000,  13600,  15000,  16600},
            {10900,  14100,  15700,  19400,  20600}
        },
        {
            {6800,   9100,   10400,  11900,  12700},
            {8500,   11000,  12800,  15000,  16000}
        },
        {
            {5800,   7900,   9100,   10300,  12100},
            {8000,   9300,   11600,  13000,  12900}
        }
    };
    static const int32_t nordigHierLpDVBTdB1000[3][2][5] = {
        {
            {12500,  14300,  15300,  16300,  16900},
            {16700,  19100,  20900,  22500,  23700}
        },
        {
            {15000,  17200,  18400,  19100,  20100},
            {18500,  21200,  23600,  24700,  25900}
        },
        {
            {19500,  21400,  22500,  23700,  24700},
            {21900,  24200,  25600,  26900,  27800}
        }
    };

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("dvbt_calcQuality");

    if ((!pDemod) || (!pQuality)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbt_monitor_TPSInfo (pDemod, &tps);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (tps.hierarchy != CXDREF_DVBT_HIERARCHY_NON) {
        uint8_t data = 0;
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x67, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        profile = ((data & 0x01) == 0x01) ? CXDREF_DVBT_PROFILE_LP : CXDREF_DVBT_PROFILE_HP;
    }

    if ((tps.constellation >= CXDREF_DVBT_CONSTELLATION_RESERVED_3) ||
        (tps.rateHP >= CXDREF_DVBT_CODERATE_RESERVED_5) ||
        (tps.rateLP >= CXDREF_DVBT_CODERATE_RESERVED_5) ||
        (tps.hierarchy > CXDREF_DVBT_HIERARCHY_4)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    if ((tps.hierarchy != CXDREF_DVBT_HIERARCHY_NON) && (tps.constellation == CXDREF_DVBT_CONSTELLATION_QPSK)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    if (tps.hierarchy == CXDREF_DVBT_HIERARCHY_NON) {
        snRel = snr - nordigNonHDVBTdB1000[tps.constellation][tps.rateHP];
    }
    else if ( profile == CXDREF_DVBT_PROFILE_LP ) {
        snRel = snr - nordigHierLpDVBTdB1000[(int32_t)tps.hierarchy-1][(int32_t)tps.constellation-1][tps.rateLP];
    }
    else {
        snRel = snr - nordigHierHpDVBTdB1000[(int32_t)tps.hierarchy-1][(int32_t)tps.constellation-1][tps.rateHP];
    }

    if (ber > 10000) {
        berSQI = 0;
    }
    else if (ber > 1) {
        berSQI = (int32_t) (10 * cxdref_math_log10 (ber));
        berSQI = 20 * (7 * 1000 - (berSQI)) - 40 * 1000;
    }
    else {
        berSQI = 100 * 1000;
    }

    if (snRel < -7 * 1000) {
        *pQuality = 0;
    }
    else if (snRel < 3 * 1000) {
        int32_t tmpSQI = (((snRel - (3 * 1000)) / 10) + 1000);
        *pQuality = (uint8_t) (((tmpSQI * berSQI) + (1000000/2)) / (1000000)) & 0xFF;
    }
    else {
        *pQuality = (uint8_t) ((berSQI + 500) / 1000);
    }

    if (*pQuality > 100) {
        *pQuality = 100;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_Quality (cxdref_demod_t * pDemod,
                                               uint8_t * pQuality)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t ber = 0;
    int32_t snr = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_Quality");

    if ((!pDemod) || (!pQuality)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbt_monitor_PreRSBER (pDemod, &ber);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt_monitor_SNR (pDemod, &snr);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = dvbt_CalcQuality (pDemod, ber, snr, pQuality);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt_monitor_PER (cxdref_demod_t * pDemod,
                                           uint32_t * pPER)
{
    uint32_t packetError = 0;
    uint32_t period = 0;
    uint8_t rdata[4];

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_PER");

    if ((!pDemod) || (!pPER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, rdata, 4) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[1] & 0x01) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    packetError = (rdata[2] << 8) | rdata[3];
    period = 1U << (rdata[0] & 0x0F);

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

cxdref_result_t cxdref_demod_dvbt_monitor_TSRate (cxdref_demod_t * pDemod,
                                              uint32_t * pTSRateKbps)
{
    cxdref_dvbt_tpsinfo_t tps;
    cxdref_result_t result = CXDREF_RESULT_OK;

    const uint8_t CRx24[5] = {12, 16, 18, 20, 21};
    const uint8_t GIx32[4] = {1, 2, 4, 8};
    const uint8_t MOD_NonH[3] = {2, 4, 6};
    const uint8_t MOD_HierHp[3] = {2, 2, 2};
    const uint8_t MOD_HierLp[3] = {0, 2, 4};

    uint8_t MOD_val = 0;
    uint8_t CRx24_val = 0;
    uint32_t div = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_TSRate");

    if ((!pDemod) || (!pTSRateKbps)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbt_monitor_TPSInfo (pDemod, &tps);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((tps.constellation >= CXDREF_DVBT_CONSTELLATION_RESERVED_3) ||
        (tps.rateHP >= CXDREF_DVBT_CODERATE_RESERVED_5) ||
        (tps.rateLP >= CXDREF_DVBT_CODERATE_RESERVED_5) ||
        (tps.hierarchy > CXDREF_DVBT_HIERARCHY_4) ||
        (tps.guard > CXDREF_DVBT_GUARD_1_4)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    if ((tps.hierarchy != CXDREF_DVBT_HIERARCHY_NON) && (tps.constellation == CXDREF_DVBT_CONSTELLATION_QPSK)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    if (tps.hierarchy == CXDREF_DVBT_HIERARCHY_NON) {
        MOD_val = MOD_NonH[tps.constellation];
        CRx24_val = CRx24[tps.rateHP];
    } else {
        uint8_t data = 0;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x67, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (data & 0x01) {
            MOD_val = MOD_HierLp[tps.constellation];
            CRx24_val = CRx24[tps.rateLP];
        } else {
            MOD_val = MOD_HierHp[tps.constellation];
            CRx24_val = CRx24[tps.rateHP];
        }
    }

    div = 119 * (32 + GIx32[tps.guard]);
    *pTSRateKbps = ((123375 * (uint32_t)pDemod->bandwidth * MOD_val * CRx24_val) + div / 2) / div;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt_monitor_AveragedPreRSBER (cxdref_demod_t * pDemod,
                                                        uint32_t * pBER)
{
    uint8_t rdata[32];
    uint8_t periodType = 0;
    uint32_t bitError = 0;
    int hNum = 0;
    uint32_t periodExp = 0;
    uint32_t periodNum = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_AveragedPreRSBER");

    if ((!pDemod) || (!pBER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x84, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[1] & 0x03) != 0x00) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    periodType = rdata[0] & 0x01;

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, rdata, 32) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (hNum = 0; hNum < 8; hNum++) {
        if ((rdata[hNum * 4] & 0x80) == 0x00) {
            break;
        }

        bitError += ((rdata[hNum * 4] & 0x7F) << 16) + (rdata[hNum * 4 + 1] << 8) + rdata[hNum * 4 + 2];
    }

    if (hNum == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (periodType == 0) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, rdata, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp = (rdata[0] & 0x1F);
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

        if (periodExp <= 8) {
            div = (1U << periodExp) * 51 * periodNum;
        }
        else {
            div = (1U << 8) * 51 * periodNum;
        }

        Q = (bitError * 50) / div;
        R = (bitError * 50) % div;

        R *= 1250;
        Q = Q * 1250 + R / div;
        R = R % div;

        if (periodExp > 8) {
            *pBER = (Q + (1 << (periodExp - 9))) >> (periodExp - 8);
        }
        else {
            if (R >= div/2) {
                *pBER = Q + 1;
            }
            else {
                *pBER = Q;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt_monitor_AveragedSNR (cxdref_demod_t * pDemod,
                                                   int32_t * pSNR)
{
    uint8_t rdata[32];
    int hNum = 0;
    uint32_t reg = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_AveragedSNR");

    if ((!pDemod) || (!pSNR)) {
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

    if (reg > 4996) {
        reg = 4996;
    }

    *pSNR = 10 * 10 * ((int32_t) cxdref_math_log10 (reg) - (int32_t) cxdref_math_log10 (5350 - reg));
    *pSNR += 28500;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt_monitor_AveragedQuality (cxdref_demod_t * pDemod,
                                                       uint8_t * pQuality)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t ber = 0;
    int32_t snr = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_AveragedQuality");

    if ((!pDemod) || (!pQuality)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbt_monitor_AveragedPreRSBER (pDemod, &ber);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt_monitor_AveragedSNR (pDemod, &snr);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = dvbt_CalcQuality (pDemod, ber, snr, pQuality);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t IsTPSLocked (cxdref_demod_t * pDemod)
{
    uint8_t sync = 0;
    uint8_t tslock = 0;
    uint8_t earlyUnlock = 0;
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("IsTPSLocked");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbt_monitor_SyncStat (pDemod, &sync, &tslock, &earlyUnlock);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (sync != 6) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
