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

#include "cxdref_demod_isdbt.h"
#include "cxdref_demod_isdbt_monitor.h"
#include "cxdref_math.h"

static cxdref_result_t IsDmdLocked (cxdref_demod_t * pDemod);

static cxdref_result_t setTmccInfo (uint8_t* pData, cxdref_isdbt_tmcc_info_t* pTmccInfo);

static cxdref_result_t checkLayerExistence (cxdref_demod_t * pDemod,
                                          uint8_t * pLayerAExist,
                                          uint8_t * pLayerBExist,
                                          uint8_t * pLayerCExist);

cxdref_result_t cxdref_demod_isdbt_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                 uint8_t * pDmdLockStat,
                                                 uint8_t * pTSLockStat,
                                                 uint8_t * pUnlockDetected)
{
    uint8_t rdata = 0x00;
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_SyncStat");

    if ((!pDemod) || (!pDmdLockStat) || (!pTSLockStat) || (!pUnlockDetected)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pUnlockDetected = (uint8_t)((rdata & 0x10)? 1 : 0);
    *pDmdLockStat = (uint8_t)((rdata & 0x02) ? 1 : 0);
    *pTSLockStat = (uint8_t)((rdata & 0x01) ? 1 : 0);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                 uint32_t * pIFAGCOut)
{
    uint8_t rdata[2];

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_IFAGCOut");

    if ((!pDemod) || (!pIFAGCOut)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x26, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pIFAGCOut = (((uint32_t)rdata[0] & 0x0F) << 8) | rdata[1];

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_ModeGuard (cxdref_demod_t * pDemod,
                                                  cxdref_isdbt_mode_t * pMode,
                                                  cxdref_isdbt_guard_t * pGuard)
{
    uint8_t rdata = 0x00;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_ModeGuard");

    if ((!pDemod) || (!pMode) || (!pGuard)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked(pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x40, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    switch ((rdata >> 2) & 0x03) {
    case 0:
        *pMode = CXDREF_ISDBT_MODE_1;
        break;
    case 1:
        *pMode = CXDREF_ISDBT_MODE_3;
        break;
    case 2:
        *pMode = CXDREF_ISDBT_MODE_2;
        break;
    default:
        *pMode = CXDREF_ISDBT_MODE_UNKNOWN;
        break;
    }

    *pGuard =( cxdref_isdbt_guard_t)(rdata & 0x03);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pOffset)
{
    uint8_t rdata[4] = {0x00, 0x00, 0x00, 0x00};
    uint32_t iregCrcgCtrlval = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_CarrierOffset");

    if ((!pDemod) || (!pOffset)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4C, rdata, 4) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    iregCrcgCtrlval = rdata[0] & 0x1F;
    iregCrcgCtrlval = (iregCrcgCtrlval << 8) + rdata[1];
    iregCrcgCtrlval = (iregCrcgCtrlval << 8) + rdata[2];
    iregCrcgCtrlval = (iregCrcgCtrlval << 8) + rdata[3];

    *pOffset = cxdref_Convert2SComplement(iregCrcgCtrlval, 29);

    switch (pDemod->bandwidth) {
    case CXDREF_DTV_BW_6_MHZ:
        *pOffset = -1 * ((*pOffset) * 8 / 264);
        break;
    case CXDREF_DTV_BW_7_MHZ:
        *pOffset = -1 * ((*pOffset) * 8 / 231);
        break;
    case CXDREF_DTV_BW_8_MHZ:
        *pOffset = -1 * ((*pOffset) * 8 / 198);
        break;
    default:
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
            *pOffset *= -1;
        }
    }

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_demod_isdbt_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                 uint32_t * pBERA, uint32_t * pBERB, uint32_t * pBERC)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t dataPacketNum[2];
    uint8_t dataBitError[9];
    uint32_t bitError[3] = {0};
    uint32_t packetNum;
    uint32_t ber[3] = {0};
    uint8_t layerExist[3] = {0};
    int i = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_PreRSBER");

    if ((!pDemod) || (!pBERA) || (!pBERB) || (!pBERC)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->isdbtEwsState != CXDREF_DEMOD_ISDBT_EWS_STATE_NORMAL) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = checkLayerExistence (pDemod, &layerExist[0], &layerExist[1], &layerExist[2]);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x16, dataBitError, 9) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg(pDemod);

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5B, dataPacketNum, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    packetNum = ((uint32_t)dataPacketNum[0] << 8) | dataPacketNum[1];
    if (packetNum == 0) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }

    for (i = 0; i < 3; i++) {
        if (!layerExist[i]) {
            continue;
        }

        bitError[i] = ((uint32_t)(dataBitError[0 + (3 * i)] & 0x7F) << 16) | ((uint32_t)dataBitError[1 + (3 * i)] << 8) | dataBitError[2 + (3 * i)];

        if (bitError[i] / 8 / 204 > packetNum) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    for (i = 0; i < 3; i++) {
        uint32_t Div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        if (!layerExist[i]) {
            continue;
        }

        Div = packetNum * 51;

        Q = (bitError[i] * 250) / Div;
        R = (bitError[i] * 250) % Div;

        R *= 1250;
        Q = Q * 1250 + R / Div;
        R = R % Div;

        if (R >= (Div/2)) {
            ber[i] = Q + 1;
        } else {
            ber[i] = Q;
        }
    }

    *pBERA = layerExist[0] ? ber[0] : CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID;
    *pBERB = layerExist[1] ? ber[1] : CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID;
    *pBERC = layerExist[2] ? ber[2] : CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID;

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_PacketErrorNumber (cxdref_demod_t * pDemod,
                                                          uint32_t * pPENA, uint32_t * pPENB, uint32_t * pPENC)
{
    uint8_t rdata[7];

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_PacketErrorNumber");

    if ((!pDemod) || (!pPENA) || (!pPENB) || (!pPENC)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->isdbtEwsState != CXDREF_DEMOD_ISDBT_EWS_STATE_NORMAL) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->ReadRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA1, rdata, 7);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    if ((rdata[0] & 0x01) == 0) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }

    *pPENA = (uint32_t)((rdata[1] << 8) | rdata[2]);
    *pPENB = (uint32_t)((rdata[3] << 8) | rdata[4]);
    *pPENC = (uint32_t)((rdata[5] << 8) | rdata[6]);

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                      cxdref_demod_terr_cable_spectrum_sense_t * pSense)
{
    uint8_t data = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_SpectrumSense");

    if ((!pDemod) || (!pSense)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->ReadRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3F, &data, 1);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    SLVT_UnFreezeReg(pDemod);

    if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
        *pSense = (data & 0x01) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV;
    } else {
        *pSense = (data & 0x01) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL;
    }

    CXDREF_TRACE_RETURN(result);

}

cxdref_result_t cxdref_demod_isdbt_monitor_SNR (cxdref_demod_t * pDemod,
                                            int32_t * pSNR)
{
    uint8_t rdata[2] = {0x00, 0x00};
    uint16_t iregSnmonOd = 0x0000;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_SNR");

    if ((!pDemod) || (!pSNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->ReadRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x28, rdata, 2);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    SLVT_UnFreezeReg(pDemod);

    iregSnmonOd = rdata[0];
    iregSnmonOd = (iregSnmonOd << 8) + rdata[1];

    if (iregSnmonOd == 0) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }

    *pSNR = 100 * (int32_t)cxdref_math_log10(iregSnmonOd) - 9031;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pPPM)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_SamplingOffset");

    if ((!pDemod) || (!pPPM)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t ctlValReg[5];
        uint8_t nominalRateReg[5];
        uint32_t trlCtlVal = 0;
        uint32_t trcgNominalRate = 0;
        int32_t num = 0;
        int32_t den = 0;
        int8_t diffUpper = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = IsDmdLocked (pDemod);
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

        diffUpper = (int8_t)((ctlValReg[0] & 0x7F) - (nominalRateReg[0] & 0x7F));

        if ((diffUpper < -1) || (diffUpper > 1)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        trlCtlVal =  (uint32_t)ctlValReg[1] << 24;
        trlCtlVal |= (uint32_t)ctlValReg[2] << 16;
        trlCtlVal |= (uint32_t)ctlValReg[3] << 8;
        trlCtlVal |= (uint32_t)ctlValReg[4];

        trcgNominalRate  = (uint32_t)nominalRateReg[1] << 24;
        trcgNominalRate |= (uint32_t)nominalRateReg[2] << 16;
        trcgNominalRate |= (uint32_t)nominalRateReg[3] << 8;
        trcgNominalRate |= (uint32_t)nominalRateReg[4];

        trlCtlVal >>= 1;
        trcgNominalRate >>= 1;

        if (diffUpper == 1) {
            if (trlCtlVal > trcgNominalRate) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
            else {
                num = (int32_t)((trlCtlVal + 0x80000000UL) - trcgNominalRate);
            }
        } else if (diffUpper == -1) {
            if (trcgNominalRate > trlCtlVal) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
            else {
                num = -(int32_t)((trcgNominalRate + 0x80000000UL) - trlCtlVal);
            }
        } else {
            num = (int32_t)(trlCtlVal - trcgNominalRate);
        }

        den =  (int32_t)((((uint32_t)nominalRateReg[0] & 0x7F) << 24) |
                          ((uint32_t)nominalRateReg[1] << 16)        |
                          ((uint32_t)nominalRateReg[2] << 8)         |
                          ((uint32_t)nominalRateReg[3]));
        den = (den + (390625/2)) / 390625;

        den /= 2;

        if (den == 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (num >= 0) {
            *pPPM = (num + (den/2)) / den;
        }
        else {
            *pPPM = (num - (den/2)) / den;
        }
    }

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_demod_isdbt_monitor_PER (cxdref_demod_t * pDemod,
                                            uint32_t * pPERA, uint32_t * pPERB, uint32_t * pPERC)
{
    uint8_t dataPacketError[6];
    uint8_t dataPacketNum[2];
    uint32_t packetError[3] = {0};
    uint32_t packetNum = 0;
    uint32_t per[3] = {0};
    uint8_t layerExist[3] = {0};
    int i = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_PER");

    if ((!pDemod) || (!pPERA) || (!pPERB) || (!pPERC)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->isdbtEwsState != CXDREF_DEMOD_ISDBT_EWS_STATE_NORMAL) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked(pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = checkLayerExistence (pDemod, &layerExist[0], &layerExist[1], &layerExist[2]);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1F, dataPacketError, 6) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg(pDemod);

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5B, dataPacketNum, 2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    packetNum = ((uint32_t)dataPacketNum[0] << 8) + dataPacketNum[1];
    if (packetNum == 0) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }

    for (i = 0; i < 3; i++) {
        if (!layerExist[i]) {
            continue;
        }

        packetError[i] = ((uint32_t)dataPacketError[0 + (2 * i)] << 8) + dataPacketError[1 + (2 * i)];

        if (packetError[i] > packetNum) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    for (i = 0; i < 3; i++) {
        uint32_t Div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        if (!layerExist[i]) {
            continue;
        }

        Div = packetNum;
        Q = (packetError[i] * 1000) / Div;
        R = (packetError[i] * 1000) % Div;

        R *= 1000;
        Q = Q * 1000 + R / Div;
        R = R % Div;

        if ((Div != 1) && (R >= (Div / 2))) {
            per[i] = Q + 1;
        } else {
            per[i] = Q;
        }
    }

    *pPERA = layerExist[0] ? per[0] : CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID;
    *pPERB = layerExist[1] ? per[1] : CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID;
    *pPERC = layerExist[2] ? per[2] : CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID;

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_TSRate (cxdref_demod_t * pDemod,
                                               uint32_t * pTSRateKbpsA, uint32_t * pTSRateKbpsB, uint32_t * pTSRateKbpsC)
{
    const uint32_t dataRate1Seg6MHzBW[3][5][4] = {
        {
            { 34043,  33042,  31206,  28085},
            { 45391,  44056,  41608,  37447},
            { 51065,  49563,  46809,  42128},
            { 56739,  55070,  52010,  46809},
            { 59576,  57823,  54611,  49150} 
        },
        {
            { 68087,  66084,  62413,  56171},
            { 90782,  88112,  83217,  74895},
            {102130,  99126,  93619,  84257},
            {113478, 110140, 104021,  93619},
            {119152, 115647, 109222,  98300} 
        },
        {
            {102130,  99126,  93619,  84257},
            {136174, 132168, 124826, 112343},
            {153195, 148690, 140429, 126386},
            {170217, 165211, 156032, 140429},
            {178728, 173471, 163834, 147450} 
        }
    };

    const uint32_t dataRate1Seg7MHzBW[3][5][4] = {
        {
            { 39717,  38549,  36407,  32766},
            { 52956,  51399,  48543,  43689},
            { 59576,  57823,  54611,  49150},
            { 66195,  64248,  60679,  54611},
            { 69505,  67461,  63713,  57342} 
        },
        {
            { 79434,  77098,  72815,  65533},
            {105913, 102798,  97087,  87378},
            {119152, 115647, 109222,  98300},
            {132391, 128497, 121358, 109222},
            {139011, 134922, 127426, 114684} 
        },
        {
            {119152, 115647, 109222,  98300},
            {158869, 154197, 145630, 131067},
            {178728, 173471, 163834, 147450},
            {198587, 192746, 182038, 163834},
            {208516, 202383, 191140, 172026} 
        }
    };

    const uint32_t dataRate1Seg8MHzBW[3][5][4] = {
        {
            { 45391,  44056,  41608,  37447},
            { 60521,  58741,  55478,  49930},
            { 68087,  66084,  62413,  56171},
            { 75652,  73427,  69347,  62413},
            { 79434,  77098,  72815,  65533} 
        },
        {
            { 90782,  88112,  83217,  74895},
            {121043, 117483, 110956,  99860},
            {136174, 132168, 124826, 112343},
            {151304, 146854, 138695, 124826},
            {158869, 154197, 145630, 131067} 
        },
        {
            {136174, 132168, 124826, 112343},
            {181565, 176225, 166434, 149791},
            {204261, 198253, 187239, 168515},
            {226956, 220281, 208043, 187239},
            {238304, 231295, 218445, 196601} 
        }
    };

    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_isdbt_mode_t mode;
    cxdref_isdbt_guard_t guard;
    cxdref_isdbt_tmcc_info_t tmccInfo;
    uint8_t modIndex[3];
    uint8_t codeRateIndex[3];
    uint8_t segmentNum[3];
    uint32_t tsRate[3];
    int i = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_TSRate");

    if ((!pDemod) || (!pTSRateKbpsA) || (!pTSRateKbpsB) || (!pTSRateKbpsC)) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_isdbt_monitor_ModeGuard (pDemod, &mode, &guard);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (guard > CXDREF_ISDBT_GUARD_1_4) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    result = cxdref_demod_isdbt_monitor_TMCCInfo (pDemod, &tmccInfo);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    modIndex[0] = (uint8_t)tmccInfo.currentInfo.layerA.modulation;
    codeRateIndex[0] = (uint8_t)tmccInfo.currentInfo.layerA.codingRate;
    segmentNum[0] = (uint8_t)tmccInfo.currentInfo.layerA.segmentsNum;

    modIndex[1] = (uint8_t)tmccInfo.currentInfo.layerB.modulation;
    codeRateIndex[1] = (uint8_t)tmccInfo.currentInfo.layerB.codingRate;
    segmentNum[1] = (uint8_t)tmccInfo.currentInfo.layerB.segmentsNum;

    modIndex[2] = (uint8_t)tmccInfo.currentInfo.layerC.modulation;
    codeRateIndex[2] = (uint8_t)tmccInfo.currentInfo.layerC.codingRate;
    segmentNum[2] = (uint8_t)tmccInfo.currentInfo.layerC.segmentsNum;

    for (i = 0; i < 3; i++) {
        if((modIndex[i] > (uint8_t)CXDREF_ISDBT_MODULATION_64QAM) || (codeRateIndex[i] > CXDREF_ISDBT_CODING_RATE_7_8)
            || (segmentNum[i] > 13)){
            tsRate[i] = 0;
            continue;
        }

        if(modIndex[i] != 0){
            modIndex[i]--;
        }

        switch(pDemod->bandwidth){
        case CXDREF_DTV_BW_6_MHZ:
            tsRate[i] = dataRate1Seg6MHzBW[modIndex[i]][codeRateIndex[i]][guard] * segmentNum[i];
            break;
        case CXDREF_DTV_BW_7_MHZ:
            tsRate[i] = dataRate1Seg7MHzBW[modIndex[i]][codeRateIndex[i]][guard] * segmentNum[i];
            break;
        case CXDREF_DTV_BW_8_MHZ:
            tsRate[i] = dataRate1Seg8MHzBW[modIndex[i]][codeRateIndex[i]][guard] * segmentNum[i];
            break;
        default:
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
        }

        tsRate[i] = (tsRate[i] + 50) / 100;
    }

    *pTSRateKbpsA = tsRate[0];
    *pTSRateKbpsB = tsRate[1];
    *pTSRateKbpsC = tsRate[2];

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbt_monitor_PresetInfo(cxdref_demod_t*                   pDemod,
                                                  cxdref_demod_isdbt_preset_info_t* pPresetInfo)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER("cxdref_demod_isdbt_monitor_PresetInfo");

    if ((!pDemod) || (!pPresetInfo)) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked(pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->ReadRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, pPresetInfo->data, 13);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    SLVT_UnFreezeReg(pDemod);

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_TMCCInfo(cxdref_demod_t*           pDemod,
                                                cxdref_isdbt_tmcc_info_t * pTMCCInfo)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[13] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CXDREF_TRACE_ENTER("cxdref_demod_isdbt_monitor_TMCCInfo");

    if ((!pDemod) || (!pTMCCInfo)) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked(pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->ReadRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, data, 13);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    SLVT_UnFreezeReg(pDemod);

    result = setTmccInfo(data, pTMCCInfo);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_ACEEWInfo(cxdref_demod_t * pDemod,
                                                 uint8_t * pIsExist,
                                                 cxdref_isdbt_aceew_info_t * pACEEWInfo)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[36];
    CXDREF_TRACE_ENTER("cxdref_demod_isdbt_monitor_ACEEWInfo");

    if ((!pDemod) || (!pIsExist) || (!pACEEWInfo)) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked(pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }
    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x61);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->ReadRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x21, data, 36);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    SLVT_UnFreezeReg(pDemod);

    if(data[0] & 0x01) {
        uint8_t i = 0;
        *pIsExist = 1;

        pACEEWInfo->startEndFlag = (uint8_t)((data[1] >> 6) & 0x03);
        pACEEWInfo->updateFlag = (uint8_t)((data[1] >> 4) & 0x03);
        pACEEWInfo->signalId = (uint8_t)((data[1] >> 1) & 0x07);

        pACEEWInfo->isAreaValid = 0;
        pACEEWInfo->isEpicenter1Valid = 0;
        pACEEWInfo->isEpicenter2Valid = 0;
        if(data[2] & 0x04) {
            pACEEWInfo->isAreaValid = 1;
            for (i = 0; i < 11; i++) {
                pACEEWInfo->areaInfo.data[i] = data[3 + i];
            }
        }
        if(data[2] & 0x02) {
            pACEEWInfo->isEpicenter1Valid = 1;
            for (i = 0; i < 11; i++) {
                pACEEWInfo->epicenter1Info.data[i] = data[14 + i];
            }
        }
        if(data[2] & 0x01) {
            pACEEWInfo->isEpicenter2Valid = 1;
            for (i = 0; i < 11; i++) {
                pACEEWInfo->epicenter2Info.data[i] = data[25 + i];
            }
        }
    } else {
        *pIsExist = 0;
    }

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                  uint8_t * pEWSChange)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_EWSChange");

    if ((!pDemod) || (!pEWSChange)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x8F, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pEWSChange =  (uint8_t)((rdata & 0x01) ? 1 : 0);

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_TMCCChange (cxdref_demod_t * pDemod,
                                                   uint8_t * pTMCCChange)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_TMCCChange");

    if ((!pDemod) || (!pTMCCChange)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked(pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pTMCCChange =  (uint8_t)((rdata & 0x20) ? 1 : 0);

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_ClearEWSChange (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_ClearEWSChange");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x63);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3D, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_ClearTMCCChange (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_ClearTMCCChange");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6F, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}

static cxdref_result_t IsDmdLocked(cxdref_demod_t* pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t dmdLock = 0;
    uint8_t tsLock  = 0;
    uint8_t unlock  = 0;
    CXDREF_TRACE_ENTER("IsDmdLocked");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_isdbt_monitor_SyncStat(pDemod, &dmdLock, &tsLock, &unlock);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    if (dmdLock) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
    } else {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }
}

static cxdref_result_t setTmccInfo(uint8_t* pData, cxdref_isdbt_tmcc_info_t* pTmccInfo)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER("setTmccInfo");

    if(!pTmccInfo){
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    pTmccInfo->systemId = (cxdref_isdbt_system_t)cxdref_BitSplitFromByteArray(pData, 0, 2);
    pTmccInfo->countDownIndex = (uint8_t)cxdref_BitSplitFromByteArray(pData, 2, 4);
    pTmccInfo->ewsFlag = (uint8_t)cxdref_BitSplitFromByteArray(pData, 6, 1);


    pTmccInfo->currentInfo.isPartial = (uint8_t)cxdref_BitSplitFromByteArray(pData, 7, 1);

    pTmccInfo->currentInfo.layerA.modulation = (cxdref_isdbt_modulation_t)cxdref_BitSplitFromByteArray(pData, 8, 3);
    pTmccInfo->currentInfo.layerA.codingRate = (cxdref_isdbt_coding_rate_t)cxdref_BitSplitFromByteArray(pData, 11, 3);
    pTmccInfo->currentInfo.layerA.ilLength = (cxdref_isdbt_il_length_t)cxdref_BitSplitFromByteArray(pData, 14, 3);
    pTmccInfo->currentInfo.layerA.segmentsNum = (uint8_t)cxdref_BitSplitFromByteArray(pData, 17, 4);


    pTmccInfo->currentInfo.layerB.modulation = (cxdref_isdbt_modulation_t)cxdref_BitSplitFromByteArray(pData, 21, 3);
    pTmccInfo->currentInfo.layerB.codingRate = (cxdref_isdbt_coding_rate_t)cxdref_BitSplitFromByteArray(pData, 24, 3);
    pTmccInfo->currentInfo.layerB.ilLength = (cxdref_isdbt_il_length_t)cxdref_BitSplitFromByteArray(pData, 27, 3);
    pTmccInfo->currentInfo.layerB.segmentsNum = (uint8_t)cxdref_BitSplitFromByteArray(pData, 30, 4);


    pTmccInfo->currentInfo.layerC.modulation = (cxdref_isdbt_modulation_t)cxdref_BitSplitFromByteArray(pData, 34, 3);
    pTmccInfo->currentInfo.layerC.codingRate = (cxdref_isdbt_coding_rate_t)cxdref_BitSplitFromByteArray(pData, 37, 3);
    pTmccInfo->currentInfo.layerC.ilLength = (cxdref_isdbt_il_length_t)cxdref_BitSplitFromByteArray(pData, 40, 3);
    pTmccInfo->currentInfo.layerC.segmentsNum = (uint8_t)cxdref_BitSplitFromByteArray(pData, 43, 4);


    pTmccInfo->nextInfo.isPartial = (uint8_t)cxdref_BitSplitFromByteArray(pData, 47, 1);

    pTmccInfo->nextInfo.layerA.modulation = (cxdref_isdbt_modulation_t)cxdref_BitSplitFromByteArray(pData, 48, 3);
    pTmccInfo->nextInfo.layerA.codingRate = (cxdref_isdbt_coding_rate_t)cxdref_BitSplitFromByteArray(pData, 51, 3);
    pTmccInfo->nextInfo.layerA.ilLength = (cxdref_isdbt_il_length_t)cxdref_BitSplitFromByteArray(pData, 54, 3);
    pTmccInfo->nextInfo.layerA.segmentsNum = (uint8_t)cxdref_BitSplitFromByteArray(pData, 57, 4);


    pTmccInfo->nextInfo.layerB.modulation = (cxdref_isdbt_modulation_t)cxdref_BitSplitFromByteArray(pData, 61, 3);
    pTmccInfo->nextInfo.layerB.codingRate = (cxdref_isdbt_coding_rate_t)cxdref_BitSplitFromByteArray(pData, 64, 3);
    pTmccInfo->nextInfo.layerB.ilLength = (cxdref_isdbt_il_length_t)cxdref_BitSplitFromByteArray(pData, 67, 3);
    pTmccInfo->nextInfo.layerB.segmentsNum = (uint8_t)cxdref_BitSplitFromByteArray(pData, 70, 4);


    pTmccInfo->nextInfo.layerC.modulation = (cxdref_isdbt_modulation_t)cxdref_BitSplitFromByteArray(pData, 74, 3);
    pTmccInfo->nextInfo.layerC.codingRate = (cxdref_isdbt_coding_rate_t)cxdref_BitSplitFromByteArray(pData, 77, 3);
    pTmccInfo->nextInfo.layerC.ilLength = (cxdref_isdbt_il_length_t)cxdref_BitSplitFromByteArray(pData, 80, 3);
    pTmccInfo->nextInfo.layerC.segmentsNum = (uint8_t)cxdref_BitSplitFromByteArray(pData, 83, 4);

    CXDREF_TRACE_RETURN(result);
}

static cxdref_result_t checkLayerExistence (cxdref_demod_t * pDemod,
                                          uint8_t * pLayerAExist,
                                          uint8_t * pLayerBExist,
                                          uint8_t * pLayerCExist)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[4];
    cxdref_isdbt_modulation_t mod = CXDREF_ISDBT_MODULATION_UNUSED_7;

    CXDREF_TRACE_ENTER ("checkLayerExistence");

    if (!pDemod || !pLayerAExist || !pLayerBExist || !pLayerCExist) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, data, 4);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    mod = (cxdref_isdbt_modulation_t) cxdref_BitSplitFromByteArray (data, 8 - 8, 3);
    *pLayerAExist = (mod == CXDREF_ISDBT_MODULATION_UNUSED_7) ? 0 : 1;

    mod = (cxdref_isdbt_modulation_t) cxdref_BitSplitFromByteArray (data, 21 - 8, 3);
    *pLayerBExist = (mod == CXDREF_ISDBT_MODULATION_UNUSED_7) ? 0 : 1;

    mod = (cxdref_isdbt_modulation_t) cxdref_BitSplitFromByteArray (data, 34 - 8, 3);
    *pLayerCExist = (mod == CXDREF_ISDBT_MODULATION_UNUSED_7) ? 0 : 1;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
