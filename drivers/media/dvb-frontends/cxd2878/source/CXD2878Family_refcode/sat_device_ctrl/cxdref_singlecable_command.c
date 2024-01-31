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

cxdref_result_t cxdref_singlecable_command_ODU_Channel_change (cxdref_diseqc_message_t * pMessage,
                                                           cxdref_singlecable_address_t address,
                                                           uint8_t userbandId,
                                                           uint32_t ubSlotFreqKHz,
                                                           cxdref_singlecable_bank_t bank,
                                                           uint32_t centerFreqKHz)
{
    int32_t index = 0;
    uint16_t T = 0;
    uint8_t data = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_Channel_change");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId <= 0) || (9 <= userbandId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    T = (uint16_t)(((centerFreqKHz + ubSlotFreqKHz + 2000) / 4000) - 350);

    if (T > 0x3FFF){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    pMessage->data[0] = 0xE0;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x5A;

    data  = (uint8_t)(((uint32_t)(userbandId - 1) & 0x07) << 5);
    data |= (uint8_t)(((uint32_t)bank   & 0x07) << 2);
    data |= (uint8_t)((T >> 8) & 0x03);
    pMessage->data[3] = data;

    pMessage->data[4] = (uint8_t)(T & 0xFF);
    pMessage->length = 5;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable_command_ODU_PowerOFF (cxdref_diseqc_message_t * pMessage,
                                                     cxdref_singlecable_address_t address,
                                                     uint8_t userbandId)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_PowerOFF");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId <= 0) || (9 <= userbandId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0xE0;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x5A;
    pMessage->data[3] = (uint8_t)((((uint32_t)(userbandId - 1)) & 0x07) << 5);
    pMessage->data[4] = 0x00;
    pMessage->length = 5;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable_command_ODU_UbxSignal_ON (cxdref_diseqc_message_t * pMessage,
                                                         cxdref_singlecable_address_t address)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_UbxSignal_ON");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0xE0;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x5B;
    pMessage->data[3] = 0x00;
    pMessage->data[4] = 0x00;
    pMessage->length = 5;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable_command_ODU_Config (cxdref_diseqc_message_t * pMessage,
                                                   cxdref_singlecable_address_t address,
                                                   uint8_t userbandId,
                                                   cxdref_singlecable_num_of_sat_pos_t numberOfSatPos,
                                                   cxdref_singlecable_rf_band_t rfBand,
                                                   cxdref_singlecable_num_of_ub_slots_t numberOfUBSlots)
{
    int32_t index = 0;

    static const struct{
        cxdref_singlecable_num_of_sat_pos_t numberOfSatPos;
        cxdref_singlecable_rf_band_t rfBand;
        cxdref_singlecable_num_of_ub_slots_t numberOfUBSlots;
        uint8_t Config_Nb;
    } ODU_Config_table[] = {
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1, CXDREF_SINGLECABLE_RF_BAND_STANDARD,  CXDREF_SINGLECABLE_NUM_OF_UB_2, 0x10},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1, CXDREF_SINGLECABLE_RF_BAND_STANDARD,  CXDREF_SINGLECABLE_NUM_OF_UB_4, 0x11},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1, CXDREF_SINGLECABLE_RF_BAND_STANDARD,  CXDREF_SINGLECABLE_NUM_OF_UB_6, 0x12},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1, CXDREF_SINGLECABLE_RF_BAND_STANDARD,  CXDREF_SINGLECABLE_NUM_OF_UB_8, 0x13},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1, CXDREF_SINGLECABLE_RF_BAND_WIDE,      CXDREF_SINGLECABLE_NUM_OF_UB_2, 0x14},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1, CXDREF_SINGLECABLE_RF_BAND_WIDE,      CXDREF_SINGLECABLE_NUM_OF_UB_4, 0x15},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1, CXDREF_SINGLECABLE_RF_BAND_WIDE,      CXDREF_SINGLECABLE_NUM_OF_UB_6, 0x16},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1, CXDREF_SINGLECABLE_RF_BAND_WIDE,      CXDREF_SINGLECABLE_NUM_OF_UB_8, 0x17},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2, CXDREF_SINGLECABLE_RF_BAND_STANDARD,  CXDREF_SINGLECABLE_NUM_OF_UB_2, 0x18},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2, CXDREF_SINGLECABLE_RF_BAND_STANDARD,  CXDREF_SINGLECABLE_NUM_OF_UB_4, 0x19},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2, CXDREF_SINGLECABLE_RF_BAND_STANDARD,  CXDREF_SINGLECABLE_NUM_OF_UB_6, 0x1A},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2, CXDREF_SINGLECABLE_RF_BAND_STANDARD,  CXDREF_SINGLECABLE_NUM_OF_UB_8, 0x1B},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2, CXDREF_SINGLECABLE_RF_BAND_WIDE,      CXDREF_SINGLECABLE_NUM_OF_UB_2, 0x1C},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2, CXDREF_SINGLECABLE_RF_BAND_WIDE,      CXDREF_SINGLECABLE_NUM_OF_UB_4, 0x1D},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2, CXDREF_SINGLECABLE_RF_BAND_WIDE,      CXDREF_SINGLECABLE_NUM_OF_UB_6, 0x1E},
        {CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2, CXDREF_SINGLECABLE_RF_BAND_WIDE,      CXDREF_SINGLECABLE_NUM_OF_UB_8, 0x1F}
    };

    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_Config");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId <= 0) || (9 <= userbandId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0xE0;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x5B;
    pMessage->data[3] = (uint8_t)(((((uint32_t)(userbandId - 1)) & 0x07) << 5) | (uint32_t)0x01);

    for(index = 0; index < (int32_t)(sizeof(ODU_Config_table)/sizeof(ODU_Config_table[0])); index++)
    {
        if((numberOfSatPos == ODU_Config_table[index].numberOfSatPos)         &&
           (rfBand == ODU_Config_table[index].rfBand) &&
           (numberOfUBSlots == ODU_Config_table[index].numberOfUBSlots))
        {
            pMessage->data[4] = ODU_Config_table[index].Config_Nb;
            pMessage->length = 5;
            CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
        }
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
}

cxdref_result_t cxdref_singlecable_command_ODU_LoFreq (cxdref_diseqc_message_t * pMessage,
                                                   cxdref_singlecable_address_t address,
                                                   uint8_t userbandId,
                                                   cxdref_singlecable_lofreq_t loFreq)
{
    int32_t index = 0;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_LoFreq");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId <= 0) || (9 <= userbandId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    for(index = 0; index < CXDREF_DISEQC_MESSAGE_LENGTH; index++){
        pMessage->data[index] = 0;
    }

    pMessage->data[0] = 0xE0;
    pMessage->data[1] = (uint8_t)address;
    pMessage->data[2] = 0x5B;
    pMessage->data[3] = (uint8_t)(((((uint32_t)(userbandId - 1)) & 0x07) << 5) | 0x02);
    pMessage->data[4] = (uint8_t)loFreq;
    pMessage->length = 5;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_singlecable_command_ODU_Channel_change_MDU (cxdref_diseqc_message_t * pMessage,
                                                               cxdref_singlecable_address_t address,
                                                               uint8_t userbandId,
                                                               uint32_t ubSlotFreqKHz,
                                                               cxdref_singlecable_bank_t bank,
                                                               uint32_t centerFreqKHz,
                                                               uint8_t pin)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_Channel_change_MDU");

    if (!pMessage) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId <= 0) || (9 <= userbandId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    result = cxdref_singlecable_command_ODU_Channel_change (pMessage, address, userbandId, ubSlotFreqKHz, bank, centerFreqKHz);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    pMessage->data[2] = 0x5C;

    result = addPINCode (pMessage, pin);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_singlecable_command_ODU_PowerOFF_MDU (cxdref_diseqc_message_t * pMessage,
                                                         cxdref_singlecable_address_t address,
                                                         uint8_t userbandId,
                                                         uint8_t pin)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_PowerOFF_MDU");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId <= 0) || (9 <= userbandId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    result = cxdref_singlecable_command_ODU_PowerOFF (pMessage, address, userbandId);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    pMessage->data[2] = 0x5C;

    result = addPINCode (pMessage, pin);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_singlecable_command_ODU_UbxSignal_ON_MDU (cxdref_diseqc_message_t * pMessage,
                                                             cxdref_singlecable_address_t address,
                                                             uint8_t pin)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_UbxSignal_ON_MDU");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_singlecable_command_ODU_UbxSignal_ON (pMessage, address);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    pMessage->data[2] = 0x5D;

    result = addPINCode (pMessage, pin);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_singlecable_command_ODU_Config_MDU (cxdref_diseqc_message_t * pMessage,
                                                       cxdref_singlecable_address_t address,
                                                       uint8_t userbandId,
                                                       cxdref_singlecable_num_of_sat_pos_t numberOfSatPos,
                                                       cxdref_singlecable_rf_band_t rfBand,
                                                       cxdref_singlecable_num_of_ub_slots_t numberOfUBSlots,
                                                       uint8_t pin)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_Config_MDU");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId <= 0) || (9 <= userbandId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    result = cxdref_singlecable_command_ODU_Config (pMessage, address, userbandId, numberOfSatPos, rfBand, numberOfUBSlots);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    pMessage->data[2] = 0x5D;

    result = addPINCode (pMessage, pin);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_singlecable_command_ODU_LoFreq_MDU (cxdref_diseqc_message_t * pMessage,
                                                       cxdref_singlecable_address_t address,
                                                       uint8_t userbandId,
                                                       cxdref_singlecable_lofreq_t loFreq,
                                                       uint8_t pin)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_singlecable_command_ODU_LoFreq_MDU");

    if (!pMessage){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((userbandId <= 0) || (9 <= userbandId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }

    result = cxdref_singlecable_command_ODU_LoFreq (pMessage, address, userbandId, loFreq);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    pMessage->data[2] = 0x5D;

    result = addPINCode (pMessage, pin);
    if (result != CXDREF_RESULT_OK) {CXDREF_TRACE_RETURN (result);}

    CXDREF_TRACE_RETURN (result);
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
