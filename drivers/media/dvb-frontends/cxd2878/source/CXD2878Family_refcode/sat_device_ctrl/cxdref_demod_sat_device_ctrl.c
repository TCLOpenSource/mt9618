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
#include "cxdref_demod.h"
#include "cxdref_demod_sat_device_ctrl.h"

cxdref_result_t cxdref_demod_sat_device_ctrl_Enable (cxdref_demod_t * pDemod, uint8_t enable)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_Enable");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, (uint8_t)(enable ? 0x00 : 0x20), 0x20) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9C, (uint8_t)(enable ? 0x40 : 0x00)) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_DSQOUTSetting (cxdref_demod_t * pDemod,
                                                        cxdref_dsqout_mode_t mode,
                                                        uint8_t toneFreqKHz)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_DSQOUTSetting");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if(toneFreqKHz == 22){
    } else if(toneFreqKHz == 44){
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x30, 0x02, 0xFF);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x31, 0x21, 0xFF);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    switch(mode)
    {
    case CXDREF_DSQOUT_MODE_PWM:
        data = 0x00;
        break;

    case CXDREF_DSQOUT_MODE_ENVELOPE:
        data = 0x01;
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x75, data, 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_RXENSetting (cxdref_demod_t * pDemod,
                                                      cxdref_rxen_mode_t mode,
                                                      uint8_t posDelay)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[2] = {0, 0};
    uint32_t value = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_RXENSetting");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(mode)
    {
    case CXDREF_RXEN_MODE_NORMAL:
        data[0] = 0x00;
        break;

    case CXDREF_RXEN_MODE_INV:
        data[0] = 0x01;
        break;

    case CXDREF_RXEN_MODE_FIXED_LOW:
        data[0] = 0x02;
        break;

    case CXDREF_RXEN_MODE_FIXED_HIGH:
        data[0] = 0x03;
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x78, data[0], 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    value = posDelay * 22;

    data[0] = (uint8_t)((value >> 8) & 0xFF);
    data[1] = (uint8_t)( value       & 0xFF);

    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x7D, data[0], 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }
    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x7E, data[1], 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_TXENSetting (cxdref_demod_t * pDemod,
                                                      cxdref_txen_mode_t mode,
                                                      uint8_t posDelay,
                                                      uint8_t negDelay)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[2] = {0, 0};
    uint32_t value = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_TXENSetting");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(mode)
    {
    case CXDREF_TXEN_MODE_NORMAL:
        data[0] = 0x00;
        break;

    case CXDREF_TXEN_MODE_INV:
        data[0] = 0x01;
        break;

    case CXDREF_TXEN_MODE_FIXED_LOW:
        data[0] = 0x02;
        break;

    case CXDREF_TXEN_MODE_FIXED_HIGH:
        data[0] = 0x03;
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x77, data[0], 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    value = posDelay * 22;

    data[0] = (uint8_t)((value >> 8) & 0xFF);
    data[1] = (uint8_t)( value       & 0xFF);

    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x79, data[0], 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }
    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x7A, data[1], 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    value = negDelay * 22;

    data[0] = (uint8_t)((value >> 8) & 0xFF);
    data[1] = (uint8_t)( value       & 0xFF);

    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x7B, data[0], 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }
    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xBB, 0x7C, data[1], 0xFF);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_OutputTone(cxdref_demod_t * pDemod,
                                                    uint8_t isEnable)
{
    uint8_t data = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_OutputTone");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (isEnable){
        data = 0x01;
    } else {
        data = 0x00;
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x36, data) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_SetToneBurst(cxdref_demod_t * pDemod,
                                                      cxdref_toneburst_mode_t toneBurstMode)
{
    uint8_t data = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_SetToneBurst");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    switch(toneBurstMode)
    {
    case CXDREF_TONEBURST_MODE_OFF:
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x34, 0x00) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);

    case CXDREF_TONEBURST_MODE_A:
        data = 0x00;
        break;

    case CXDREF_TONEBURST_MODE_B:
        data = 0x01;
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x34, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x35, data) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_SetDiseqcCommand(cxdref_demod_t * pDemod,
                                                          uint8_t isEnable,
                                                          cxdref_diseqc_message_t * pCommand1,
                                                          uint8_t count1,
                                                          cxdref_diseqc_message_t * pCommand2,
                                                          uint8_t count2)
{
    int32_t index = 0;
    uint8_t data[CXDREF_DISEQC_MESSAGE_LENGTH];
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_SetDiseqcCommand");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (isEnable){
        if ((!pCommand1) || (count1 == 0)){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, 0x01) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pCommand1->length > CXDREF_DISEQC_MESSAGE_LENGTH){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3D, pCommand1->length) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
            data[index] = 0x00;
        }
        for(index = 0; index < pCommand1->length; index++){
            data[index] = pCommand1->data[index];
        }
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3E, data, CXDREF_DISEQC_MESSAGE_LENGTH) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x37, count1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((!pCommand2) || (count2 == 0)){
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, 0x00) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        } else {
            if (pCommand2->length > CXDREF_DISEQC_MESSAGE_LENGTH){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
            }
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4A, pCommand2->length) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
                data[index] = 0x00;
            }
            for(index = 0; index < pCommand2->length; index++){
                data[index] = pCommand2->data[index];
            }
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, data, CXDREF_DISEQC_MESSAGE_LENGTH) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, count2) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
    } else {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, 0x00) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_SetTxIdleTime(cxdref_demod_t * pDemod,
                                                       uint8_t idleTimeMs)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t value = 0;
    uint8_t data[2];
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_SetTxIdleTime");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    value = idleTimeMs * 22;
    if (value > 0x0FFF){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    data[0] = (uint8_t)((value >> 8) & 0x0F);
    data[1] = (uint8_t)( value       & 0xFF);

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x39, data, 2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_SetRxIdleTime(cxdref_demod_t * pDemod,
                                                       uint8_t idleTimeMs)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t value = 0;
    uint8_t data[2];
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_SetRxIdleTime");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    value = idleTimeMs * 220;
    if (value > 0xFFFF){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    data[0] = (uint8_t)((value >> 8) & 0xFF);
    data[1] = (uint8_t)( value       & 0xFF);

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x67, data, 2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_SetR2RTime (cxdref_demod_t * pDemod,
                                                     uint32_t r2rTime)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t value = 0;
    uint8_t data[2];
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_SetR2RTime");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    value = r2rTime * 22;
    if (value > 0x3FFF){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    data[0] = (uint8_t)((value >> 8) & 0x3F);
    data[1] = (uint8_t)( value       & 0xFF);

    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x71, data, 2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode(cxdref_demod_t * pDemod,
                                                            uint8_t isEnable,
                                                            uint16_t replyTimeoutMs)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t value = 0;
    uint8_t data[2];
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (isEnable){

        value = (uint32_t)replyTimeoutMs * 22;
        if (value > 0x3FFF){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }
        data[0] = (uint8_t)((value >> 8) & 0x3F);
        data[1] = (uint8_t)( value       & 0xFF);

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3B, data, 2) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6A, 0x01) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x74, 0x12) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x74, 0x00) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_StartTransmit(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_StartTransmit");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_GetTransmitStatus(cxdref_demod_t * pDemod,
                                                           uint8_t * pStatus)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_GetTransmitStatus");

    if ((!pDemod) || (!pStatus)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pStatus = data;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_GetReplyMessage(cxdref_demod_t * pDemod,
                                                         cxdref_diseqc_message_t * pReplyMessage)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data = 0;
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_GetReplyMessage");

    if ((!pDemod) || (!pReplyMessage)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x11, &data, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data == 0x08){
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x12, &data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (data > CXDREF_DISEQC_MESSAGE_LENGTH){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        pReplyMessage->length = data;
        for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
            pReplyMessage->data[index] = 0;
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x13, pReplyMessage->data, pReplyMessage->length) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_GetRXENMode (cxdref_demod_t * pDemod,
                                                      uint8_t * pIsEnable)
{
    uint8_t hiz = 0;
    uint8_t data[3] = {0, 0, 0};
    uint8_t tshiz = 0;
    uint8_t tsdata[4] = {0, 0, 0, 0};

    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_GetRXENMode");

    if ((!pDemod) || (!pIsEnable)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x82, &hiz, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xA3, data, 3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xB3, tsdata, 4) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x81, &tshiz, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((((hiz & 0x01) == 0) && (data[0] == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((hiz & 0x02) == 0) && (data[1] == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((hiz & 0x04) == 0) && (data[2] == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((tshiz & 0x01) == 0) && ((tsdata[0] & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((tshiz & 0x02) == 0) && (((tsdata[0] >> 4) & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((tshiz & 0x04) == 0) && ((tsdata[1] & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((tshiz & 0x08) == 0) && (((tsdata[1] >> 4) & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((tshiz & 0x10) == 0) && ((tsdata[2] & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((tshiz & 0x20) == 0) && (((tsdata[2] >> 4) & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((tshiz & 0x40) == 0) && ((tsdata[3] & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN)) ||
        (((tshiz & 0x80) == 0) && (((tsdata[3] >> 4) & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN))) {

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x78, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        switch(data[0])
        {
        case 0x00:
        case 0x01:
            *pIsEnable = 1;
            break;

        case 0x02:
        case 0x03:
            *pIsEnable = 0;
            break;

        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    } else {
        *pIsEnable = 0;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_GetTXENMode (cxdref_demod_t * pDemod,
                                                      uint8_t * pIsEnable)
{
    uint8_t hiz = 0;
    uint8_t data[3] = {0, 0, 0};
    uint8_t tshiz = 0;
    uint8_t tsdata[4] = {0, 0, 0, 0};

    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_GetTXENMode");

    if ((!pDemod) || (!pIsEnable)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x82, &hiz, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xA3, data, 3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xB3, tsdata, 4) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x81, &tshiz, 1) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((((hiz & 0x01) == 0) && (data[0] == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((hiz & 0x02) == 0) && (data[1] == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((hiz & 0x04) == 0) && (data[2] == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((tshiz & 0x01) == 0) && ((tsdata[0] & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((tshiz & 0x02) == 0) && (((tsdata[0] >> 4) & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((tshiz & 0x04) == 0) && ((tsdata[1] & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((tshiz & 0x08) == 0) && (((tsdata[1] >> 4) & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((tshiz & 0x10) == 0) && ((tsdata[2] & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((tshiz & 0x20) == 0) && (((tsdata[2] >> 4) & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((tshiz & 0x40) == 0) && ((tsdata[3] & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN)) ||
        (((tshiz & 0x80) == 0) && (((tsdata[3] >> 4) & 0x0F) == (uint8_t)CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN))) {

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x77, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        switch(data[0])
        {
        case 0x00:
        case 0x01:
            *pIsEnable = 1;
            break;

        case 0x02:
        case 0x03:
            *pIsEnable = 0;
            break;

        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    } else {
        *pIsEnable = 0;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_SetDiseqcReplyMask (cxdref_demod_t * pDemod,
                                                             uint8_t pattern,
                                                             uint8_t mask)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_SetDiseqcReplyMask");
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xBB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6B, pattern) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6C, mask) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_sat_device_ctrl_EnableSinglecable (cxdref_demod_t * pDemod,
                                                            uint8_t enable)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_sat_device_ctrl_EnableSinglecable");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pDemod->isSinglecable = enable;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
