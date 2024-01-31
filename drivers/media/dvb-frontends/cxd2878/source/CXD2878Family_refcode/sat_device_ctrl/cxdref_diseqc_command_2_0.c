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

#include "cxdref_diseqc_command_2_0.h"


cxdref_result_t cxdref_diseqc_command_ClrReset (cxdref_diseqc_message_t * pMessage,
                                            cxdref_diseqc_framing_t framing,
                                            cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_ClrReset");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x01;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_SetContend (cxdref_diseqc_message_t * pMessage,
                                              cxdref_diseqc_framing_t framing,
                                              cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_SetContend");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x04;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Contend (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Contend");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x05;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_ClrContend (cxdref_diseqc_message_t * pMessage,
                                              cxdref_diseqc_framing_t framing,
                                              cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_ClrContend");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x06;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Address (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Address");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x07;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_MoveC (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address,
                                         uint8_t data)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_MoveC");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x08;
    pMessage->data[3] = data;
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Move (cxdref_diseqc_message_t * pMessage,
                                        cxdref_diseqc_framing_t framing,
                                        cxdref_diseqc_address_t address,
                                        uint8_t data)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Move");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x09;
    pMessage->data[3] = data;
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Status (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Status");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x10;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Config (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Config");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x11;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Switch0 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Switch0");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x14;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_Switch1 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_Switch1");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x15;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_ReadA0 (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_ReadA0");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x40;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_ReadA1 (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_ReadA1");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x41;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_LOstring (cxdref_diseqc_message_t * pMessage,
                                            cxdref_diseqc_framing_t framing,
                                            cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_LOstring");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x50;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_LOnow (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_LOnow");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x51;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_LOLo (cxdref_diseqc_message_t * pMessage,
                                        cxdref_diseqc_framing_t framing,
                                        cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_LOLo");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x52;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_diseqc_command_LOHi (cxdref_diseqc_message_t * pMessage,
                                        cxdref_diseqc_framing_t framing,
                                        cxdref_diseqc_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_diseqc_command_LOHi");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = (uint8_t)framing;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x53;
    pMessage->length = 3;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
