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

#include "cxd_dmd_monitor_bit_error.h"
#include "cxdref_demod_isdbt_monitor.h"
#include "cxdref_demod_isdbs_monitor.h"
#include "cxdref_demod_isdbs3_monitor.h"
#include "cxdref_demod_dvbs_s2_monitor.h"
#include "cxdref_demod_atsc3_monitor.h"

static cxdref_result_t checkVQLock (cxdref_demod_t * pDemod);
static cxdref_result_t checkLayerExistence (cxdref_demod_t * pDemod, uint8_t * pLayerAExist,
                                         uint8_t * pLayerBExist, uint8_t * pLayerCExist);
static cxdref_result_t IsDmdLocked_isdbt(cxdref_demod_t* pDemod);
static cxdref_result_t IsDmdLocked_isdbs(cxdref_demod_t* pDemod);

cxdref_result_t cxdref_demod_atsc_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count, uint32_t *post_bit_count)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_PreRSBER");

    if ((!pDemod) || (!post_bit_error_count) || (!post_bit_count)) {
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

        *post_bit_error_count = bitError;
        *post_bit_count = blocks * bitPerBlock;

    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count, uint32_t *block_count)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_monitor_PostRSWER");

    if ((!pDemod) || (!block_error_count) || (!block_count)) {
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

        *block_error_count = wordError;
        *block_count = blocks;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}


cxdref_result_t cxdref_demod_atsc3_monitor_pre_bit_error (cxdref_demod_t * pDemod,
                uint32_t pre_bit_error_count[4], uint32_t pre_bit_count[4])
{
   CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_PreLDPCBER");

    if ((!pDemod) || (!pre_bit_error_count) || (!pre_bit_count)) {
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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x94) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[18];
        uint8_t valid[4] = {0};
        uint32_t bitError[4] = {0};
        uint32_t periodExp[4] = {0};
        uint32_t n_ldpc[4] = {0};
        cxdref_atsc3_plp_fec_t fecType[4];
        int i = 0;


        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x30, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        periodExp[0] = (data[0] >> 4) & 0x0F;
        periodExp[1] = data[0] & 0x0F;
        periodExp[2] = (data[1] >> 4) & 0x0F;
        periodExp[3] = data[1] & 0x0F;
        for (i = 0; i < 4; i++) {
            valid[i] = data[i * 4 + 2] & 0x10;
            bitError[i] = ((data[i * 4 + 2] & 0x0F) << 24) | (data[i * 4 + 3] << 16) | (data[i * 4 + 4] << 8) | data[i * 4 + 5];
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x90, data, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        fecType[0] = (cxdref_atsc3_plp_fec_t)((data[0] >> 4) & 0x0F);
        fecType[1] = (cxdref_atsc3_plp_fec_t)(data[0] & 0x0F);
        fecType[2] = (cxdref_atsc3_plp_fec_t)((data[1] >> 4) & 0x0F);
        fecType[3] = (cxdref_atsc3_plp_fec_t)(data[1] & 0x0F);

        SLVT_UnFreezeReg (pDemod);

        for (i = 0;i < 4;i++) {

            if (!valid[i]) {\

                pre_bit_error_count[i] = CXDREF_DEMOD_ATSC3_MONITOR_PRELDPCBER_INVALID;
                continue;
            }

            switch (fecType[i]) {
            case CXDREF_ATSC3_PLP_FEC_BCH_LDPC_16K:
            case CXDREF_ATSC3_PLP_FEC_CRC_LDPC_16K:
            case CXDREF_ATSC3_PLP_FEC_LDPC_16K:
                n_ldpc[i] = 16200;
                break;
            case CXDREF_ATSC3_PLP_FEC_BCH_LDPC_64K:
            case CXDREF_ATSC3_PLP_FEC_CRC_LDPC_64K:
            case CXDREF_ATSC3_PLP_FEC_LDPC_64K:
                n_ldpc[i] = 64800;
                break;
            default:
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }

            if (bitError[i] > ((1U << periodExp[i]) * n_ldpc[i])) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }

            pre_bit_error_count[i] = bitError[i];
            pre_bit_count[i] = (1U << periodExp[i]) * n_ldpc[i];
        }
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t post_bit_error_count[4], uint32_t post_bit_count[4])
{
    uint8_t valid[4] = {0};
    uint32_t bitError[4] = {0};
    uint32_t periodExp[4] = {0};
    uint32_t n_bch[4] = {0};
    int i = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_PreBCHBER");

    if ((!pDemod) || (!post_bit_error_count) ||(!post_bit_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[19];
        cxdref_atsc3_plp_fec_t plpFecType[4];
        cxdref_atsc3_plp_cod_t plpCr[4];
        static const uint16_t nBCHBitsLookup[2][12] = {
            {2160,  3240,  4320,  5400,  6480,  7560,  8640,  9720, 10800, 11880, 12960, 14040},
            {8640, 12960, 17280, 21600, 25920, 30240, 34560, 38880, 43200, 47520, 51840, 56160},
        };

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x94) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4A, data, sizeof(data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp[0] = (data[0] >> 4) & 0x0F;
        periodExp[1] = data[0] & 0x0F;
        periodExp[2] = (data[1] >> 4) & 0x0F;
        periodExp[3] = data[1] & 0x0F;

        for (i = 0; i < 4; i++) {
            valid[i] = data[i * 4 + 3] & 0x01;
            bitError[i] = ((data[i * 4 + 4] & 0x3F) << 16) | (data[i * 4 + 5] << 8) | data[i * 4 + 6];
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x90, data, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        plpFecType[0] = (cxdref_atsc3_plp_fec_t) ((data[0] >> 4) & 0x0F);
        plpFecType[1] = (cxdref_atsc3_plp_fec_t) (data[0] & 0x0F);
        plpFecType[2] = (cxdref_atsc3_plp_fec_t) ((data[1] >> 4) & 0x0F);
        plpFecType[3] = (cxdref_atsc3_plp_fec_t) (data[1] & 0x0F);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x94, data, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        plpCr[0] = (cxdref_atsc3_plp_cod_t) ((data[0] >> 4) & 0x0F);
        plpCr[1] = (cxdref_atsc3_plp_cod_t) (data[0] & 0x0F);
        plpCr[2] = (cxdref_atsc3_plp_cod_t) ((data[1] >> 4) & 0x0F);
        plpCr[3] = (cxdref_atsc3_plp_cod_t) (data[1] & 0x0F);

        SLVT_UnFreezeReg (pDemod);

        for (i = 0; i < 4; i++) {
            if (!valid[i]) {
                continue;
            }
            if ((plpFecType[i] > CXDREF_ATSC3_PLP_FEC_LDPC_64K) || (plpCr[i] > CXDREF_ATSC3_PLP_COD_13_15)) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
            if ((plpFecType[i] != CXDREF_ATSC3_PLP_FEC_BCH_LDPC_16K) && (plpFecType[i] != CXDREF_ATSC3_PLP_FEC_BCH_LDPC_64K)) {
                valid[i] = 0;
                continue;
            }

            n_bch[i] = nBCHBitsLookup[plpFecType[i]][plpCr[i]];
        }

    }

    {
        for (i = 0; i < 4; i++) {
            if (!valid[i]) {
                post_bit_error_count[i] = CXDREF_DEMOD_ATSC3_MONITOR_PREBCHBER_INVALID;
                continue;
            }

            if (bitError[i] > ((1U << periodExp[i]) * n_bch[i])) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }

            post_bit_error_count[i] = bitError[i];
            post_bit_count[i] = (1U << periodExp[i]) * n_bch[i];
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t block_error_count[4], uint32_t block_count[4])
{
    uint8_t valid[4] = {0};
    uint32_t fecError[4] = {0};
    uint32_t period[4]= {0};
    int i = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_PostBCHFER");

    if ((!pDemod) || (!block_error_count) ||(!block_count)) {
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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x94) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[8];


        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5D, data, sizeof(data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        for (i = 0; i < 4; i++) {
            fecError[i] = ((data[i * 2] & 0x7F) << 8) | (data[i * 2 + 1]);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4D, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        valid[0] = data[0] & 0x01;
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x51, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        valid[1] = data[0] & 0x01;
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x55, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        valid[2] = data[0] & 0x01;
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x59, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        valid[3] = data[0] & 0x01;

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4A, data, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        period[0] = 1 << ((data[0] >> 4) & 0x0F);
        period[1] = 1 << (data[0] & 0x0F);
        period[2] = 1 << ((data[1] >> 4) & 0x0F);
        period[3] = 1 << (data[1] & 0x0F);
    }

    {
        for (i = 0; i < 4; i++) {
            if (!valid[i]) {
                block_error_count[i] = CXDREF_DEMOD_ATSC3_MONITOR_POSTBCHFER_INVALID;
                continue;
            }

            if ((period[i] == 0) || (fecError[i] > period[i])) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }

            block_error_count[i] = fecError[i];
            block_count[i] = period[i];
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}


cxdref_result_t cxdref_demod_dvbc_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count, uint32_t *post_bit_count)
{
    uint8_t rdata[3];
    uint32_t bitError = 0;
    uint32_t periodExp = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_PreRSBER");

    if (!pDemod || !post_bit_error_count || !post_bit_count) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    rdata[0] = rdata[1] = rdata[2] = 0;

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x62, rdata, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x80) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    bitError = ((rdata[0] & 0x3F) << 16) | (rdata[1] << 8) | rdata[2];

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    periodExp = (rdata[0] & 0x1F);

    if ((periodExp <= 11) && (bitError > (1U << periodExp) * 204 * 8)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *post_bit_error_count = bitError;
    *post_bit_count = (1U << periodExp) * 204 * 8;
    
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbc_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count, uint32_t *block_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t packetError = 0;
    uint32_t periodExp = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbc_monitor_PER");

    if (!pDemod || !block_error_count || !block_count) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t rdata[4];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, rdata, sizeof (rdata)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((rdata[1] & 0x04) == 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        packetError = ((rdata[1] & 0x03) << 16) | (rdata[2] << 8) | rdata[3];
        periodExp = rdata[0] & 0x1F;
    }

    if (packetError > (1U << periodExp)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *block_error_count = packetError;
    *block_count = (1U << periodExp);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_j83b_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count, uint32_t *post_bit_count)
{
    uint8_t rdata[3];
    uint32_t bitError = 0;
    uint32_t periodExp = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_j83b_monitor_PreRSBER");

    if (!pDemod || !post_bit_error_count || !post_bit_count) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_J83B) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    rdata[0] = rdata[1] = rdata[2] = 0;

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x62, rdata, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x80) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    bitError = ((rdata[0] & 0x3F) << 16) | (rdata[1] << 8) | rdata[2];

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    periodExp = (rdata[0] & 0x1F);

    if ((periodExp <= 11) && (bitError > (1U << periodExp) * 188 * 8)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *post_bit_error_count = bitError;
    *post_bit_count = (1U << periodExp) * 188 * 8;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_j83b_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count, uint32_t *block_count)
{
    uint32_t packetError = 0;
    uint32_t periodExp = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_j83b_monitor_PER");

    if (!pDemod || !block_error_count || !block_count) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_J83B) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t rdata[4];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, rdata, sizeof (rdata)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((rdata[1] & 0x04) == 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        packetError = ((rdata[1] & 0x03) << 16) | (rdata[2] << 8) | rdata[3];
        periodExp = rdata[0] & 0x1F;
    }

    if (packetError > (1U << periodExp)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *block_error_count = packetError;
    *block_count = (1U << periodExp);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_pre_bit_error (cxdref_demod_t * pDemod,
                uint32_t *pre_bit_error_count, uint32_t *pre_bit_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint32_t bitError = 0;
    uint32_t period = 0;
    uint8_t data[5];
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs2_monitor_PreLDPCBER");

    if ((!pDemod) || (!pre_bit_error_count) || (!pre_bit_count)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS2:
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xB2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x30, data, 5) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        bitError = ((uint32_t)(data[1] & 0x0F) << 24) |
                   ((uint32_t)(data[2] & 0xFF) << 16) |
                   ((uint32_t)(data[3] & 0xFF) <<  8) |
                    (uint32_t)(data[4] & 0xFF);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7A, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = (uint32_t)(1 << (data[0] & 0x0F));

        if (bitError > (period * 64800)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        *pre_bit_error_count = bitError;
        *pre_bit_count = (period * 64800);
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count, uint32_t *post_bit_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t bitError = 0;
    uint32_t period = 0;
    uint8_t data[4];
    cxdref_dvbs2_plscode_t plscode;
    uint32_t tempA = 0;
    uint32_t tempB = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs2_monitor_PreBCHBER");

    if ((!pDemod) || (!post_bit_error_count) || (!post_bit_count)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2F, data, 4) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        bitError = ((uint32_t)(data[1] & 0x7F) << 16) |
                   ((uint32_t)(data[2] & 0xFF) <<  8) |
                    (uint32_t)(data[3] & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBC, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = (uint32_t)(1 << (data[0] & 0x0F));

        result = cxdref_demod_dvbs2_monitor_PLSCode (pDemod, &plscode);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        switch(plscode.codeRate)
        {
        case CXDREF_DVBS2_CODERATE_1_4:  tempA = 1; tempB =  4; break;
        case CXDREF_DVBS2_CODERATE_1_3:  tempA = 1; tempB =  3; break;
        case CXDREF_DVBS2_CODERATE_2_5:  tempA = 2; tempB =  5; break;
        case CXDREF_DVBS2_CODERATE_1_2:  tempA = 1; tempB =  2; break;
        case CXDREF_DVBS2_CODERATE_3_5:  tempA = 3; tempB =  5; break;
        case CXDREF_DVBS2_CODERATE_2_3:  tempA = 2; tempB =  3; break;
        case CXDREF_DVBS2_CODERATE_3_4:  tempA = 3; tempB =  4; break;
        case CXDREF_DVBS2_CODERATE_4_5:  tempA = 4; tempB =  5; break;
        case CXDREF_DVBS2_CODERATE_5_6:  tempA = 5; tempB =  6; break;
        case CXDREF_DVBS2_CODERATE_8_9:  tempA = 8; tempB =  9; break;
        case CXDREF_DVBS2_CODERATE_9_10: tempA = 9; tempB = 10; break;
        default: CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if((bitError * tempB) > (period * 64800 * tempA)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        *post_bit_error_count = bitError;
        *post_bit_count = (period * 64800 * tempA/tempB);

    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs2_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count, uint32_t *block_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_PER");

    if ((!pDemod) || (!block_error_count) || (!block_count)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
        if ((dtvSystem != CXDREF_DTV_SYSTEM_DVBS) && (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2)){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
    case CXDREF_DTV_SYSTEM_DVBS2:
        dtvSystem = pDemod->system;
        break;

    default:
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (dtvSystem == CXDREF_DTV_SYSTEM_DVBS2) {
        uint8_t data[3];
        uint32_t packetError = 0;
        uint32_t packetCount = 0;
        uint32_t tempDiv = 0;
        CXDREF_TRACE_ENTER ("s2_monitor_PER");

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xB8) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x26, data, 3) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (data[0] & 0x01){
            packetError = ((uint32_t)(data[1] & 0xFF) << 8) |
                           (uint32_t)(data[2] & 0xFF);

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
            if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBA, data, 1) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            packetCount = (uint32_t)(1 << (data[0] & 0x0F));

            tempDiv = packetCount;
            if ((tempDiv == 0) || (packetError > packetCount)){
                CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
            }
                *block_error_count = packetError; 
                *block_count = packetCount;

        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_monitor_pre_bit_error (cxdref_demod_t * pDemod,
                uint32_t *pre_bit_error_count, uint32_t *pre_bit_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint8_t data[11];
    uint32_t bitError = 0;
    uint32_t bitCount = 0;
    uint32_t tempDiv = 0;    
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_monitor_PreViterbiBER");

    if ((!pDemod) || (!pre_bit_error_count) || (!pre_bit_count)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x35, data, 11) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        bitError = ((uint32_t)(data[1]  & 0x3F) << 16) |
                   ((uint32_t)(data[2]  & 0xFF) <<  8) |
                    (uint32_t)(data[3]  & 0xFF);
        bitCount = ((uint32_t)(data[8]  & 0x3F) << 16) |
                   ((uint32_t)(data[9]  & 0xFF) <<  8) |
                    (uint32_t)(data[10] & 0xFF);
        tempDiv = bitCount;        
        if ((tempDiv == 0) || (bitError > bitCount)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }
        *pre_bit_error_count = bitError;
        *pre_bit_count = bitCount;
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count, uint32_t *post_bit_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    uint8_t data[4];
    uint32_t bitError = 0;
    uint32_t period = 0;
    uint32_t bitCount = 0;
    uint32_t tempDiv = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_monitor_PreRSBER");

    if ((!pDemod) || (!post_bit_error_count) || (!post_bit_count)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if (dtvSystem != CXDREF_DTV_SYSTEM_DVBS){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x25, data, 4) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (data[0] & 0x01){
        bitError = ((uint32_t)(data[1]  & 0xFF) << 16) |
                   ((uint32_t)(data[2]  & 0xFF) <<  8) |
                    (uint32_t)(data[3]  & 0xFF);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBA, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = (uint32_t)(1 << (data[0] & 0x0F));
        bitCount = period * 102;
        tempDiv = bitCount;
        if (tempDiv == 0){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }

        *post_bit_error_count = ((bitError * 2)/10);
        *post_bit_count =  period * 204 * 8 * 8;
        
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count, uint32_t *block_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_UNKNOWN;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_monitor_PER");

    if ((!pDemod) || (!block_error_count) || (!block_count)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    SLVT_FreezeReg (pDemod);

    switch(pDemod->system)
    {
    case CXDREF_DTV_SYSTEM_ANY:
        result = cxdref_demod_dvbs_s2_monitor_System (pDemod, &dtvSystem);
        if (result != CXDREF_RESULT_OK){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }
        if ((dtvSystem != CXDREF_DTV_SYSTEM_DVBS) && (dtvSystem != CXDREF_DTV_SYSTEM_DVBS2)){
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
        break;

    case CXDREF_DTV_SYSTEM_DVBS:
    case CXDREF_DTV_SYSTEM_DVBS2:
        dtvSystem = pDemod->system;
        break;

    default:
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (dtvSystem == CXDREF_DTV_SYSTEM_DVBS) {
        uint8_t data[3];
        uint32_t packetError = 0;
        uint32_t period = 0;
        uint32_t tempDiv = 0;
        CXDREF_TRACE_ENTER ("s_monitor_PER");

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x25, data, 1) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (data[0] & 0x01){
            if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2C, data, 3) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
            packetError = ((uint32_t)(data[0]  & 0x03) << 16) |
                          ((uint32_t)(data[1]  & 0xFF) <<  8) |
                           (uint32_t)(data[2]  & 0xFF);

            if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBA, data, 1) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            period = (uint32_t)(1 << (data[0] & 0x0F));
            tempDiv = period;
            if ((tempDiv == 0) || (packetError > (period * 8))){
                CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
            }
            *block_error_count = packetError;
            *block_count = (period * 8);
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    SLVT_UnFreezeReg (pDemod);
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbt_monitor_pre_bit_error (cxdref_demod_t * pDemod,
                uint32_t *pre_bit_error_count, uint32_t *pre_bit_count)
{
    uint8_t rdata[2];
    uint32_t bitError = 0;
    uint32_t period = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_PreViterbiBER");

    if ((!pDemod) || (!pre_bit_error_count) || (!pre_bit_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x39, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x01) == 0) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x22, rdata, 2) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    bitError = (rdata[0] << 8) | rdata[1];

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6F, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    period = ((rdata[0] & 0x07) == 0) ? 256 : (0x1000 << (rdata[0] & 0x07));

    if ((period == 0) || (bitError > period)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }
    *pre_bit_error_count = bitError;
    *pre_bit_count = period;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count, uint32_t *post_bit_count)
{
    uint8_t rdata[3];
    uint32_t bitError = 0;
    uint32_t periodExp = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_PreRSBER");

    if ((!pDemod) || (!post_bit_error_count) || (!post_bit_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x62, rdata, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x80) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    bitError = ((rdata[0] & 0x3F) << 16) | (rdata[1] << 8) | rdata[2];

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    periodExp = (rdata[0] & 0x1F);

    if ((periodExp <= 11) && (bitError > (1U << periodExp) * 204 * 8)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *post_bit_error_count = ((bitError * 2)/10);
    *post_bit_count = (1U << periodExp) * 204 * 8;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count, uint32_t *block_count)
{
    uint32_t packetError = 0;
    uint32_t period = 0;
    uint8_t rdata[4];

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt_monitor_PER");

    if ((!pDemod) || (!block_error_count) || (!block_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, rdata, 4) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[1] & 0x01) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    packetError = (rdata[2] << 8) | rdata[3];
    period = 1U << (rdata[0] & 0x0F);

    if ((period == 0) || (packetError > period)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *block_error_count = packetError;
    *block_count = period;
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_pre_bit_error (cxdref_demod_t * pDemod,
                uint32_t *pre_bit_error_count, uint32_t *pre_bit_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t bitError = 0;
    uint32_t periodExp = 0;
    uint32_t n_ldpc = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_PreLDPCBER");

    if ((!pDemod) || (!pre_bit_error_count) || (!pre_bit_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[4];
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x39, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(data[0] & 0x10)) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        bitError = ((data[0] & 0x0F) << 24) | (data[1] << 16) | (data[2] << 8) | data[3];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6F, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp = data[0] & 0x0F;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5E, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        if (((cxdref_dvbt2_plp_fec_t) (data[0] & 0x03)) == CXDREF_DVBT2_FEC_LDPC_16K) {
            n_ldpc = 16200;
        }
        else {
            n_ldpc = 64800;
        }
    }

    	if (bitError > ((1U << periodExp) * n_ldpc)) {
        	CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    	}

        *pre_bit_error_count = bitError;
        *pre_bit_count = (1U << periodExp) * n_ldpc;

	CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbt2_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count, uint32_t *post_bit_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t bitError = 0;
    uint32_t periodExp = 0;
    uint32_t n_bch = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_PreBCHBER");

    if ((!pDemod) || (!post_bit_error_count) || (!post_bit_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[4];
        cxdref_dvbt2_plp_fec_t plpFecType = CXDREF_DVBT2_FEC_LDPC_16K;
        cxdref_dvbt2_plp_code_rate_t plpCr = CXDREF_DVBT2_R1_2;

        static const uint16_t nBCHBitsLookup[2][8] = {
            {7200,  9720,  10800, 11880, 12600, 13320, 5400,  6480},
            {32400, 38880, 43200, 48600, 51840, 54000, 21600, 25920}
        };

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x24) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x40, data, 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(data[0] & 0x01)) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        bitError = ((data[1] & 0x3F) << 16) | (data[2] << 8) | data[3];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x22) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5E, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        plpFecType = (cxdref_dvbt2_plp_fec_t) (data[0] & 0x03);

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5B, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        plpCr = (cxdref_dvbt2_plp_code_rate_t) (data[0] & 0x07);

        SLVT_UnFreezeReg (pDemod);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x20) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x72, data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        periodExp = data[0] & 0x0F;

        if ((plpFecType > CXDREF_DVBT2_FEC_LDPC_64K) || (plpCr > CXDREF_DVBT2_R2_5)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        n_bch = nBCHBitsLookup[plpFecType][plpCr];
    }

    if (bitError > ((1U << periodExp) * n_bch)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *post_bit_error_count = bitError;
    *post_bit_count = (1U << periodExp) * n_bch;

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_demod_dvbt2_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count, uint32_t *block_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t packetError = 0;
    uint32_t period = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbt2_monitor_PER");
    if ((!pDemod) || (!block_error_count) || (!block_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t rdata[3];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x24) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFA, rdata, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((rdata[0] & 0x01) == 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        packetError = (rdata[1] << 8) | rdata[2];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xDC, rdata, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        period = 1U << (rdata[0] & 0x0F);
    }

    if ((period == 0) || (packetError > period)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);

    }

    *block_error_count = packetError;
    *block_count = period;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbc_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count, uint32_t *post_bit_count)
{
    uint8_t rdata[3];
    uint32_t bitError = 0;
    uint32_t periodExp = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_monitor_PreRSBER");

    if (!pDemod || !post_bit_error_count || !post_bit_count) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    rdata[0] = rdata[1] = rdata[2] = 0;

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x62, rdata, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((rdata[0] & 0x80) == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    bitError = ((rdata[0] & 0x3F) << 16) | (rdata[1] << 8) | rdata[2];

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    periodExp = (rdata[0] & 0x1F);

    if ((periodExp <= 11) && (bitError > (1U << periodExp) * 204 * 8)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *post_bit_error_count = bitError;
    *post_bit_count = (1U << periodExp) * 204 * 8;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbc_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count, uint32_t *block_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t packetError = 0;
    uint32_t periodExp = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbc_monitor_PER");

    if (!pDemod || !block_error_count || !block_count) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t rdata[4];

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, rdata, sizeof (rdata)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if ((rdata[1] & 0x04) == 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        packetError = ((rdata[1] & 0x03) << 16) | (rdata[2] << 8) | rdata[3];
        periodExp = rdata[0] & 0x1F;
    }

    if (packetError > (1U << periodExp)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    *block_error_count = packetError;
    *block_count = (1U << periodExp);
    
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count_h, uint32_t *post_bit_count_h,
                uint32_t *post_bit_error_count_l, uint32_t *post_bit_count_l,
                uint32_t *post_bit_error_count_tmcc, uint32_t *post_bit_count_tmcc)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata[3];
    uint32_t bitError[3];
    uint32_t measuredSlots[3];
    uint8_t valid[3];
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_PreRSBER");

    if ((!pDemod) || (!post_bit_error_count_h) || (!post_bit_count_h) || 
            (!post_bit_error_count_l) || (!post_bit_count_l) || 
            (!post_bit_error_count_tmcc) || (!post_bit_count_tmcc)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    
    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked_isdbs (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x79, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    valid[0] = (rdata[0] & 0x10) ? 0 : 1;
    valid[1] = (rdata[0] & 0x08) ? 0 : 1;
    valid[2] = (rdata[0] & 0x01) ? 0 : 1;

    if (valid[0]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x50, rdata, 3) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[0] = ((rdata[0] & 0x7F) << 16) | (rdata[1] << 8) | rdata[2];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x58, rdata, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        measuredSlots[0] = (rdata[0] << 8) | rdata[1];
    }

    if (valid[1]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5A, rdata, 3) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[1] = ((rdata[0] & 0x7F) << 16) | (rdata[1] << 8) | rdata[2];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x62, rdata, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        measuredSlots[1] = (rdata[0] << 8) | rdata[1];
    }

    if (valid[2]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7B, rdata, 2) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        bitError[2] = ((rdata[0] & 0x3F) << 8) | rdata[1];

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7F, rdata, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        measuredSlots[2] = rdata[0];
    }

    SLVT_UnFreezeReg (pDemod);

    if (!valid[0] || (measuredSlots[0] == 0))
        *post_bit_error_count_h = CXDREF_DEMOD_ISDBS_MONITOR_PRERSBER_INVALID;
    else 
        *post_bit_error_count_h = bitError[0];

    if (!valid[1] || (measuredSlots[1] == 0))
        *post_bit_error_count_l = CXDREF_DEMOD_ISDBS_MONITOR_PRERSBER_INVALID;
    else
        *post_bit_error_count_l = bitError[1];

    *post_bit_count_h = measuredSlots[0] * 8 * 204;
    *post_bit_count_l = measuredSlots[1] * 8 * 204;

    if (!valid[2] || (measuredSlots[1] == 0))
        *post_bit_error_count_tmcc = CXDREF_DEMOD_ISDBS_MONITOR_PRERSBER_INVALID;
    else 
        *post_bit_error_count_tmcc = bitError[2];

    *post_bit_count_tmcc = measuredSlots[2] * 8 * 64;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count_h, uint32_t *block_count_h,
                uint32_t *block_error_count_l, uint32_t *block_count_l)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata[4];
    uint32_t packetError[2];
    uint32_t measuredSlots[2];
    uint8_t valid[2];
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs_monitor_PER");

    if ((!pDemod) || (!block_error_count_h) || (!block_count_h) ||
            (!block_error_count_l) || (!block_count_l)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked_isdbs (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xC0) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x79, rdata, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    valid[0] = (rdata[0] & 0x10) ? 0 : 1;
    valid[1] = (rdata[0] & 0x08) ? 0 : 1;

    if (valid[0]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x56, rdata, 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        packetError[0] = (rdata[0] << 8) | rdata[1];
        measuredSlots[0] = (rdata[2] << 8) | rdata[3];
    }

    if (valid[1]) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, rdata, 4) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        packetError[1] = (rdata[0] << 8) | rdata[1];
        measuredSlots[1] = (rdata[2] << 8) | rdata[3];
    }

    SLVT_UnFreezeReg (pDemod);
    if (!valid[0] || (measuredSlots[0] == 0) || (measuredSlots[0] < packetError[0]))
        *block_error_count_h = CXDREF_DEMOD_ISDBS_MONITOR_PER_INVALID;
    else 
        *block_error_count_h = packetError[0];

    *block_count_h = measuredSlots[0];

    if (!valid[1] || (measuredSlots[1] == 0) || (measuredSlots[1] < packetError[1]))
        *block_error_count_l = CXDREF_DEMOD_ISDBS_MONITOR_PER_INVALID;
    else 
        *block_error_count_l = packetError[1];

    *block_count_l = measuredSlots[1];
    
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_pre_bit_error (cxdref_demod_t * pDemod,
                uint32_t *pre_bit_error_count_h, uint32_t *pre_bit_count_h,
                uint32_t *pre_bit_error_count_l, uint32_t *pre_bit_count_l)
{
    uint8_t rdata[4];
    uint8_t valid[2];
    uint32_t bitError[2];
    uint32_t measuredPeriod[2];

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_PreLDPCBER");

    if ((!pDemod) || (!pre_bit_error_count_h) || (!pre_bit_count_h) ||
        (!pre_bit_error_count_l) || (!pre_bit_count_l)) {
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

    if (!valid[0]) {
            *pre_bit_error_count_h = CXDREF_DEMOD_ISDBS3_MONITOR_PRELDPCBER_INVALID;
    } else {
        if (bitError[0] > (measuredPeriod[0] * 44880)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }
        *pre_bit_error_count_h = bitError[0];
        *pre_bit_count_h = measuredPeriod[0] * 44880;        
    }

    if (!valid[1]){
        *pre_bit_error_count_l = CXDREF_DEMOD_ISDBS3_MONITOR_PRELDPCBER_INVALID;
    } else {
        if (bitError[1] > (measuredPeriod[1] * 44880)){
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }
        *pre_bit_error_count_l = bitError[1];
        *pre_bit_count_l = measuredPeriod[1] * 44880;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count_h, uint32_t *post_bit_count_h,
                uint32_t *post_bit_error_count_l, uint32_t *post_bit_count_l)
{
    uint8_t rdata[2];
    uint8_t valid[2];
    uint32_t frameError[2];
    uint32_t frameCount[2];

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_PostBCHFER");

    if ((!pDemod) || (!post_bit_error_count_h)|| (!post_bit_count_h) ||
        (!post_bit_error_count_l) || (!post_bit_count_l)) {
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

    if (!valid[0]){
        *post_bit_error_count_h = CXDREF_DEMOD_ISDBS3_MONITOR_PRELDPCBER_INVALID;
    } else {
        if (frameError[0] > frameCount[0])
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    
            *post_bit_error_count_h = frameError[0];
            *post_bit_count_h = frameCount[0];
    }

    if (!valid[1]){
        *post_bit_error_count_l = CXDREF_DEMOD_ISDBS3_MONITOR_PRELDPCBER_INVALID;
    } else {
        if (frameError[1] > frameCount[1])
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
                
        *post_bit_error_count_l = frameError[1];
        *post_bit_count_l = frameCount[1];
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbs3_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count_h, uint32_t *block_count_h,
                uint32_t *block_error_count_l, uint32_t *block_count_l)
{
    uint8_t rdata[2];
    uint8_t valid[2];
    uint32_t frameError[2];
    uint32_t frameCount[2];

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_monitor_PostBCHFER");

    if ((!pDemod) || (!block_error_count_h)|| (!block_count_h) ||
         (!block_error_count_l) || (!block_count_l)) {
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

    if (!valid[0]){
        *block_error_count_h = CXDREF_DEMOD_ISDBS3_MONITOR_POSTBCHFER_INVALID;
    } else {
         if (frameError[0] > frameCount[0])
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);   

        *block_error_count_h = frameError[0];
        *block_count_h = frameCount[0];
    }

    if (!valid[1]) {
        *block_error_count_l = CXDREF_DEMOD_ISDBS3_MONITOR_POSTBCHFER_INVALID;
    } else {
        if (frameError[1] > frameCount[1])
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);

        *block_error_count_l = frameError[1];
        *block_count_l = frameCount[1];
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_isdbt_monitor_post_bit_error (cxdref_demod_t * pDemod,
                uint32_t *post_bit_error_count_a, uint32_t *post_bit_error_count_b, 
                uint32_t *post_bit_error_count_c, uint32_t *post_bit_count)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t dataPacketNum[2];
    uint8_t dataBitError[9];
    uint32_t bitError[3];
    uint32_t packetNum;
    uint8_t layerExist[3];
    int i = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_PreRSBER");

    if ((!pDemod) || (!post_bit_error_count_a) || (!post_bit_error_count_b) || 
        (!post_bit_error_count_c) || (!post_bit_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->isdbtEwsState != CXDREF_DEMOD_ISDBT_EWS_STATE_NORMAL) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked_isdbt (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = checkLayerExistence (pDemod, &layerExist[0], &layerExist[1], &layerExist[2]);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x16, dataBitError, 9) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg(pDemod);

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5B, dataPacketNum, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    packetNum = ((uint32_t)dataPacketNum[0] << 8) | dataPacketNum[1];
    if (packetNum == 0) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }

    for (i = 0; i < 3; i++) {
        if (!layerExist[i]) {
            continue;
        }

        bitError[i] = ((uint32_t)(dataBitError[0 + (3 * i)] & 0x7F) << 16) | ((uint32_t)dataBitError[1 + (3 * i)] << 8) | dataBitError[2 + (3 * i)];

        if (bitError[i] / 8 / 204 > packetNum) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    *post_bit_error_count_a = layerExist[0] ? bitError[0] : CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID;
    *post_bit_error_count_b = layerExist[1] ? bitError[1] : CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID;
    *post_bit_error_count_c = layerExist[2] ? bitError[2] : CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID;

    *post_bit_count = packetNum * 8 * 204;

    CXDREF_TRACE_RETURN(result);
}

cxdref_result_t cxdref_demod_isdbt_monitor_block_error (cxdref_demod_t * pDemod,
                uint32_t *block_error_count_a, uint32_t *block_error_count_b,
                uint32_t *block_error_count_c, uint32_t *block_count)

{
    uint8_t dataPacketError[6];
    uint8_t dataPacketNum[2];
    uint32_t packetError[3];
    uint32_t packetNum = 0;
    uint8_t layerExist[3];
    int i = 0;

    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbt_monitor_PER");

    if ((!pDemod) || (!block_error_count_a) || (!block_error_count_b) || 
            (!block_error_count_c) || (!block_count)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ISDBT) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->isdbtEwsState != CXDREF_DEMOD_ISDBT_EWS_STATE_NORMAL) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = IsDmdLocked_isdbt(pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    result = checkLayerExistence (pDemod, &layerExist[0], &layerExist[1], &layerExist[2]);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN(result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1F, dataPacketError, 6) != CXDREF_RESULT_OK){
        SLVT_UnFreezeReg(pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg(pDemod);

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5B, dataPacketNum, 2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    packetNum = ((uint32_t)dataPacketNum[0] << 8) + dataPacketNum[1];
    if (packetNum == 0) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }

    for (i = 0; i < 3; i++) {
        if (!layerExist[i]) {
            continue;
        }

        packetError[i] = ((uint32_t)dataPacketError[0 + (2 * i)] << 8) + dataPacketError[1 + (2 * i)];

        if (packetError[i] > packetNum) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
        }
    }

    *block_error_count_a = layerExist[0] ? packetError[0] : CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID;
    *block_error_count_b = layerExist[1] ? packetError[1] : CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID;
    *block_error_count_c = layerExist[2] ? packetError[2] : CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID;

    *block_count = packetNum;

    CXDREF_TRACE_RETURN(result);
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

static cxdref_result_t checkLayerExistence (cxdref_demod_t * pDemod,
                                          uint8_t * pLayerAExist,
                                          uint8_t * pLayerBExist,
                                          uint8_t * pLayerCExist)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data[4];
    cxdref_isdbt_modulation_t mod = CXDREF_ISDBT_MODULATION_UNUSED_7;

    CXDREF_TRACE_ENTER ("checkLayerExistence");

    if (!pDemod || !pLayerAExist || !pLayerBExist || !pLayerCExist) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x60);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, data, 4);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    mod = (cxdref_isdbt_modulation_t) cxdref_BitSplitFromByteArray (data, 8 - 8, 3);
    *pLayerAExist = (mod == CXDREF_ISDBT_MODULATION_UNUSED_7) ? 0 : 1;

    mod = (cxdref_isdbt_modulation_t) cxdref_BitSplitFromByteArray (data, 21 - 8, 3);
    *pLayerBExist = (mod == CXDREF_ISDBT_MODULATION_UNUSED_7) ? 0 : 1;

    mod = (cxdref_isdbt_modulation_t) cxdref_BitSplitFromByteArray (data, 34 - 8, 3);
    *pLayerCExist = (mod == CXDREF_ISDBT_MODULATION_UNUSED_7) ? 0 : 1;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t IsDmdLocked_isdbt(cxdref_demod_t* pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t dmdLock = 0;
    uint8_t tsLock  = 0;
    uint8_t unlock  = 0;
    CXDREF_TRACE_ENTER("IsDmdLocked");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_isdbt_monitor_SyncStat(pDemod, &dmdLock, &tsLock, &unlock);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    if (dmdLock) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
    } else {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }
}

static cxdref_result_t IsDmdLocked_isdbs(cxdref_demod_t* pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t agcLock = 0;
    uint8_t tsLock  = 0;
    uint8_t tmccLock  = 0;
    CXDREF_TRACE_ENTER("IsDmdLocked");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_isdbs_monitor_SyncStat(pDemod, &agcLock, &tsLock, &tmccLock);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(result);
    }

    if (tmccLock) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
    } else {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_HW_STATE);
    }
}
