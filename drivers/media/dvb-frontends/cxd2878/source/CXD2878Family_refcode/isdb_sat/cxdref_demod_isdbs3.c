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

#include "cxdref_demod_isdbs3.h"
#include "cxdref_demod_isdbs3_monitor.h"

static cxdref_result_t SLtoAIS3 (cxdref_demod_t * pDemod);

static cxdref_result_t AIS3toSL (cxdref_demod_t * pDemod);

static cxdref_result_t AIS3toAIS3 (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbs3_Tune (cxdref_demod_t * pDemod,
                                      cxdref_isdbs3_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_Tune");

    if ((!pDemod) || (!pTuneParam)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->system == CXDREF_DTV_SYSTEM_ISDBS3)) {
        result = cxdref_demod_isdbs3_SetStreamID (pDemod, pTuneParam->streamid, pTuneParam->streamidType);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = AIS3toAIS3 (pDemod);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }
    else if ((pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->system != CXDREF_DTV_SYSTEM_ISDBS3)) {
        result = cxdref_demod_Sleep (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        pDemod->system = CXDREF_DTV_SYSTEM_ISDBS3;
        result = cxdref_demod_isdbs3_SetStreamID (pDemod, pTuneParam->streamid, pTuneParam->streamidType);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = SLtoAIS3 (pDemod);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }
    else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP){
        pDemod->system = CXDREF_DTV_SYSTEM_ISDBS3;
        result = cxdref_demod_isdbs3_SetStreamID (pDemod, pTuneParam->streamid, pTuneParam->streamidType);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = SLtoAIS3 (pDemod);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->state = CXDREF_DEMOD_STATE_ACTIVE;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs3_Sleep (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_Sleep");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->system == CXDREF_DTV_SYSTEM_ISDBS3)) {
        result = AIS3toSL (pDemod);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }

    pDemod->state = CXDREF_DEMOD_STATE_SLEEP;
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs3_CheckTMCCLock (cxdref_demod_t * pDemod,
                                                cxdref_demod_lock_result_t * pLock)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t agcLock = 0;
    uint8_t tstlvLock = 0;
    uint8_t tmccLock = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_CheckTMCCLock");

    if ((!pDemod) || (!pLock)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_isdbs3_monitor_SyncStat(pDemod, &agcLock, &tstlvLock, &tmccLock);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if(tmccLock){
        *pLock = CXDREF_DEMOD_LOCK_RESULT_LOCKED;
    } else {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs3_CheckTSTLVLock (cxdref_demod_t * pDemod,
                                                cxdref_demod_lock_result_t * pLock)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t agcLock = 0;
    uint8_t tstlvLock = 0;
    uint8_t tmccLock = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_CheckTSTLVLock");

    if ((!pDemod) || (!pLock)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_isdbs3_monitor_SyncStat (pDemod, &agcLock, &tstlvLock, &tmccLock);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (tstlvLock) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_LOCKED;
    } else {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_isdbs3_SetStreamID (cxdref_demod_t * pDemod,
                                             uint16_t streamid,
                                             cxdref_isdbs3_streamid_type_t streamidType)
{
    uint8_t data[2];

    CXDREF_TRACE_ENTER ("cxdref_demod_isdbs3_SetStreamID");

    if (!pDemod) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (streamidType == CXDREF_ISDBS3_STREAMID_TYPE_RELATIVE_STREAM_NUMBER) {
            if (streamid > 15) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x8A, (uint8_t)(0x10 | streamid)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        } else {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x8A, 0x00) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
    }

    data[0] = (uint8_t)((streamid >> 8) & 0xFF);
    data[1] = (uint8_t)(streamid & 0xFF);

    if (pDemod->pI2c->WriteRegister(pDemod->pI2c, pDemod->i2cAddressSLVT, 0x87, data, 2) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t SLtoAIS3 (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("SLtoAIS3");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

#ifdef CXDREF_DEMOD_SUPPORT_TLV
    if (pDemod->isdbs3Output == CXDREF_DEMOD_OUTPUT_ISDBS3_TLV) {
        result = cxdref_demod_SetTLVClockModeAndFreq (pDemod, CXDREF_DTV_SYSTEM_ISDBS3);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else
#endif
    {
        result = cxdref_demod_SetTSClockModeAndFreq (pDemod, CXDREF_DTV_SYSTEM_ISDBS3);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x0D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2D, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        uint8_t data = 0;

        if (pDemod->isdbs3Output == CXDREF_DEMOD_OUTPUT_ISDBS3_TLV) {
            data = 0x01;
        } else {
            data = 0x00;
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, data) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2C, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x28, 0x31) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, 0x31) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6A, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x18, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x20, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xDA) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBC, 0x02) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = {0x0B, 0x0B};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x43, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xAE) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = {0x04, 0x91};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x46, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xB6) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x74, 0x59) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD5) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = {0x48, 0xFE, 0x4E, 0x6E, 0xFE};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x61, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            const uint8_t data[] = {0x4E, 0x6E};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x67, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            const uint8_t data[] = {0x10, 0x03, 0x10};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x90, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD6) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = {0x33, 0xEB};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x18, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1C, 0x59) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x1E, 0x6E) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = {0x4E, 0x80, 0x80, 0x10};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x21, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            const uint8_t data[] = {0xB3, 0xB3, 0xC3};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x51, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xDA) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = {0x60, 0x70};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD6, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
    } else if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD3) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF9, 0x06) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xB6) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            const uint8_t data[] = {0x04, 0xB0};

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x84, data, sizeof (data)) != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD4) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, 0x01) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA4) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x90, 0x00) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xAE) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        const uint8_t data[] = {0x08, 0x70, 0x64};

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x20, data, sizeof (data)) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xDA) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xCB, 0x01) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, 0x01) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x01) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE8, 0x01) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7,
        (uint8_t)((pDemod->satTunerIqSense == CXDREF_DEMOD_SAT_IQ_SENSE_INV)? 0x01 : 0x00)) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, 0x10, 0x1F) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_SetTSDataPinHiZ (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t AIS3toSL (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("AIS3toSL");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, 0x1F, 0x1F) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_SetTSDataPinHiZ (pDemod, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        const uint8_t data[] = {0x0A, 0x0A};

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x43, data, sizeof (data)) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x18, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6A, 0x11) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, 0x21) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x28, 0x13) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFE, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2C, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2D, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t AIS3toAIS3 (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("AIS3toAIS3");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
