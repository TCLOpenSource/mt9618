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

#include "cxdref_demod_atsc_monitor.h"
#include "cxdref_math.h"
#include "cxdref_stdlib.h"

static cxdref_result_t checkVQLock (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                uint8_t * pVQLockStat,
                                                uint8_t * pAGCLockStat,
                                                uint8_t * pTSLockStat,
                                                uint8_t * pUnlockDetected)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_SyncStat");

    if ((!pDemod) || (!pVQLockStat) || (!pAGCLockStat) || (!pTSLockStat) || (!pUnlockDetected)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0F) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x11, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        *pTSLockStat = (uint8_t)(data & 0x01);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x09) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x62, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        *pAGCLockStat = (uint8_t)((data & 0x10) ? 1 : 0);
        *pUnlockDetected = (uint8_t)((data & 0x40) ? 0 : 1);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0D) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x86, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        *pVQLockStat = (uint8_t)(data & 0x01);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                uint32_t * pIFAGCOut)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_IFAGCOut");

    if ((!pDemod) || (!pIFAGCOut)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x09) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[2];
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x63, data, 2) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pIFAGCOut = ((uint32_t)(data[1] & 0x3F) << 8) | data[0];
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                     int32_t * pOffset)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_CarrierOffset");

    if ((!pDemod) || (!pOffset)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (checkVQLock (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x06) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t  data[3];
        int32_t  freqError[2];
        uint32_t ctlVal = 0;

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0xF8, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        ctlVal = ((uint32_t)(data[2] & 0x0F) << 16) | ((uint32_t)data[1] << 8) | data[0];
        freqError[0] = cxdref_Convert2SComplement (ctlVal, 20);
        freqError[0] *= 1155;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x07) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x30, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        ctlVal = ((uint32_t)(data[2] & 0x0F) << 16) | ((uint32_t)data[1] << 8) | data[0];
        freqError[1] = cxdref_Convert2SComplement (ctlVal, 20);
        freqError[1] *= 3125;

        if ((freqError[0] + freqError[1]) >= 0) {
            *pOffset = - ((freqError[0] + freqError[1] + 273) / 546);
        } else {
            *pOffset = - ((freqError[0] + freqError[1] - 273) / 546);
        }

        if (pDemod->rfSpectrumSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
            *pOffset *= -1;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                uint32_t * pBER)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_PreRSBER");

    if ((!pDemod) || (!pBER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (checkVQLock (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t  data[3];
        uint32_t bitError = 0;
        uint32_t blocks = 0;
        uint16_t bitPerBlock = 0;
        uint8_t  ecntEn = 0;
        uint8_t  overFlow = 1;

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x90, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        ecntEn =   (uint8_t)((data[2] & 0x20) ? 1 : 0);
        overFlow = (uint8_t)((data[2] & 0x40) ? 1 : 0);
        bitError =((uint32_t)(data[2] & 0x1F) << 16) | ((uint32_t)(data[1] & 0xFF) << 8) | data[0];

        if (overFlow){
            *pBER = 10000000;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
        if(!ecntEn){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0D) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitPerBlock = 207 * 8;

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x9A, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        blocks = ((uint32_t)(data[2] & 0xFF) << 16) | ((uint32_t)(data[1] & 0xFF) << 8) | data[0] ;

        if ((blocks == 0) || (blocks < (bitError / bitPerBlock))){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (blocks < 0xFFFFF){
            uint32_t Q = 0;
            uint32_t R = 0;
            uint32_t Div = blocks * (bitPerBlock / 8);

            Q = (bitError  * 1000) / Div;
            R = (bitError  * 1000) % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            R *= 10;
            Q = (Q * 10) + (R / Div);
            R = R % Div;
            if (R  <= (Div/2)) {
                *pBER = Q;
            }
            else {
                *pBER = Q + 1;
            }
        }
        else {
            uint32_t Q = 0;
            uint32_t R = 0;
            uint32_t Div = (blocks * (bitPerBlock / 8)) / 16;

            Q = (bitError  * 625) / Div;
            R = (bitError  * 625) % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            if (R  <= (Div/2)) {
                *pBER = Q;
            }
            else {
                *pBER = Q + 1;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_PreRSSER (cxdref_demod_t * pDemod,
                                                uint32_t * pSER)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_PreRSSER");

    if ((!pDemod) || (!pSER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (checkVQLock (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t  data[3];
        uint32_t symbolError = 0;
        uint32_t symbolPerBlock = 0;
        uint32_t blocks = 0;
        uint8_t  ecntEn = 0;
        uint8_t  overFlow = 1;

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x93, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        ecntEn      =  (uint8_t) ((data[2] & 0x04) ? 1 : 0);
        overFlow    =  (uint8_t) ((data[2] & 0x08) ? 1 : 0);
        symbolError =  ((uint32_t)(data[2] & 0x03) << 16) | ((uint32_t)(data[1] & 0xFF) << 8) | data[0];

        if (overFlow){
            *pSER = 10000000;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
        if (!ecntEn){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        symbolPerBlock = 207;

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x9A, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        blocks = ((uint32_t)(data[2] & 0xFF) << 16) | ((uint32_t)(data[1] & 0xFF) << 8) | data[0];

        if ((blocks == 0) || (blocks < (symbolError / symbolPerBlock))){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (blocks < 0xFFFFF){
            uint32_t Q = 0;
            uint32_t R = 0;
            uint32_t Div =  blocks * symbolPerBlock;

            Q = (symbolError * 10000) / Div;
            R = (symbolError * 10000) % Div;
            R *= 10;
            Q = (Q * 10) + (R / Div);
            R = R % Div;
            R *= 10;
            Q = (Q * 10) + (R / Div);
            R = R % Div;
            R *= 10;
            Q = (Q * 10) + (R / Div);
            R = R % Div;
            if (R  <= (Div/2)) {
                *pSER = Q;
            }
            else {
                *pSER = Q + 1;
            }
        }
        else {
            uint32_t Q = 0;
            uint32_t R = 0;
            uint32_t Div = (blocks * symbolPerBlock) / 8;

            Q = (symbolError * 10000) / Div;
            R = (symbolError * 10000) % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            R *= 5;
            Q = (Q * 5) + (R / Div);
            R = R % Div;
            if (R  <= (Div/2)) {
                *pSER = Q;
            }
            else {
                *pSER = Q + 1;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_PostRSWER (cxdref_demod_t * pDemod,
                                                 uint32_t * pWER)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_PostRSWER");

    if ((!pDemod) || (!pWER)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (checkVQLock (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t  data[3];
        uint32_t wordError = 0;
        uint32_t blocks = 0;
        uint8_t  ecntEn = 0;
        uint8_t  overFlow = 1;

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x96, data, 2) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        ecntEn    =  (uint8_t)((data[1] & 0x40) ? 1 : 0);
        overFlow  =  (uint8_t)((data[1] & 0x80) ? 1 : 0);
        wordError = ((uint32_t)(data[1] & 0x3F) << 8) | data[0];

        if (overFlow){
            *pWER = 1000000;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
        if (!ecntEn){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x9A, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        blocks = ((uint32_t)(data[2] & 0xFF) << 16) | ((uint32_t)(data[1] & 0xFF) << 8) | data[0];

        if (( blocks == 0) || (blocks < wordError)){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (blocks < 0xFFF ){
            *pWER = wordError * 1000000 / blocks;
        }
        else {
            {
                uint32_t Q = 0;
                uint32_t R = 0;
                uint32_t Div = blocks;

                Q = (wordError * 100000) / Div;
                R = (wordError * 100000) % Div;
                R *= 10;
                Q = (Q * 10) + (R / Div);
                R = R % Div;
                if (R  <= (Div/2)) {
                    *pWER = Q;
                }
                else {
                    *pWER = Q + 1;
                }
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_PacketError (cxdref_demod_t * pDemod,
                                                   uint8_t * pPacketErrorDetected)
{
    uint8_t  data = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_PacketError");

    if ((!pDemod) || (!pPacketErrorDetected)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (checkVQLock (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x82, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pPacketErrorDetected = (uint8_t)((data & 0x01) ? 1 : 0);
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_SNR (cxdref_demod_t * pDemod,
                                           int32_t * pSNR)
{
    uint8_t  data[3];
    uint32_t dcl_avgerr_fine;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_SNR");

    if ((!pDemod) || (!pSNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (checkVQLock (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x70, data, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    dcl_avgerr_fine = ((uint32_t)(data[2] & 0x7F) << 16) | ((uint32_t)(data[1] & 0xFF) << 8) | data[0];

    *pSNR = (int32_t)(70418 - (100 * cxdref_math_log10(dcl_avgerr_fine)));

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pPPM)
{
    uint8_t   data[6];
    uint32_t  dtrLfaccHigh;
    uint32_t  dtrLfaccLow;
    int32_t   dtrlfacc;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_SamplingOffset");

    if ((!pDemod) || (!pPPM)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (checkVQLock (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x06) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x17, data, 6) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    dtrLfaccLow =  ((uint32_t)(data[3] & 0xFF) << 24) | ((uint32_t)(data[2] & 0xFF) << 16) | ((uint32_t)(data[1] & 0xFF) << 8) | data[0];
    dtrLfaccHigh = ((uint32_t)(data[5] & 0x0F) << 8)  | data[4];

    dtrlfacc = cxdref_Convert2SComplement (((dtrLfaccHigh & 0xFFF) << 14)  | (dtrLfaccLow >> 18), 26);
    if (dtrlfacc < 0){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint32_t Q = 0;
        uint32_t R = 0;
        uint32_t Div = 0;
        int32_t  symbolRateOffset = 0;
        int8_t   flgNegative = 1;

        Div = ((uint32_t)dtrlfacc + 0x2000000);
        Q = (4800U * 10000U * 64U) / Div;
        R = (4800U * 10000U * 64U) % Div;
        R *= 64;
        Q = (Q * 64) + (R / Div);
        R = R % Div;
        R *= 64;
        Q = (Q * 64) + (R / Div);
        R = R % Div;
        R *= 32;
        Q = (Q * 32) + (R / Div);
        R = R % Div;
        if (R  <= (Div/2)) {
            symbolRateOffset = (int32_t)(10762238 - Q);
        }
         else {
            symbolRateOffset = (int32_t)(10762238 - (Q + 1));
        }
        if (symbolRateOffset < 0) {
            flgNegative = -1;
            symbolRateOffset *= flgNegative;
        }
        Div = 5381119;
        Q = (uint32_t)symbolRateOffset / Div;
        R = (uint32_t)symbolRateOffset % Div;
        R *= 500;
        Q = (Q * 500) + (R / Div);
        R = R % Div;
        R *= 500;
        Q = (Q * 500) + (R / Div);
        R = R % Div;
        R *= 200;
        Q = (Q * 200) + (R / Div);
        R = R % Div;
        if (R  <= (Div/2)) {
            *pPPM = flgNegative * (int32_t) Q;
        } else {
            *pPPM = flgNegative * (int32_t)(Q + 1);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_SignalLevelData_ForUnlockOptimization (cxdref_demod_t * pDemod,
                                                                             uint32_t * pSignalLevelData)
{
    uint8_t data[3];

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_SignalLevelData_ForUnlockOptimization");

    if ((!pDemod) || (!pSignalLevelData)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x09) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x68, data, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pSignalLevelData = ((uint32_t) (data[2] & 0xFF) << 16) | ((uint32_t) (data[1] & 0xFF) << 8) | data[0];
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_InternalDigitalAGCOut (cxdref_demod_t * pDemod,
                                                             uint32_t * pDigitalAGCOut)
{
    uint8_t data[3];

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_InternalDigitalAGCOut");

    if ((!pDemod) || (!pDigitalAGCOut)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x08) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x1B, data, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pDigitalAGCOut = ((data[2] & 0x0F) << 16) | (data[1] << 8) | data[0];

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

/* customization from original reference code */
cxdref_result_t cxdref_demod_atsc_custom_monitor_SegmentSync (cxdref_demod_t * pDemod,
                                                          uint8_t * pSegmentSyncStat)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_custom_monitor_SegmentSync_t");

    if ((!pDemod) || (!pSegmentSyncStat)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x07) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x94, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        *pSegmentSyncStat = (uint8_t)(data & 0x01);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t checkVQLock (cxdref_demod_t * pDemod)
{
    uint8_t data = 0x00;

    CXDREF_TRACE_ENTER ("checkVQLock");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x00, 0x0D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVM, 0x86, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data & 0x01) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }
}
