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
#include "cxdref_demod_isdbs.h"
#include "cxdref_demod_isdbs_monitor.h"

static cxdref_result_t IsDmdLocked(cxdref_demod_t* pDemod);

cxdref_result_t cxdref_demod_isdbs_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                 uint8_t * pAGCLockStat,
                                                 uint8_t * pTSLockStat,
                                                 uint8_t * pTMCCLockStat)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[3];
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_SyncStat");

    if ((!pDemod) || (!pAGCLockStat) || (!pTSLockStat) || (!pTMCCLockStat)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pAGCLockStat = (uint8_t)((data[0] & 0x20) ? 1 : 0);
    *pTSLockStat = (uint8_t)((data[2] & 0x40) ? 1 : 0);
    *pTMCCLockStat = (uint8_t)((data[2] & 0x20) ? 1 : 0);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pOffset)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[3];
    uint32_t regValue = 0;
    int32_t cfrl_ctrlval = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_CarrierOffset");

    if ((!pDemod) || (!pOffset)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x45, data, 3) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    regValue = (((uint32_t)data[0] & 0x1F) << 16) | (((uint32_t)data[1] & 0xFF) <<  8) | ((uint32_t)data[2] & 0xFF);
    cfrl_ctrlval = cxdref_Convert2SComplement (regValue, 21);

    if(cfrl_ctrlval > 0){
        *pOffset = ((cfrl_ctrlval * (-125)) - 1024) / 2048;
    } else {
        *pOffset = ((cfrl_ctrlval * (-125)) + 1024) / 2048;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                 uint32_t * pIFAGCOut)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[2] = {0, 0};
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_IFAGCOut");

    if ((!pDemod) || (!pIFAGCOut)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1F, data, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pIFAGCOut = (((uint32_t)data[0] & 0x1F) << 8) | (uint32_t)(data[1] & 0xFF);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_CNR (cxdref_demod_t * pDemod,
                                            int32_t * pCNR)
{
    uint8_t rdata[3] = {0x00, 0x00, 0x00};
    uint16_t value = 0x0000;
    int32_t index = 0;
    int32_t minIndex = 0;
    int32_t maxIndex = 0;
    const struct {
        uint32_t value;
        int32_t cnr_x1000;
    } isdbs_cn_data[] = {
        {0x05af, 0},
        {0x0597, 100},
        {0x057e, 200},
        {0x0567, 300},
        {0x0550, 400},
        {0x0539, 500},
        {0x0522, 600},
        {0x050c, 700},
        {0x04f6, 800},
        {0x04e1, 900},
        {0x04cc, 1000},
        {0x04b6, 1100},
        {0x04a1, 1200},
        {0x048c, 1300},
        {0x0477, 1400},
        {0x0463, 1500},
        {0x044f, 1600},
        {0x043c, 1700},
        {0x0428, 1800},
        {0x0416, 1900},
        {0x0403, 2000},
        {0x03ef, 2100},
        {0x03dc, 2200},
        {0x03c9, 2300},
        {0x03b6, 2400},
        {0x03a4, 2500},
        {0x0392, 2600},
        {0x0381, 2700},
        {0x036f, 2800},
        {0x035f, 2900},
        {0x034e, 3000},
        {0x033d, 3100},
        {0x032d, 3200},
        {0x031d, 3300},
        {0x030d, 3400},
        {0x02fd, 3500},
        {0x02ee, 3600},
        {0x02df, 3700},
        {0x02d0, 3800},
        {0x02c2, 3900},
        {0x02b4, 4000},
        {0x02a6, 4100},
        {0x0299, 4200},
        {0x028c, 4300},
        {0x027f, 4400},
        {0x0272, 4500},
        {0x0265, 4600},
        {0x0259, 4700},
        {0x024d, 4800},
        {0x0241, 4900},
        {0x0236, 5000},
        {0x022b, 5100},
        {0x0220, 5200},
        {0x0215, 5300},
        {0x020a, 5400},
        {0x0200, 5500},
        {0x01f6, 5600},
        {0x01ec, 5700},
        {0x01e2, 5800},
        {0x01d8, 5900},
        {0x01cf, 6000},
        {0x01c6, 6100},
        {0x01bc, 6200},
        {0x01b3, 6300},
        {0x01aa, 6400},
        {0x01a2, 6500},
        {0x0199, 6600},
        {0x0191, 6700},
        {0x0189, 6800},
        {0x0181, 6900},
        {0x0179, 7000},
        {0x0171, 7100},
        {0x0169, 7200},
        {0x0161, 7300},
        {0x015a, 7400},
        {0x0153, 7500},
        {0x014b, 7600},
        {0x0144, 7700},
        {0x013d, 7800},
        {0x0137, 7900},
        {0x0130, 8000},
        {0x012a, 8100},
        {0x0124, 8200},
        {0x011e, 8300},
        {0x0118, 8400},
        {0x0112, 8500},
        {0x010c, 8600},
        {0x0107, 8700},
        {0x0101, 8800},
        {0x00fc, 8900},
        {0x00f7, 9000},
        {0x00f2, 9100},
        {0x00ec, 9200},
        {0x00e7, 9300},
        {0x00e2, 9400},
        {0x00dd, 9500},
        {0x00d8, 9600},
        {0x00d4, 9700},
        {0x00cf, 9800},
        {0x00ca, 9900},
        {0x00c6, 10000},
        {0x00c2, 10100},
        {0x00be, 10200},
        {0x00b9, 10300},
        {0x00b5, 10400},
        {0x00b1, 10500},
        {0x00ae, 10600},
        {0x00aa, 10700},
        {0x00a6, 10800},
        {0x00a3, 10900},
        {0x009f, 11000},
        {0x009b, 11100},
        {0x0098, 11200},
        {0x0095, 11300},
        {0x0091, 11400},
        {0x008e, 11500},
        {0x008b, 11600},
        {0x0088, 11700},
        {0x0085, 11800},
        {0x0082, 11900},
        {0x007f, 12000},
        {0x007c, 12100},
        {0x007a, 12200},
        {0x0077, 12300},
        {0x0074, 12400},
        {0x0072, 12500},
        {0x006f, 12600},
        {0x006d, 12700},
        {0x006b, 12800},
        {0x0068, 12900},
        {0x0066, 13000},
        {0x0064, 13100},
        {0x0061, 13200},
        {0x005f, 13300},
        {0x005d, 13400},
        {0x005b, 13500},
        {0x0059, 13600},
        {0x0057, 13700},
        {0x0055, 13800},
        {0x0053, 13900},
        {0x0051, 14000},
        {0x004f, 14100},
        {0x004e, 14200},
        {0x004c, 14300},
        {0x004a, 14400},
        {0x0049, 14500},
        {0x0047, 14600},
        {0x0045, 14700},
        {0x0044, 14800},
        {0x0042, 14900},
        {0x0041, 15000},
        {0x003f, 15100},
        {0x003e, 15200},
        {0x003c, 15300},
        {0x003b, 15400},
        {0x003a, 15500},
        {0x0038, 15600},
        {0x0037, 15700},
        {0x0036, 15800},
        {0x0034, 15900},
        {0x0033, 16000},
        {0x0032, 16100},
        {0x0031, 16200},
        {0x0030, 16300},
        {0x002f, 16400},
        {0x002e, 16500},
        {0x002d, 16600},
        {0x002c, 16700},
        {0x002b, 16800},
        {0x002a, 16900},
        {0x0029, 17000},
        {0x0028, 17100},
        {0x0027, 17200},
        {0x0026, 17300},
        {0x0025, 17400},
        {0x0024, 17500},
        {0x0023, 17600},
        {0x0022, 17800},
        {0x0021, 17900},
        {0x0020, 18000},
        {0x001f, 18200},
        {0x001e, 18300},
        {0x001d, 18500},
        {0x001c, 18700},
        {0x001b, 18900},
        {0x001a, 19000},
        {0x0019, 19200},
        {0x0018, 19300},
        {0x0017, 19500},
        {0x0016, 19700},
        {0x0015, 19900},
        {0x0014, 20000},
    };

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_CNR");

    if ((!pDemod) || (!pCNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, rdata, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x01) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    value = (uint16_t)((((uint32_t)rdata[1] & 0x1F) << 8) | rdata[2]);

    minIndex = 0;
    maxIndex = sizeof(isdbs_cn_data)/sizeof(isdbs_cn_data[0]) - 1;

    if(value >= isdbs_cn_data[minIndex].value) {
        *pCNR = isdbs_cn_data[minIndex].cnr_x1000;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }
    if(value <= isdbs_cn_data[maxIndex].value) {
        *pCNR = isdbs_cn_data[maxIndex].cnr_x1000;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    while (1) {
        index = (maxIndex + minIndex) / 2;

        if (value == isdbs_cn_data[index].value) {
            *pCNR = isdbs_cn_data[index].cnr_x1000;
            break;
        } else if (value > isdbs_cn_data[index].value) {
            maxIndex = index;
        } else {
            minIndex = index;
        }

        if ((maxIndex - minIndex) <= 1) {
            if (value == isdbs_cn_data[maxIndex].value) {
                *pCNR = isdbs_cn_data[maxIndex].cnr_x1000;
                break;
            } else {
                *pCNR = isdbs_cn_data[minIndex].cnr_x1000;
                break;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs_monitor_PER (cxdref_demod_t * pDemod,
                                            uint32_t * pPERH, uint32_t * pPERL)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata[4];
    uint32_t packetError[2] = {0};
    uint32_t measuredSlots[2] = {0};
    uint8_t valid[2] = {0};
    uint32_t * pPER[2];
    int i = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_PER");

    if ((!pDemod) || (!pPERH) || (!pPERL)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    pPER[0] = pPERH;
    pPER[1] = pPERL;

    result = IsDmdLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x79, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    valid[0] = (rdata[0] & 0x10) ? 0 : 1;
    valid[1] = (rdata[0] & 0x08) ? 0 : 1;

    if (valid[0]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x56, rdata, 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        packetError[0] = (rdata[0] << 8) | rdata[1];
        measuredSlots[0] = (rdata[2] << 8) | rdata[3];
    }

    if (valid[1]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, rdata, 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        packetError[1] = (rdata[0] << 8) | rdata[1];
        measuredSlots[1] = (rdata[2] << 8) | rdata[3];
    }

    SLVT_UnFreezeReg (pDemod);

    for (i = 0; i <= 1; i++) {
        uint32_t Div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        if (!valid[i] || (measuredSlots[i] == 0) || (measuredSlots[i] < packetError[i])) {
            *pPER[i] = CXDREF_DEMOD_ISDBS_MONITOR_PER_INVALID;
            continue;
        }

        Div = measuredSlots[i];

        Q = (packetError[i] * 1000) / Div;
        R = (packetError[i] * 1000) % Div;
        R *= 1000;
        Q = Q * 1000 + R / Div;
        R = R % Div;
        if ((Div != 1) && (R <= (Div/2))) {
            *pPER[i] = Q;
        }
        else {
            *pPER[i] = Q + 1;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs_monitor_ModCod (cxdref_demod_t * pDemod,
                                               cxdref_isdbs_modcod_t * pModCodH, cxdref_isdbs_modcod_t * pModCodL)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_ModCod");

    if ((!pDemod) || (!pModCodH) || (!pModCodL)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x8A, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pModCodH = (cxdref_isdbs_modcod_t)((rdata >> 4) & 0x0F);
    *pModCodL = (cxdref_isdbs_modcod_t)(rdata & 0x0F);

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_demod_isdbs_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                 uint32_t * pBERH, uint32_t * pBERL, uint32_t * pBERTMCC)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata[3];
    uint32_t bitError[3] = {0};
    uint32_t measuredSlots[3] = {0};
    uint8_t valid[3] = {0};
    uint32_t * pBER[3];
    int i = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_PreRSBER");

    if ((!pDemod) || (!pBERH) || (!pBERL) || (!pBERTMCC)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pBER[0] = pBERH;
    pBER[1] = pBERL;
    pBER[2] = pBERTMCC;

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x79, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    valid[0] = (rdata[0] & 0x10) ? 0 : 1;
    valid[1] = (rdata[0] & 0x08) ? 0 : 1;
    valid[2] = (rdata[0] & 0x01) ? 0 : 1;

    if (valid[0]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x50, rdata, 3) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[0] = ((rdata[0] & 0x7F) << 16) | (rdata[1] << 8) | rdata[2];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x58, rdata, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        measuredSlots[0] = (rdata[0] << 8) | rdata[1];
    }

    if (valid[1]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5A, rdata, 3) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[1] = ((rdata[0] & 0x7F) << 16) | (rdata[1] << 8) | rdata[2];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x62, rdata, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        measuredSlots[1] = (rdata[0] << 8) | rdata[1];
    }

    if (valid[2]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7B, rdata, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[2] = ((rdata[0] & 0x3F) << 8) | rdata[1];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7F, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        measuredSlots[2] = rdata[0];
    }

    SLVT_UnFreezeReg (pDemod);

    for (i = 0; i <= 2; i++) {
        uint32_t Div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        if (!valid[i] || (measuredSlots[i] == 0)) {
            *pBER[i] = CXDREF_DEMOD_ISDBS_MONITOR_PRERSBER_INVALID;
            continue;
        }

        if ((i == 0) || (i == 1)) {
            Div = measuredSlots[i] * 51;
            Q = (bitError[i] * 250) / Div;
            R = (bitError[i] * 250) % Div;

            R *= 1250;
            Q = Q * 1250 + R / Div;
            R = R % Div;

            if (R >= (Div/2)) {
                *pBER[i] = Q + 1;
            } else {
                *pBER[i] = Q;
            }
        } else {
            Div = measuredSlots[i] * 4;
            Q = (bitError[i] * 625) / Div;
            R = (bitError[i] * 625) % Div;

            R *= 125;
            Q = Q * 125 + R / Div;
            R = R % Div;

            if (R >= (Div/2)) {
                *pBER[i] = Q + 1;
            } else {
                *pBER[i] = Q;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs_monitor_TMCCInfo (cxdref_demod_t * pDemod,
                                                 cxdref_isdbs_tmcc_info_t * pTMCCInfo)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[57] = {0x00};
    uint32_t i = 0;
    CXDREF_TRACE_ENTER("cxdref_demod_isdbs_monitor_TMCCInfo");

    if ((!pDemod) || (!pTMCCInfo)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC9);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }
    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 1);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    pTMCCInfo->changeOrder = (data[0] & 0x1F);

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x11, data, 57);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    SLVT_UnFreezeReg (pDemod);

    for (i = 0; i < 4; i++) {
        pTMCCInfo->modcodSlotInfo[i].modCod = (cxdref_isdbs_modcod_t) (data[(2 * i)] & 0x0F);
        pTMCCInfo->modcodSlotInfo[i].slotNum = (data[(2 * i + 1)] & 0x3F);
    }

    for (i = 0;i < 24; i++) {
        pTMCCInfo->relativeTSForEachSlot[2 * i] =      ((data[i + 8] >> 4) & 0x07);
        pTMCCInfo->relativeTSForEachSlot[(2 * i) + 1] = (data[i + 8] & 0x07);
    }

    for (i = 0;i < 8; i++) {
        pTMCCInfo->tsidForEachRelativeTS[i] = (uint16_t)(((uint32_t)data[(2 * i) + 32] << 8) | data[(2 * i + 1) + 32]);
    }

    pTMCCInfo->ewsFlag = (uint8_t)((data[48] & 0x20) ? 1 : 0);
    pTMCCInfo->uplinkInfo = (uint8_t)((data[48] & 0x1E) >> 1);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_SlotNum (cxdref_demod_t * pDemod,
                                                uint8_t * pSlotNumH, uint8_t * pSlotNumL)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata[2];
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_SlotNum");

    if ((!pDemod) || (!pSlotNumH) || (!pSlotNumL)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x8B, rdata, 2) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pSlotNumH = rdata[0] & 0x3F;
    *pSlotNumL = rdata[1] & 0x3F;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_SiteDiversityInfo (cxdref_demod_t * pDemod,
                                                          uint8_t * pSiteDiversityInfo)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_SiteDiversityInfo");

    if ((!pDemod) || (!pSiteDiversityInfo)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pSiteDiversityInfo =  (uint8_t)((rdata & 0x02) ? 1 : 0);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                  uint8_t * pEWSChange)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_EWSChange");

    if ((!pDemod) || (!pEWSChange)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pEWSChange =  (uint8_t)((rdata & 0x04) ? 1 : 0);

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_TMCCChange (cxdref_demod_t * pDemod,
                                                   uint8_t * pTMCCChange)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_TMCCChange");

    if ((!pDemod) || (!pTMCCChange)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);
    *pTMCCChange =  (uint8_t)((rdata & 0x01) ? 1 : 0);

    CXDREF_TRACE_RETURN(result);
}
cxdref_result_t cxdref_demod_isdbs_ClearEWSChange (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_ClearEWSChange");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xED, 0x02);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}
cxdref_result_t cxdref_demod_isdbs_ClearTMCCChange (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_ClearTMCCChange");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xED, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_TSIDError (cxdref_demod_t * pDemod, uint8_t * pTSIDError)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_TSIDError");

    if ((!pDemod) || (!pTSIDError)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
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
    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC9);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);
    *pTSIDError =  (uint8_t)((rdata & 0x80) ? 1 : 0);

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_LowCN (cxdref_demod_t * pDemod,
                                              uint8_t * pLowCN)
{
    uint8_t rdata[4];
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_LowCN");

    if ((!pDemod) || (!pLowCN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, rdata, 4) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x01) == 0x00) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *pLowCN = (uint8_t)(rdata[3] & 0x01);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs_ClearLowCN (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_ClearLowCN");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x14, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t IsDmdLocked(cxdref_demod_t* pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t agcLock = 0;
    uint8_t tsLock  = 0;
    uint8_t tmccLock  = 0;
    CXDREF_TRACE_ENTER("IsDmdLocked");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_isdbs_monitor_SyncStat(pDemod, &agcLock, &tsLock, &tmccLock);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    if (tmccLock) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
    } else {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }
}
