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

#include "cxdref_demod_dvbc_monitor.h"
#include "cxdref_math.h"
#include "cxdref_common.h"

static cxdref_result_t IsARLocked (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbc_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                uint8_t * pARLock,
                                                uint8_t * pTSLockStat,
                                                uint8_t * pUnlockDetected)
{
    uint8_t rdata = 0x00;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_SyncStat");

    if ((!pDemod) || (!pARLock) || (!pTSLockStat) || (!pUnlockDetected)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x88, &rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pARLock = (uint8_t) ((rdata & 0x01) ? 1 : 0);
    *pUnlockDetected = (uint8_t) ((rdata & 0x02) ? 1 : 0);

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pTSLockStat = (uint8_t) ((rdata & 0x20) ? 1 : 0);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_IFAGCOut (cxdref_demod_t * pDemod, uint32_t * pIFAGCOut)
{
    uint8_t rdata[2];

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_IFAGCOut");

    if (!pDemod || !pIFAGCOut) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x49, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pIFAGCOut = (rdata[0] & 0x0F) << 8 | rdata[1];

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_QAM (cxdref_demod_t * pDemod, cxdref_dvbc_constellation_t * pQAM)
{
    uint8_t rdata = 0x00;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_QAM");

    if (!pDemod || !pQAM) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        cxdref_result_t result = IsARLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x19, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pQAM = (cxdref_dvbc_constellation_t) (rdata & 0x07);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_SymbolRate (cxdref_demod_t * pDemod, uint32_t * pSymRate)
{
    uint8_t rdata1[2];
    uint8_t rdata2[5];
    uint32_t trialsrate;
    int32_t  tlfIntegral;
    uint32_t tsmDrate;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_SymbolRate");

    if (!pDemod || !pSymRate) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        cxdref_result_t result = IsARLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1A, rdata1, sizeof(rdata1)) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9B, rdata2, sizeof(rdata2)) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    trialsrate = ((rdata1[0] & 0x0F) << 8) | rdata1[1];
    tlfIntegral = (int32_t)((rdata2[0] << 24) | (rdata2[1] << 16) | (rdata2[2] << 8) | rdata2[3]);
    tsmDrate = rdata2[4] & 0x03;

    *pSymRate = (trialsrate * 46875 + 8) / 16;

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        uint8_t isMinus = 0;

        if (tlfIntegral < 0) {
            isMinus = 1;
            tlfIntegral *= -1;
        }

        div = 65536;

        Q = tlfIntegral / div;
        R = tlfIntegral % div;

        R *= 46875;
        Q = Q * 46875 + R / div;
        R = R % div;

        if (R >= div/2) {
            Q += 1;
        }

        Q = (Q + (1U << (11 + tsmDrate))) >> (12 + tsmDrate);

        if (isMinus) {
            *pSymRate -= Q;
        } else {
            *pSymRate += Q;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_CarrierOffset (cxdref_demod_t * pDemod, int32_t * pOffset)
{
    uint8_t rdata[2];

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_CarrierOffset");

    if (!pDemod || !pOffset) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        cxdref_result_t result = IsARLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x15, rdata, 2) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pOffset = cxdref_Convert2SComplement (((rdata[0] & 0x3F) << 8) | rdata[1], 14);

    *pOffset = *pOffset * 48000 / 16384;

    {
        if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
            *pOffset *= -1;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_SpectrumSense (cxdref_demod_t * pDemod, cxdref_demod_terr_cable_spectrum_sense_t * pSense)
{
    uint8_t rdata = 0x00;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_SpectrumSense");

    if (!pDemod || !pSense) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        cxdref_result_t result = IsARLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x19, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
        *pSense = (rdata & 0x80) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV;
    } else {
        *pSense = (rdata & 0x80) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_SNR (cxdref_demod_t * pDemod, int32_t * pSNR)
{
    cxdref_dvbc_constellation_t qam = CXDREF_DVBC_CONSTELLATION_16QAM;
    uint16_t reg = 0x00;
    uint8_t rdata[2];
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_SNR");

    if (!pDemod || !pSNR) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        result = IsARLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x19, &rdata[0], 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    qam = (cxdref_dvbc_constellation_t) (rdata[0] & 0x07);

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4C, rdata, 2) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    reg = ((rdata[0] & 0x1F) << 8) | rdata[1];

    if (reg == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    switch (qam) {
    case CXDREF_DVBC_CONSTELLATION_16QAM:
    case CXDREF_DVBC_CONSTELLATION_64QAM:
    case CXDREF_DVBC_CONSTELLATION_256QAM:
        if (reg < 126) {
            reg = 126;
        }

        *pSNR = -95 * (int32_t) cxdref_math_log (reg) + 95941;
        break;
    case CXDREF_DVBC_CONSTELLATION_32QAM:
    case CXDREF_DVBC_CONSTELLATION_128QAM:
        if (reg < 69) {
            reg = 69;
        }

        *pSNR = -88 * (int32_t) cxdref_math_log (reg) + 86999;
        break;
    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbc_monitor_PreRSBER (cxdref_demod_t * pDemod, uint32_t * pBER)
{
    uint8_t rdata[3];
    uint32_t bitError = 0;
    uint32_t periodExp = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_PreRSBER");

    if (!pDemod || !pBER) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    rdata[0] = rdata[1] = rdata[2] = 0;

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

        R *= 1250;
        Q = Q * 1250 + R / div;
        R = R % div;

        if (periodExp > 8) {
            *pBER = (Q + (1 << (periodExp - 9))) >> (periodExp - 8);
        }
        else {
            if (R >= div/2) {
                *pBER = Q + 1;
            } else {
                *pBER = Q;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_PacketErrorNumber (cxdref_demod_t * pDemod, uint32_t * pPEN)
{
    uint8_t rdata[3];

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_PacketErrorNumber");

    if (!pDemod || !pPEN) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xEA, rdata, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(rdata[2] & 0x01)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *pPEN = (rdata[0] << 8) | rdata[1];

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_PER (cxdref_demod_t * pDemod, uint32_t * pPER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t packetError = 0;
    uint32_t periodExp = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_PER");

    if (!pDemod || !pPER) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t rdata[4];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, rdata, sizeof (rdata)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((rdata[1] & 0x04) == 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        packetError = ((rdata[1] & 0x03) << 16) | (rdata[2] << 8) | rdata[3];
        periodExp = rdata[0] & 0x1F;
    }

    if (packetError > (1U << periodExp)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (periodExp == 0) {
        *pPER = packetError * 1000000;
    } else if (periodExp > 6) {
        *pPER = (packetError * 15625 + (1 << (periodExp - 7))) >> (periodExp - 6);
    } else {
        *pPER = (packetError * 1000000 + (1 << (periodExp - 1))) >> periodExp;
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t IsARLocked (cxdref_demod_t * pDemod)
{
    uint8_t ar = 0;
    uint8_t tslock = 0;
    uint8_t earlyUnlock = 0;
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("IsARLocked");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbc_monitor_SyncStat (pDemod, &ar, &tslock,&earlyUnlock);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (ar != 1) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
