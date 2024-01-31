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

#include "cxdref_common.h"
#include "cxdref_math.h"
#include "cxdref_demod.h"
#include "cxdref_demod_dvbs_s2.h"
#include "cxdref_demod_dvbs_s2_monitor.h"

static const struct {
    uint32_t value;
    int32_t cnr_x1000;
} s_cn_data[] = {
    {0x033e, 0},
    {0x0339, 100},
    {0x0333, 200},
    {0x032e, 300},
    {0x0329, 400},
    {0x0324, 500},
    {0x031e, 600},
    {0x0319, 700},
    {0x0314, 800},
    {0x030f, 900},
    {0x030a, 1000},
    {0x02ff, 1100},
    {0x02f4, 1200},
    {0x02e9, 1300},
    {0x02de, 1400},
    {0x02d4, 1500},
    {0x02c9, 1600},
    {0x02bf, 1700},
    {0x02b5, 1800},
    {0x02ab, 1900},
    {0x02a1, 2000},
    {0x029b, 2100},
    {0x0295, 2200},
    {0x0290, 2300},
    {0x028a, 2400},
    {0x0284, 2500},
    {0x027f, 2600},
    {0x0279, 2700},
    {0x0274, 2800},
    {0x026e, 2900},
    {0x0269, 3000},
    {0x0262, 3100},
    {0x025c, 3200},
    {0x0255, 3300},
    {0x024f, 3400},
    {0x0249, 3500},
    {0x0242, 3600},
    {0x023c, 3700},
    {0x0236, 3800},
    {0x0230, 3900},
    {0x022a, 4000},
    {0x0223, 4100},
    {0x021c, 4200},
    {0x0215, 4300},
    {0x020e, 4400},
    {0x0207, 4500},
    {0x0201, 4600},
    {0x01fa, 4700},
    {0x01f4, 4800},
    {0x01ed, 4900},
    {0x01e7, 5000},
    {0x01e0, 5100},
    {0x01d9, 5200},
    {0x01d2, 5300},
    {0x01cb, 5400},
    {0x01c4, 5500},
    {0x01be, 5600},
    {0x01b7, 5700},
    {0x01b1, 5800},
    {0x01aa, 5900},
    {0x01a4, 6000},
    {0x019d, 6100},
    {0x0196, 6200},
    {0x018f, 6300},
    {0x0189, 6400},
    {0x0182, 6500},
    {0x017c, 6600},
    {0x0175, 6700},
    {0x016f, 6800},
    {0x0169, 6900},
    {0x0163, 7000},
    {0x015c, 7100},
    {0x0156, 7200},
    {0x0150, 7300},
    {0x014a, 7400},
    {0x0144, 7500},
    {0x013e, 7600},
    {0x0138, 7700},
    {0x0132, 7800},
    {0x012d, 7900},
    {0x0127, 8000},
    {0x0121, 8100},
    {0x011c, 8200},
    {0x0116, 8300},
    {0x0111, 8400},
    {0x010b, 8500},
    {0x0106, 8600},
    {0x0101, 8700},
    {0x00fc, 8800},
    {0x00f7, 8900},
    {0x00f2, 9000},
    {0x00ee, 9100},
    {0x00ea, 9200},
    {0x00e6, 9300},
    {0x00e2, 9400},
    {0x00de, 9500},
    {0x00da, 9600},
    {0x00d7, 9700},
    {0x00d3, 9800},
    {0x00d0, 9900},
    {0x00cc, 10000},
    {0x00c7, 10100},
    {0x00c3, 10200},
    {0x00bf, 10300},
    {0x00ba, 10400},
    {0x00b6, 10500},
    {0x00b2, 10600},
    {0x00ae, 10700},
    {0x00aa, 10800},
    {0x00a7, 10900},
    {0x00a3, 11000},
    {0x009f, 11100},
    {0x009c, 11200},
    {0x0098, 11300},
    {0x0094, 11400},
    {0x0091, 11500},
    {0x008e, 11600},
    {0x008a, 11700},
    {0x0087, 11800},
    {0x0084, 11900},
    {0x0081, 12000},
    {0x007e, 12100},
    {0x007b, 12200},
    {0x0079, 12300},
    {0x0076, 12400},
    {0x0073, 12500},
    {0x0071, 12600},
    {0x006e, 12700},
    {0x006c, 12800},
    {0x0069, 12900},
    {0x0067, 13000},
    {0x0065, 13100},
    {0x0062, 13200},
    {0x0060, 13300},
    {0x005e, 13400},
    {0x005c, 13500},
    {0x005a, 13600},
    {0x0058, 13700},
    {0x0056, 13800},
    {0x0054, 13900},
    {0x0052, 14000},
    {0x0050, 14100},
    {0x004e, 14200},
    {0x004c, 14300},
    {0x004b, 14400},
    {0x0049, 14500},
    {0x0047, 14600},
    {0x0046, 14700},
    {0x0044, 14800},
    {0x0043, 14900},
    {0x0041, 15000},
    {0x003f, 15100},
    {0x003e, 15200},
    {0x003c, 15300},
    {0x003b, 15400},
    {0x003a, 15500},
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

static const struct {
    uint32_t value;
    int32_t cnr_x1000;
} s2_cn_data[] = {
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

static cxdref_result_t monitor_SamplingRateMode (cxdref_demod_t * pDemod,
                                               uint8_t * pIsHSMode);

static cxdref_result_t monitor_System (cxdref_demod_t * pDemod,
                                     cxdref_dtv_system_t * pSystem);

static cxdref_result_t monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                            int32_t * pOffset);

static cxdref_result_t s_monitor_CodeRate (cxdref_demod_t * pDemod,
                                         cxdref_dvbs_coderate_t * pCodeRate);

static cxdref_result_t s_monitor_IQSense (cxdref_demod_t * pDemod,
                                        cxdref_demod_sat_iq_sense_t * pSense);

static cxdref_result_t s_monitor_CNR (cxdref_demod_t * pDemod,
                                    int32_t * pCNR);

static cxdref_result_t s_monitor_PER (cxdref_demod_t * pDemod,
                                    uint32_t * pPER);

static cxdref_result_t s2_monitor_IQSense (cxdref_demod_t * pDemod,
                                         cxdref_demod_sat_iq_sense_t * pSense);

static cxdref_result_t s2_monitor_CNR (cxdref_demod_t * pDemod,
                                     int32_t * pCNR);

static cxdref_result_t s2_monitor_PER (cxdref_demod_t * pDemod,
                                     uint32_t * pPER);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                   uint8_t * pTSLockStat)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_SyncStat");

    if ((!pDemod) || (!pTSLockStat)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_DVBS) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_DVBS2) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_ANY)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x11, &data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data & 0x04){
        *pTSLockStat = 1;
    } else {
        *pTSLockStat = 0;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                        int32_t * pOffset)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t tsLock = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_CarrierOffset");

    if ((!pDemod) || (!pOffset)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_DVBS) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_DVBS2) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_ANY)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    result = cxdref_demod_dvbs_s2_monitor_SyncStat(pDemod, &tsLock);
    if (result != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (tsLock == 0){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    result = monitor_CarrierOffset (pDemod, pOffset);

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                   uint32_t * pIFAGC)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[2] = {0, 0};
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_IFAGCOut");

    if ((!pDemod) || (!pIFAGC)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_DVBS) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_DVBS2) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_ANY)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1F, data, 2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pIFAGC = (((uint32_t)data[0] & 0x1F) << 8) | (uint32_t)(data[1] & 0xFF);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_System (cxdref_demod_t * pDemod,
                                                 cxdref_dtv_system_t * pSystem)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_System");

    if ((!pDemod) || (!pSystem)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_DVBS) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_DVBS2) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_ANY)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    result = monitor_System (pDemod, pSystem);

    SLVT_UnFreezeReg (pDemod);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_SymbolRate (cxdref_demod_t * pDemod,
                                                     uint32_t * pSymbolRateSps)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t isHSMode = 0;
    uint32_t itrl_ckferr_sr = 0;
    int32_t ckferr_sr = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    uint32_t tempDiv = 0;
    uint8_t data[4];

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_SymbolRate");

    if ((!pDemod) || (!pSymbolRateSps)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_DVBS) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_DVBS2) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_ANY)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 1) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((data[0] & 0x01) == 0x00){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    result = monitor_SamplingRateMode (pDemod, &isHSMode);
    if (result != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xAF) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, data, 4) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    itrl_ckferr_sr = ((uint32_t)(data[0] & 0x07) << 24) |
                 ((uint32_t)(data[1] & 0xFF) << 16) |
                 ((uint32_t)(data[2] & 0xFF) <<  8) |
                  (uint32_t)(data[3] & 0xFF);
    ckferr_sr = cxdref_Convert2SComplement (itrl_ckferr_sr, 27);

    SLVT_UnFreezeReg (pDemod);

    tempDiv = (uint32_t)((int32_t)65536 - ckferr_sr);

    if (ckferr_sr >= 64051 || tempDiv >= 0x01FFFFFF){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (isHSMode){
        tempQ = 3145728000u / tempDiv;
        tempR = 3145728000u % tempDiv;

        tempR *= 100;
        tempQ = (tempQ * 100) + tempR / tempDiv;
        tempR = tempR % tempDiv;

        tempR *= 10;
        tempQ = (tempQ * 10) + tempR / tempDiv;
        tempR = tempR % tempDiv;

        if (tempR >= tempDiv/2) {
            *pSymbolRateSps = tempQ + 1;
        }
        else {
            *pSymbolRateSps = tempQ;
        }
    } else {
        tempQ = 2097152000u / tempDiv;
        tempR = 2097152000u % tempDiv;

        tempR *= 100;
        tempQ = (tempQ * 100) + tempR / tempDiv;
        tempR = tempR % tempDiv;

        tempR *= 10;
        tempQ = (tempQ * 10) + tempR / tempDiv;
        tempR = tempR % tempDiv;

        if (tempR >= tempDiv/2) {
            *pSymbolRateSps = tempQ + 1;
        }
        else {
            *pSymbolRateSps = tempQ;
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_IQSense (cxdref_demod_t * pDemod,
                                                  cxdref_demod_sat_iq_sense_t * pSense)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_IQSense");

    if ((!pDemod) || (!pSense)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
        if ((dtvSystem != CXDREF_DTV_SYSTEM_DVBS) && (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2)){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
    case CXDREF_DTV_SYSTEM_DVBS2:
        dtvSystem = pDemod->system;
        break;

    default:
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (dtvSystem == CXDREF_DTV_SYSTEM_DVBS) {
        result = s_monitor_IQSense (pDemod, pSense);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN(result);
        }
    } else {
        result = s2_monitor_IQSense (pDemod, pSense);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN(result);
        }
    }

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_CNR (cxdref_demod_t * pDemod,
                                              int32_t * pCNR)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_CNR");

    if ((!pDemod) || (!pCNR)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
        if ((dtvSystem != CXDREF_DTV_SYSTEM_DVBS) && (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2)){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
    case CXDREF_DTV_SYSTEM_DVBS2:
        dtvSystem = pDemod->system;
        break;

    default:
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (dtvSystem == CXDREF_DTV_SYSTEM_DVBS) {
        result = s_monitor_CNR (pDemod, pCNR);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN(result);
        }
    } else {
        result = s2_monitor_CNR (pDemod, pCNR);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN(result);
        }
    }

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_PER (cxdref_demod_t * pDemod,
                                              uint32_t * pPER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_PER");

    if ((!pDemod) || (!pPER)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
        if ((dtvSystem != CXDREF_DTV_SYSTEM_DVBS) && (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2)){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
    case CXDREF_DTV_SYSTEM_DVBS2:
        dtvSystem = pDemod->system;
        break;

    default:
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (dtvSystem == CXDREF_DTV_SYSTEM_DVBS) {
        result = s_monitor_PER (pDemod, pPER);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN(result);
        }
    } else {
        result = s2_monitor_PER (pDemod, pPER);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN(result);
        }
    }

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_monitor_CodeRate (cxdref_demod_t * pDemod,
                                                cxdref_dvbs_coderate_t * pCodeRate)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_monitor_CodeRate");

    if ((!pDemod) || (!pCodeRate)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN(result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
        break;

    default:
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = s_monitor_CodeRate (pDemod, pCodeRate);

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_monitor_PreViterbiBER (cxdref_demod_t * pDemod,
                                                     uint32_t * pBER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint8_t data[11];
    uint32_t bitError = 0;
    uint32_t bitCount = 0;
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_monitor_PreViterbiBER");

    if ((!pDemod) || (!pBER)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x35, data, 11) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        bitError = ((uint32_t)(data[1]  & 0x3F) << 16) |
                   ((uint32_t)(data[2]  & 0xFF) <<  8) |
                    (uint32_t)(data[3]  & 0xFF);
        bitCount = ((uint32_t)(data[8]  & 0x3F) << 16) |
                   ((uint32_t)(data[9]  & 0xFF) <<  8) |
                    (uint32_t)(data[10] & 0xFF);
        tempDiv = bitCount;
        if ((tempDiv == 0) || (bitError > bitCount)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (bitError * 625) / tempDiv;
        tempR = (bitError * 625) % tempDiv;

        tempR *= 125;
        tempQ = (tempQ * 125) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        tempR *= 128;
        tempQ = (tempQ * 128) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if ((tempDiv != 1) && (tempR >= (tempDiv/2))){
            *pBER = tempQ + 1;
        } else {
            *pBER = tempQ;
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                uint32_t * pBER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint8_t data[4];
    uint32_t bitError = 0;
    uint32_t period = 0;
    uint32_t bitCount = 0;
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_monitor_PreRSBER");

    if ((!pDemod) || (!pBER)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x25, data, 4) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        bitError = ((uint32_t)(data[1]  & 0xFF) << 16) |
                   ((uint32_t)(data[2]  & 0xFF) <<  8) |
                    (uint32_t)(data[3]  & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBA, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = (uint32_t)(1 << (data[0] & 0x0F));
        bitCount = period * 102;
        tempDiv = bitCount;
        if (tempDiv == 0){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (bitError * 125) / tempDiv;
        tempR = (bitError * 125) % tempDiv;

        tempR *= 125;
        tempQ = (tempQ * 125) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if (tempR >= (tempDiv/2)){
            *pBER = tempQ + 1;
        } else {
            *pBER = tempQ;
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_PLSCode (cxdref_demod_t * pDemod,
                                                cxdref_dvbs2_plscode_t * pPLSCode)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint8_t data = 0;
    uint8_t isTSLock = 0;
    uint8_t mdcd_type = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs2_monitor_PLSCode");

    if ((!pDemod) || (!pPLSCode)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS2:
        break;

    default:
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbs_s2_monitor_SyncStat (pDemod, &isTSLock);
    if (result != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (!isTSLock){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x21, &data, 1) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    mdcd_type = (uint8_t)(data & 0x7F);

    switch((mdcd_type >> 2) & 0x1F)
    {
    case 0x01:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_1_4;
        break;
    case 0x02:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_1_3;
        break;
    case 0x03:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_2_5;
        break;
    case 0x04:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_1_2;
        break;
    case 0x05:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_3_5;
        break;
    case 0x06:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_2_3;
        break;
    case 0x07:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_3_4;
        break;
    case 0x08:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_4_5;
        break;
    case 0x09:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_5_6;
        break;
    case 0x0A:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_8_9;
        break;
    case 0x0B:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_QPSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_9_10;
        break;
    case 0x0C:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_8PSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_3_5;
        break;
    case 0x0D:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_8PSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_2_3;
        break;
    case 0x0E:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_8PSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_3_4;
        break;
    case 0x0F:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_8PSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_5_6;
        break;
    case 0x10:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_8PSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_8_9;
        break;
    case 0x11:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_8PSK;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_9_10;
        break;
    case 0x1D:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_RESERVED_29;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_RESERVED_29;
        break;
    case 0x1E:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_RESERVED_30;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_RESERVED_30;
        break;
    case 0x1F:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_RESERVED_31;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_RESERVED_31;
        break;
    default:
        pPLSCode->modulation = CXDREF_DVBS2_MODULATION_INVALID;
        pPLSCode->codeRate = CXDREF_DVBS2_CODERATE_INVALID;
        break;
    }

    pPLSCode->isShortFrame = 0;

    if (mdcd_type & 0x01){
        pPLSCode->isPilotOn = 1;
    } else {
        pPLSCode->isPilotOn = 0;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_PreLDPCBER (cxdref_demod_t * pDemod,
                                                   uint32_t * pBER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint32_t bitError = 0;
    uint32_t period = 0;
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    uint8_t data[5];
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs2_monitor_PreLDPCBER");

    if ((!pDemod) || (!pBER)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS2:
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xB2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x30, data, 5) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        bitError = ((uint32_t)(data[1] & 0x0F) << 24) |
                   ((uint32_t)(data[2] & 0xFF) << 16) |
                   ((uint32_t)(data[3] & 0xFF) <<  8) |
                    (uint32_t)(data[4] & 0xFF);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7A, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = (uint32_t)(1 << (data[0] & 0x0F));

        if (bitError > (period * 64800)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempDiv = period * 81;
        if (tempDiv == 0){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (bitError * 10) / tempDiv;
        tempR = (bitError * 10) % tempDiv;

        tempR *= 1250;
        tempQ = (tempQ * 1250) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if (tempR >= (tempDiv/2)){
            *pBER = tempQ + 1;
        } else {
            *pBER = tempQ;
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_PreBCHBER (cxdref_demod_t * pDemod,
                                                  uint32_t * pBER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint32_t bitError = 0;
    uint32_t period = 0;
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    uint8_t data[4];
    cxdref_dvbs2_plscode_t plscode;
    uint32_t tempA = 0;
    uint32_t tempB = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs2_monitor_PreBCHBER");

    if ((!pDemod) || (!pBER)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS2:
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2F, data, 4) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        bitError = ((uint32_t)(data[1] & 0x7F) << 16) |
                   ((uint32_t)(data[2] & 0xFF) <<  8) |
                    (uint32_t)(data[3] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBC, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = (uint32_t)(1 << (data[0] & 0x0F));

        result = cxdref_demod_dvbs2_monitor_PLSCode (pDemod, &plscode);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        switch(plscode.codeRate)
        {
        case CXDREF_DVBS2_CODERATE_1_4:  tempA = 1; tempB =  4; break;
        case CXDREF_DVBS2_CODERATE_1_3:  tempA = 1; tempB =  3; break;
        case CXDREF_DVBS2_CODERATE_2_5:  tempA = 2; tempB =  5; break;
        case CXDREF_DVBS2_CODERATE_1_2:  tempA = 1; tempB =  2; break;
        case CXDREF_DVBS2_CODERATE_3_5:  tempA = 3; tempB =  5; break;
        case CXDREF_DVBS2_CODERATE_2_3:  tempA = 2; tempB =  3; break;
        case CXDREF_DVBS2_CODERATE_3_4:  tempA = 3; tempB =  4; break;
        case CXDREF_DVBS2_CODERATE_4_5:  tempA = 4; tempB =  5; break;
        case CXDREF_DVBS2_CODERATE_5_6:  tempA = 5; tempB =  6; break;
        case CXDREF_DVBS2_CODERATE_8_9:  tempA = 8; tempB =  9; break;
        case CXDREF_DVBS2_CODERATE_9_10: tempA = 9; tempB = 10; break;
        default: CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if((bitError * tempB) > (period * 64800 * tempA)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempDiv = period * 81 * tempA;
        if (tempDiv == 0){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (bitError * tempB * 10) / tempDiv;
        tempR = (bitError * tempB * 10) % tempDiv;

        tempR *= 125;
        tempQ = (tempQ * 125) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        tempR *= 100;
        tempQ = (tempQ * 100) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        tempR *= 10;
        tempQ = (tempQ * 10) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if (tempR >= (tempDiv/2)){
            *pBER = tempQ + 1;
        } else {
            *pBER = tempQ;
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_PostBCHFER (cxdref_demod_t * pDemod,
                                                   uint32_t * pFER)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint8_t data[6];
    uint32_t frameError = 0;
    uint32_t frameCount = 0;
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs2_monitor_PostBCHFER");

    if ((!pDemod) || (!pFER)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS2:
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2F, data, 6) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        frameError = ((uint32_t)(data[4] & 0xFF) << 8) |
                      (uint32_t)(data[5] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBC, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        frameCount = (uint32_t)(1 << (data[0] & 0x0F));

        tempDiv = frameCount;
        if ((tempDiv == 0) || (frameError > frameCount)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (frameError * 10000) / tempDiv;
        tempR = (frameError * 10000) % tempDiv;

        tempR *= 100;
        tempQ = (tempQ * 100) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if ((tempDiv != 1) && (tempR >= (tempDiv/2))){
            *pFER = tempQ + 1;
        } else {
            *pFER = tempQ;
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_BBHeader (cxdref_demod_t * pDemod,
                                                 cxdref_dvbs2_bbheader_t * pBBHeader)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint8_t isTSLock = 0;
    uint8_t data[7];
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs2_monitor_BBHeader");

    if ((!pDemod) || (!pBBHeader)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS2:
        break;

    default:
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbs_s2_monitor_SyncStat (pDemod, &isTSLock);
    if (result != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (!isTSLock){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x49, data, sizeof (data)) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    SLVT_UnFreezeReg (pDemod);

    pBBHeader->streamInput = (cxdref_dvbs2_stream_t) ((data[0] & 0xC0) >> 6);
    pBBHeader->isSingleInputStream = (data[0] & 0x20) >> 5;
    pBBHeader->isConstantCodingModulation = (data[0] & 0x10) >> 4;
    pBBHeader->issyIndicator = (data[0] & 0x08) >> 3;
    pBBHeader->nullPacketDeletion = (data[0] & 0x04) >> 2;
    pBBHeader->rollOff = (cxdref_dvbs2_rolloff_t) (data[0] & 0x03);
    pBBHeader->inputStreamIdentifier = data[1];
    pBBHeader->userPacketLength = (data[2] << 8);
    pBBHeader->userPacketLength |= data[3];
    pBBHeader->dataFieldLength = (data[4] << 8);
    pBBHeader->dataFieldLength |= data[5];
    pBBHeader->syncByte = data[6];

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_Pilot (cxdref_demod_t * pDemod,
                                                uint8_t * pPlscLock,
                                                uint8_t * pPilotOn)
{
    uint8_t data = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_Pilot");

    if ((!pDemod) || (!pPlscLock) || (!pPilotOn)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x11, &data, 1) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    *pPlscLock = (uint8_t)(data & 0x01);
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x21, &data, 1) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    *pPilotOn = (uint8_t)(data & 0x01);

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_ScanInfo (cxdref_demod_t * pDemod,
                                                   uint8_t * pTSLock,
                                                   int32_t * pOffset,
                                                   cxdref_dtv_system_t * pSystem)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_ScanInfo");

    if ((!pDemod) || (!pTSLock) || (!pOffset) || (!pSystem)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    SLVT_FreezeReg (pDemod);

    result = cxdref_demod_dvbs_s2_monitor_SyncStat (pDemod, pTSLock);
    if ((result != CXDREF_RESULT_OK) || (*pTSLock == 0)){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = monitor_CarrierOffset (pDemod, pOffset);
    if (result != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = monitor_System (pDemod, pSystem);
    if (result != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_monitor_TSRate (cxdref_demod_t * pDemod,
                                                 uint32_t * pTSRateKbps)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem;
    uint32_t symbolRateSps;
    uint32_t symbolRateKSps;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_TSRate");

    if ((!pDemod) || (!pTSRateKbps)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }
    result = cxdref_demod_dvbs_s2_monitor_SymbolRate (pDemod, &symbolRateSps);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }
    symbolRateKSps = (symbolRateSps + 500) / 1000;
    if (dtvSystem == CXDREF_DTV_SYSTEM_DVBS) {
        cxdref_dvbs_coderate_t codeRate;
        uint32_t numerator;
        uint32_t denominator;
        result = cxdref_demod_dvbs_monitor_CodeRate (pDemod, &codeRate);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        switch (codeRate) {
        case CXDREF_DVBS_CODERATE_1_2:
            numerator = 1;
            denominator = 2;
            break;
        case CXDREF_DVBS_CODERATE_2_3:
            numerator = 2;
            denominator = 3;
            break;
        case CXDREF_DVBS_CODERATE_3_4:
            numerator = 3;
            denominator = 4;
            break;
        case CXDREF_DVBS_CODERATE_5_6:
            numerator = 5;
            denominator = 6;
            break;
        case CXDREF_DVBS_CODERATE_7_8:
            numerator = 7;
            denominator = 8;
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
        }
        if (symbolRateKSps >= 60000) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        } else {
            *pTSRateKbps = ((symbolRateKSps * numerator * 94) + (51 * denominator / 2)) / (51 * denominator);
        }
    } else if (dtvSystem == CXDREF_DTV_SYSTEM_DVBS2) {
        cxdref_dvbs2_plscode_t plsCode;
        uint32_t kbch;
        result = cxdref_demod_dvbs2_monitor_PLSCode (pDemod, &plsCode);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        switch (plsCode.codeRate) {
        case CXDREF_DVBS2_CODERATE_1_2:
            kbch = 32208;
            break;
        case CXDREF_DVBS2_CODERATE_3_5:
            kbch = 38688;
            break;
        case CXDREF_DVBS2_CODERATE_2_3:
            kbch = 43040;
            break;
        case CXDREF_DVBS2_CODERATE_3_4:
            kbch = 48408;
            break;
        case CXDREF_DVBS2_CODERATE_4_5:
            kbch = 51648;
            break;
        case CXDREF_DVBS2_CODERATE_5_6:
            kbch = 53840;
            break;
        case CXDREF_DVBS2_CODERATE_8_9:
            kbch = 57472;
            break;
        case CXDREF_DVBS2_CODERATE_9_10:
            kbch = 58192;
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
        }
        if (plsCode.modulation == CXDREF_DVBS2_MODULATION_8PSK) {
            if (plsCode.isPilotOn) {
                *pTSRateKbps = ((symbolRateKSps * (kbch - 80)) + 11097) / 22194;
            } else {
                *pTSRateKbps = ((symbolRateKSps * (kbch - 80)) + 10845) / 21690;
            }
        } else if (plsCode.modulation == CXDREF_DVBS2_MODULATION_QPSK) {
            if (plsCode.isPilotOn) {
                *pTSRateKbps = ((symbolRateKSps * (kbch - 80)) + 16641) / 33282;
            } else {
                *pTSRateKbps = ((symbolRateKSps * (kbch - 80)) + 16245) / 32490;
            }
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t monitor_System (cxdref_demod_t * pDemod,
                                     cxdref_dtv_system_t * pSystem)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t isTSLock = 0;
    uint8_t data = 0;

    CXDREF_TRACE_ENTER ("monitor_System");

    if ((!pDemod) || (!pSystem)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbs_s2_monitor_SyncStat (pDemod, &isTSLock);
    if (result != CXDREF_RESULT_OK){CXDREF_TRACE_RETURN (result);}

    if (!isTSLock){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x50, &data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    switch(data & 0x03)
    {
    case 0x00:
        *pSystem = CXDREF_DTV_SYSTEM_DVBS2;
        break;

    case 0x01:
        *pSystem = CXDREF_DTV_SYSTEM_DVBS;
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                            int32_t * pOffset)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t iqInv = 0;
    uint8_t isHSMode = 0;
    uint8_t data[3];
    uint32_t regValue = 0;
    int32_t cfrl_ctrlval = 0;

    CXDREF_TRACE_ENTER ("monitor_CarrierOffset");

    result = monitor_SamplingRateMode (pDemod, &isHSMode);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x45, data, 3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    regValue = (((uint32_t)data[0] & 0x1F) << 16) | (((uint32_t)data[1] & 0xFF) <<  8) | ((uint32_t)data[2] & 0xFF);
    cfrl_ctrlval = cxdref_Convert2SComplement(regValue, 21);

    if (isHSMode){
        if(cfrl_ctrlval > 0){
            *pOffset = ((cfrl_ctrlval * (-375)) - 2048) / 4096;
        } else {
            *pOffset = ((cfrl_ctrlval * (-375)) + 2048) / 4096;
        }
    } else {
        if(cfrl_ctrlval > 0){
            *pOffset = ((cfrl_ctrlval * (-125)) - 1024) / 2048;
        } else {
            *pOffset = ((cfrl_ctrlval * (-125)) + 1024) / 2048;
        }
    }

    result = cxdref_demod_dvbs_s2_CheckIQInvert (pDemod, &iqInv);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (iqInv){
        *pOffset *= (-1);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t s_monitor_CodeRate (cxdref_demod_t * pDemod,
                                         cxdref_dvbs_coderate_t * pCodeRate)
{
    uint8_t data = 0;

    CXDREF_TRACE_ENTER ("s_monitor_CodeRate");

    if ((!pDemod) || (!pCodeRate)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xB1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data & 0x80){
        switch(data & 0x07)
        {
        case 0x00:
            *pCodeRate = CXDREF_DVBS_CODERATE_1_2;
            break;

        case 0x01:
            *pCodeRate = CXDREF_DVBS_CODERATE_2_3;
            break;

        case 0x02:
            *pCodeRate = CXDREF_DVBS_CODERATE_3_4;
            break;

        case 0x03:
            *pCodeRate = CXDREF_DVBS_CODERATE_5_6;
            break;

        case 0x04:
            *pCodeRate = CXDREF_DVBS_CODERATE_7_8;
            break;

        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t monitor_SamplingRateMode (cxdref_demod_t * pDemod,
                                               uint8_t * pIsHSMode)
{
    uint8_t data = 0;

    CXDREF_TRACE_ENTER ("monitor_SamplingRateMode");

    if ((!pDemod) || (!pIsHSMode)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (data & 0x01){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x50, &data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (data & 0x10){
            *pIsHSMode = 1;
        } else {
            *pIsHSMode = 0;
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t s_monitor_IQSense (cxdref_demod_t * pDemod,
                                        cxdref_demod_sat_iq_sense_t * pSense)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t isTSLock = 0;
    uint8_t isInverted = 0;
    uint8_t data = 0;
    cxdref_dvbs_coderate_t codeRate;

    CXDREF_TRACE_ENTER ("s_monitor_IQSense");

    if ((!pDemod) || (!pSense)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbs_s2_monitor_SyncStat (pDemod, &isTSLock);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (!isTSLock){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    result = s_monitor_CodeRate (pDemod, &codeRate);
    if (result != CXDREF_RESULT_OK){CXDREF_TRACE_RETURN (result);}

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xB1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    switch(codeRate)
    {
    case CXDREF_DVBS_CODERATE_1_2:
    case CXDREF_DVBS_CODERATE_2_3:
    case CXDREF_DVBS_CODERATE_3_4:
    case CXDREF_DVBS_CODERATE_7_8:
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3E, &data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        break;

    case CXDREF_DVBS_CODERATE_5_6:
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5D, &data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    result = cxdref_demod_dvbs_s2_CheckIQInvert (pDemod, &isInverted);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (isInverted){
        *pSense = (data & 0x01) ? CXDREF_DEMOD_SAT_IQ_SENSE_NORMAL : CXDREF_DEMOD_SAT_IQ_SENSE_INV;
    } else {
        *pSense = (data & 0x01) ? CXDREF_DEMOD_SAT_IQ_SENSE_INV : CXDREF_DEMOD_SAT_IQ_SENSE_NORMAL;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t s_monitor_CNR (cxdref_demod_t * pDemod,
                                    int32_t * pCNR)
{
    int32_t index = 0;
    int32_t minIndex = 0;
    int32_t maxIndex = 0;
    uint8_t data[3];
    uint32_t value = 0;

    CXDREF_TRACE_ENTER ("s_monitor_CNR");

    if ((!pDemod) || (!pCNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if(data[0] & 0x01){
        value = ((uint32_t)(data[1] & 0x1F) << 8) | (uint32_t)(data[2] & 0xFF);

        minIndex = 0;
        maxIndex = sizeof(s_cn_data)/sizeof(s_cn_data[0]) - 1;

        if(value >= s_cn_data[minIndex].value){
            *pCNR = s_cn_data[minIndex].cnr_x1000;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
        if(value <= s_cn_data[maxIndex].value){
            *pCNR = s_cn_data[maxIndex].cnr_x1000;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }

        while ((maxIndex - minIndex) > 1){

            index = (maxIndex + minIndex) / 2;

            if (value == s_cn_data[index].value){
                *pCNR = s_cn_data[index].cnr_x1000;
                CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
            } else if (value > s_cn_data[index].value){
                maxIndex = index;
            } else {
                minIndex = index;
            }

            if ((maxIndex - minIndex) <= 1){
                if (value == s_cn_data[maxIndex].value){
                    *pCNR = s_cn_data[maxIndex].cnr_x1000;
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
                } else {
                    *pCNR = s_cn_data[minIndex].cnr_x1000;
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
                }
            }
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
}

static cxdref_result_t s_monitor_PER (cxdref_demod_t * pDemod,
                                    uint32_t * pPER)
{
    uint8_t data[3];
    uint32_t packetError = 0;
    uint32_t period = 0;
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    CXDREF_TRACE_ENTER ("s_monitor_PER");

    if ((!pDemod) || (!pPER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x25, data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2C, data, 3) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        packetError = ((uint32_t)(data[0]  & 0x03) << 16) |
                      ((uint32_t)(data[1]  & 0xFF) <<  8) |
                       (uint32_t)(data[2]  & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBA, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = (uint32_t)(1 << (data[0] & 0x0F));
        tempDiv = period;
        if ((tempDiv == 0) || (packetError > (period * 8))){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (packetError * 1000) / tempDiv;
        tempR = (packetError * 1000) % tempDiv;

        tempR *= 125;
        tempQ = (tempQ * 125) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if ((tempDiv != 1) && (tempR >= (tempDiv/2))){
            *pPER = tempQ + 1;
        } else {
            *pPER = tempQ;
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t s2_monitor_IQSense (cxdref_demod_t * pDemod,
                                         cxdref_demod_sat_iq_sense_t * pSense)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t isTSLock = 0;
    uint8_t isInverted = 0;
    uint8_t data = 0;

    CXDREF_TRACE_ENTER ("s2_monitor_IQSense");

    if ((!pDemod) || (!pSense)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbs_s2_monitor_SyncStat (pDemod, &isTSLock);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (!isTSLock){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xAB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x23, &data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_dvbs_s2_CheckIQInvert (pDemod, &isInverted);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (isInverted){
        *pSense = (data & 0x01) ? CXDREF_DEMOD_SAT_IQ_SENSE_NORMAL : CXDREF_DEMOD_SAT_IQ_SENSE_INV;
    } else {
        *pSense = (data & 0x01) ? CXDREF_DEMOD_SAT_IQ_SENSE_INV : CXDREF_DEMOD_SAT_IQ_SENSE_NORMAL;
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t s2_monitor_CNR (cxdref_demod_t * pDemod,
                                     int32_t * pCNR)
{
    int32_t index = 0;
    int32_t minIndex = 0;
    int32_t maxIndex = 0;
    uint8_t data[3];
    uint32_t value = 0;

    CXDREF_TRACE_ENTER ("s2_monitor_CNR");

    if ((!pDemod) || (!pCNR)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_DVBS2) &&
        (pDemod->system != CXDREF_DTV_SYSTEM_ANY)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if(data[0] & 0x01){
        value = ((uint32_t)(data[1] & 0x1F) << 8) | (uint32_t)(data[2] & 0xFF);

        minIndex = 0;
        maxIndex = sizeof(s2_cn_data)/sizeof(s2_cn_data[0]) - 1;

        if(value >= s2_cn_data[minIndex].value){
            *pCNR = s2_cn_data[minIndex].cnr_x1000;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
        if(value <= s2_cn_data[maxIndex].value){
            *pCNR = s2_cn_data[maxIndex].cnr_x1000;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }

        while ((maxIndex - minIndex) > 1){

            index = (maxIndex + minIndex) / 2;

            if (value == s2_cn_data[index].value){
                *pCNR = s2_cn_data[index].cnr_x1000;
                CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
            } else if (value > s2_cn_data[index].value){
                maxIndex = index;
            } else {
                minIndex = index;
            }

            if ((maxIndex - minIndex) <= 1){
                if (value == s2_cn_data[maxIndex].value){
                    *pCNR = s2_cn_data[maxIndex].cnr_x1000;
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
                } else {
                    *pCNR = s2_cn_data[minIndex].cnr_x1000;
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
                }
            }
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
}

static cxdref_result_t s2_monitor_PER (cxdref_demod_t * pDemod,
                                     uint32_t * pPER)
{
    uint8_t data[3];
    uint32_t packetError = 0;
    uint32_t packetCount = 0;
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    CXDREF_TRACE_ENTER ("s2_monitor_PER");

    if ((!pDemod) || (!pPER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xB8) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x26, data, 3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        packetError = ((uint32_t)(data[1] & 0xFF) << 8) |
                       (uint32_t)(data[2] & 0xFF);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBA, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        packetCount = (uint32_t)(1 << (data[0] & 0x0F));

        tempDiv = packetCount;
        if ((tempDiv == 0) || (packetError > packetCount)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (packetError * 10000) / tempDiv;
        tempR = (packetError * 10000) % tempDiv;

        tempR *= 100;
        tempQ = (tempQ * 100) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if ((tempDiv != 1) && (tempR >= (tempDiv/2))){
            *pPER = tempQ + 1;
        } else {
            *pPER = tempQ;
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_Rolloff (cxdref_demod_t * pDemod,
                                                uint8_t * pRolloff)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data=0x00;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs2_monitor_Rolloff");

    if ((!pDemod) || (!pRolloff)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    if (pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if(pDemod->pI2c->ReadRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x49, &data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pRolloff=(data & 0x03);
    CXDREF_TRACE_RETURN (result);
}
