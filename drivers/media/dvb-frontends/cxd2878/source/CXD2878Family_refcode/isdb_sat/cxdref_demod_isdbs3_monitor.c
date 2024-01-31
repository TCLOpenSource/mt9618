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
#include "cxdref_demod_isdbs3.h"
#include "cxdref_demod_isdbs3_monitor.h"

static cxdref_result_t IsDmdLocked(cxdref_demod_t* pDemod);

static cxdref_result_t FreezeTMCCInfo(cxdref_demod_t* pDemod);

static cxdref_result_t UnfreezeTMCCInfo(cxdref_demod_t* pDemod);

static cxdref_result_t IsTMCCReadOK(cxdref_demod_t* pDemod);

cxdref_result_t cxdref_demod_isdbs3_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                  uint8_t * pAGCLockStat,
                                                  uint8_t * pTSTLVLockStat,
                                                  uint8_t * pTMCCLockStat)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[2];
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_SyncStat");

    if ((!pDemod) || (!pAGCLockStat) || (!pTSTLVLockStat) || (!pTMCCLockStat)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pAGCLockStat = (uint8_t)((data[0] & 0x20) ? 1 : 0);
    *pTSTLVLockStat = (uint8_t)((data[1] & 0x40) ? 1 : 0);
    *pTMCCLockStat = (uint8_t)((data[1] & 0x80) ? 1 : 0);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                       int32_t * pOffset)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[3];
    uint32_t regValue = 0;
    int32_t cfrl_ctrlval = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_CarrierOffset");

    if ((!pDemod) || (!pOffset)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
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
        *pOffset = ((cfrl_ctrlval * (-375)) - 2048) / 4096;
    } else {
        *pOffset = ((cfrl_ctrlval * (-375)) + 2048) / 4096;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                  uint32_t * pIFAGCOut)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[2] = {0, 0};
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_IFAGCOut");

    if ((!pDemod) || (!pIFAGCOut)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
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


cxdref_result_t cxdref_demod_isdbs3_monitor_IQSense (cxdref_demod_t * pDemod,
                                                 cxdref_demod_sat_iq_sense_t * pSense)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_IQSense");

    if ((!pDemod) || (!pSense)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xAB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x23, &rdata, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pSense = (rdata & 0x01) ? CXDREF_DEMOD_SAT_IQ_SENSE_INV : CXDREF_DEMOD_SAT_IQ_SENSE_NORMAL;

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_demod_isdbs3_monitor_CNR (cxdref_demod_t * pDemod,
                                             int32_t * pCNR)
{
    uint8_t rdata[4] = {0x00, 0x00, 0x00, 0x00};
    uint32_t value = 0;
    int32_t index = 0;
    int32_t minIndex = 0;
    int32_t maxIndex = 0;
    const struct {
        uint32_t value;
        int32_t cnr_x1000;
    } isdbs3_cn_data[] = {
        {0x10da5, -4000},
        {0x107dd, -3900},
        {0x1023c, -3800},
        {0x0fcc1, -3700},
        {0x0f76b, -3600},
        {0x0f237, -3500},
        {0x0ed25, -3400},
        {0x0e834, -3300},
        {0x0e361, -3200},
        {0x0deab, -3100},
        {0x0da13, -3000},
        {0x0d596, -2900},
        {0x0d133, -2800},
        {0x0cceb, -2700},
        {0x0c8bb, -2600},
        {0x0c4a4, -2500},
        {0x0c0a3, -2400},
        {0x0bcba, -2300},
        {0x0b8e6, -2200},
        {0x0b527, -2100},
        {0x0b17d, -2000},
        {0x0ade6, -1900},
        {0x0aa63, -1800},
        {0x0a6f3, -1700},
        {0x0a395, -1600},
        {0x0a049, -1500},
        {0x09d0e, -1400},
        {0x099e4, -1300},
        {0x096cb, -1200},
        {0x093c1, -1100},
        {0x090c7, -1000},
        {0x08ddc, -900},
        {0x08b00, -800},
        {0x08833, -700},
        {0x08573, -600},
        {0x082c2, -500},
        {0x0801e, -400},
        {0x07d87, -300},
        {0x07afd, -200},
        {0x0787f, -100},
        {0x0760c, 0},
        {0x073a9, 100},
        {0x0714f, 200},
        {0x06f02, 300},
        {0x06cbf, 400},
        {0x06a88, 500},
        {0x0685b, 600},
        {0x06639, 700},
        {0x06421, 800},
        {0x06214, 900},
        {0x06010, 1000},
        {0x05e16, 1100},
        {0x05c26, 1200},
        {0x05a40, 1300},
        {0x05862, 1400},
        {0x0568e, 1500},
        {0x054c2, 1600},
        {0x05300, 1700},
        {0x05146, 1800},
        {0x04f94, 1900},
        {0x04deb, 2000},
        {0x04c49, 2100},
        {0x04ab0, 2200},
        {0x0491f, 2300},
        {0x04795, 2400},
        {0x04613, 2500},
        {0x04498, 2600},
        {0x04325, 2700},
        {0x041b8, 2800},
        {0x04053, 2900},
        {0x03ef5, 3000},
        {0x03d9e, 3100},
        {0x03c4d, 3200},
        {0x03b02, 3300},
        {0x039bf, 3400},
        {0x03881, 3500},
        {0x0374a, 3600},
        {0x03619, 3700},
        {0x034ee, 3800},
        {0x033c9, 3900},
        {0x032aa, 4000},
        {0x03191, 4100},
        {0x0307d, 4200},
        {0x02f6f, 4300},
        {0x02e66, 4400},
        {0x02d62, 4500},
        {0x02c64, 4600},
        {0x02b6b, 4700},
        {0x02a77, 4800},
        {0x02988, 4900},
        {0x0289e, 5000},
        {0x027b9, 5100},
        {0x026d9, 5200},
        {0x025fd, 5300},
        {0x02526, 5400},
        {0x02453, 5500},
        {0x02385, 5600},
        {0x022bb, 5700},
        {0x021f5, 5800},
        {0x02134, 5900},
        {0x02076, 6000},
        {0x01fbd, 6100},
        {0x01f08, 6200},
        {0x01e56, 6300},
        {0x01da9, 6400},
        {0x01cff, 6500},
        {0x01c58, 6600},
        {0x01bb5, 6700},
        {0x01b16, 6800},
        {0x01a7b, 6900},
        {0x019e2, 7000},
        {0x0194d, 7100},
        {0x018bc, 7200},
        {0x0182d, 7300},
        {0x017a2, 7400},
        {0x0171a, 7500},
        {0x01695, 7600},
        {0x01612, 7700},
        {0x01593, 7800},
        {0x01517, 7900},
        {0x0149d, 8000},
        {0x01426, 8100},
        {0x013b2, 8200},
        {0x01340, 8300},
        {0x012d1, 8400},
        {0x01264, 8500},
        {0x011fa, 8600},
        {0x01193, 8700},
        {0x0112d, 8800},
        {0x010ca, 8900},
        {0x01069, 9000},
        {0x0100b, 9100},
        {0x00fae, 9200},
        {0x00f54, 9300},
        {0x00efb, 9400},
        {0x00ea5, 9500},
        {0x00e51, 9600},
        {0x00dff, 9700},
        {0x00dae, 9800},
        {0x00d60, 9900},
        {0x00d13, 10000},
        {0x00cc8, 10100},
        {0x00c7e, 10200},
        {0x00c37, 10300},
        {0x00bf1, 10400},
        {0x00bac, 10500},
        {0x00b6a, 10600},
        {0x00b28, 10700},
        {0x00ae8, 10800},
        {0x00aaa, 10900},
        {0x00a6d, 11000},
        {0x00a32, 11100},
        {0x009f8, 11200},
        {0x009bf, 11300},
        {0x00987, 11400},
        {0x00951, 11500},
        {0x0091c, 11600},
        {0x008e8, 11700},
        {0x008b6, 11800},
        {0x00884, 11900},
        {0x00854, 12000},
        {0x00825, 12100},
        {0x007f7, 12200},
        {0x007ca, 12300},
        {0x0079e, 12400},
        {0x00773, 12500},
        {0x00749, 12600},
        {0x00720, 12700},
        {0x006f8, 12800},
        {0x006d0, 12900},
        {0x006aa, 13000},
        {0x00685, 13100},
        {0x00660, 13200},
        {0x0063c, 13300},
        {0x00619, 13400},
        {0x005f7, 13500},
        {0x005d6, 13600},
        {0x005b5, 13700},
        {0x00595, 13800},
        {0x00576, 13900},
        {0x00558, 14000},
        {0x0053a, 14100},
        {0x0051d, 14200},
        {0x00501, 14300},
        {0x004e5, 14400},
        {0x004c9, 14500},
        {0x004af, 14600},
        {0x00495, 14700},
        {0x0047b, 14800},
        {0x00462, 14900},
        {0x0044a, 15000},
        {0x00432, 15100},
        {0x0041b, 15200},
        {0x00404, 15300},
        {0x003ee, 15400},
        {0x003d8, 15500},
        {0x003c3, 15600},
        {0x003ae, 15700},
        {0x0039a, 15800},
        {0x00386, 15900},
        {0x00373, 16000},
        {0x00360, 16100},
        {0x0034d, 16200},
        {0x0033b, 16300},
        {0x00329, 16400},
        {0x00318, 16500},
        {0x00307, 16600},
        {0x002f6, 16700},
        {0x002e6, 16800},
        {0x002d6, 16900},
        {0x002c7, 17000},
        {0x002b7, 17100},
        {0x002a9, 17200},
        {0x0029a, 17300},
        {0x0028c, 17400},
        {0x0027e, 17500},
        {0x00270, 17600},
        {0x00263, 17700},
        {0x00256, 17800},
        {0x00249, 17900},
        {0x0023d, 18000},
        {0x00231, 18100},
        {0x00225, 18200},
        {0x00219, 18300},
        {0x0020e, 18400},
        {0x00203, 18500},
        {0x001f8, 18600},
        {0x001ed, 18700},
        {0x001e3, 18800},
        {0x001d9, 18900},
        {0x001cf, 19000},
        {0x001c5, 19100},
        {0x001bc, 19200},
        {0x001b2, 19300},
        {0x001a9, 19400},
        {0x001a0, 19500},
        {0x00198, 19600},
        {0x0018f, 19700},
        {0x00187, 19800},
        {0x0017f, 19900},
        {0x00177, 20000},
        {0x0016f, 20100},
        {0x00168, 20200},
        {0x00160, 20300},
        {0x00159, 20400},
        {0x00152, 20500},
        {0x0014b, 20600},
        {0x00144, 20700},
        {0x0013e, 20800},
        {0x00137, 20900},
        {0x00131, 21000},
        {0x0012b, 21100},
        {0x00125, 21200},
        {0x0011f, 21300},
        {0x00119, 21400},
        {0x00113, 21500},
        {0x0010e, 21600},
        {0x00108, 21700},
        {0x00103, 21800},
        {0x000fe, 21900},
        {0x000f9, 22000},
        {0x000f4, 22100},
        {0x000ef, 22200},
        {0x000eb, 22300},
        {0x000e6, 22400},
        {0x000e2, 22500},
        {0x000de, 22600},
        {0x000da, 22700},
        {0x000d5, 22800},
        {0x000d1, 22900},
        {0x000cd, 23000},
        {0x000ca, 23100},
        {0x000c6, 23200},
        {0x000c2, 23300},
        {0x000be, 23400},
        {0x000bb, 23500},
        {0x000b7, 23600},
        {0x000b4, 23700},
        {0x000b1, 23800},
        {0x000ae, 23900},
        {0x000aa, 24000},
        {0x000a7, 24100},
        {0x000a4, 24200},
        {0x000a2, 24300},
        {0x0009f, 24400},
        {0x0009c, 24500},
        {0x00099, 24600},
        {0x00097, 24700},
        {0x00094, 24800},
        {0x00092, 24900},
        {0x0008f, 25000},
        {0x0008d, 25100},
        {0x0008b, 25200},
        {0x00088, 25300},
        {0x00086, 25400},
        {0x00084, 25500},
        {0x00082, 25600},
        {0x00080, 25700},
        {0x0007e, 25800},
        {0x0007c, 25900},
        {0x0007a, 26000},
        {0x00078, 26100},
        {0x00076, 26200},
        {0x00074, 26300},
        {0x00073, 26400},
        {0x00071, 26500},
        {0x0006f, 26600},
        {0x0006d, 26700},
        {0x0006c, 26800},
        {0x0006a, 26900},
        {0x00069, 27000},
        {0x00067, 27100},
        {0x00066, 27200},
        {0x00064, 27300},
        {0x00063, 27400},
        {0x00061, 27500},
        {0x00060, 27600},
        {0x0005f, 27700},
        {0x0005d, 27800},
        {0x0005c, 27900},
        {0x0005b, 28000},
        {0x0005a, 28100},
        {0x00059, 28200},
        {0x00057, 28300},
        {0x00056, 28400},
        {0x00055, 28500},
        {0x00054, 28600},
        {0x00053, 28700},
        {0x00052, 28800},
        {0x00051, 28900},
        {0x00050, 29000},
        {0x0004f, 29100},
        {0x0004e, 29200},
        {0x0004d, 29300},
        {0x0004c, 29400},
        {0x0004b, 29500},
        {0x0004a, 29600},
        {0x00049, 29700},
        {0x00048, 29900},
        {0x00047, 30000},
    };

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_CNR");

    if ((!pDemod) || (!pCNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF3, rdata, 4) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x01) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    value = (uint32_t)((((uint32_t)rdata[1] & 0x01) << 16) | ((uint32_t)rdata[2] << 8) | rdata[3]);

    minIndex = 0;
    maxIndex = sizeof(isdbs3_cn_data)/sizeof(isdbs3_cn_data[0]) - 1;

    if(value >= isdbs3_cn_data[minIndex].value) {
        *pCNR = isdbs3_cn_data[minIndex].cnr_x1000;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }
    if(value <= isdbs3_cn_data[maxIndex].value) {
        *pCNR = isdbs3_cn_data[maxIndex].cnr_x1000;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    while (1) {
        index = (maxIndex + minIndex) / 2;

        if (value == isdbs3_cn_data[index].value) {
            *pCNR = isdbs3_cn_data[index].cnr_x1000;
            break;
        } else if (value > isdbs3_cn_data[index].value) {
            maxIndex = index;
        } else {
            minIndex = index;
        }

        if ((maxIndex - minIndex) <= 1) {
            if (value == isdbs3_cn_data[maxIndex].value) {
                *pCNR = isdbs3_cn_data[maxIndex].cnr_x1000;
                break;
            } else {
                *pCNR = isdbs3_cn_data[minIndex].cnr_x1000;
                break;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_PreLDPCBER (cxdref_demod_t * pDemod,
                                                    uint32_t * pBERH, uint32_t * pBERL)
{
    uint8_t rdata[4];
    uint8_t valid[2] = {0};
    uint32_t bitError[2] = {0};
    uint32_t measuredPeriod[2] = {0};
    uint32_t * pBER[2];
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    int i = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_PreLDPCBER");

    if ((!pDemod) || (!pBERH) || (!pBERL)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC0, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    valid[0] = (rdata[0] & 0x01) ? 1 : 0;
    valid[1] = (rdata[0] & 0x02) ? 1 : 0;

    if (valid[0]){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, rdata, 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[0] = ((uint32_t)(rdata[0] & 0x0F) << 24) |
                      ((uint32_t)(rdata[1] & 0xFF) << 16) |
                      ((uint32_t)(rdata[2] & 0xFF) <<  8) |
                      (uint32_t)(rdata[3] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD0, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        measuredPeriod[0] = (uint32_t)(1 << (rdata[0] & 0x0F));
    }

    if (valid[1]){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC5, rdata, 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[1] = ((uint32_t)(rdata[0] & 0x0F) << 24) |
                      ((uint32_t)(rdata[1] & 0xFF) << 16) |
                      ((uint32_t)(rdata[2] & 0xFF) <<  8) |
                      (uint32_t)(rdata[3] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD1, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        measuredPeriod[1] = (uint32_t)(1 << (rdata[0] & 0x0F));
    }

    SLVT_UnFreezeReg (pDemod);

    pBER[0] = pBERH;
    pBER[1] = pBERL;

    for (i = 0; i < 2; i++) {
        if (!valid[i]) {
            *pBER[i] = CXDREF_DEMOD_ISDBS3_MONITOR_PRELDPCBER_INVALID;
            continue;
        }

        if (bitError[i] > (measuredPeriod[i] * 44880)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempDiv = measuredPeriod[i] * 561;
        if (tempDiv == 0){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (bitError[i] * 10) / tempDiv;
        tempR = (bitError[i] * 10) % tempDiv;

        tempR *= 125;
        tempQ = (tempQ * 125) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        tempR *= 100;
        tempQ = (tempQ * 100) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if (tempR >= (tempDiv/2)){
            *pBER[i] = tempQ + 1;
        } else {
            *pBER[i] = tempQ;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_PreBCHBER (cxdref_demod_t * pDemod,
                                                   uint32_t * pBERH, uint32_t * pBERL)
{
    uint8_t rdata[3];
    uint8_t valid[2] = {0};
    uint32_t bitError[2] = {0};
    uint32_t periodExp[2] = {0};
    uint32_t * pBER[2];
    cxdref_isdbs3_cod_t cod[2];
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    uint32_t tempA = 0;
    uint32_t tempB = 0;
    int i = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_PreBCHBER");

    if ((!pDemod) || (!pBERL) || (!pBERH)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }


    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA0, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    valid[0] = (rdata[0] & 0x01) ? 1 : 0;
    valid[1] = (rdata[0] & 0x02) ? 1 : 0;

    if (valid[0]){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA1, rdata, 3) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[0] = ((uint32_t)(rdata[0] & 0x7F) << 16) |
                      ((uint32_t)(rdata[1] & 0xFF) <<  8) |
                      (uint32_t)(rdata[2] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB3, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp[0] = (uint32_t)(rdata[0] & 0x0F);
    }

    if (valid[1]){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA4, rdata, 3) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[1] = ((uint32_t)(rdata[0] & 0x7F) << 16) |
                      ((uint32_t)(rdata[1] & 0xFF) <<  8) |
                      (uint32_t)(rdata[2] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB4, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp[1] = (uint32_t)(rdata[0] & 0x0F);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xAB, rdata, 2) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    cod[0] = (cxdref_isdbs3_cod_t)(rdata[0] & 0x0F);
    cod[1] = (cxdref_isdbs3_cod_t)(rdata[1] & 0x0F);

    pBER[0] = pBERH;
    pBER[1] = pBERL;

    for (i = 0; i < 2; i++) {
        if (!valid[i]) {
            *pBER[i] = CXDREF_DEMOD_ISDBS3_MONITOR_PREBCHBER_INVALID;
            continue;
        }

        switch(cod[i])
        {
        case CXDREF_ISDBS3_COD_41_120:  tempA =  41; tempB = 120; break;
        case CXDREF_ISDBS3_COD_49_120:  tempA =  49; tempB = 120; break;
        case CXDREF_ISDBS3_COD_61_120:  tempA =  61; tempB = 120; break;
        case CXDREF_ISDBS3_COD_73_120:  tempA =  73; tempB = 120; break;
        case CXDREF_ISDBS3_COD_81_120:  tempA =  81; tempB = 120; break;
        case CXDREF_ISDBS3_COD_89_120:  tempA =  89; tempB = 120; break;
        case CXDREF_ISDBS3_COD_93_120:  tempA =  93; tempB = 120; break;
        case CXDREF_ISDBS3_COD_97_120:  tempA =  97; tempB = 120; break;
        case CXDREF_ISDBS3_COD_101_120: tempA = 101; tempB = 120; break;
        case CXDREF_ISDBS3_COD_105_120: tempA = 105; tempB = 120; break;
        case CXDREF_ISDBS3_COD_109_120: tempA = 109; tempB = 120; break;
        default: CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if(periodExp[i] >= 10) {
            tempDiv = (1 << (periodExp[i] - 5)) * 561 * tempA;
            if (tempDiv == 0){
                CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
            }

            tempQ = (bitError[i] * tempB) / tempDiv;
            tempR = (bitError[i] * tempB) % tempDiv;

            tempR *= 25;
            tempQ = (tempQ * 25) + (tempR / tempDiv);
            tempR = tempR % tempDiv;

            tempR *= 25;
            tempQ = (tempQ * 25) + (tempR / tempDiv);
            tempR = tempR % tempDiv;

            tempR *= 25;
            tempQ = (tempQ * 25) + (tempR / tempDiv);
            tempR = tempR % tempDiv;

            tempR *= 25;
            tempQ = (tempQ * 25) + (tempR / tempDiv);
            tempR = tempR % tempDiv;

            if (tempR >= (tempDiv/2)){
                *pBER[i] = tempQ + 1;
            } else {
                *pBER[i] = tempQ;
            }
        }
        else {
            tempDiv = (1 << periodExp[i]) * 561 * tempA;
            if (tempDiv == 0){
                CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
            }

            tempQ = (bitError[i] * tempB) / tempDiv;
            tempR = (bitError[i] * tempB) % tempDiv;

            tempR *= 125;
            tempQ = (tempQ * 125) + (tempR / tempDiv);
            tempR = tempR % tempDiv;

            tempR *= 100;
            tempQ = (tempQ * 100) + (tempR / tempDiv);
            tempR = tempR % tempDiv;

            tempR *= 100;
            tempQ = (tempQ * 100) + (tempR / tempDiv);
            tempR = tempR % tempDiv;

            tempR *= 10;
            tempQ = (tempQ * 10) + (tempR / tempDiv);
            tempR = tempR % tempDiv;

            if (tempR >= (tempDiv/2)){
                *pBER[i] = tempQ + 1;
            } else {
                *pBER[i] = tempQ;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_PostBCHFER (cxdref_demod_t * pDemod,
                                                    uint32_t * pFERH, uint32_t * pFERL)
{
    uint8_t rdata[2];
    uint8_t valid[2] = {0};
    uint32_t frameError[2] = {0};
    uint32_t frameCount[2] = {0};
    uint32_t * pFER[2];
    uint32_t tempDiv = 0;
    uint32_t tempQ = 0;
    uint32_t tempR = 0;
    int i = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_PostBCHFER");

    if ((!pDemod) || (!pFERL)|| (!pFERH)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA0, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    valid[0] = (rdata[0] & 0x01) ? 1 : 0;
    valid[1] = (rdata[0] & 0x02) ? 1 : 0;

    if (valid[0]){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA7, rdata, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        frameError[0] = ((uint32_t)(rdata[0] & 0xFF) << 8) |
                        (uint32_t)(rdata[1] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB3, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        frameCount[0] = (uint32_t)(1 << (rdata[0] & 0x0F));
    }

    if (valid[1]){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, rdata, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        frameError[1] = ((uint32_t)(rdata[0] & 0xFF) << 8) |
                        (uint32_t)(rdata[1] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB4, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        frameCount[1] = (uint32_t)(1 << (rdata[0] & 0x0F));
    }

    SLVT_UnFreezeReg (pDemod);

    pFER[0] = pFERH;
    pFER[1] = pFERL;

    for (i = 0; i < 2; i++) {
        if (!valid[i]) {
            *pFER[i] = CXDREF_DEMOD_ISDBS3_MONITOR_POSTBCHFER_INVALID;
            continue;
        }

        tempDiv = frameCount[i];
        if ((tempDiv == 0) || (frameError[i] > frameCount[i])){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        tempQ = (frameError[i] * 10000) / tempDiv;
        tempR = (frameError[i] * 10000) % tempDiv;

        tempR *= 100;
        tempQ = (tempQ * 100) + (tempR / tempDiv);
        tempR = tempR % tempDiv;

        if ((tempDiv != 1) && (tempR >= (tempDiv/2))){
            *pFER[i] = tempQ + 1;
        } else {
            *pFER[i] = tempQ;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_ModCod (cxdref_demod_t * pDemod,
                                                cxdref_isdbs3_mod_t * pModH, cxdref_isdbs3_mod_t * pModL,
                                                cxdref_isdbs3_cod_t * pCodH, cxdref_isdbs3_cod_t * pCodL)
{
    uint8_t rdata[2];
    uint8_t valid[2];

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_ModCod");

    if ((!pDemod) || (!pModH) || (!pCodH) || (!pModL) || (!pCodL)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA0, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    valid[0] = (rdata[0] & 0x01) ? 1 : 0;
    valid[1] = (rdata[0] & 0x02) ? 1 : 0;

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xAB, rdata, 2) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    if (valid[0]) {
        *pModH = (cxdref_isdbs3_mod_t)((rdata[0] >> 4) & 0x07);
        *pCodH = (cxdref_isdbs3_cod_t)(rdata[0] & 0x0F);
    } else {
        *pModH = CXDREF_ISDBS3_MOD_UNUSED_15;
        *pCodH = CXDREF_ISDBS3_COD_UNUSED_15;
    }
    if (valid[1]) {
        *pModL = (cxdref_isdbs3_mod_t)((rdata[1] >> 4) & 0x07);
        *pCodL = (cxdref_isdbs3_cod_t)(rdata[1] & 0x0F);
    } else {
        *pModL = CXDREF_ISDBS3_MOD_UNUSED_15;
        *pCodL = CXDREF_ISDBS3_COD_UNUSED_15;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_ValidSlotNum (cxdref_demod_t * pDemod,
                                                      uint8_t * pSlotNumH, uint8_t * pSlotNumL)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata[8];
    int i = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_ValidSlotNum");

    if ((!pDemod) || (!pSlotNumH) || (!pSlotNumL)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD1);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB0, rdata, 8) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pSlotNumH = 0;
    *pSlotNumL = 0;

    for (i = 0; i < 8; i++) {
        if (rdata[i] > 0) {
            if (*pSlotNumH == 0) {
                *pSlotNumH = rdata[i];
            } else if (*pSlotNumL == 0) {
                *pSlotNumL = rdata[i];
                break;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_TMCCInfo (cxdref_demod_t * pDemod,
                                                  cxdref_isdbs3_tmcc_info_t * pTMCCInfo)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[240] = {0x00};
    uint32_t i = 0;
    CXDREF_TRACE_ENTER("cxdref_demod_isdbs3_monitor_TMCCInfo");

    if ((!pDemod) || (!pTMCCInfo)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = IsDmdLocked(pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    if (FreezeTMCCInfo (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (IsTMCCReadOK (pDemod) != CXDREF_RESULT_OK) {
        UnfreezeTMCCInfo (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xDB);
    if (result != CXDREF_RESULT_OK) {
        UnfreezeTMCCInfo (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 240);
    if (result != CXDREF_RESULT_OK) {
        UnfreezeTMCCInfo (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    pTMCCInfo->changeOrder = (data[0] & 0xFF);

    for (i = 0; i < 8; i++) {
        pTMCCInfo->modcodSlotInfo[i].mod = (cxdref_isdbs3_mod_t) ((data[(3 * i) + 1] >> 4) & 0x0F);
        pTMCCInfo->modcodSlotInfo[i].cod = (cxdref_isdbs3_cod_t) (data[(3 * i) + 1] & 0x0F);
        pTMCCInfo->modcodSlotInfo[i].slotNum = (data[(3 * i + 2)] & 0xFF);
        pTMCCInfo->modcodSlotInfo[i].backoff = (data[(3 * i + 3)] & 0xFF);
    }

    for (i = 0; i < 16; i++) {
        pTMCCInfo->streamTypeForEachRelativeStream[i] = (cxdref_isdbs3_stream_type_t) (data[i + 25] & 0xFF);
    }

    for (i = 0; i < 16; i++) {
        pTMCCInfo->packetLengthForEachRelativeStream[i] = (uint16_t)((data[(2 * i) + 41] << 8) | data[(2 * i + 1) + 41]);
    }

    for (i = 0; i < 16; i++) {
        pTMCCInfo->syncPatternBitLengthForEachRelativeStream[i] = (uint8_t) (data[i + 73] & 0xFF);
    }

    for (i = 0; i < 16; i++) {
        pTMCCInfo->syncPatternForEachRelativeStream[i] = (uint32_t) ((data[(4 * i)     + 89] << 24) |
                                                                     (data[(4 * i + 1) + 89] << 16) |
                                                                     (data[(4 * i + 2) + 89] << 8)  |
                                                                     (data[(4 * i + 3) + 89]));
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xDD);
    if (result != CXDREF_RESULT_OK) {
        UnfreezeTMCCInfo (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 240);
    if (result != CXDREF_RESULT_OK) {
        UnfreezeTMCCInfo (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    for (i = 0;i < 60; i++) {
        pTMCCInfo->relativeStreamForEachSlot[2 * i]       = (uint8_t)((data[i + 153] >> 4) & 0x0F);
        pTMCCInfo->relativeStreamForEachSlot[(2 * i) + 1] = (uint8_t)(data[i + 153] & 0x0F);
    }

    for (i = 0; i < 13; i++) {
        pTMCCInfo->streamidForEachRelativeStream[i] = (uint16_t)((data[(2 * i) + 213] << 8) | data[(2 * i + 1) + 213]);
    }

    pTMCCInfo->streamidForEachRelativeStream[13] = (uint16_t)(data[239] << 8);

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xDE);
    if (result != CXDREF_RESULT_OK) {
        UnfreezeTMCCInfo (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 6);
    if (result != CXDREF_RESULT_OK) {
        UnfreezeTMCCInfo (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    pTMCCInfo->streamidForEachRelativeStream[13] |= data[0];

    for (i = 0; i < 2; i++) {
        pTMCCInfo->streamidForEachRelativeStream[i + 14] = (uint16_t)((data[(2 * i) + 1] << 8) | data[(2 * i + 1) + 1]);
    }

    pTMCCInfo->ewsFlag = (uint8_t)((data[5] & 0x80) ? 1 : 0);
    pTMCCInfo->uplinkInfo = (uint8_t)((data[5] >> 4) & 0x07);

    UnfreezeTMCCInfo (pDemod);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_CurrentStreamModCodSlotNum (cxdref_demod_t *pDemod, cxdref_isdbs3_tmcc_info_t * pTmccInfo,
                                                                    cxdref_isdbs3_mod_t * pModH, cxdref_isdbs3_mod_t * pModL,
                                                                    cxdref_isdbs3_cod_t * pCodH, cxdref_isdbs3_cod_t * pCodL,
                                                                    uint8_t * pSlotNumH, uint8_t * pSlotNumL)
{
    uint16_t streamID = 0;
    uint8_t relativeStreamNum = 255;

    CXDREF_TRACE_ENTER("cxdref_demod_isdbs3_monitor_CurrentStreamModCodSlotNum");

    if ((!pDemod) || (!pTmccInfo) || (!pModH) || (!pModL) || (!pCodH) || (!pCodL) || (!pSlotNumH) || (!pSlotNumL)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    {
        uint8_t data[2];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x8A, data, 1) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (data[0] & 0x10) {
                relativeStreamNum = (uint8_t)(data[0] & 0x0F);
                goto GOT_RELATIVE_STREAM_NUM;
            }
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x87, data, 2) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        streamID = (uint16_t)((data[0] << 8) | (data[1]));
    }

    {
        int i = 0;

        for (i = 0; i < 16; i++) {
            if (pTmccInfo->streamidForEachRelativeStream[i] == streamID) {
                relativeStreamNum = (uint8_t)i;
                break;
            }
        }
    }

GOT_RELATIVE_STREAM_NUM:
    if (relativeStreamNum > 15) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    {
        int i = 0;
        int j = 0;
        int slotTotal = 0;

        *pModH = CXDREF_ISDBS3_MOD_UNUSED_15;
        *pModL = CXDREF_ISDBS3_MOD_UNUSED_15;
        *pCodH = CXDREF_ISDBS3_COD_UNUSED_15;
        *pCodL = CXDREF_ISDBS3_COD_UNUSED_15;
        *pSlotNumH = 0;
        *pSlotNumL = 0;

        for (i = 0; i < 8; i++) {
            for (j = slotTotal; j < slotTotal + pTmccInfo->modcodSlotInfo[i].slotNum; j++) {
                if (j >= 120) {
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
                }

                if (pTmccInfo->relativeStreamForEachSlot[j] == relativeStreamNum) {
                    if (*pModH == CXDREF_ISDBS3_MOD_UNUSED_15) {
                        (*pSlotNumH)++;
                    } else if (*pModL == CXDREF_ISDBS3_MOD_UNUSED_15) {
                        (*pSlotNumL)++;
                    }
                }
            }

            if ((*pModH == CXDREF_ISDBS3_MOD_UNUSED_15) && (*pSlotNumH > 0)) {
                *pModH = pTmccInfo->modcodSlotInfo[i].mod;
                *pCodH = pTmccInfo->modcodSlotInfo[i].cod;
            } else if ((*pModL == CXDREF_ISDBS3_MOD_UNUSED_15) && (*pSlotNumL > 0)) {
                *pModL = pTmccInfo->modcodSlotInfo[i].mod;
                *pCodL = pTmccInfo->modcodSlotInfo[i].cod;
            }

            if ((*pModH != CXDREF_ISDBS3_MOD_UNUSED_15) && (*pModL != CXDREF_ISDBS3_MOD_UNUSED_15)) {
                break;
            }

            slotTotal += pTmccInfo->modcodSlotInfo[i].slotNum;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_SiteDiversityInfo (cxdref_demod_t * pDemod,
                                                           uint8_t * pSiteDiversityInfo)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_SiteDiversityInfo");

    if ((!pDemod) || (!pSiteDiversityInfo)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF0, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pSiteDiversityInfo =  (uint8_t)((rdata & 0x02) ? 1 : 0);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                   uint8_t * pEWSChange)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_EWSChange");

    if ((!pDemod) || (!pEWSChange)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF0, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pEWSChange =  (uint8_t)((rdata & 0x04) ? 1 : 0);

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_TMCCChange (cxdref_demod_t * pDemod,
                                                    uint8_t * pTMCCChange)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_TMCCChange");

    if ((!pDemod) || (!pTMCCChange)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF0, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);
    *pTMCCChange =  (uint8_t)((rdata & 0x01) ? 1 : 0);

    CXDREF_TRACE_RETURN(result);
}
cxdref_result_t cxdref_demod_isdbs3_ClearEWSChange (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_ClearEWSChange");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF1, 0x04);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}
cxdref_result_t cxdref_demod_isdbs3_ClearTMCCChange (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_ClearTMCCChange");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF1, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_StreamIDError (cxdref_demod_t * pDemod, uint8_t * pStreamIDError)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_StreamIDError");

    if ((!pDemod) || (!pStreamIDError)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
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

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x95, &rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);
    *pStreamIDError =  (uint8_t)((rdata & 0x01) ? 1 : 0);

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_LowCN (cxdref_demod_t * pDemod,
                                               uint8_t * pLowCN)
{
    uint8_t rdata[5];
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_LowCN");

    if ((!pDemod) || (!pLowCN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF3, rdata, 5) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x01) == 0x00) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *pLowCN = (uint8_t)(rdata[4] & 0x01);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_ClearLowCN (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_ClearLowCN");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF8, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t IsDmdLocked(cxdref_demod_t* pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t agcLock = 0;
    uint8_t tstlvLock  = 0;
    uint8_t tmccLock  = 0;
    CXDREF_TRACE_ENTER("IsDmdLocked");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_isdbs3_monitor_SyncStat(pDemod, &agcLock, &tstlvLock, &tmccLock);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    if (tmccLock) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
    } else {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }
}

static cxdref_result_t FreezeTMCCInfo(cxdref_demod_t* pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER("FreezeTMCCInfo");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFA, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}

static cxdref_result_t UnfreezeTMCCInfo(cxdref_demod_t* pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER("UnfreezeTMCCInfo");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFA, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN(result);
}

static cxdref_result_t IsTMCCReadOK(cxdref_demod_t* pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata = 0;
    CXDREF_TRACE_ENTER("IsTMCCReadOK");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFB, &rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (rdata & 0x01) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
    } else {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }
}
