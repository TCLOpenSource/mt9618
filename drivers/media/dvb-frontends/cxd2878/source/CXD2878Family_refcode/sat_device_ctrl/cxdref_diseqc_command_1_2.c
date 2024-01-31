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

#include "cxdref_diseqc_command_1_2.h"

static cxdref_result_t generateDriveXData(cxdref_diseqc_motor_mode_t mode,
                                        uint8_t amount,
                                        uint8_t * pData);

cxdref_result_t cxdref_diseqc_command_WriteA0 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t data)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_WriteA0");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x48;
    pMessage->data[3] = data;
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_WriteA1 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t data)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_WriteA1");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x49;
    pMessage->data[3] = data;
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Halt (cxdref_diseqc_message_t * pMessage,
                                        cxdref_diseqc_framing_t framing,
                                        cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Halt");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x60;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_LimitsOff (cxdref_diseqc_message_t * pMessage,
                                             cxdref_diseqc_framing_t framing,
                                             cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_LimitsOff");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x63;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_LimitE (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_LimitE");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x66;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_LimitW (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_LimitW");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x67;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_DriveEast (cxdref_diseqc_message_t * pMessage,
                                             cxdref_diseqc_framing_t framing,
                                             cxdref_diseqc_address_t address,
                                             cxdref_diseqc_motor_mode_t mode,
                                             uint8_t amount)
{
    int32_t index = 0;
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_DriveEast");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x68;
    result = generateDriveXData(mode, amount, &pMessage->data[3]);
    if (result != CXDREF_RESULT_OK){CXDREF_TRACE_RETURN (result);}
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_DriveWest (cxdref_diseqc_message_t * pMessage,
                                             cxdref_diseqc_framing_t framing,
                                             cxdref_diseqc_address_t address,
                                             cxdref_diseqc_motor_mode_t mode,
                                             uint8_t amount)
{
    int32_t index = 0;
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_DriveWest");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x69;
    result = generateDriveXData(mode, amount, &pMessage->data[3]);
    if (result != CXDREF_RESULT_OK){CXDREF_TRACE_RETURN (result);}
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Storenn (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t posNumber)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Storenn");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x6A;
    pMessage->data[3] = posNumber;
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Gotonn (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address,
                                          uint8_t posNumber)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Gotonn");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x6B;
    pMessage->data[3] = posNumber;
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetPosns (cxdref_diseqc_message_t * pMessage,
                                            cxdref_diseqc_framing_t framing,
                                            cxdref_diseqc_address_t address,
                                            uint8_t * pData,
                                            uint8_t length)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetPosns");

    if ((!pMessage) || (!pData)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x6F;
    if (length == 1){
        pMessage->data[3] = *pData;
        pMessage->length = 4;
    } else if (length == 3){
        pMessage->data[3] = *(pData);
        pMessage->data[4] = *(pData + 1);
        pMessage->data[5] = *(pData + 2);
        pMessage->length = 6;
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t generateDriveXData(cxdref_diseqc_motor_mode_t mode,
                                        uint8_t amount,
                                        uint8_t * pData)
{
    CXDREF_TRACE_ENTER ("generateDriveXData");

    if (!pData) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    switch(mode)
    {
    case CXDREF_DISEQC_MOTOR_MODE_STEPPED:
        if((amount > 128) || (amount == 0)){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        } else {
            *pData = (uint8_t)((0xFF - amount) + 1);
        }
        break;

    case CXDREF_DISEQC_MOTOR_MODE_TIMED:
        if((amount > 127) || (amount == 0)){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        } else {
            *pData = amount;
        }
        break;

    case CXDREF_DISEQC_MOTOR_MODE_NONSTOP:
        *pData = 0;
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
