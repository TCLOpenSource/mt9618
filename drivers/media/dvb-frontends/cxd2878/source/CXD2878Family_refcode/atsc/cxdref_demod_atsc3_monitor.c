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

#include "cxdref_demod_atsc3_monitor.h"
#include "cxdref_math.h"
#include "cxdref_stdlib.h"

#define CXDREF_ATSC3_WAIT_L1_DETAIL_TIMEOUT  100
#define CXDREF_ATSC3_WAIT_L1_DETAIL_TIMEOUT2 100
#define CXDREF_ATSC3_WAIT_L1_DETAIL_INTERVAL 10
#define L1RAM_READ_SIZE 240

static cxdref_result_t isDemodLocked (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc3_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                 uint8_t * pSyncStat,
                                                 uint8_t alpLockStat[4],
                                                 uint8_t * pALPLockAll,
                                                 uint8_t * pUnlockDetected)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_SyncStat");

    if ((!pDemod) || (!pSyncStat) || (!alpLockStat) || (!pALPLockAll) || (!pUnlockDetected)) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data;
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pSyncStat = data & 0x07;
        *pUnlockDetected = (data & 0x10) ? 1 : 0;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x95) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x40, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pALPLockAll = (data & 0x10) ? 1 : 0;
        alpLockStat[0] = (data & 0x01) ? 1 : 0;
        alpLockStat[1] = (data & 0x02) ? 1 : 0;
        alpLockStat[2] = (data & 0x04) ? 1 : 0;
        alpLockStat[3] = (data & 0x08) ? 1 : 0;
    }

    if (*pSyncStat == 0x07) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pOffset)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t sinv;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_CarrierOffset");

    if ((!pDemod) || (!pOffset)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[4];
        uint32_t ctlVal = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = isDemodLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4C, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF3, &sinv, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        ctlVal = ((data[0] & 0x0F) << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        *pOffset = cxdref_Convert2SComplement (ctlVal, 28);

        switch (pDemod->bandwidth) {
        case CXDREF_DTV_BW_8_MHZ:
            *pOffset = (*pOffset) / 117;
            break;
        case CXDREF_DTV_BW_7_MHZ:
            *pOffset = (*pOffset) / 133;
            break;
        case CXDREF_DTV_BW_6_MHZ:
            *pOffset = (*pOffset) / 155;
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }
    }

    {
        if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL) {
            if (!(sinv & 0x01)) {
                *pOffset *= -1;
            }
        } else {
            if (sinv & 0x01) {
                *pOffset *= -1;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                 uint32_t * pIFAGC)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_IFAGCOut");


    if ((!pDemod) || (!pIFAGC)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data[2];
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x26, data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pIFAGC = ((data[0] & 0x0F) << 8) | (data[1]);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_Bootstrap (cxdref_demod_t * pDemod,
                                                  cxdref_atsc3_bootstrap_t * pBootstrap)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_Bootstrap");

    if ((!pDemod) || (!pBootstrap)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[3];

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE6, data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (!(data[0] & 0x01)) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE0, data, 3) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        pBootstrap->bw_diff = data[0] & 0x01;
        pBootstrap->system_bw = (cxdref_atsc3_system_bw_t)(data[1] & 0x03);
        pBootstrap->ea_wake_up = data[2] & 0x03;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_OFDM (cxdref_demod_t * pDemod,
                                             cxdref_atsc3_ofdm_t * pOfdm)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_OFDM");

    if ((!pDemod) || (!pOfdm)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    {
        uint8_t data[9];

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = isDemodLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x57, data, sizeof(data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        switch ((data[0] >> 4) & 0x03) {
        case 0:
            pOfdm->system_bw = CXDREF_ATSC3_SYSTEM_BW_8_MHZ;
            break;
        case 1:
            pOfdm->system_bw = CXDREF_ATSC3_SYSTEM_BW_7_MHZ;
            break;
        case 2:
            pOfdm->system_bw = CXDREF_ATSC3_SYSTEM_BW_6_MHZ;
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        pOfdm->bs_num_symbol = data[0] & 0x0F;
        pOfdm->num_subframe = data[1];
        pOfdm->l1b_fec_type = (cxdref_atsc3_l1b_fec_type_t)((data[2] >> 5) & 0x07);
        pOfdm->papr = (cxdref_atsc3_papr_t)((data[2] >> 3) & 0x03);
        pOfdm->pb_num_symbol = data[2] & 0x07;

        switch ((data[3] >> 5) & 0x07) {
        case 1:
            pOfdm->pb_fft_size = CXDREF_ATSC3_FFT_SIZE_8K;
            break;
        case 4:
            pOfdm->pb_fft_size = CXDREF_ATSC3_FFT_SIZE_16K;
            break;
        case 5:
            pOfdm->pb_fft_size = CXDREF_ATSC3_FFT_SIZE_32K;
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        switch (data[3] & 0x1F) {
        case 0:
        case 1:
            pOfdm->pb_pilot = CXDREF_ATSC3_PREAMBLE_PILOT_3;
            break;
        case 2:
        case 3:
            pOfdm->pb_pilot = CXDREF_ATSC3_PREAMBLE_PILOT_4;
            break;
        case 4:
        case 5:
            pOfdm->pb_pilot = CXDREF_ATSC3_PREAMBLE_PILOT_6;
            break;
        case 6:
        case 7:
            pOfdm->pb_pilot = CXDREF_ATSC3_PREAMBLE_PILOT_8;
            break;
        case 8:
        case 9:
            pOfdm->pb_pilot = CXDREF_ATSC3_PREAMBLE_PILOT_12;
            break;
        case 10:
        case 11:
            pOfdm->pb_pilot = CXDREF_ATSC3_PREAMBLE_PILOT_16;
            break;
        case 12:
        case 13:
            pOfdm->pb_pilot = CXDREF_ATSC3_PREAMBLE_PILOT_24;
            break;
        case 14:
        case 15:
            pOfdm->pb_pilot = CXDREF_ATSC3_PREAMBLE_PILOT_32;
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        pOfdm->pb_reduced_carriers = (data[4] >> 4) & 0x07;
        pOfdm->pb_gi = (cxdref_atsc3_gi_t)(data[4] & 0x0F);

        switch ((data[5] >> 5) & 0x07) {
        case 1:
            pOfdm->sf0_fft_size = CXDREF_ATSC3_FFT_SIZE_8K;
            break;
        case 4:
            pOfdm->sf0_fft_size = CXDREF_ATSC3_FFT_SIZE_16K;
            break;
        case 5:
            pOfdm->sf0_fft_size = CXDREF_ATSC3_FFT_SIZE_32K;
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        pOfdm->sf0_sp = (cxdref_atsc3_sp_t)(data[5] & 0x1F);
        pOfdm->sf0_reduced_carriers = (data[6] >> 4) & 0x07;
        pOfdm->sf0_gi = (cxdref_atsc3_gi_t)(data[6] & 0x0F);
        pOfdm->sf0_sp_boost = (data[7] >> 5) & 0x07;
        pOfdm->sf0_sbs_first = (data[7] >> 4) & 0x01;
        pOfdm->sf0_sbs_last = (data[7] >> 3) & 0x01;
        pOfdm->sf0_num_ofdm_symbol = (data[7] & 0x07) << 8 | data[8];

    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_L1Basic (cxdref_demod_t * pDemod,
                                                cxdref_atsc3_l1basic_t * pL1Basic)
{
    uint8_t data[25];
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_L1Basic");

    if ((!pDemod) || (!pL1Basic)) {
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
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data[0] & 0x01)) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x60, data, sizeof(data)) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    pL1Basic->version = (data[0] >> 5) & 0x07;
    pL1Basic->mimo_sp_enc = (data[0] >> 4) & 0x01;
    pL1Basic->lls_flg = (data[0] >> 3) & 0x01;
    pL1Basic->time_info_flg = (cxdref_atsc3_time_info_flag_t)((data[0] >> 1) & 0x03);
    pL1Basic->return_ch_flg = data[0] & 0x01;
    pL1Basic->papr = (cxdref_atsc3_papr_t)((data[1] >> 1) & 0x03);
    pL1Basic->frame_length_mode = data[1] & 0x01;
    if (pL1Basic->frame_length_mode == 0) {
        pL1Basic->frame_length = ((data[2] << 2) & 0x3FC) | ((data[3] >> 6) & 0x03);
        pL1Basic->excess_smp_per_sym = ((data[3] << 7) & 0x1F80) | (data[4] & 0x7F);
    } else {
        pL1Basic->frame_length = 0;
        pL1Basic->excess_smp_per_sym = 0;
    }
    if (pL1Basic->frame_length_mode == 1) {
        pL1Basic->time_offset = (data[5] << 8) | data[6];
        pL1Basic->additional_smp = data[7] & 0x7F;
    } else {
        pL1Basic->time_offset = 0;
        pL1Basic->additional_smp = 0;
    }
    pL1Basic->num_subframe = data[8];
    pL1Basic->pb_num_symbol = (data[9] >> 5) & 0x07;
    pL1Basic->pb_reduced_carriers = (data[9] >> 2) & 0x07;
    pL1Basic->l1d_content_tag = data[9] & 0x03;
    pL1Basic->l1d_size = ((data[10] << 5) & 0x1FE0) | ((data[11] >> 3) & 0x1F);
    pL1Basic->l1d_fec_type = (cxdref_atsc3_l1d_fec_type_t)(data[11] & 0x07);
    pL1Basic->l1d_add_parity_mode = (data[12] >> 6) & 0x03;
    pL1Basic->l1d_total_cells = ((data[12] << 13) & 0x7E000) | (data[13] << 5) | ((data[14] >> 3) & 0x1F);
    pL1Basic->sf0_mimo = (data[14] >> 2) & 0x01;
    pL1Basic->sf0_miso = (cxdref_atsc3_miso_t)(data[14] & 0x03);
    pL1Basic->sf0_fft_size = (cxdref_atsc3_fft_size_t)((data[15] >> 6) & 0x03);
    pL1Basic->sf0_reduced_carriers = (data[15] >> 3) & 0x07;
    pL1Basic->sf0_gi = (cxdref_atsc3_gi_t)(((data[15] << 1) & 0x0E) | ((data[16] >> 7) & 0x01));
    pL1Basic->sf0_num_ofdm_symbol = ((data[16] << 4) & 0x7F0) | ((data[17] >> 4) & 0x0F);
    pL1Basic->sf0_sp = (cxdref_atsc3_sp_t)(((data[17] << 1) & 0x1E) | ((data[18] >> 5) & 0x01));
    pL1Basic->sf0_sp_boost = (data[18] >> 2) & 0x07;
    pL1Basic->sf0_sbs_first = (data[18] >> 1) & 0x01;
    pL1Basic->sf0_sbs_last = data[18] & 0x01;
    pL1Basic->reserved[0] = data[19];
    pL1Basic->reserved[1] = data[20];
    pL1Basic->reserved[2] = data[21];
    pL1Basic->reserved[3] = data[22];
    pL1Basic->reserved[4] = data[23];
    pL1Basic->reserved[5] = data[24];

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_L1Detail_raw (cxdref_demod_t * pDemod,
                                                     cxdref_atsc3_l1basic_t * pL1Basic,
                                                     cxdref_atsc3_l1detail_raw_t * pL1Detail)
{
    uint8_t data[2];
    cxdref_result_t result;
    cxdref_stopwatch_t timer;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;
    uint16_t ramSize = 0;
    uint8_t subBank = 0;
    uint16_t totalReadSize = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_L1Detail_raw");

    if (!pDemod || !pL1Basic || !pL1Detail) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
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

    result = cxdref_stopwatch_start (&timer);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    for (;;) {
        result = cxdref_stopwatch_elapsed(&timer, &elapsed);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (elapsed >= CXDREF_ATSC3_WAIT_L1_DETAIL_TIMEOUT) {
            continueWait = 0;
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE1, data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (data[0] & 0x80) {
            break;
        }
        if (continueWait) {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_ATSC3_WAIT_L1_DETAIL_INTERVAL);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_TIMEOUT);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE0, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_atsc3_monitor_L1Basic (pDemod, pL1Basic);
    if (result != CXDREF_RESULT_OK) {
        goto unfreeze_l1_info;
    }

    continueWait = 1;
    elapsed = 0;

    result = cxdref_stopwatch_start (&timer);
    if (result != CXDREF_RESULT_OK) {
        goto unfreeze_l1_info;
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        result = CXDREF_RESULT_ERROR_I2C;
        goto unfreeze_l1_info;
    }

    for (;;) {
        result = cxdref_stopwatch_elapsed(&timer, &elapsed);
        if (result != CXDREF_RESULT_OK) {
            goto unfreeze_l1_info;
        }

        if (elapsed >= CXDREF_ATSC3_WAIT_L1_DETAIL_TIMEOUT2) {
            continueWait = 0;
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE1, data, 1) != CXDREF_RESULT_OK) {
            result = CXDREF_RESULT_ERROR_I2C;
            goto unfreeze_l1_info;
        }
        if (data[0] & 0x40) {
            result = CXDREF_RESULT_ERROR_HW_STATE;
            goto unfreeze_l1_info;
        }
        if (data[0] & 0x20) {
            break;
        }
        if (continueWait) {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_ATSC3_WAIT_L1_DETAIL_INTERVAL);
            if (result != CXDREF_RESULT_OK) {
                goto unfreeze_l1_info;
            }
        } else {
            result = CXDREF_RESULT_ERROR_TIMEOUT;
            goto unfreeze_l1_info;
        }
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE1, data, 2) != CXDREF_RESULT_OK) {
        result = CXDREF_RESULT_ERROR_I2C;
        goto unfreeze_l1_info;
    }
    ramSize = ((data[0] & 0x1F) << 8) | data[1];
    pL1Detail->size = ramSize;

    for (subBank = 0; ramSize > 0; subBank++) {
        uint16_t readSize = 0;
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
            result = CXDREF_RESULT_ERROR_I2C;
            goto unfreeze_l1_info;
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE3, subBank) != CXDREF_RESULT_OK) {
            result = CXDREF_RESULT_ERROR_I2C;
            goto unfreeze_l1_info;
        }
        if (ramSize < L1RAM_READ_SIZE) {
            readSize = ramSize;
        } else {
            readSize = L1RAM_READ_SIZE;
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x92) != CXDREF_RESULT_OK) {
            result = CXDREF_RESULT_ERROR_I2C;
            goto unfreeze_l1_info;
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &pL1Detail->data[totalReadSize], readSize) != CXDREF_RESULT_OK) {
            result = CXDREF_RESULT_ERROR_I2C;
            goto unfreeze_l1_info;
        }
        ramSize -= readSize;
        totalReadSize += readSize;
    }

unfreeze_l1_info:

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE0, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_atsc3_monitor_L1Detail_convert (cxdref_demod_t * pDemod,
                                                         cxdref_atsc3_l1basic_t * pL1Basic,
                                                         cxdref_atsc3_l1detail_raw_t * pL1Detail,
                                                         uint8_t plpID,
                                                         cxdref_atsc3_l1detail_common_t * pL1DetailCommon,
                                                         cxdref_atsc3_l1detail_subframe_t * pL1DetailSubframe,
                                                         cxdref_atsc3_l1detail_plp_t * pL1DetailPlp)
{
    uint32_t bp = 0;
    uint16_t sf_index = 0;
    uint8_t plp_index = 0;
    uint8_t i = 0;

    cxdref_atsc3_l1detail_subframe_t tmpL1dSubframe;
    cxdref_atsc3_l1detail_plp_t tmpL1dPlp;
    uint8_t plpFound = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_L1Detail_convert");

    if ((!pDemod) || (!pL1Basic) || (!pL1Detail) || (!pL1DetailCommon) || (!pL1DetailSubframe) || (!pL1DetailPlp)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (plpID >= CXDREF_ATSC3_NUM_PLP_MAX) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pL1Detail->size < 25) || (pL1Detail->size > 8191)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pL1Detail->size != pL1Basic->l1d_size) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    cxdref_memset (pL1DetailCommon, 0, sizeof (cxdref_atsc3_l1detail_common_t));
    cxdref_memset (pL1DetailSubframe, 0, sizeof (cxdref_atsc3_l1detail_subframe_t));
    cxdref_memset (pL1DetailPlp, 0, sizeof (cxdref_atsc3_l1detail_plp_t));

    pL1DetailCommon->version = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 4);
    bp += 4;

    pL1DetailCommon->num_rf = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 3);
    bp += 3;

    for (i = 0; i < pL1DetailCommon->num_rf; i++) {
        if (i == 0) {
            pL1DetailCommon->bonded_bsid = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 16);
        }
        bp += 16;

        bp += 3;
    }

    if (pL1Basic->time_info_flg != CXDREF_ATSC3_TIME_INFO_FLAG_NONE) {
        pL1DetailCommon->time_sec = (uint32_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 32);
        bp += 32;

        pL1DetailCommon->time_msec = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 10);
        bp += 10;

        if (pL1Basic->time_info_flg != CXDREF_ATSC3_TIME_INFO_FLAG_MS) {
            pL1DetailCommon->time_usec = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 10);
            bp += 10;

            if (pL1Basic->time_info_flg != CXDREF_ATSC3_TIME_INFO_FLAG_US) {
                pL1DetailCommon->time_nsec = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 10);
                bp += 10;
            }
        }
    }

    for (sf_index = 0; sf_index <= pL1Basic->num_subframe; sf_index++) {
        if (bp + 48 >= (uint32_t)(pL1Detail->size) * 8) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
        }

        cxdref_memset (&tmpL1dSubframe, 0, sizeof (cxdref_atsc3_l1detail_subframe_t));

        tmpL1dSubframe.index = (uint8_t) sf_index;

        if (sf_index == 0) {
            tmpL1dSubframe.mimo = pL1Basic->sf0_mimo;
            tmpL1dSubframe.miso = pL1Basic->sf0_miso;
            tmpL1dSubframe.fft_size = pL1Basic->sf0_fft_size;
            tmpL1dSubframe.reduced_carriers = pL1Basic->sf0_reduced_carriers;
            tmpL1dSubframe.gi = pL1Basic->sf0_gi;
            tmpL1dSubframe.num_ofdm_symbol = pL1Basic->sf0_num_ofdm_symbol;
            tmpL1dSubframe.sp = pL1Basic->sf0_sp;
            tmpL1dSubframe.sp_boost = pL1Basic->sf0_sp_boost;
            tmpL1dSubframe.sbs_first = pL1Basic->sf0_sbs_first;
            tmpL1dSubframe.sbs_last = pL1Basic->sf0_sbs_last;
        } else {
            tmpL1dSubframe.mimo = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
            bp += 1;
            tmpL1dSubframe.miso = (cxdref_atsc3_miso_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 2);
            bp += 2;
            tmpL1dSubframe.fft_size = (cxdref_atsc3_fft_size_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 2);
            bp += 2;
            tmpL1dSubframe.reduced_carriers = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 3);
            bp += 3;
            tmpL1dSubframe.gi = (cxdref_atsc3_gi_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 4);
            bp += 4;
            tmpL1dSubframe.num_ofdm_symbol = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 11);
            bp += 11;
            tmpL1dSubframe.sp = (cxdref_atsc3_sp_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 5);
            bp += 5;
            tmpL1dSubframe.sp_boost = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 3);
            bp += 3;
            tmpL1dSubframe.sbs_first = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
            bp += 1;
            tmpL1dSubframe.sbs_last = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
            bp += 1;
        }

        if (pL1Basic->num_subframe > 0) {
            tmpL1dSubframe.subframe_mux = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
            bp += 1;
        }

        tmpL1dSubframe.freq_interleaver = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
        bp += 1;

        if (tmpL1dSubframe.sbs_first || tmpL1dSubframe.sbs_last) {
            tmpL1dSubframe.sbs_null_cells = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 13);
            bp += 13;
        }

        tmpL1dSubframe.num_plp = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 6);
        bp += 6;

        for (plp_index = 0; plp_index <= tmpL1dSubframe.num_plp; plp_index++) {
            if (bp + 48 >= (uint32_t)(pL1Detail->size) * 8) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
            }

            cxdref_memset (&tmpL1dPlp, 0, sizeof (cxdref_atsc3_l1detail_plp_t));

            tmpL1dPlp.id = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 6);
            bp += 6;
            tmpL1dPlp.lls_flg = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
            bp += 1;
            tmpL1dPlp.layer = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 2);
            bp += 2;
            tmpL1dPlp.start = (uint32_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 24);
            bp += 24;
            tmpL1dPlp.size = (uint32_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 24);
            bp += 24;
            tmpL1dPlp.scrambler_type = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 2);
            bp += 2;
            tmpL1dPlp.fec_type = (cxdref_atsc3_plp_fec_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 4);
            bp += 4;
            if (tmpL1dPlp.fec_type <= CXDREF_ATSC3_PLP_FEC_LDPC_64K) {
                tmpL1dPlp.mod = (cxdref_atsc3_plp_mod_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 4);
                bp += 4;
                tmpL1dPlp.cod = (cxdref_atsc3_plp_cod_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 4);
                bp += 4;
            }

            tmpL1dPlp.ti_mode = (cxdref_atsc3_plp_ti_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 2);
            bp += 2;
            if (tmpL1dPlp.ti_mode == CXDREF_ATSC3_PLP_TI_NONE) {
                tmpL1dPlp.fec_block_start = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 15);
                bp += 15;
            } else if (tmpL1dPlp.ti_mode == CXDREF_ATSC3_PLP_TI_CTI) {
                tmpL1dPlp.cti_fec_block_start = (uint32_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 22);
                bp += 22;
            }

            if (pL1DetailCommon->num_rf > 0) {
                tmpL1dPlp.num_ch_bonded = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 3);
                bp += 3;
                if (tmpL1dPlp.num_ch_bonded > 0) {
                    tmpL1dPlp.ch_bonding_format = (cxdref_atsc3_plp_ch_bond_fmt_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 2);
                    bp += 2;
                    for (i = 0; i <= tmpL1dPlp.num_ch_bonded; i++) {
                        if (i == 0) {
                            tmpL1dPlp.bonded_rf_id_0 = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 3);
                        } else if (i == 1) {
                            tmpL1dPlp.bonded_rf_id_1 = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 3);
                        }
                        bp += 3;
                    }
                }
            }

            if (tmpL1dSubframe.mimo) {
                tmpL1dPlp.mimo_stream_combine = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
                bp += 1;
                tmpL1dPlp.mimo_iq_interleave = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
                bp += 1;
                tmpL1dPlp.mimo_ph = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
                bp += 1;
            }

            if (tmpL1dPlp.layer == 0) {
                tmpL1dPlp.plp_type = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
                bp += 1;
                if (tmpL1dPlp.plp_type) {
                    tmpL1dPlp.num_subslice = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 14);
                    bp += 14;
                    tmpL1dPlp.subslice_interval = (uint32_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 24);
                    bp += 24;
                }

                if (((tmpL1dPlp.ti_mode == CXDREF_ATSC3_PLP_TI_CTI) || (tmpL1dPlp.ti_mode == CXDREF_ATSC3_PLP_TI_HTI))
                    && (tmpL1dPlp.mod == CXDREF_ATSC3_PLP_MOD_QPSK)) {
                    tmpL1dPlp.ti_ext_interleave = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
                    bp += 1;
                }

                if (tmpL1dPlp.ti_mode == CXDREF_ATSC3_PLP_TI_CTI) {
                    tmpL1dPlp.cti_depth = (cxdref_atsc3_plp_cti_depth_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 3);
                    bp += 3;
                    tmpL1dPlp.cti_start_row = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 11);
                    bp += 11;
                } else if (tmpL1dPlp.ti_mode == CXDREF_ATSC3_PLP_TI_HTI) {
                    tmpL1dPlp.hti_inter_subframe = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
                    bp += 1;
                    tmpL1dPlp.hti_num_ti_blocks = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 4);
                    bp += 4;
                    tmpL1dPlp.hti_num_fec_block_max = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 12);
                    bp += 12;

                    if (tmpL1dPlp.hti_inter_subframe == 0) {
                        tmpL1dPlp.hti_num_fec_block[0] = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 12);
                        bp += 12;
                    } else {
                        for (i = 0; i <= tmpL1dPlp.hti_num_ti_blocks; i++) {
                            tmpL1dPlp.hti_num_fec_block[i] = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 12);
                            bp += 12;
                        }
                    }
                    tmpL1dPlp.hti_cell_interleave = (uint8_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 1);
                    bp += 1;
                }
            } else {
                tmpL1dPlp.ldm_inj_level = (cxdref_atsc3_plp_ldm_inj_level_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 5);
                bp += 5;
            }

            if (tmpL1dPlp.id == plpID) {
                if (!plpFound) {
                    cxdref_memcpy (pL1DetailSubframe, &tmpL1dSubframe, sizeof (cxdref_atsc3_l1detail_subframe_t));
                    cxdref_memcpy (pL1DetailPlp, &tmpL1dPlp, sizeof (cxdref_atsc3_l1detail_plp_t));
                    plpFound = 1;
                }
            }
        }
    }

    if (pL1DetailCommon->version >= 1) {
        pL1DetailCommon->bsid = (uint16_t) cxdref_BitSplitFromByteArray (pL1Detail->data, bp, 16);
        bp += 16;
    }

    if (bp + 32 > (uint32_t)(pL1Detail->size) * 8) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    pL1DetailCommon->reserved_bitlen = (uint16_t)(pL1Basic->l1d_size * 8 - bp - 32);

    if (plpFound) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK_CONFIRM);
    }
}

cxdref_result_t cxdref_demod_atsc3_monitor_PLPList (cxdref_demod_t * pDemod,
                                                cxdref_atsc3_plp_list_entry_t plpList[CXDREF_ATSC3_NUM_PLP_MAX],
                                                uint8_t * pNumPLPs)
{
    uint8_t data[36];
    uint8_t plpInfoRdy = 0;
    uint8_t plpInfoChg = 0;
    uint8_t l1dNumRf = 0;
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t k = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_PLPList");

    if ((!pDemod) || (!pNumPLPs)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data[0] & 0x02)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (!plpList) {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9D, data, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9D, data, 36) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    plpInfoRdy = (data[0] & 0x01) ? 1 : 0;
    plpInfoChg = (data[1] & 0x01) ? 1 : 0;
    l1dNumRf   = data[35] & 0x07;

    if(plpInfoRdy == 1) {
        if (plpInfoChg == 1) {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9C, 0x01) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
            }
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if ((data[2] & 0x7f) > 64) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }
    *pNumPLPs = data[2] & 0x7F;

    if (!plpList) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            if (((data[3 + i] >> (7 - j)) & 0x01)) {
                plpList[k].id = 8 * i + j;
                plpList[k].lls_flg = (data[11 + i] >> (7 - j)) & 0x01;
                plpList[k].layer = (data[19 + i] >> (7 - j)) & 0x01;
                plpList[k].chbond = l1dNumRf > 0 ? (data[27 + i] >> (7 - j)) & 0x01 : 0;
                k++;
            }
        }
    }
    if (k != *pNumPLPs) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_PLPError (cxdref_demod_t * pDemod,
                                                 uint8_t * pPLPError)
{
    uint8_t data;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_PLPError");

    if ((!pDemod) || (!pPLPError)) {
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

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, &data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data & 0x02)) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9D, &data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(data & 0x01)) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x84, &data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    *pPLPError = data & 0x01;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_SelectedPLPValid (cxdref_demod_t * pDemod,
                                                         uint8_t plpValid[4])
{
    cxdref_result_t result;
    uint8_t data[6];

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_SelectedPLPValid");

    if ((!pDemod) || (!plpValid)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    plpValid[0] = 0;
    plpValid[1] = 0;
    plpValid[2] = 0;
    plpValid[3] = 0;

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

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, data, sizeof (data)) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (data[4] & 0x01) {
        if ((data[0] & 0x80) && !(data[1] & 0x80) && !(data[2] & 0x80) && !(data[3] & 0x80) && (data[5] & 0x01)) {
            uint8_t numPLP = 0;

            result = cxdref_demod_atsc3_monitor_PLPList (pDemod, NULL, &numPLP);
            if ((result == CXDREF_RESULT_OK) && (numPLP == 1)) {
                plpValid[0] = 1;
                CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
            } else {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
    } else {
        plpValid[0] = (data[0] & 0x80) ? 1 : 0;
        plpValid[1] = (data[1] & 0x80) ? 1 : 0;
        plpValid[2] = (data[2] & 0x80) ? 1 : 0;
        plpValid[3] = (data[3] & 0x80) ? 1 : 0;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }
}

cxdref_result_t cxdref_demod_atsc3_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                      cxdref_demod_terr_cable_spectrum_sense_t * pSense)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_SpectrumSense");

    if ((!pDemod) || (!pSense)) {
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

    result = isDemodLocked (pDemod);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    {
        uint8_t data = 0;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF3, &data, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        if (pDemod->confSense == CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV) {
            *pSense = (data & 0x01) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV;
        } else {
            *pSense = (data & 0x01) ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL;
        }
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_SNR (cxdref_demod_t * pDemod,
                                            int32_t * pSNR)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_SNR");

    if ((!pDemod) || (!pSNR)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->atsc3EasState == CXDREF_DEMOD_ATSC3_EAS_STATE_EAS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        uint32_t reg;
        uint8_t data[3];
        uint8_t ofdmNsbf;
        uint32_t L;
        uint32_t S;
        int32_t C;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = isDemodLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x28, data, sizeof (data)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x58, &ofdmNsbf, 1) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        reg = (data[0] << 16) | (data[1] << 8) | data[2];

        if (ofdmNsbf == 0x00) {
            L = 419404;
            S = 519913;
            C = 33800;
        } else {
            L = 704089;
            S = 971204;
            C = 35800;
        }

        if (reg > L) {
            reg = L;
        }

        *pSNR = 10 * 10 * ((int32_t) cxdref_math_log10 (reg) - (int32_t) cxdref_math_log10 (S - reg));
        *pSNR += C;
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_PreLDPCBER (cxdref_demod_t * pDemod,
                                                   uint32_t ber[4])
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_PreLDPCBER");

    if ((!pDemod) || (!ber)) {
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
            if (!valid[i]) {
#if 0 /* customization from original reference code */
                ber[i] = CXDREF_DEMOD_ATSC3_MONITOR_PRELDPCBER_INVALID;
#else
                ber[i] = 0;
#endif
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

            {
                uint32_t div = 0;
                uint32_t Q = 0;
                uint32_t R = 0;

                if (periodExp[i] >= 4) {
                    div = (1U << (periodExp[i] - 4)) * (n_ldpc[i] / 200);

                    Q = (bitError[i] * 5) / div;
                    R = (bitError[i] * 5) % div;

                    R *= 625;
                    Q = Q * 625 + R / div;
                    R = R % div;
                }
                else {
                    div = (1U << periodExp[i]) * (n_ldpc[i] / 200);

                    Q = (bitError[i] * 10) / div;
                    R = (bitError[i] * 10) % div;

                    R *= 5000;
                    Q = Q * 5000 + R / div;
                    R = R % div;
                }

                if (R >= div/2) {
                    ber[i] = Q + 1;
                }
                else {
                    ber[i] = Q;
                }
            }
        }

    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_PreBCHBER (cxdref_demod_t * pDemod,
                                                  uint32_t ber[4])
{
    uint8_t valid[4] = {0};
    uint32_t bitError[4] = {0};
    uint32_t periodExp[4] = {0};
    uint32_t n_bch[4] = {0};
    int i = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_PreBCHBER");

    if ((!pDemod) || (!ber)) {
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
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        for (i = 0; i < 4; i++) {

            if (!valid[i]) {
#if 0 /* customization from original reference code */
                ber[i] = CXDREF_DEMOD_ATSC3_MONITOR_PREBCHBER_INVALID;
#else
                ber[i] = 0;
#endif
                continue;
            }

            if (bitError[i] > ((1U << periodExp[i]) * n_bch[i])) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
            if (periodExp[i] >= 6) {
                div = (1U << (periodExp[i] - 6)) * (n_bch[i] / 40);

                Q = (bitError[i] * 625) / div;
                R = (bitError[i] * 625) % div;

                R *= 625;
                Q = Q * 625 + R / div;
                R = R % div;
            }
            else {
                div = (1U << periodExp[i]) * (n_bch[i] / 40);

                Q = (bitError[i] * 1000) / div;
                R = (bitError[i] * 1000) % div;

                R *= 25000;
                Q = Q * 25000 + R / div;
                R = R % div;
            }

            if (R >= div/2) {
                ber[i] = Q + 1;
            }
            else {
                ber[i] = Q;
            }
        }
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_PostBCHFER (cxdref_demod_t * pDemod,
                                                   uint32_t fer[4])
{
    uint8_t valid[4] = {0};
    uint32_t fecError[4] = {0};
    uint32_t period[4]= {0};
    int i = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_PostBCHFER");

    if ((!pDemod) || (!fer)) {
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
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        for (i = 0; i < 4; i++) {
            if (!valid[i]) {
                fer[i] = CXDREF_DEMOD_ATSC3_MONITOR_POSTBCHFER_INVALID;
                continue;
            }

            if ((period[i] == 0) || (fecError[i] > period[i])) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
            div = period[i];
            Q = (fecError[i] * 1000) / div;
            R = (fecError[i] * 1000) % div;

            R *= 1000;
            Q = Q * 1000 + R / div;
            R = R % div;

            if ((div != 1) && (R >= div/2)) {
                fer[i] = Q + 1;
            }
            else {
                fer[i] = Q;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_BBPacketErrorNumber (cxdref_demod_t * pDemod,
                                                            uint32_t pen[4])
{
    uint8_t data[12];
    int i = 0;
    int invalidCount = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_BBPacketErrorNumber");

    if ((!pDemod) || (!pen)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x94) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x94, data, sizeof (data)) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (i = 0; i < 4; i++) {
        if (!(data[i * 3] & 0x01) ) {
            pen[i] = 0;
            invalidCount++;
            continue;
        }

        pen[i] =  ((data[i * 3 + 1] << 8) | data[i * 3 + 2]);
    }

    if (invalidCount == 4) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                       int32_t * pPPM)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_SamplingOffset");

    if ((!pDemod) || (!pPPM)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->atsc3EasState == CXDREF_DEMOD_ATSC3_EAS_STATE_EAS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    {
        cxdref_result_t result = CXDREF_RESULT_OK;
        uint8_t ctlValReg[5];
        uint8_t nominalRateReg[5];
        uint32_t trlCtlVal = 0;
        uint32_t trcgNominalRate = 0;
        int32_t num;
        int32_t den;
        int8_t diffUpper = 0;

        if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = isDemodLocked (pDemod);
        if (result != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (result);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x52, ctlValReg, sizeof (ctlValReg)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRateReg, sizeof (nominalRateReg)) != CXDREF_RESULT_OK) {
            SLVT_UnFreezeReg (pDemod);
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        SLVT_UnFreezeReg (pDemod);

        diffUpper = (ctlValReg[0] & 0x7F) - (nominalRateReg[0] & 0x7F);

        if ((diffUpper < -1) || (diffUpper > 1)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
        }

        trlCtlVal = ctlValReg[1] << 24;
        trlCtlVal |= ctlValReg[2] << 16;
        trlCtlVal |= ctlValReg[3] << 8;
        trlCtlVal |= ctlValReg[4];

        trcgNominalRate = nominalRateReg[1] << 24;
        trcgNominalRate |= nominalRateReg[2] << 16;
        trcgNominalRate |= nominalRateReg[3] << 8;
        trcgNominalRate |= nominalRateReg[4];

        trlCtlVal >>= 1;
        trcgNominalRate >>= 1;

        if (diffUpper == 1) {
            num = (int32_t)((trlCtlVal + 0x80000000u) - trcgNominalRate);
        } else if (diffUpper == -1) {
            num = -(int32_t)((trcgNominalRate + 0x80000000u) - trlCtlVal);
        } else {
            num = (int32_t)(trlCtlVal - trcgNominalRate);
        }

        den = (nominalRateReg[0] & 0x7F) << 24;
        den |= nominalRateReg[1] << 16;
        den |= nominalRateReg[2] << 8;
        den |= nominalRateReg[3];
        den = (den + (390625/2)) / 390625;

        den >>= 1;

        if (num >= 0) {
            *pPPM = (num + (den/2)) / den;
        }
        else {
            *pPPM = (num - (den/2)) / den;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_FecModCod (cxdref_demod_t * pDemod,
                                                  cxdref_atsc3_plp_fecmodcod_t fecmodcod[4])
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t plpValid[4];
    uint8_t data[6] = {0};
    int i = 0;
    uint8_t tempFecType[4];
    uint8_t tempMod[4];
    uint8_t tempCod[4];

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_FecModCod");

    if ((!pDemod) || (!fecmodcod)) {
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

    result = cxdref_demod_atsc3_monitor_SelectedPLPValid (pDemod, plpValid);
    if (result != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x90, data, sizeof(data)) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    tempFecType[0] = (data[0] >> 4) & 0x0F;
    tempFecType[1] = data[0] & 0x0F;
    tempFecType[2] = (data[1] >> 4) & 0x0F;
    tempFecType[3] = data[1] & 0x0F;
    tempMod[0] = (data[2] >> 4) & 0x0F;
    tempMod[1] = data[2] & 0x0F;
    tempMod[2] = (data[3] >> 4) & 0x0F;
    tempMod[3] = data[3] & 0x0F;
    tempCod[0] = (data[4] >> 4) & 0x0F;
    tempCod[1] = data[4] & 0x0F;
    tempCod[2] = (data[5] >> 4) & 0x0F;
    tempCod[3] = data[5] & 0x0F;

    for (i = 0; i < 4; i ++) {
        if (!plpValid[i]) {
            fecmodcod[i].valid = 0;
            continue;
        }
        fecmodcod[i].valid = 1;
        fecmodcod[i].fec_type = (cxdref_atsc3_plp_fec_t)tempFecType[i];
        fecmodcod[i].mod = (cxdref_atsc3_plp_mod_t)tempMod[i];
        fecmodcod[i].cod = (cxdref_atsc3_plp_cod_t)tempCod[i];
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_ALPHeaderLengthError (cxdref_demod_t * pDemod,
                                                             uint8_t errorStandard[4],
                                                             uint8_t errorKorean[4])
{
    uint32_t fer[4];
    int i;
    uint8_t data[8];
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_ALPHeaderLengthError");

    if ((!pDemod) || (!errorStandard) || (!errorKorean)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (!CXDREF_DEMOD_CHIP_ID_2878A_FAMILY (pDemod->chipId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_atsc3_monitor_PostBCHFER (pDemod, fer);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    for (i = 0; i < 4; i++) {
        if (fer[i] > 0) {
            errorStandard[i] = CXDREF_DEMOD_ATSC3_MONITOR_ALPHEADERLENGTHERROR_INVALID;
            errorKorean[i] = CXDREF_DEMOD_ATSC3_MONITOR_ALPHEADERLENGTHERROR_INVALID;
        } else {
            errorStandard[i] = 0;
            errorKorean[i] = 0;
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9E) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE0, data, 8) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (i = 0; i < 4; i++) {
        if (errorStandard[i] == 0) {
            if (data[i * 2] & 0x80) {
                errorStandard[i] = 1;
            }
        }

        if (errorKorean[i] == 0) {
            if (data[i * 2 + 1] & 0x80) {
                errorKorean[i] = 1;
            }
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE8, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_AveragedPreBCHBER (cxdref_demod_t * pDemod,
                                                          uint32_t ber[4])
{
    uint8_t rdata[128];
    uint8_t invalid[4] = {0};
    uint8_t periodType[4] = {0};
    uint32_t bitError[4] = {0};
    int hNum[4] = {0};
    uint32_t periodExp[4] = {0};
    uint32_t periodExpData[4] = {0};
    uint32_t periodNum[4] = {0};
    uint32_t n_bch[4] = {0};
    int i;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_AveragedPreBCHBER");

    if ((!pDemod) || (!ber)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x91) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7C, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    periodType[0] = rdata[0] & 0x01;
    if ((rdata[1] & 0x03) != 0x00) {
        invalid[0] = 1;
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    periodType[1] = rdata[0] & 0x01;
    if ((rdata[1] & 0x03) != 0x00) {
        invalid[1] = 1;
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x84, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    periodType[2] = rdata[0] & 0x01;
    if ((rdata[1] & 0x03) != 0x00) {
        invalid[2] = 1;
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x88, rdata, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    periodType[3] = rdata[0] & 0x01;
    if ((rdata[1] & 0x03) != 0x00) {
        invalid[3] = 1;
    }

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, rdata, 32) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x53, rdata + 32, 96) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }


    for (i = 0; i < 4; i++) {
        for (hNum[i] = 0; hNum[i] < 8; hNum[i]++) {
            if ((rdata[hNum[i] * 4 + i * 32] & 0x80) == 0x00) {
                break;
            }

            bitError[i] += ((rdata[hNum[i] * 4 + i * 32] & 0x7F) << 16) + (rdata[hNum[i] * 4 + i * 32 + 1] << 8) + rdata[hNum[i] * 4 + i * 32 + 2];
        }

        if (hNum[i] == 0) {
            invalid[i] = 1;
        }
    }

    {
        uint8_t data[2];
        cxdref_atsc3_plp_fec_t plpFecType[4];
        cxdref_atsc3_plp_cod_t plpCr[4];
        static const uint16_t nBCHBitsLookup[2][12] = {
            {2160,  3240,  4320,  5400,  6480,  7560,  8640,  9720, 10800, 11880, 12960, 14040},
            {8640, 12960, 17280, 21600, 25920, 30240, 34560, 38880, 43200, 47520, 51840, 56160},
        };

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
            if (invalid[i]) {
                continue;
            }
            if ((plpFecType[i] > CXDREF_ATSC3_PLP_FEC_LDPC_64K) || (plpCr[i] > CXDREF_ATSC3_PLP_COD_13_15)) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
            if ((plpFecType[i] != CXDREF_ATSC3_PLP_FEC_BCH_LDPC_16K) && (plpFecType[i] != CXDREF_ATSC3_PLP_FEC_BCH_LDPC_64K)) {
                invalid[i] = 1;
                continue;
            }

            n_bch[i] = nBCHBitsLookup[plpFecType[i]][plpCr[i]];
        }

    }

    if ((!invalid[0] && periodType[0] == 0) || (!invalid[1] && periodType[1] == 0)
        || (!invalid[2] && periodType[2] == 0) || (!invalid[3] && periodType[3] == 0)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x94) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4A, rdata, 2) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        periodExpData[0] = (rdata[0] >> 4) & 0x0F;
        periodExpData[1] = rdata[0] & 0x0F;
        periodExpData[2] = (rdata[1] >> 4) & 0x0F;
        periodExpData[3] = rdata[1] & 0x0F;
    }

    for (i = 0; i < 4; i++) {
        if (invalid[i]) {
            continue;
        }
        if (periodType[i] == 0) {
            periodExp[i] = periodExpData[i];
            periodNum[i] = hNum[i];
        } else {
            int j = 0;

            periodExp[i] = 0xFF;

            for (j = 0; j < hNum[i]; j++) {
                uint8_t expTmp = rdata[j * 4 + 3 + i * 32];

                if (periodExp[i] > expTmp) {
                    periodExp[i] = expTmp;
                }
            }
            for (j = 0; j < hNum[i]; j++) {
                periodNum[i] += 1 << (rdata[j * 4 + 3 + i * 32] - periodExp[i]);
            }
        }
    }

    {
        uint32_t div = 0;
        uint32_t Q = 0;
        uint32_t R = 0;

        for (i = 0; i < 4; i++) {
            if (invalid[i]) {
                ber[i] = CXDREF_DEMOD_ATSC3_MONITOR_PREBCHBER_INVALID;
                continue;
            }
            if (periodExp[i] >= 6) {
                div = (1U << (periodExp[i] - 6)) * (n_bch[i] / 40) * periodNum[i];

                Q = (bitError[i] * 125) / div;
                R = (bitError[i] * 125) % div;

                R *= 125;
                Q = Q * 125 + R / div;
                R = R % div;

                R *= 25;
                Q = Q * 25 + R / div;
                R = R % div;
            }
            else {
                div = (1U << periodExp[i]) * (n_bch[i] / 40) * periodNum[i];

                Q = (bitError[i] * 100) / div;
                R = (bitError[i] * 100) % div;

                R *= 500;
                Q = Q * 500 + R / div;
                R = R % div;

                R *= 500;
                Q = Q * 500 + R / div;
                R = R % div;
            }

            if (R >= div/2) {
                ber[i] = Q + 1;
            }
            else {
                ber[i] = Q;
            }
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_monitor_AveragedSNR (cxdref_demod_t * pDemod,
                                                    int32_t * pSNR)
{
    uint8_t rdata[32];
    int hNum = 0;
    uint32_t reg = 0;
    uint8_t ofdmNsbf = 0;
    uint32_t L = 0;
    uint32_t S = 0;
    int32_t C = 0;
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_AveragedSNR");

    if ((!pDemod) || (!pSNR)) {
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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x30, rdata, 32) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x58, &ofdmNsbf, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    SLVT_UnFreezeReg (pDemod);

    for (hNum = 0; hNum < 8; hNum++) {
        if ((rdata[hNum * 4] & 0x01) == 0x00) {
            break;
        }

        reg += (rdata[hNum * 4 + 1] << 16) + (rdata[hNum * 4 + 2] << 8) + rdata[hNum * 4 + 3];
    }

    if (hNum == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    reg = (reg + hNum / 2) / hNum;

    if (ofdmNsbf == 0x00) {
        L = 419404;
        S = 519913;
        C = 33800;
    } else {
        L = 704089;
        S = 971204;
        C = 35800;
    }

    if (reg > L) {
        reg = L;
    }

    *pSNR = 10 * 10 * ((int32_t) cxdref_math_log10 (reg) - (int32_t) cxdref_math_log10 (S - reg));
    *pSNR += C;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

/* customization from original reference code */
cxdref_result_t cxdref_demod_atsc3_monitor_SignalChange(cxdref_demod_t* pDemod, uint8_t* pFlag)
{
    uint8_t data[1] = {0};

    CXDREF_TRACE_ENTER("cxdref_demod_atsc3_monitor_SignalChange");

    if ((!pDemod) || (!pFlag)) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x94) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFC, data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
    }

    if ((data[0] & 0xF0) != 0) {
        if (pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
        }
        if (pDemod->pI2c->WriteOneRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF2, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
        }
    }

    *pFlag = data[0];

    CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
}

/* 
 *  customization from original reference code
 *  ASTC3.0 standard does not define signal quality spec. This is our original spec.
 */
cxdref_result_t cxdref_demod_atsc3_monitor_Quality (cxdref_demod_t * pDemod, uint8_t * pQuality)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_atsc3_plp_fecmodcod_t fmc[4];
    cxdref_atsc3_plp_cod_t codeRate;
    cxdref_atsc3_plp_mod_t constellation;

	uint32_t ber[4] = {0};
    int32_t snr = 0;
    int32_t snrRel = 0;
    int32_t berSQI = 0;

    static const int32_t snrRefdB1000[6][12] = {
        { -3000,   -1400,   -100,   1000,   2100,   3100,   3800,   4600,   5400,   6300,   7200,   8200},
        {   900,    2600,   4300,   5600,   6900,   7900,   9000,  10000,  11000,  12200,  13300,  14600},
        {  2000,    4600,   6400,   8300,   9900,  11300,  12500,  13800,  15100,  16500,  17800,  19300},
        {  3900,    6500,   8900,  10800,  12900,  14400,  16100,  17900,  19500,  21200,  22900,  24800},
        {  5300,    8400,  11000,  13300,  15900,  17500,  19800,  21800,  23800,  25900,  28600,  30900},
        {  7000,   10300,  13300,  15900,  18900,  20900,  23600,  26000,  29400,  31500,  34800,  40000}
    };

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_monitor_Quality");

    if ((!pDemod) || (!pQuality)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_atsc3_monitor_FecModCod (pDemod, fmc);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }
    codeRate = fmc[0].cod;
    constellation = fmc[0].mod;

    result = cxdref_demod_atsc3_monitor_PreBCHBER (pDemod, &ber[0]);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_atsc3_monitor_SNR(pDemod, &snr);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN(result);
    }

    if ((codeRate >= CXDREF_ATSC3_PLP_COD_RESERVED_12) || (constellation >= CXDREF_ATSC3_PLP_MOD_RESERVED_6)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    if (ber[0] > 100000) {
        berSQI = 0;
    }
    else if (ber[0] >= 100) {
        berSQI = 6667;
    }
    else {
        berSQI = 16667;
    }

    snrRel = snr - snrRefdB1000[constellation][codeRate];

    if (snrRel < -3000) {
        *pQuality = 0;
    }
    else if (snrRel <= 3000) {
        uint32_t tempSQI = (((snrRel + 3000) * berSQI) + 500000) / 1000000;
        *pQuality = (tempSQI > 100)? 100 : (uint8_t) tempSQI;
    }
    else {
        *pQuality = 100;
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t isDemodLocked (cxdref_demod_t * pDemod)
{
    cxdref_result_t result;
    uint8_t syncState = 0;
    uint8_t alpLockStat[4] = {0};
    uint8_t alpLockAll = 0;
    uint8_t unlockDetected = 0;

    CXDREF_TRACE_ENTER ("isDemodLocked");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_atsc3_monitor_SyncStat (pDemod, &syncState, alpLockStat, &alpLockAll, &unlockDetected);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (syncState != 6) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    CXDREF_TRACE_RETURN (result);
}
