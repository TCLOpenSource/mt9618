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

#include "cxdref_demod_atsc3_chbond.h"
#include "cxdref_demod_atsc3_monitor.h"

cxdref_result_t cxdref_demod_atsc3_chbond_Enable (cxdref_demod_t * pDemod,
                                              uint8_t enable)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_chbond_Enable");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN)
        && (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xC1, enable ? pDemod->chbondStreamIn : 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (enable) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB ? 0x00 : 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x01};
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB2, data, 5) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF4, enable ? 0x03 : 0x23) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_chbond_SetPLPConfig (cxdref_demod_t * pDemod,
                                                    uint8_t plpIDNum,
                                                    uint8_t plpID[4],
                                                    cxdref_demod_atsc3_chbond_plp_bond_t plpBond[4])
{
    int i = 0;
    uint8_t data[4];

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_chbond_SetPLPConfig");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN) && (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((plpIDNum == 0) || (plpIDNum > 4)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for (i = 0; i < plpIDNum; i++) {
        if (plpID[i] > 0x3F) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }
    }

    if (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN) {
        if ((plpBond[0] != CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE) &&
            (plpBond[0] != CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_MAIN)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }

        for (i = 1; i < plpIDNum; i++) {
            switch (plpBond[i]) {
            case CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE:
            case CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_MAIN:
                if (plpBond[i - 1] == CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_SUB) {
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
                }
                break;
            case CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_SUB:
                break;
            default:
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
            }
        }
    } else {
        for (i = 0; i < plpIDNum; i++) {
            switch (plpBond[i]) {
            case CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE:
            case CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_SUB:
                break;
            case CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_MAIN:
            default:
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
            }
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (i = 0; i < 4; i++) {
        if (i < plpIDNum) {
            if (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN) {
                data[i] = plpID[i] | (plpBond[i] == CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_SUB ? 0x40 : 0xC0);
            } else {
                data[i] = plpID[i] | 0x80;
            }
        } else {
            data[i] = 0;
        }
    }

    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, data, sizeof (data)) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x85, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (i = 0; i < 4; i++) {
        if ((i < plpIDNum) && (plpBond[i] == CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE)) {
            data[i] = 0x01;
        } else {
            data[i] = 0x00;
        }
    }

    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB2, data, sizeof (data)) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9C, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_chbond_monitor_BondStat (cxdref_demod_t * pDemod,
                                                        uint8_t bondLockStat[4],
                                                        uint8_t * pBondLockAll,
                                                        uint8_t * pBondUnlockDetected)
{
    uint8_t data;
    int i;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_chbond_monitor_BondStat");

    if ((!pDemod) || (!bondLockStat) || (!pBondLockAll) || (!pBondUnlockDetected)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) || (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    *pBondUnlockDetected = 0;

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (data & 0x01) {
        *pBondUnlockDetected = 1;
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x13, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (data & 0x10) {
        *pBondUnlockDetected = 1;
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB0, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (i = 0; i < 4; i++) {
        bondLockStat[i] = (data >> i) & 0x01;
    }
    *pBondLockAll = data >> 4 & 0x01;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_chbond_monitor_SelectedPLPValid (cxdref_demod_t * pDemod,
                                                                cxdref_demod_atsc3_chbond_plp_valid_t plpValid[4])
{
    cxdref_result_t result;
    int i, j;
    uint8_t plpIdValid[4];
    uint8_t chBondValid[4];
    uint8_t chBondOn[4];
    uint8_t selectedPlpId[4];
    uint8_t data[5];

    const int plpConfigCheckTable[8][2] = {
        {CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_UNUSED,   CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_UNUSED},
        {                                      -1,                                         -1},
        {CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_FROM_SUB,                                       -1},
        {                                      -1,                                         -1},
        {                                      -1,   CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_OK},
        {                                      -1,   CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_OK},
        {CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_OK,                                             -1},
        {CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_OK,                                             -1}
    };

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_chbond_monitor_SelectedPLPValid");

    if ((!pDemod) || (!plpValid)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) ||
        ((pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN) &&
        (pDemod->chbondConfig != CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB))) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data[0] & 0x02)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9D, data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data[0] & 0x01)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, data, 5) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    for (i = 0; i < 4; i++) {
        plpIdValid[i] = (data[i] >> 7) & 0x01;
        chBondValid[i] = (data[i] >> 6) & 0x01;
        selectedPlpId[i] = data[i] & 0x3F;
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB2, chBondOn, 4) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (i = 0; i < 4; i++) {
        uint8_t index = (plpIdValid[i] << 2) + (chBondValid[i] << 1) + (chBondOn[i] & 0x01);
        uint8_t isSub = pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN ? 0 : 1;
        if (plpConfigCheckTable[index][isSub] == -1) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
        } else {
            plpValid[i] = (cxdref_demod_atsc3_chbond_plp_valid_t)plpConfigCheckTable[index][isSub];
        }
    }

    if (data[4] & 0x01) {
        for (i = 0; i < 4; i++) {
            if (plpValid[i] == CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_OK) {
                plpValid[i] = CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_ERR_ID;
            }
        }
    } else {
        cxdref_atsc3_plp_list_entry_t plpList[CXDREF_ATSC3_NUM_PLP_MAX];
        uint8_t numPLP;

        result = cxdref_demod_atsc3_monitor_PLPList (pDemod, plpList, &numPLP);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        for (i = 0; i < 4; i++) {
            if (plpValid[i] == CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_OK) {
                for (j = 0; j < numPLP; j++) {
                    if (selectedPlpId[i] == plpList[j].id) {
                        if ((chBondOn[i] & 0x01) != plpList[j].chbond) {
                            plpValid[i] = CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_ERR_BONDCONF;
                        }
                        break;
                    }
                }
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_chbond_monitor_L1Detail (cxdref_demod_t * pDemod,
                                                        cxdref_atsc3_chbond_l1detail_t * pL1Detail)
{
    uint8_t data[5];

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_chbond_monitor_L1Detail");

    if ((!pDemod) || (!pL1Detail)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data[0] & 0x02)) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9D, data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data[0] & 0x01)) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC0, data, 5) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    pL1Detail->num_rf = data[0] & 0x07;
    if (pL1Detail->num_rf == 0) {
        pL1Detail->bonded_bsid = 0;
        pL1Detail->bsid = 0;
    } else {
        pL1Detail->bonded_bsid = (data[1] << 8) | data[2];
        pL1Detail->bsid = (data[3] << 8) | data[4];
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
