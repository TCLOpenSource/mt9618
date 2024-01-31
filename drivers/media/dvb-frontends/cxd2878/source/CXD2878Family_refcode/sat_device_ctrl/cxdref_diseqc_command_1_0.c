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

#include "cxdref_diseqc_command_1_0.h"

cxdref_result_t cxdref_diseqc_command_Reset (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Reset");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x00;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Standby (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Standby");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x02;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_PowerOn (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_PowerOn");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x03;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetLo (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetLo");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x20;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetVR (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetVR");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x21;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetPosA (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetPosA");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x22;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetSOA (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetSOA");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x23;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetHi (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetHi");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x24;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetHL (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetHL");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x25;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetPosB (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetPosB");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x26;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetSOB (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetSOB");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x27;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_WriteN0 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t data)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_WriteN0");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x38;
    pMessage->data[3] = data;
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
