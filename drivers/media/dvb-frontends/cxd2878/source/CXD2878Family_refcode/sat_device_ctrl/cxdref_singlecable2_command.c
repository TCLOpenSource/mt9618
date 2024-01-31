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
#include "cxdref_demod_sat_device_ctrl.h"
#include "cxdref_singlecable_command.h"

static cxdref_result_t addPINCode (cxdref_diseqc_message_t * pMessage,
                                 uint8_t pin);

cxdref_result_t cxdref_singlecable2_command_ODU_Channel_change (cxdref_diseqc_message_t * pMessage,
                                                            uint8_t userbandId,
                                                            uint32_t ifFreqMHz,
                                                            uint8_t uncommittedSwitches,
                                                            uint8_t committedSwitches)
{
    int32_t index = 0;
    uint16_t T = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_Channel_change");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId < 1) || (userbandId > 32) || (uncommittedSwitches > 0x0F) || (committedSwitches > 0x0F)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    if (ifFreqMHz <= 9) {
        T = (uint16_t)ifFreqMHz;
    } else {
        T = (uint16_t)(ifFreqMHz - 100);
    }

    if (T > 0x7FF){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    pMessage->data[0] = 0x70;
    pMessage->data[1] = (uint8_t)(((userbandId - 1) << 3) | ((T >> 8) & 0x07));
    pMessage->data[2] = (uint8_t)(T & 0xFF);
    pMessage->data[3] = (uint8_t)((uncommittedSwitches << 4) | (committedSwitches));
    pMessage->length = 4;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable2_command_ODU_Channel_change_PIN (cxdref_diseqc_message_t * pMessage,
                                                                uint8_t userbandId,
                                                                uint32_t ifFreqMHz,
                                                                uint8_t uncommittedSwitches,
                                                                uint8_t committedSwitches,
                                                                uint8_t pin)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_Channel_change_PIN");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId < 1) || (userbandId > 32)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    result = cxdref_singlecable2_command_ODU_Channel_change (pMessage, userbandId, ifFreqMHz, uncommittedSwitches, committedSwitches);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    pMessage->data[0] = 0x71;

    result = addPINCode (pMessage, pin);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_singlecable2_command_ODU_PowerOFF (cxdref_diseqc_message_t * pMessage,
                                                      uint8_t userbandId)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_PowerOFF");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId < 1) || (userbandId > 32)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    result = cxdref_singlecable2_command_ODU_Channel_change (pMessage, userbandId, 0, 0, 0);
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_singlecable2_command_ODU_PowerOFF_PIN (cxdref_diseqc_message_t * pMessage,
                                                          uint8_t userbandId, uint8_t pin)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_PowerOFF_PIN");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId < 1) || (userbandId > 32)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    result = cxdref_singlecable2_command_ODU_Channel_change_PIN (pMessage, userbandId, 0, 0, 0, pin);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = addPINCode (pMessage, pin);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_singlecable2_command_ODU_UB_avail (cxdref_diseqc_message_t * pMessage)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_UB_avail");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0x7A;
    pMessage->length = 1;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable2_command_ODU_UB_freq (cxdref_diseqc_message_t * pMessage,
                                                     uint8_t userbandId)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_UB_freq");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId < 1) || (userbandId > 32)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0x7D;
    pMessage->data[1] = (uint8_t)(((userbandId - 1) & 0x1F) << 3);
    pMessage->length = 2;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable2_command_ODU_UB_inuse (cxdref_diseqc_message_t * pMessage)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_UB_inuse");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0x7C;
    pMessage->length = 1;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable2_command_ODU_UB_PIN (cxdref_diseqc_message_t * pMessage)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_UB_PIN");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0x7B;
    pMessage->length = 1;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable2_command_ODU_UB_switches (cxdref_diseqc_message_t * pMessage,
                                                         uint8_t userbandId)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable2_command_ODU_UB_switches");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId < 1) || (userbandId > 32)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0x7E;
    pMessage->data[1] = (uint8_t)(((userbandId - 1) & 0x1F) << 3);
    pMessage->length = 2;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t addPINCode (cxdref_diseqc_message_t * pMessage,
                                 uint8_t pin)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("addPIN");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pMessage->length < (CXDREF_DISEQC_MESSAGE_LENGTH - 1)){
        pMessage->data[pMessage->length] = pin;
        pMessage->length++;
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    CXDREF_TRACE_RETURN (result);
}
