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

#include "cxdref_stdlib.h"
#include "cxdref_demod_isdbc_chbond.h"
#include "cxdref_demod_isdbc_monitor.h"

cxdref_result_t cxdref_demod_isdbc_chbond_Enable (cxdref_demod_t * pDemod,
                                              uint8_t enable)
{
    uint8_t enableStreamIn = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_Enable");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (enable) {
        enableStreamIn = pDemod->chbondStreamIn;
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xC1, enableStreamIn) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (enable) {
        uint8_t data[2] = {0x01, 0x09};
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x17, data, 2) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB1, enable) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE8, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_SetTSMFConfig (cxdref_demod_t * pDemod,
                                                     cxdref_isdbc_tsid_type_t tsidType,
                                                     uint16_t tsid,
                                                     uint16_t networkId)
{
    uint8_t data[4];

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_SetTSMFConfig");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((tsidType == CXDREF_ISDBC_TSID_TYPE_RELATIVE_TS_NUMBER) && (tsid > 0x0F || tsid == 0)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (tsidType == CXDREF_ISDBC_TSID_TYPE_TSID) {
        data[0] = (uint8_t)((tsid >> 8) & 0xFF);
        data[1] = (uint8_t)(tsid & 0xFF);
        data[2] = (uint8_t)((networkId >> 8) & 0xFF);
        data[3] = (uint8_t)(networkId & 0xFF);
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 4) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        data[0] = data[1] = data[2] = data[3] = 0x00;
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3F, data[0]) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x56, &data[1], 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        data[0] = data[1] = data[2] = data[3] = 0x10 | (uint8_t)(tsid & 0x0F);
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3F, data[0]) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x56, &data[1], 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_SoftReset (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_SoftReset");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->state != CXDREF_DEMOD_STATE_SLEEP)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFA, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_BondStat (cxdref_demod_t * pDemod,
                                                        cxdref_demod_isdbc_chbond_state_t * pState)
{
    uint8_t data;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_monitor_BondStat");

    if ((!pDemod) || (!pState)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) || (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x20, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    pState->bondOK = (data >> 6) & 0x01;
    pState->groupIDCheck = (data >> 4) & 0x01;
    pState->streamTypeCheck = (data >> 3) & 0x01;
    pState->numCarriersCheck = (data >> 1) & 0x01;
    pState->carrierSeqCheck = data & 0x01;
    pState->calcNumCarriersCheck = (data >> 2) & 0x01;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_BondStat_hold (cxdref_demod_t * pDemod,
                                                             cxdref_demod_isdbc_chbond_state_t * pState)
{
    uint8_t data;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_monitor_BondStat_hold");

    if ((!pDemod) || (!pState)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig) != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x22, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    data ^= 0x7F;
    pState->bondOK = ((data >> 6) & 0x01);
    pState->groupIDCheck = ((data >> 4) & 0x01);
    pState->streamTypeCheck = ((data >> 3) & 0x01);
    pState->numCarriersCheck = ((data >> 1) & 0x01);
    pState->carrierSeqCheck = (data & 0x01);
    pState->calcNumCarriersCheck = ((data >> 2) & 0x01);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_ClearBondStat (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_ClearBondStat");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1B, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_TLVConvError (cxdref_demod_t * pDemod,
                                                            uint8_t * pError)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data;
    cxdref_demod_isdbc_chbond_state_t chbondState;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_monitor_TLVConvError");

    if ((!pDemod) || (!pError)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig) != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_isdbc_chbond_monitor_BondStat (pDemod, &chbondState);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (!chbondState.bondOK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x16, &data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data & 0x01)) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x24, &data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pError = (data >> 4) & 0x01;
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_ClearTLVConvError (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_ClearTLVConvError");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x19, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_CarrierDelay (cxdref_demod_t * pDemod,
                                                            uint32_t * pDelay)
{
    uint8_t data[3];

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_monitor_CarrierDelay");

    if ((!pDemod) || (!pDelay)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig) != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1D, data, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data[0] & 0x80)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *pDelay = ((data[0] & 0x1F) << 16) | (data[1] << 8) | data[2];
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                         uint8_t * pEWSChange)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data;
    cxdref_demod_isdbc_chbond_state_t chbondState;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_monitor_EWSChange");

    if ((!pDemod) || (!pEWSChange)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig) != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_isdbc_chbond_monitor_BondStat (pDemod, &chbondState);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (!chbondState.bondOK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x52, &data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pEWSChange = (data >> 4) & 0x0F;
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_ClearEWSChange (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_ClearEWSChange");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x53, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_VersionChange (cxdref_demod_t * pDemod,
                                                             uint8_t * pVersionChange)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data;
    cxdref_demod_isdbc_chbond_state_t chbondState;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_monitor_VersionChange");

    if ((!pDemod) || (!pVersionChange)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig) != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_isdbc_chbond_monitor_BondStat (pDemod, &chbondState);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (!chbondState.bondOK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x52, &data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pVersionChange = data & 0x0F;
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_ClearVersionChange (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_ClearVersionChange");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x54, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}


cxdref_result_t cxdref_demod_isdbc_chbond_monitor_StreamType (cxdref_demod_t * pDemod,
                                                          cxdref_isdbc_chbond_stream_type_t * pStreamType)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_isdbc_chbond_state_t chbondState;
    uint8_t data;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_monitor_StreamType");

    if ((!pDemod) || (!pStreamType)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig) != CXDREF_DEMOD_CHBOND_CONFIG_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_isdbc_chbond_monitor_BondStat (pDemod, &chbondState);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (!chbondState.bondOK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x25, &data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    if (data & 0x40) {
        *pStreamType = CXDREF_ISDBC_CHBOND_STREAM_TYPE_TLV;
    } else {
        *pStreamType = CXDREF_ISDBC_CHBOND_STREAM_TYPE_TS_NONE;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_TSMFHeaderExt (cxdref_demod_t * pDemod,
                                                             cxdref_isdbc_tsmf_header_t * pTSMFHeader,
                                                             cxdref_isdbc_chbond_tsmf_header_ext_t * pTSMFHeaderExt)
{
    int i;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_chbond_monitor_TSMFHeaderExt");

    if ((!pDemod) || (!pTSMFHeader) || (!pTSMFHeaderExt)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    cxdref_memcpy(&pTSMFHeaderExt->aceewData[0], &pTSMFHeader->privateData[0], 26);
    for (i = 0; i < 8; i++) {
        pTSMFHeaderExt->streamType[i] = (cxdref_isdbc_chbond_stream_type_t)((pTSMFHeader->privateData[26] >> (7 - i)) & 0x01);
    }
    for (i = 0; i < 7; i++) {
        pTSMFHeaderExt->streamType[i + 8] = (cxdref_isdbc_chbond_stream_type_t)((pTSMFHeader->privateData[27] >> (7 - i)) & 0x01);
    }
    pTSMFHeaderExt->groupID = pTSMFHeader->privateData[28];
    pTSMFHeaderExt->numCarriers = pTSMFHeader->privateData[29];
    pTSMFHeaderExt->carrierSequence = pTSMFHeader->privateData[30];
    pTSMFHeaderExt->numFrames = (pTSMFHeader->privateData[31] >> 4) & 0x0F;
    pTSMFHeaderExt->framePosition = pTSMFHeader->privateData[31] & 0x0F;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
