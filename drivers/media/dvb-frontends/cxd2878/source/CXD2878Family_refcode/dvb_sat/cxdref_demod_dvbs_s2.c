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

#include "cxdref_dtv.h"
#include "cxdref_common.h"
#include "cxdref_demod_dvbs_s2.h"
#include "cxdref_demod_dvbs_s2_monitor.h"

static cxdref_result_t SLtoASX (cxdref_demod_t * pDemod,
                               cxdref_dtv_system_t dtvSystem,
                               uint32_t symbolRateKSps);

static cxdref_result_t ASXtoSL (cxdref_demod_t * pDemod);

static cxdref_result_t ASXtoASX (cxdref_demod_t * pDemod,
                               uint32_t symbolRateKSps);

static cxdref_result_t TSRStoSL(cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbs_s2_Tune (cxdref_demod_t * pDemod,
                                       cxdref_dvbs_s2_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_Tune");

    if ((!pDemod) || (!pTuneParam)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pTuneParam->system != CXDREF_DTV_SYSTEM_DVBS) &&
        (pTuneParam->system != CXDREF_DTV_SYSTEM_DVBS2) &&
        (pTuneParam->system != CXDREF_DTV_SYSTEM_ANY)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) {
        if (pDemod->dvbss2ScanMode > 0){
            result = TSRStoSL (pDemod);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
            result = SLtoASX (pDemod, pTuneParam->system, pTuneParam->symbolRateKSps);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        } else if (pDemod->system == pTuneParam->system){
            result = ASXtoASX (pDemod, pTuneParam->symbolRateKSps);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            result = cxdref_demod_Sleep (pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
            result = SLtoASX (pDemod, pTuneParam->system, pTuneParam->symbolRateKSps);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        }
    } else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP){
        result = SLtoASX (pDemod, pTuneParam->system, pTuneParam->symbolRateKSps);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->state = CXDREF_DEMOD_STATE_ACTIVE;
    pDemod->system = pTuneParam->system;
    pDemod->dvbss2ScanMode = 0;
    
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_Sleep (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_Sleep");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state == CXDREF_DEMOD_STATE_ACTIVE){
        if (pDemod->dvbss2ScanMode > 0){
            result = TSRStoSL (pDemod);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        } else if ((pDemod->system == CXDREF_DTV_SYSTEM_DVBS)
            || (pDemod->system == CXDREF_DTV_SYSTEM_DVBS2) || (pDemod->system == CXDREF_DTV_SYSTEM_ANY)){
            result = ASXtoSL (pDemod);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        }
        pDemod->state = CXDREF_DEMOD_STATE_SLEEP;
    } else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP){
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->dvbss2ScanMode = 0;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_CheckTSLock (cxdref_demod_t * pDemod,
                                              cxdref_demod_lock_result_t * pLock)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t isLock = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_CheckTSLock");

    if ((!pDemod) || (!pLock)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbs_s2_monitor_SyncStat(pDemod, &isLock);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if(isLock){
        *pLock = CXDREF_DEMOD_LOCK_RESULT_LOCKED;
    } else {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_dvbs_s2_CheckIQInvert (cxdref_demod_t * pDemod,
                                                uint8_t * pIsInvert)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_CheckIQInvert");

    if ((!pDemod) || (!pIsInvert)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) &&
        (pDemod->state != CXDREF_DEMOD_STATE_SLEEP)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    *pIsInvert = 0;
    if (pDemod->isSinglecable){
        if (pDemod->isSinglecableSpectrumInv){
            if (pDemod->satTunerIqSense == CXDREF_DEMOD_SAT_IQ_SENSE_NORMAL){
                *pIsInvert = 1;
            }
        } else {
            if (pDemod->satTunerIqSense == CXDREF_DEMOD_SAT_IQ_SENSE_INV){
                *pIsInvert = 1;
            }
        }
    } else {
        if (pDemod->satTunerIqSense == CXDREF_DEMOD_SAT_IQ_SENSE_INV){
            *pIsInvert = 1;
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_dvbs_s2_SetSymbolRate (cxdref_demod_t * pDemod,
                                                uint32_t symbolRateKSps)
{
    uint32_t regValue = 0;
    uint8_t data[3] = {0, 0, 0};
    CXDREF_TRACE_ENTER ("cxdref_demod_dvbs_s2_SetSymbolRate");

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) &&
        (pDemod->state != CXDREF_DEMOD_STATE_SLEEP)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    regValue = ((symbolRateKSps * 16384) + 500) / 1000;
    if ((regValue == 0) || (regValue > 0x0FFFFF)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
    }
    data[0] = (uint8_t)((regValue >> 16) & 0x0F);
    data[1] = (uint8_t)((regValue >>  8) & 0xFF);
    data[2] = (uint8_t)( regValue        & 0xFF);

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xAE) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x20, data, 3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t SLtoASX (cxdref_demod_t * pDemod,
                               cxdref_dtv_system_t dtvSystem,
                               uint32_t symbolRateKSps)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t data = 0;
    CXDREF_TRACE_ENTER ("SLtoASX");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetTSClockModeAndFreq (pDemod, CXDREF_DTV_SYSTEM_DVBS);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if(dtvSystem == CXDREF_DTV_SYSTEM_DVBS){
        data = 0x0A;
    } else if(dtvSystem == CXDREF_DTV_SYSTEM_DVBS2){
        data = 0x0B;
    } else if(dtvSystem == CXDREF_DTV_SYSTEM_ANY){
        data = 0x0B;
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, data) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((dtvSystem == CXDREF_DTV_SYSTEM_DVBS) || (dtvSystem == CXDREF_DTV_SYSTEM_DVBS2)){
        data = 0x00;
    } else {
        data = 0x01;
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2D, data) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x20, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x18) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE3, 0x02) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE5, 0x04) != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_dvbs_s2_SetSymbolRate (pDemod, symbolRateKSps);
    if (result != CXDREF_RESULT_OK){CXDREF_TRACE_RETURN (result);}

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

static cxdref_result_t ASXtoSL (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("ASXtoSL");

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

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t ASXtoASX (cxdref_demod_t * pDemod, uint32_t symbolRateKSps)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("ASXtoASX");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbs_s2_SetSymbolRate(pDemod, symbolRateKSps);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t TSRStoSL(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("TSRStoSL");

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

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x24, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xAE) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x24, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2E, 0x06) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x30, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2B, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x56, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xAB) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF9, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA3) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBD, 0x01) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = ASXtoSL (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xA0) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x00) != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
