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

#include "cxdref_integ.h"
#include "cxdref_demod.h"
#include "cxdref_stdlib.h"

#ifdef CXDREF_DEMOD_SUPPORT_DVBT
#include "cxdref_demod_dvbt.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_DVBC
#include "cxdref_demod_dvbc.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_DVBT2
#include "cxdref_demod_dvbt2.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_DVBC2
#include "cxdref_demod_dvbc2.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_DVBS_S2
#include "cxdref_demod_dvbs_s2.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ISDBT
#include "cxdref_demod_isdbt.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ISDBC
#include "cxdref_demod_isdbc.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ISDBS
#include "cxdref_demod_isdbs.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ISDBS3
#include "cxdref_demod_isdbs3.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_J83B
#include "cxdref_demod_j83b.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ATSC
#include "cxdref_demod_atsc.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ATSC3
#include "cxdref_demod_atsc3.h"
#endif

static cxdref_result_t XtoSL (cxdref_demod_t * pDemod);

static cxdref_result_t SDtoSL (cxdref_demod_t * pDemod);

static cxdref_result_t SLtoSD (cxdref_demod_t * pDemod);

static cxdref_result_t loadConfigMemory (cxdref_demod_t * pDemod);

static cxdref_result_t setConfigMemory (cxdref_demod_t * pDemod,
                                      uint8_t slaveAddress,
                                      uint8_t bank,
                                      uint8_t registerAddress,
                                      uint8_t value,
                                      uint8_t bitMask);

static cxdref_result_t setRegisterBitsForConfig (cxdref_demod_t * pDemod,
                                               uint8_t slaveAddress,
                                               uint8_t bank,
                                               uint8_t registerAddress,
                                               uint8_t value,
                                               uint8_t bitMask);

#ifndef CXDREF_DEMOD_SUPPORT_DVBC2
#define cxdref_demod_dvbc2_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_DVBT2
#define cxdref_demod_dvbt2_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_DVBC
#define cxdref_demod_dvbc_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_DVBT
#define cxdref_demod_dvbt_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_DVBS_S2
#define cxdref_demod_dvbs_s2_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_ISDBT
#define cxdref_demod_isdbt_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_ISDBC
#define cxdref_demod_isdbc_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_ISDBS
#define cxdref_demod_isdbs_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_ISDBS3
#define cxdref_demod_isdbs3_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_J83B
#define cxdref_demod_j83b_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_ATSC
#define cxdref_demod_atsc_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

#ifndef CXDREF_DEMOD_SUPPORT_ATSC3
#define cxdref_demod_atsc3_Sleep(pDemod) CXDREF_RESULT_ERROR_NOSUPPORT
#endif

cxdref_result_t cxdref_demod_Create (cxdref_demod_t * pDemod,
                                 cxdref_demod_create_param_t * pCreateParam,
                                 cxdref_i2c_t * pDemodI2c)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_Create");

    if ((!pDemod) || (!pCreateParam) || (!pDemodI2c)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    switch (pCreateParam->i2cAddressSLVT) {
    case 0xC8:
    case 0xCA:
    case 0xD8:
    case 0xDA:
        break;
    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    cxdref_memset (pDemod, 0, sizeof (cxdref_demod_t));

    pDemod->xtalFreq = pCreateParam->xtalFreq;
    pDemod->i2cAddressSLVT = pCreateParam->i2cAddressSLVT;
    pDemod->i2cAddressSLVX = pCreateParam->i2cAddressSLVT + 4;
    pDemod->i2cAddressSLVR = pCreateParam->i2cAddressSLVT - 0x40;
    pDemod->i2cAddressSLVM = pCreateParam->i2cAddressSLVT - 0xA8;
    pDemod->pI2c = pDemodI2c;
    pDemod->tunerI2cConfig = pCreateParam->tunerI2cConfig;

#ifdef CXDREF_DEMOD_SUPPORT_ATSC
    pDemod->atscCoreDisable = pCreateParam->atscCoreDisable;
#else
    pDemod->atscCoreDisable = 1;
#endif

    pDemod->serialTSClockModeContinuous = 1;
#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
    pDemod->iffreqConfig.configDVBT_5 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.6);
    pDemod->iffreqConfig.configDVBT_6 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.6);
    pDemod->iffreqConfig.configDVBT_7 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.2);
    pDemod->iffreqConfig.configDVBT_8 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.8);

    pDemod->iffreqConfig.configDVBT2_1_7 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.5);
    pDemod->iffreqConfig.configDVBT2_5 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.6);
    pDemod->iffreqConfig.configDVBT2_6 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.6);
    pDemod->iffreqConfig.configDVBT2_7 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.2);
    pDemod->iffreqConfig.configDVBT2_8 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.8);

    pDemod->iffreqConfig.configDVBC_6 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.7);
    pDemod->iffreqConfig.configDVBC_7 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.9);
    pDemod->iffreqConfig.configDVBC_8 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.9);
    pDemod->iffreqConfig.configDVBC2_6 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.7);
    pDemod->iffreqConfig.configDVBC2_8 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.9);

    pDemod->iffreqConfig.configATSC = CXDREF_DEMOD_ATSC_MAKE_IFFREQ_CONFIG(5); /* customization from original reference code */

    pDemod->iffreqConfig.configATSC3_6 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(5); /* customization from original reference code */
    pDemod->iffreqConfig.configATSC3_7 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(5); /* customization from original reference code */
    pDemod->iffreqConfig.configATSC3_8 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(5); /* customization from original reference code */

    pDemod->iffreqConfig.configISDBT_6 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.55);
    pDemod->iffreqConfig.configISDBT_7 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.15);
    pDemod->iffreqConfig.configISDBT_8 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(4.75);

    pDemod->iffreqConfig.configISDBC_6 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.7);
    pDemod->iffreqConfig.configJ83B_5_06_5_36 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.7);
    pDemod->iffreqConfig.configJ83B_5_60 = CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(3.75);

    pDemod->tunerOptimize = CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON;
    pDemod->serialTSClkFreqTerrCable = CXDREF_DEMOD_SERIAL_TS_CLK_MID_FULL;
    pDemod->twoBitParallelTSClkFreqTerrCable = CXDREF_DEMOD_2BIT_PARALLEL_TS_CLK_MID;
    pDemod->atsc3AutoSpectrumInv = 1;
    pDemod->atsc3CWDetection = 1;
    pDemod->atsc3AutoPLPID_SPLP = 1;
    pDemod->atscNoSignalThresh = 0x7FFB61;
    pDemod->atscSignalThresh = 0x7C4926;
#endif
#ifdef CXDREF_DEMOD_SUPPORT_SAT
    pDemod->serialTSClkFreqSat = CXDREF_DEMOD_SERIAL_TS_CLK_MID_FULL;
    pDemod->twoBitParallelTSClkFreqSat = CXDREF_DEMOD_2BIT_PARALLEL_TS_CLK_MID;
    pDemod->isSinglecableSpectrumInv = 1;
    pDemod->dvbss2PowerSmooth = 1;
#ifdef CXDREF_DEMOD_SUPPORT_DVBS_S2_BLINDSCAN_VER2
    pDemod->dvbss2BlindScanVersion = CXDREF_DEMOD_DVBSS2_BLINDSCAN_VERSION2;
#endif
#endif
#ifdef CXDREF_DEMOD_SUPPORT_TLV
    pDemod->serialTLVClockModeContinuous = 1;
    pDemod->serialTLVClkFreqTerrCable = CXDREF_DEMOD_SERIAL_TS_CLK_MID_FULL;
    pDemod->serialTLVClkFreqSat = CXDREF_DEMOD_SERIAL_TS_CLK_MID_FULL;
    pDemod->twoBitParallelTLVClkFreqTerrCable = CXDREF_DEMOD_2BIT_PARALLEL_TS_CLK_MID;
    pDemod->twoBitParallelTLVClkFreqSat = CXDREF_DEMOD_2BIT_PARALLEL_TS_CLK_MID;
#endif
#ifdef CXDREF_DEMOD_SUPPORT_ALP
    pDemod->serialALPClockModeContinuous = 1;
#endif

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_Initialize (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_Initialize");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pDemod->state = CXDREF_DEMOD_STATE_UNKNOWN;
    pDemod->system = CXDREF_DTV_SYSTEM_UNKNOWN;
    pDemod->chipId = CXDREF_DEMOD_CHIP_ID_UNKNOWN;
#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
    pDemod->bandwidth = CXDREF_DTV_BW_UNKNOWN;
    pDemod->scanMode = 0;
    pDemod->isdbtEwsState = CXDREF_DEMOD_ISDBT_EWS_STATE_NORMAL;
    pDemod->atsc3EasState = CXDREF_DEMOD_ATSC3_EAS_STATE_NORMAL;
    pDemod->atsc3AutoSpectrumInv_flag = 0;
    pDemod->atsc3CWDetection_flag = 0;
    pDemod->atscCPUState = CXDREF_DEMOD_ATSC_CPU_STATE_IDLE;
#endif
#ifdef CXDREF_DEMOD_SUPPORT_SAT
    pDemod->dvbss2ScanMode = 0;
#endif

    result = cxdref_demod_ChipID (pDemod, &(pDemod->chipId));
    if (result != CXDREF_RESULT_OK) {
        pDemod->state = CXDREF_DEMOD_STATE_INVALID;
        CXDREF_TRACE_RETURN (result);
    }

    switch (pDemod->chipId) {
    case CXDREF_DEMOD_CHIP_ID_CXD2856:
    case CXDREF_DEMOD_CHIP_ID_CXD2857:
    case CXDREF_DEMOD_CHIP_ID_CXD2878:
    case CXDREF_DEMOD_CHIP_ID_CXD2879:
    case CXDREF_DEMOD_CHIP_ID_CXD6802:
    case CXDREF_DEMOD_CHIP_ID_CXD2878A:
        break;

    default:
        pDemod->state = CXDREF_DEMOD_STATE_INVALID;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
        pDemod->atscCoreDisable = 1;
    }

    result = XtoSL (pDemod);
    if (result != CXDREF_RESULT_OK) {
        pDemod->state = CXDREF_DEMOD_STATE_INVALID;
        CXDREF_TRACE_RETURN (result);
    }

    pDemod->state = CXDREF_DEMOD_STATE_SLEEP;

    result = cxdref_demod_TunerI2cEnable (pDemod, pDemod->tunerI2cConfig);
    if (result != CXDREF_RESULT_OK) {
        pDemod->state = CXDREF_DEMOD_STATE_INVALID;
        CXDREF_TRACE_RETURN (result);
    }

    result = loadConfigMemory (pDemod);
    if (result != CXDREF_RESULT_OK) {
        pDemod->state = CXDREF_DEMOD_STATE_INVALID;
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_Sleep (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_Sleep");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state == CXDREF_DEMOD_STATE_SHUTDOWN) {
        result = SDtoSL (pDemod);
        if (result != CXDREF_RESULT_OK) {
            pDemod->state = CXDREF_DEMOD_STATE_INVALID;
            CXDREF_TRACE_RETURN (result);
        }

        pDemod->state = CXDREF_DEMOD_STATE_SLEEP;

        result = loadConfigMemory (pDemod);
        if (result != CXDREF_RESULT_OK) {
            pDemod->state = CXDREF_DEMOD_STATE_INVALID;
            CXDREF_TRACE_RETURN (result);
        }
    }
    else if (pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) {
        switch (pDemod->system) {
        case CXDREF_DTV_SYSTEM_DVBT:
            result = cxdref_demod_dvbt_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_DVBT2:
            result = cxdref_demod_dvbt2_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_DVBC:
            result = cxdref_demod_dvbc_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_DVBC2:
            result = cxdref_demod_dvbc2_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_DVBS:
        case CXDREF_DTV_SYSTEM_DVBS2:
        case CXDREF_DTV_SYSTEM_ANY:
            result = cxdref_demod_dvbs_s2_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_ISDBT:
            result = cxdref_demod_isdbt_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_ISDBC:
            result = cxdref_demod_isdbc_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_ISDBS:
            result = cxdref_demod_isdbs_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_ISDBS3:
            result = cxdref_demod_isdbs3_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_J83B:
            result = cxdref_demod_j83b_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_ATSC:
            result = cxdref_demod_atsc_Sleep (pDemod);
            break;

        case CXDREF_DTV_SYSTEM_ATSC3:
            result = cxdref_demod_atsc3_Sleep (pDemod);
            break;

        default:
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (result != CXDREF_RESULT_OK) {
            pDemod->state = CXDREF_DEMOD_STATE_INVALID;
            CXDREF_TRACE_RETURN (result);
        }
    }
    else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP) {
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->state = CXDREF_DEMOD_STATE_SLEEP;
    pDemod->system = CXDREF_DTV_SYSTEM_UNKNOWN;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_Shutdown (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_Shutdown");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) {
        result = cxdref_demod_Sleep (pDemod);
        if (result != CXDREF_RESULT_OK) {
            pDemod->state = CXDREF_DEMOD_STATE_INVALID;
            CXDREF_TRACE_RETURN (result);
        }

        result = SLtoSD (pDemod);
        if (result != CXDREF_RESULT_OK) {
            pDemod->state = CXDREF_DEMOD_STATE_INVALID;
            CXDREF_TRACE_RETURN (result);
        }
    }
    else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP) {
        result = SLtoSD (pDemod);
        if (result != CXDREF_RESULT_OK) {
            pDemod->state = CXDREF_DEMOD_STATE_INVALID;
            CXDREF_TRACE_RETURN (result);
        }
    }
    else if (pDemod->state == CXDREF_DEMOD_STATE_SHUTDOWN) {
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->state = CXDREF_DEMOD_STATE_SHUTDOWN;
    pDemod->system = CXDREF_DTV_SYSTEM_UNKNOWN;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_TuneEnd (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_TuneEnd");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

#ifdef CXDREF_DEMOD_SUPPORT_ATSC
    if (pDemod->system == CXDREF_DTV_SYSTEM_ATSC) {
        result = cxdref_demod_atsc_TuneEnd (pDemod);
        CXDREF_TRACE_RETURN (result);
    }
#endif

    result = cxdref_demod_SoftReset (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_SoftReset (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_SoftReset");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->state != CXDREF_DEMOD_STATE_SLEEP)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

#ifdef CXDREF_DEMOD_SUPPORT_ATSC
    if (pDemod->system == CXDREF_DTV_SYSTEM_ATSC) {
        result = cxdref_demod_atsc_SoftReset (pDemod);
        CXDREF_TRACE_RETURN (result);
    }
#endif

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFE, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_SetConfig (cxdref_demod_t * pDemod,
                                    cxdref_demod_config_id_t configId,
                                    int32_t value)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_SetConfig");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->state != CXDREF_DEMOD_STATE_SLEEP)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch (configId) {
    case CXDREF_DEMOD_CONFIG_PARALLEL_SEL:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId) && (value == 2)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xC4, (uint8_t) (value ? 0x00 : 0x80), 0x80);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xE4, (uint8_t) ((value == 2) ? 0x01 : 0x00), 0x01);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_SER_DATA_ON_MSB:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xC4, (uint8_t) (value ? 0x08 : 0x00), 0x08);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_OUTPUT_SEL_MSB:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xC4, (uint8_t) (value ? 0x00 : 0x10), 0x10);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TSVALID_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xC5, (uint8_t) (value ? 0x00 : 0x02), 0x02);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TSSYNC_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xC5, (uint8_t) (value ? 0x00 : 0x04), 0x04);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TSERR_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xCB, (uint8_t) (value ? 0x00 : 0x01), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_LATCH_ON_POSEDGE:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xC5, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TSCLK_CONT:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        pDemod->serialTSClockModeContinuous = (uint8_t) (value ? 0x01 : 0x00);
        break;

    case CXDREF_DEMOD_CONFIG_TSCLK_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00,  0xC6, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x60,  0x52, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TSVALID_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xC8, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TSERR_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xC9, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_PARALLEL_TSCLK_MANUAL:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0xFF)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->parallelTSClkManualSetting = (uint8_t) value;

        break;

    case CXDREF_DEMOD_CONFIG_TS_PACKET_GAP:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 7)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xD6, (uint8_t)value, 0x07);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        break;

    case CXDREF_DEMOD_CONFIG_TS_BACKWARDS_COMPATIBLE:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        pDemod->isTSBackwardsCompatibleMode = (uint8_t) (value ? 1 : 0);

        break;

    case CXDREF_DEMOD_CONFIG_PWM_VALUE:
        if ((value < 0) || (value > 0x1000)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xB7, (uint8_t)(value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        {
            uint8_t data[2];
            data[0] = (uint8_t) (((uint16_t)value >> 8) & 0x1F);
            data[1] = (uint8_t) ((uint16_t)value & 0xFF);

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xB2, data[0], 0x1F);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0xB3, data[1], 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_TSCLK_CURRENT:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0x95, (uint8_t) (value & 0x03), 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TS_CURRENT:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        {
            uint8_t data = ((value & 0x03) << 2) | ((value & 0x03) << 4);

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0x95, data, 0x3C);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }

        {
            uint8_t data = (value & 0x03) | ((value & 0x03) << 2) | ((value & 0x03) << 4) | ((value & 0x03) << 6);

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0x96, data, 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0x97, data, 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_GPIO0_CURRENT:
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0x86, (uint8_t) (value & 0x03), 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_GPIO1_CURRENT:
        if (cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0x86, (uint8_t) ((value & 0x03) << 2), 0x0C) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        break;

    case CXDREF_DEMOD_CONFIG_GPIO2_CURRENT:
        if (cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0x86, (uint8_t) ((value & 0x03) << 4), 0x30) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        break;

    case CXDREF_DEMOD_CONFIG_CHBOND:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < CXDREF_DEMOD_CHBOND_CONFIG_DISABLE) || (value > CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        switch (value) {
        case CXDREF_DEMOD_CHBOND_CONFIG_DISABLE:
        case CXDREF_DEMOD_CHBOND_CONFIG_ISDBC:
        case CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN:
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0xC0, (uint8_t)value, 0x03);
            break;
        case CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB:
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0xC0, 0x00, 0x03);
            break;
        }
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        pDemod->chbondConfig = (cxdref_demod_chbond_config_t)value;
        break;

    case CXDREF_DEMOD_CONFIG_CHBOND_STREAMIN:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 7)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        pDemod->chbondStreamIn = (uint8_t)value;
        break;

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
    
    case CXDREF_DEMOD_CONFIG_TERR_CABLE_TS_SERIAL_CLK_FREQ:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 5)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->serialTSClkFreqTerrCable = (cxdref_demod_serial_ts_clk_t) value;
        break;

    case CXDREF_DEMOD_CONFIG_TERR_CABLE_TS_2BIT_PARALLEL_CLK_FREQ:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 1) || (value > 2)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        pDemod->twoBitParallelTSClkFreqTerrCable = (cxdref_demod_2bit_parallel_ts_clk_t) value;
        break;

    case CXDREF_DEMOD_CONFIG_TUNER_OPTIMIZE:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        pDemod->tunerOptimize = (cxdref_demod_tuner_optimize_t)value;

        break;

    case CXDREF_DEMOD_CONFIG_IFAGCNEG:
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x10, 0xCB, (uint8_t) (value ? 0x40 : 0x00), 0x40);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (!pDemod->atscCoreDisable) {
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVR, 0xA3, 0x54, (uint8_t) (value ? 0x80 : 0x00), 0x80);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }

        break;

    case CXDREF_DEMOD_CONFIG_IFAGC_ADC_FS:
        {
            uint8_t data;

            if (value == 0) {
                data = 0x50;
            }
            else if (value == 1) {
                data = 0x39;
            }
            else if (value == 2) {
                data = 0x28;
            }
            else {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x10, 0xCD, data, 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }

        if (!pDemod->atscCoreDisable) {
            uint8_t data[2];

            if (value == 0) {
                data[0] = 0x58;
                data[1] = 0x17;
            }
            else if (value == 1) {
                data[0] = 0xF6;
                data[1] = 0x07;
            }
            else if (value == 2) {
                data[0] = 0xAC;
                data[1] = 0x07;
            }
            else {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVR, 0xA3, 0x50, data[0], 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVR, 0xA3, 0x52, data[1], 0x70);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }

        break;

    case CXDREF_DEMOD_CONFIG_SPECTRUM_INV:
        pDemod->confSense = value ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL;
        break;

    case CXDREF_DEMOD_CONFIG_RF_SPECTRUM_INV:
        pDemod->rfSpectrumSense = value ? CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV : CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL;
        break;

    case CXDREF_DEMOD_CONFIG_TERR_BLINDTUNE_DVBT2_FIRST:
        pDemod->blindTuneDvbt2First = (uint8_t)(value ? 1 : 0);
        break;

    case CXDREF_DEMOD_CONFIG_DVBT_BERN_PERIOD:
        if ((value < 0) || (value > 31)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x10, 0x60, (uint8_t) (value & 0x1F), 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBC_BERN_PERIOD:
        if ((value < 0) || (value > 31)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x40, 0x60, (uint8_t) (value & 0x1F), 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT_VBER_PERIOD:
        if ((value < 0) || (value > 7)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x10, 0x6F, (uint8_t) (value & 0x07), 0x07);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT2C2_BBER_MES:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x20, 0x72, (uint8_t) (value & 0x0F), 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT2C2_LBER_MES:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x20, 0x6F, (uint8_t) (value & 0x0F), 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT_PER_MES:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x10, 0x5C, (uint8_t) (value & 0x0F), 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBC_PER_MES:
        if ((value < 0) || (value > 31)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x40, 0x5C, (uint8_t) (value & 0x1F), 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT2C2_PER_MES:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x24, 0xDC, (uint8_t) (value & 0x0F), 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBT_BERPER_PERIOD:
        {
            uint8_t data[2];

            data[0] = (uint8_t)((value & 0x00007F00) >> 8);
            data[1] = (uint8_t)(value & 0x000000FF);

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x60, 0x5B, data[0], 0x7F);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x60, 0x5C, data[1], 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC_RSERR_BKLEN:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0xFFFFFF)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        if (pDemod->atscCoreDisable) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits(pDemod, pDemod->i2cAddressSLVR, 0xA2, 0xF2, (uint8_t)(value & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        result = cxdref_demod_SetAndSaveRegisterBits(pDemod, pDemod->i2cAddressSLVR, 0xA2, 0xF3, (uint8_t)((value >> 8) & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        result = cxdref_demod_SetAndSaveRegisterBits(pDemod, pDemod->i2cAddressSLVR, 0xA2, 0xF4, (uint8_t)((value >> 16) & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_BBER_MES:
        {
            uint8_t data[2];

            if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
            }

            data[0] = (uint8_t)((value & 0xFF00) >> 8);
            data[1] = (uint8_t)(value & 0x00FF);
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x94, 0x4A, data[0], 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x94, 0x4B, data[1], 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_LBER_MES:
        {
            uint8_t data[2];

            if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
            }

            data[0] = (uint8_t)((value & 0xFF00) >> 8);
            data[1] = (uint8_t)(value & 0x00FF);
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x94, 0x30, data[0], 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x94, 0x31, data[1], 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT_AVEBER_PERIOD_TYPE:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x10, 0x84, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT_AVEBER_PERIOD_TIME:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (value <= 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        {
            uint32_t reg = ((375 * value) + 256) / 512;

            if (reg > 0x7FF) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x10, 0x82, (uint8_t) ((reg >> 8) & 0x07), 0x07);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x10, 0x83, (uint8_t) (reg & 0xFF), 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT2_AVEBER_PERIOD_TYPE:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x20, 0x7C, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBT2_AVEBER_PERIOD_TIME:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (value <= 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        {
            uint32_t reg = ((375 * value) + 256) / 512;

            if (reg > 0x7FF) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x20, 0x7A, (uint8_t) ((reg >> 8) & 0x07), 0x07);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x20, 0x7B, (uint8_t) (reg & 0xFF), 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TYPE_0:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x7C, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TIME_0:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        if (value <= 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        {
            uint32_t reg = ((375 * value) + 256) / 512;

            if (reg > 0x7FF) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x7A, (uint8_t) ((reg >> 8) & 0x07), 0x07);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x7B, (uint8_t) (reg & 0xFF), 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TYPE_1:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x80, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TIME_1:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        if (value <= 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        {
            uint32_t reg = ((375 * value) + 256) / 512;

            if (reg > 0x7FF) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x7E, (uint8_t) ((reg >> 8) & 0x07), 0x07);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x7F, (uint8_t) (reg & 0xFF), 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TYPE_2:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x84, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TIME_2:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        if (value <= 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        {
            uint32_t reg = ((375 * value) + 256) / 512;

            if (reg > 0x7FF) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x82, (uint8_t) ((reg >> 8) & 0x07), 0x07);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x83, (uint8_t) (reg & 0xFF), 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TYPE_3:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x88, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN(result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TIME_3:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        if (value <= 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        {
            uint32_t reg = ((375 * value) + 256) / 512;

            if (reg > 0x7FF) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x86, (uint8_t) ((reg >> 8) & 0x07), 0x07);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x91, 0x87, (uint8_t) (reg & 0xFF), 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN(result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_AVESNR_PERIOD_TIME:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (value <= 0) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        {
            uint32_t reg = ((375 * value) + 256) / 512;

            if (reg > 0x7FF) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
                result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0x48, (uint8_t) ((reg >> 8) & 0x07), 0x07);
                if (result != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN(result);
                }

                result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0x49, (uint8_t) (reg & 0xFF), 0xFF);
                if (result != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN(result);
                }
            } else if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
                result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0x50, (uint8_t) ((reg >> 8) & 0x07), 0x07);
                if (result != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN(result);
                }

                result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0x51, (uint8_t) (reg & 0xFF), 0xFF);
                if (result != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN(result);
                }
            } else {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_GPIO_EWS_FLAG:
        {
            uint8_t data;

            if (value == 0) {
                data = 0x01;
            }
            else if (value == 1) {
                data = 0x80;
            }
            else if (value == 2) {
                data = 0x81;
            }
            else {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x63, 0x3B, data, 0xFF);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBC_TSMF_HEADER_NULL:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x49, 0x36, (uint8_t)(value ? 0x00 : 0x04), 0x04);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBC_NULL_REPLACED_TS_TSVALID_LOW:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x49, 0x36, (uint8_t)(value ? 0x02 : 0x00), 0x02);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_OUTPUT_DVBC2:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < CXDREF_DEMOD_OUTPUT_DVBC2_TS) || (value > CXDREF_DEMOD_OUTPUT_DVBC2_TLV)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->dvbc2Output = (cxdref_demod_output_dvbc2_t) value;

        break;

    case CXDREF_DEMOD_CONFIG_OUTPUT_ATSC3:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < CXDREF_DEMOD_OUTPUT_ATSC3_ALP) || (value > CXDREF_DEMOD_OUTPUT_ATSC3_BBP)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->atsc3Output = (cxdref_demod_output_atsc3_t) value;

        break;

    case CXDREF_DEMOD_CONFIG_OUTPUT_ISDBC_CHBOND:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        if ((value < CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TS_TLV_AUTO) || (value > CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TLV)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        {
            uint8_t data[2];

            if (value == CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TS_TLV_AUTO) {
                data[0] = 0x01;
                data[1] = 0x01;
            } else if (value == CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TS) {
                data[0] = 0x01;
                data[1] = 0x00;
            } else if (value == CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TLV) {
                data[0] = 0x00;
                data[1] = 0x01;
            } else {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x03, 0xB0, (uint8_t) data[0], 0x01);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x03, 0x16, (uint8_t) data[1], 0x01);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }

        pDemod->isdbcChBondOutput = (cxdref_demod_output_isdbc_chbond_t) value;

        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_KOREAN_MODE:
        if (!CXDREF_DEMOD_CHIP_ID_2878A_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT,
            0x9E, 0xE9, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_PLPID_PACKET_SPLP:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT,
            0x95, 0x23, (uint8_t) (value & 0x03), 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AUTO_SPECTRUM_INV:
        pDemod->atsc3AutoSpectrumInv = value ? 1 : 0;
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_CW_DETECTION:
        pDemod->atsc3CWDetection = value ? 1 : 0;
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_AUTO_PLP_ID_SPLP:
        pDemod->atsc3AutoPLPID_SPLP = value ? 1 : 0;
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_GPIO_EAS_PN_STATE:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x99, 0xAE, (uint8_t) (value & 0x0F), 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC3_GPIO_EAS_PN_TRANS:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x99, 0xB0, (uint8_t) (value & 0x03), 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ATSC_UNLOCK_DETECTION:
        pDemod->atscUnlockDetection = value ? 1 : 0;
        break;

    case CXDREF_DEMOD_CONFIG_ATSC_AUTO_SIGNAL_CHECK_OFF:
        pDemod->atscAutoSignalCheckOff = value ? 1 : 0;
        break;

    case CXDREF_DEMOD_CONFIG_ATSC_NO_SIGNAL_THRESH:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0xFFFFFF)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->atscNoSignalThresh = value;
        break;

    case CXDREF_DEMOD_CONFIG_ATSC_SIGNAL_THRESH:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0xFFFFFF)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->atscSignalThresh = value;
        break;

#endif

#ifdef CXDREF_DEMOD_SUPPORT_SAT

    case CXDREF_DEMOD_CONFIG_SAT_TS_SERIAL_CLK_FREQ:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 5)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->serialTSClkFreqSat = (cxdref_demod_serial_ts_clk_t) value;
        break;

    case CXDREF_DEMOD_CONFIG_SAT_TS_2BIT_PARALLEL_CLK_FREQ:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 1) || (value > 2)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        pDemod->twoBitParallelTSClkFreqSat = (cxdref_demod_2bit_parallel_ts_clk_t) value;
        break;

    case CXDREF_DEMOD_CONFIG_SAT_TUNER_IQ_SENSE_INV:
        pDemod->satTunerIqSense = value ? CXDREF_DEMOD_SAT_IQ_SENSE_INV : CXDREF_DEMOD_SAT_IQ_SENSE_NORMAL;
        break;

    case CXDREF_DEMOD_CONFIG_SAT_SINGLECABLE_SPECTRUM_INV:
        pDemod->isSinglecableSpectrumInv = value ? 1 : 0;
        break;

    case CXDREF_DEMOD_CONFIG_SAT_IFAGCNEG:
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA0, 0xB9, (uint8_t) (value? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBSS2_BER_PER_MES:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA0, 0xBA, (uint8_t)value, 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBS2_BER_FER_MES:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA0, 0xBC, (uint8_t)value, 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBS_VBER_MES:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA0, 0xBB, (uint8_t)value, 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBS2_LBER_MES:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA0, 0x7A, (uint8_t)value, 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_DVBSS2_BLINDSCAN_POWER_SMOOTH:
        if ((value < 0) || (value > 7)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        pDemod->dvbss2PowerSmooth = (uint8_t)value;
        break;

    case CXDREF_DEMOD_CONFIG_DVBSS2_BLINDSCAN_VERSION:
        if ((value < CXDREF_DEMOD_DVBSS2_BLINDSCAN_VERSION1) || (value > CXDREF_DEMOD_DVBSS2_BLINDSCAN_VERSION2)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        pDemod->dvbss2BlindScanVersion = (cxdref_demod_dvbss2_blindscan_version_t)value;
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS_BERNUMCONF:
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xC0, 0xA6, (uint8_t)(value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS_BER_PERIOD1:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xC0, 0xB1, (uint8_t)value, 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS_BER_PERIOD2:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xC0, 0xB2, (uint8_t)value, 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS_BER_PERIODT:
        if ((value < 0) || (value > 7)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xC0, 0xB0, (uint8_t)value, 0x07);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS3_LBER_MES1:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xD0, 0xD0, (uint8_t)value, 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS3_LBER_MES2:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xD0, 0xD1, (uint8_t)value, 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS3_BER_FER_MES1:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xD0, 0xB3, (uint8_t)value, 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS3_BER_FER_MES2:
        if ((value < 0) || (value > 15)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }
        
        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xD0, 0xB4, (uint8_t)value, 0x0F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_OUTPUT_ISDBS3:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < CXDREF_DEMOD_OUTPUT_ISDBS3_TLV) || (value > CXDREF_DEMOD_OUTPUT_ISDBS3_TLV_DIV_TS)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId) && (value == CXDREF_DEMOD_OUTPUT_ISDBS3_TLV_DIV_TS)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        pDemod->isdbs3Output = (cxdref_demod_output_isdbs3_t) value;

        break;

    case CXDREF_DEMOD_CONFIG_ISDBS_LOWCN_HOLD:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x54, (uint8_t)(value ? 0x11 : 0x00), 0x11);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS_LOWCN_THRESH_H:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x50, (uint8_t)((value >> 8) & 0x1F), 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x51, (uint8_t)(value & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        break;

    case CXDREF_DEMOD_CONFIG_ISDBS_LOWCN_THRESH_L:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x52, (uint8_t)((value >> 8) & 0x1F), 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x53, (uint8_t)(value & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        break;

    case CXDREF_DEMOD_CONFIG_ISDBS3_LOWCN_HOLD:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x5B, (uint8_t)(value ? 0x11 : 0x00), 0x11);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS3_LOWCN_THRESH_H:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x55, (uint8_t)((value >> 16) & 0x01), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x56, (uint8_t)((value >> 8) & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x57, (uint8_t)(value & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ISDBS3_LOWCN_THRESH_L:
        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x58, (uint8_t)((value >> 16) & 0x01), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x59, (uint8_t)((value >> 8) & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0xA1, 0x5A, (uint8_t)(value & 0xFF), 0xFF);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

#endif

#ifdef CXDREF_DEMOD_SUPPORT_TLV

        case CXDREF_DEMOD_CONFIG_TLV_PARALLEL_SEL:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC1, (uint8_t) (value ? 0x00 : 0x80), 0x80);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xCF, (uint8_t) ((value == 2) ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLV_SER_DATA_ON_MSB:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC1, (uint8_t) (value ? 0x08 : 0x00), 0x08);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLV_OUTPUT_SEL_MSB:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC1, (uint8_t) (value ? 0x00 : 0x10), 0x10);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLVVALID_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC2, (uint8_t) (value ? 0x00 : 0x02), 0x02);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLVSYNC_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC2, (uint8_t) (value ? 0x00 : 0x04), 0x04);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLVERR_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC8, (uint8_t) (value ? 0x00 : 0x01), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLV_LATCH_ON_POSEDGE:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC2, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLVCLK_CONT:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        pDemod->serialTLVClockModeContinuous = (uint8_t) (value ? 0x01 : 0x00);
        break;

    case CXDREF_DEMOD_CONFIG_TLVCLK_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01,  0xC3, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLVVALID_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC5, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLVERR_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC6, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLVVALID_MASK_IN_ERRNULL:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x01, 0xC8, (uint8_t) (value ? 0x08 : 0x00), 0x08);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TLV_NULL_FILTER:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xEE, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_TERR_CABLE_TLV_SERIAL_CLK_FREQ:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 5)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->serialTLVClkFreqTerrCable = (cxdref_demod_serial_ts_clk_t) value;
        break;

    case CXDREF_DEMOD_CONFIG_SAT_TLV_SERIAL_CLK_FREQ:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 5)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->serialTLVClkFreqSat = (cxdref_demod_serial_ts_clk_t) value;
        break;

    case CXDREF_DEMOD_CONFIG_TERR_CABLE_TLV_2BIT_PARALLEL_CLK_FREQ:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 1) || (value > 2)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->twoBitParallelTLVClkFreqTerrCable = (cxdref_demod_2bit_parallel_ts_clk_t) value;
        break;

    case CXDREF_DEMOD_CONFIG_SAT_TLV_2BIT_PARALLEL_CLK_FREQ:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 1) || (value > 2)) {
            CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_RANGE);
        }

        pDemod->twoBitParallelTLVClkFreqSat = (cxdref_demod_2bit_parallel_ts_clk_t) value;
        break;

#endif

#ifdef CXDREF_DEMOD_SUPPORT_ALP

    case CXDREF_DEMOD_CONFIG_ALP_PARALLEL_SEL:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC1, (uint8_t) (value ? 0x00 : 0x80), 0x80);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALP_SER_DATA_ON_MSB:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC1, (uint8_t) (value ? 0x08 : 0x00), 0x08);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALP_OUTPUT_SEL_MSB:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC1, (uint8_t) (value ? 0x00 : 0x10), 0x10);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALP_SYNC_LENGTH:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC1, (uint8_t) (value ? 0x40 : 0x00), 0x40);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALPVALID_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC2, (uint8_t) (value ? 0x00 : 0x02), 0x02);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALPSYNC_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC2, (uint8_t) (value ? 0x00 : 0x04), 0x04);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALPERR_ACTIVE_HI:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC8, (uint8_t) (value ? 0x00 : 0x01), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALP_LATCH_ON_POSEDGE:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC2, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALPCLK_CONT:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        pDemod->serialALPClockModeContinuous = (uint8_t) (value ? 0x01 : 0x00);
        break;

    case CXDREF_DEMOD_CONFIG_ALPCLK_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02,  0xC3, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALPVALID_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC5, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALPERR_MASK:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if ((value < 0) || (value > 0x1F)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_RANGE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xC6, (uint8_t) value, 0x1F);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    case CXDREF_DEMOD_CONFIG_ALP_VALIDCLK_IN_GAP:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x02, 0xEB, (uint8_t) (value ? 0x02 : 0x00), 0x02);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

#endif

    case CXDREF_DEMOD_CONFIG_BBP_LATCH_ON_POSEDGE:
        if (pDemod->state != CXDREF_DEMOD_STATE_SLEEP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }

        if (!CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x9D, 0xF2, (uint8_t) (value ? 0x01 : 0x00), 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    CXDREF_TRACE_RETURN (result);
}

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
cxdref_result_t cxdref_demod_SetIFFreqConfig (cxdref_demod_t * pDemod,
                                          cxdref_demod_iffreq_config_t * pIffreqConfig)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_SetIFFreqConfig");

    if ((!pDemod) || (!pIffreqConfig)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pDemod->iffreqConfig = *pIffreqConfig;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
#endif

cxdref_result_t cxdref_demod_TunerI2cEnable (cxdref_demod_t * pDemod, cxdref_demod_tuner_i2c_config_t tunerI2cConfig)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_TunerI2cEnable");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0x1A,
        (uint8_t) ((tunerI2cConfig == CXDREF_DEMOD_TUNER_I2C_CONFIG_DISABLE) ? 0x00 : 0x01), 0xFF);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    pDemod->tunerI2cConfig = tunerI2cConfig;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_I2cRepeaterEnable (cxdref_demod_t * pDemod, uint8_t enable)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_I2cRepeaterEnable");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->tunerI2cConfig == CXDREF_DEMOD_TUNER_I2C_CONFIG_REPEATER) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x08, (uint8_t) (enable ? 0x01 : 0x00)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_GPIOSetConfig (cxdref_demod_t * pDemod,
                                        cxdref_demod_gpio_pin_t pin,
                                        uint8_t enable,
                                        cxdref_demod_gpio_mode_t mode)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_GPIOSetConfig");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) &&
        (pDemod->state != CXDREF_DEMOD_STATE_SHUTDOWN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pin > CXDREF_DEMOD_GPIO_PIN_TSDATA7) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (mode == CXDREF_DEMOD_GPIO_MODE_TS_OUTPUT) {
        if ((pin == CXDREF_DEMOD_GPIO_PIN_GPIO0) || (pin == CXDREF_DEMOD_GPIO_PIN_GPIO1)) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }
    }

    switch (pin) {
    case CXDREF_DEMOD_GPIO_PIN_GPIO0:
    case CXDREF_DEMOD_GPIO_PIN_GPIO1:
    case CXDREF_DEMOD_GPIO_PIN_GPIO2:
        {
            uint8_t gpioModeSelAddr = 0;
            uint8_t gpioBitSel = 0;
            uint8_t enableHiZ = 0;

            gpioModeSelAddr = (uint8_t)(0xA3 + ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_GPIO0));
            gpioBitSel = (uint8_t)(1 << ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_GPIO0));

            if (cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, gpioModeSelAddr, (uint8_t)mode, 0x0F) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (mode == CXDREF_DEMOD_GPIO_MODE_INPUT) {
                enableHiZ = 0x07;
            }
            else {
                enableHiZ = enable ? 0x00 : 0x07;
            }

            if (cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0x82, enableHiZ, gpioBitSel) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
        break;

    case CXDREF_DEMOD_GPIO_PIN_TSDATA0:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA1:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA2:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA3:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA4:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA5:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA6:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA7:
        {
            uint8_t gpioModeSelAddr = 0;
            uint8_t gpioModeSelBitHigh = 0;
            uint8_t gpioBitSel = 0;
            uint8_t enableHiZ = 0;

            gpioModeSelAddr = (uint8_t)(0xB3 + ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_TSDATA0) / 2);
            gpioModeSelBitHigh = ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_TSDATA0) % 2;
            gpioBitSel = (uint8_t)(1 << ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_TSDATA0));

            if (gpioModeSelBitHigh) {
                if (cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, gpioModeSelAddr, (uint8_t) ((unsigned int)mode << 4), 0xF0) != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
                }
            } else {
                if (cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, gpioModeSelAddr, (uint8_t)mode, 0x0F) != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
                }
            }

            if (mode == CXDREF_DEMOD_GPIO_MODE_INPUT) {
                enableHiZ = 0xFF;
            } else if (mode == CXDREF_DEMOD_GPIO_MODE_TS_OUTPUT) {
                enableHiZ = 0xFF;
            } else {
                enableHiZ = enable ? 0x00 : 0xFF;
            }

            result = cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVT, 0x00, 0x81, enableHiZ, gpioBitSel);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_GPIORead (cxdref_demod_t * pDemod,
                                   cxdref_demod_gpio_pin_t pin,
                                   uint8_t * pValue)
{
    uint8_t rdata = 0x00;

    CXDREF_TRACE_ENTER ("cxdref_demod_GPIORead");

    if ((!pDemod) || (!pValue)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) &&
        (pDemod->state != CXDREF_DEMOD_STATE_SHUTDOWN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pin > CXDREF_DEMOD_GPIO_PIN_TSDATA7) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    switch (pin) {
    case CXDREF_DEMOD_GPIO_PIN_GPIO0:
    case CXDREF_DEMOD_GPIO_PIN_GPIO1:
    case CXDREF_DEMOD_GPIO_PIN_GPIO2:
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xA0, &rdata, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pValue = (rdata & (0x01 << ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_GPIO0))) ? 1 : 0;
        break;

    case CXDREF_DEMOD_GPIO_PIN_TSDATA0:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA1:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA2:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA3:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA4:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA5:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA6:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA7:
        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xB0, &rdata, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        *pValue = (rdata & (0x01 << ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_TSDATA0))) ? 1 : 0;
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_GPIOWrite (cxdref_demod_t * pDemod,
                                    cxdref_demod_gpio_pin_t pin,
                                    uint8_t value)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_GPIOWrite");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) &&
        (pDemod->state != CXDREF_DEMOD_STATE_SHUTDOWN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    switch (pin) {
    case CXDREF_DEMOD_GPIO_PIN_GPIO0:
    case CXDREF_DEMOD_GPIO_PIN_GPIO1:
    case CXDREF_DEMOD_GPIO_PIN_GPIO2:
        if (cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0xA2, (uint8_t) (value ? 0x07 : 0x00),
            (uint8_t) (0x01 << ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_GPIO0))) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        break;

    case CXDREF_DEMOD_GPIO_PIN_TSDATA0:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA1:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA2:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA3:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA4:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA5:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA6:
    case CXDREF_DEMOD_GPIO_PIN_TSDATA7:
        if (cxdref_demod_SetAndSaveRegisterBits (pDemod, pDemod->i2cAddressSLVX, 0x00, 0xB2, (uint8_t) (value ? 0xFF : 0x00),
            (uint8_t) (0x01 << ((int)pin - (int)CXDREF_DEMOD_GPIO_PIN_TSDATA0))) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_ChipID (cxdref_demod_t * pDemod,
                                 cxdref_demod_chip_id_t * pChipId)
{
    uint8_t data[2];
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_ChipID");

    if ((!pDemod) || (!pChipId)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xFB, &data[0], 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0xFD, &data[1], 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pChipId = (cxdref_demod_chip_id_t) (((data[0] & 0x03) << 8) | data[1]);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
cxdref_result_t cxdref_demod_terr_cable_monitor_InternalDigitalAGCOut (cxdref_demod_t * pDemod,
                                                                   uint16_t * pDigitalAGCOut)
{
    uint8_t data[2];
    CXDREF_TRACE_ENTER ("cxdref_demod_terr_cable_monitor_InternalDigitalAGCOut");

    if ((!pDemod) || (!pDigitalAGCOut)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (!CXDREF_DTV_SYSTEM_IS_TERR_CABLE(pDemod->system)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system == CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x11) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6D, &data[0], 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pDigitalAGCOut = (uint16_t)(((uint32_t)(data[0] & 0x3F) << 8) | data[1]);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
#endif

cxdref_result_t cxdref_demod_SetAndSaveRegisterBits (cxdref_demod_t * pDemod,
                                                 uint8_t slaveAddress,
                                                 uint8_t bank,
                                                 uint8_t registerAddress,
                                                 uint8_t value,
                                                 uint8_t bitMask)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_SetAndSaveRegisterBits");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) &&
        (pDemod->state != CXDREF_DEMOD_STATE_SHUTDOWN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = setRegisterBitsForConfig (pDemod, slaveAddress, bank, registerAddress, value, bitMask);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = setConfigMemory (pDemod, slaveAddress, bank, registerAddress, value, bitMask);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
cxdref_result_t cxdref_demod_terr_cable_SetScanMode (cxdref_demod_t * pDemod, cxdref_dtv_system_t system, uint8_t scanModeEnabled)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_terr_cable_SetScanMode");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((system == CXDREF_DTV_SYSTEM_DVBC) || (system == CXDREF_DTV_SYSTEM_ISDBC) || (system == CXDREF_DTV_SYSTEM_J83B)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            uint8_t data = scanModeEnabled ? 0x20 : 0x00;

            if (cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x86, data, 0x20) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
    }

    pDemod->scanMode = scanModeEnabled;

    CXDREF_TRACE_RETURN (result);
}
#endif

typedef struct {
    uint8_t serialClkMode;
    uint8_t serialDutyMode;
    uint8_t tsClkPeriod;
    uint8_t clkSelTSIf;
} cxdref_demod_ts_clk_configuration_t;

cxdref_result_t cxdref_demod_SetTSClockModeAndFreq (cxdref_demod_t * pDemod, cxdref_dtv_system_t system)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t serialTS = 0;
    uint8_t twoBitParallel = 0;
    uint8_t backwardsCompatible = 0;
    cxdref_demod_ts_clk_configuration_t tsClkConfiguration;

    const cxdref_demod_ts_clk_configuration_t serialTSClkSettings [2][6] =
    {{ 
        {      3,          1,            8,             0        },
        {      3,          1,            8,             1        },
        {      3,          1,            8,             2        },
        {      0,          2,            16,            0        },
        {      0,          2,            16,            1        },
        {      0,          2,            16,            2        }
    },
    {
        {      1,          1,            8,             0        },
        {      1,          1,            8,             1        },
        {      1,          1,            8,             2        },
        {      2,          2,            16,            0        },
        {      2,          2,            16,            1        },
        {      2,          2,            16,            2        } 
    }};

    const cxdref_demod_ts_clk_configuration_t parallelTSClkSetting =
    {
               0,          0,            8,             0
    };
    
    const cxdref_demod_ts_clk_configuration_t backwardsCompatibleSerialTSClkSetting [2] =
    {
        {      3,          1,            8,             1        },
        {      1,          1,            8,             1        }
    };

    const cxdref_demod_ts_clk_configuration_t backwardsCompatibleParallelTSClkSetting =
    {
               0,          0,            8,             1
    };

    const cxdref_demod_ts_clk_configuration_t twoBitParallelTSClkSetting [3] =
    {
        {       0,         0,            8,             0,       },
        {       0,         0,            8,             1,       },
        {       0,         0,            8,             2,       }
    };

    CXDREF_TRACE_ENTER ("cxdref_demod_SetTSClockModeAndFreq");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC4, &serialTS, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (!(serialTS & 0x80) && CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE4, &twoBitParallel, 1);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    {
        uint8_t tsRateCtrlOff = 0;
        uint8_t tsInOff = 0;
        uint8_t tsClkManaulOn = 0;

        switch (system) {
        case CXDREF_DTV_SYSTEM_DVBT:
        case CXDREF_DTV_SYSTEM_DVBT2:
        case CXDREF_DTV_SYSTEM_DVBC:
        case CXDREF_DTV_SYSTEM_DVBC2:
        case CXDREF_DTV_SYSTEM_DVBS:
        case CXDREF_DTV_SYSTEM_DVBS2:
            if (pDemod->isTSBackwardsCompatibleMode) {
                backwardsCompatible = 1;
                tsRateCtrlOff = 1;
                tsInOff = 1;
            } else {
                backwardsCompatible = 0;
                tsRateCtrlOff = 0;
                tsInOff = 0;
            }
            break;
        case CXDREF_DTV_SYSTEM_ISDBT:
        case CXDREF_DTV_SYSTEM_ISDBC:
        case CXDREF_DTV_SYSTEM_ISDBS:
        case CXDREF_DTV_SYSTEM_ATSC:
            backwardsCompatible = 0;
            tsRateCtrlOff = 1;
            tsInOff = 0;
            break;
        case CXDREF_DTV_SYSTEM_ISDBS3:
            backwardsCompatible = 0;
            tsRateCtrlOff = 1;
            tsInOff = 1;
            break;
        case CXDREF_DTV_SYSTEM_J83B:
            backwardsCompatible = 0;
            tsRateCtrlOff = 0;
            tsInOff = 0;
            break;
        case CXDREF_DTV_SYSTEM_ATSC3:
            backwardsCompatible = 0;
            tsRateCtrlOff = 0;
            tsInOff = 1;
            break;
        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }

        if (!(serialTS & 0x80) && pDemod->parallelTSClkManualSetting) {
            tsClkManaulOn = 1;
            tsRateCtrlOff = 0;
        }


#ifdef CXDREF_DEMOD_SUPPORT_SAT
        if ((system == CXDREF_DTV_SYSTEM_ISDBS3) && (pDemod->isdbs3Output == CXDREF_DEMOD_OUTPUT_ISDBS3_TLV_DIV_TS)) {
            tsClkManaulOn = 0;
            tsRateCtrlOff = 0;
            tsInOff = 1;
        }
#endif
#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
        if ((system == CXDREF_DTV_SYSTEM_ATSC3) && (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_ALP_DIV_TS)) {
            tsClkManaulOn = 1;
            tsRateCtrlOff = 0;
            tsInOff = 1;
        }
#endif

        result = cxdref_i2c_SetRegisterBits(pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD3, tsRateCtrlOff, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_i2c_SetRegisterBits(pDemod->pI2c, pDemod->i2cAddressSLVT, 0xDE, tsInOff, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_i2c_SetRegisterBits(pDemod->pI2c, pDemod->i2cAddressSLVT, 0xDA, tsClkManaulOn, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (backwardsCompatible) {
        if (serialTS & 0x80) {
            tsClkConfiguration = backwardsCompatibleSerialTSClkSetting[pDemod->serialTSClockModeContinuous];
        }
        else {
            tsClkConfiguration = backwardsCompatibleParallelTSClkSetting;

            tsClkConfiguration.tsClkPeriod = (uint8_t)(pDemod->parallelTSClkManualSetting ? pDemod->parallelTSClkManualSetting : 0x08);
        }
    }
    else if (serialTS & 0x80) {
#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
        if (CXDREF_DTV_SYSTEM_IS_TERR_CABLE (system)) {
            if ((system == CXDREF_DTV_SYSTEM_ATSC3) && (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_ALP_DIV_TS)) {
                tsClkConfiguration = serialTSClkSettings[pDemod->serialTSClockModeContinuous][CXDREF_DEMOD_SERIAL_TS_CLK_MID_FULL];
            } else if (pDemod->serialTSClkFreqTerrCable == CXDREF_DEMOD_SERIAL_TS_CLK_HIGH_FULL) {
                if ((system == CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ISDBC)) {
                    tsClkConfiguration = serialTSClkSettings[pDemod->serialTSClockModeContinuous][CXDREF_DEMOD_SERIAL_TS_CLK_HIGH_FULL];
                } else {
                    tsClkConfiguration = serialTSClkSettings[pDemod->serialTSClockModeContinuous][CXDREF_DEMOD_SERIAL_TS_CLK_MID_FULL];
                }
            } else {
                tsClkConfiguration = serialTSClkSettings[pDemod->serialTSClockModeContinuous][pDemod->serialTSClkFreqTerrCable];
            }
        } else
#endif
#ifdef CXDREF_DEMOD_SUPPORT_SAT
        if (CXDREF_DTV_SYSTEM_IS_SAT (system)) {
            tsClkConfiguration = serialTSClkSettings[pDemod->serialTSClockModeContinuous][pDemod->serialTSClkFreqSat];
        } else
#endif
        {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }
    }
    else {
        if (twoBitParallel & 0x1) {
#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
            if (CXDREF_DTV_SYSTEM_IS_TERR_CABLE (system)) {
                tsClkConfiguration = twoBitParallelTSClkSetting[pDemod->twoBitParallelTSClkFreqTerrCable];
            } else
#endif
#ifdef CXDREF_DEMOD_SUPPORT_SAT
            if (CXDREF_DTV_SYSTEM_IS_SAT (system)) {
                tsClkConfiguration = twoBitParallelTSClkSetting[pDemod->twoBitParallelTSClkFreqSat];
            } else
#endif
            {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
            }
        } else {
            tsClkConfiguration = parallelTSClkSetting;

            tsClkConfiguration.tsClkPeriod = (uint8_t)(pDemod->parallelTSClkManualSetting ? pDemod->parallelTSClkManualSetting : 0x08);

#ifdef CXDREF_DEMOD_SUPPORT_SAT
            if (system == CXDREF_DTV_SYSTEM_ISDBS3) {
                tsClkConfiguration.clkSelTSIf = 1;
            }
#endif
        }

#ifdef CXDREF_DEMOD_SUPPORT_SAT
        if ((system == CXDREF_DTV_SYSTEM_ISDBS3) && (pDemod->isdbs3Output == CXDREF_DEMOD_OUTPUT_ISDBS3_TLV_DIV_TS)) {
            tsClkConfiguration.tsClkPeriod = 0x04;
        }
#endif
#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
        if ((system == CXDREF_DTV_SYSTEM_ATSC3) && (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_ALP_DIV_TS)) {
            tsClkConfiguration.clkSelTSIf = 1;
            tsClkConfiguration.tsClkPeriod = 0x08;
        }
#endif
    }

    if (serialTS & 0x80) {
        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC4, tsClkConfiguration.serialClkMode, 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD1, tsClkConfiguration.serialDutyMode, 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD9, tsClkConfiguration.tsClkPeriod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, tsClkConfiguration.clkSelTSIf);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (((serialTS & 0x80) || (twoBitParallel & 0x01)) && ((system == CXDREF_DTV_SYSTEM_ISDBS3) || (system == CXDREF_DTV_SYSTEM_ATSC3)
        || ((system == CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ISDBC)))) {
        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE7, 0x00, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE7, 0x01, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    {
        uint8_t data = backwardsCompatible ? 0x00 : 0x01;

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x66, data, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x40) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x66, data, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
#ifdef CXDREF_DEMOD_SUPPORT_SAT
        {
            uint8_t data = 0x01;
            if ((system == CXDREF_DTV_SYSTEM_ISDBS3) && (pDemod->isdbs3Output == CXDREF_DEMOD_OUTPUT_ISDBS3_TS)) {
                data = 0x00;
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x8B, data) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            uint8_t data = 0x00;
            if ((system == CXDREF_DTV_SYSTEM_ISDBS3) && (pDemod->isdbs3Output == CXDREF_DEMOD_OUTPUT_ISDBS3_TLV_DIV_TS)) {
                data = 0x01;
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE3, data) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
#endif

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE

        {
            uint8_t data = 0x60;
            uint8_t data2 = 0x00;
            if ((system == CXDREF_DTV_SYSTEM_ATSC3) && (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_ALP_DIV_TS)) {
                data = 0x5C;
                data2 = 0x01;
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x95) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x11, data) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE7, data2) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
#endif
    }

    CXDREF_TRACE_RETURN (result);
}

#ifdef CXDREF_DEMOD_SUPPORT_TLV
typedef struct {
    uint8_t serialClkMode;
    uint8_t serialDutyMode;
    uint8_t clkSelTLVIf;
    uint8_t inputPeriod;
} cxdref_demod_tlv_clk_configuration_t;

cxdref_result_t cxdref_demod_SetTLVClockModeAndFreq (cxdref_demod_t * pDemod, cxdref_dtv_system_t system)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t serialTLV = 0;
    uint8_t twoBitParallel = 0;
    cxdref_demod_tlv_clk_configuration_t tlvClkConfiguration;

    const cxdref_demod_tlv_clk_configuration_t serialTLVClkSettings [2][6] =
    {{
        {       3,               1,                 0,                  3       },
        {       3,               1,                 1,                  4       },
        {       3,               1,                 2,                  5       },
        {       0,               2,                 0,                  3       },
        {       0,               2,                 1,                  4       },
        {       0,               2,                 2,                  5       }
    },
    {
        {       1,               1,                 0,                  3       },
        {       1,               1,                 1,                  4       },
        {       1,               1,                 2,                  5       },
        {       2,               2,                 0,                  3       },
        {       2,               2,                 1,                  4       },
        {       2,               2,                 2,                  5       } 
    }};

    const cxdref_demod_tlv_clk_configuration_t parallelTLVClkSetting =
    {
                0,               0,                 1,                  4          
    };

    const cxdref_demod_tlv_clk_configuration_t twoBitParallelTLVClkSetting [3] =
    {
        {       0,               0,                 0,                  3       },
        {       0,               0,                 1,                  4       },
        {       0,               0,                 2,                  5       }
    };

    CXDREF_TRACE_ENTER ("cxdref_demod_SetTLVClockModeAndFreq");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, &serialTLV, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xCF, &twoBitParallel, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (((system == CXDREF_DTV_SYSTEM_ISDBS3) || (system == CXDREF_DTV_SYSTEM_ISDBC)) && ((serialTLV & 0x80) || (twoBitParallel & 0x01))) {
        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE7, 0x00, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE7, 0x01, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (serialTLV & 0x80) {
        if (CXDREF_DTV_SYSTEM_IS_TERR_CABLE (system)) {
            tlvClkConfiguration = serialTLVClkSettings[pDemod->serialTLVClockModeContinuous][pDemod->serialTLVClkFreqTerrCable];
        }
        else if (CXDREF_DTV_SYSTEM_IS_SAT (system)) {
            tlvClkConfiguration = serialTLVClkSettings[pDemod->serialTLVClockModeContinuous][pDemod->serialTLVClkFreqSat];
        }
        else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }
    }
    else {
        if (twoBitParallel & 0x1) {
            if (CXDREF_DTV_SYSTEM_IS_TERR_CABLE (system)) {
                tlvClkConfiguration = twoBitParallelTLVClkSetting[pDemod->twoBitParallelTLVClkFreqTerrCable];
            }
            else if (CXDREF_DTV_SYSTEM_IS_SAT (system)) {
                tlvClkConfiguration = twoBitParallelTLVClkSetting[pDemod->twoBitParallelTLVClkFreqSat];
            }
            else {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
            }
        } else {
            tlvClkConfiguration = parallelTLVClkSetting;
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x56) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x83, tlvClkConfiguration.inputPeriod, 0x07);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (serialTLV & 0x80) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, tlvClkConfiguration.serialClkMode, 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xCC, tlvClkConfiguration.serialDutyMode, 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, tlvClkConfiguration.clkSelTLVIf);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0xD0) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x8B, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (result);
}
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ALP
typedef struct {
    uint8_t serialClkMode;
    uint8_t clkSelALPIf;
} cxdref_demod_alp_clk_configuration_t;

cxdref_result_t cxdref_demod_SetALPClockModeAndFreq (cxdref_demod_t * pDemod, cxdref_dtv_system_t system)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t serialALP = 0;
    cxdref_demod_alp_clk_configuration_t alpClkConfiguration;

    const cxdref_demod_alp_clk_configuration_t serialALPClkSettings [2] =
    {
        {       3,                1       },
        {       1,                1       }
    };

    const cxdref_demod_alp_clk_configuration_t parallelALPClkSetting =
    {
                0,                1,
    };

    CXDREF_TRACE_ENTER ("cxdref_demod_SetALPClockModeAndFreq");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, &serialALP, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (serialALP & 0x80) {
        alpClkConfiguration = serialALPClkSettings[pDemod->serialALPClockModeContinuous];
    }
    else {
        alpClkConfiguration = parallelALPClkSetting;
    }

    if (serialALP & 0x80) {
        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, alpClkConfiguration.serialClkMode, 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, alpClkConfiguration.clkSelALPIf);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x32, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if ((system == CXDREF_DTV_SYSTEM_ATSC3) && (serialALP & 0x80)) {
        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE7, 0x00, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE7, 0x01, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x95) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x11, 0x60) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}
#endif

cxdref_result_t cxdref_demod_SetTSDataPinHiZ (cxdref_demod_t * pDemod, uint8_t enable)
{
    uint8_t data = 0;
    uint8_t tsDataMask = 0;
    uint8_t tsDataMaskAll = 0;
    uint8_t isIsdbCTSTLVAutoOutput = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_SetTSDataPinHiZ");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
    if ((pDemod->system == CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ISDBC)
        && (pDemod->isdbcChBondOutput == CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TS_TLV_AUTO)) {
        isIsdbCTSTLVAutoOutput = 1;
    }

    if ((pDemod->system == CXDREF_DTV_SYSTEM_ATSC3) && (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_BBP)) {
        tsDataMaskAll = 0x01;
        goto register_write;
    }
#endif

    if ((pDemod->system == CXDREF_DTV_SYSTEM_ATSC3) && (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB)) {
        tsDataMaskAll = 0x01;
        goto register_write;
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

#ifdef CXDREF_DEMOD_SUPPORT_TLV
    if (((data & 0x03) == 0x01) || isIsdbCTSTLVAutoOutput) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        switch (data & 0x88) {
        case 0x80:
            tsDataMask = 0x01;
            break;
        case 0x88:
            tsDataMask = 0x80;
            break;
        case 0x08:
        case 0x00:
        default:
            if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xCF, &data, 1) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (data & 0x01) {
                if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xEA, &data, 1) != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
                }

                tsDataMask = (0x01 << (data & 0x07));
                tsDataMask |= (0x01 << ((data >> 4) & 0x07));
            } else {
                tsDataMask = 0xFF;
            }
            break;
        }

        tsDataMaskAll |= tsDataMask;
    }
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ALP
    if ((data & 0x03) == 0x02) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC1, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        switch (data & 0x88) {
        case 0x80:
            tsDataMask = 0x01;
            break;
        case 0x88:
            tsDataMask = 0x80;
            break;
        case 0x08:
        case 0x00:
        default:
            tsDataMask = 0xFF;
            break;
        }

        tsDataMaskAll |= tsDataMask;
    }
#endif

    if (((data & 0x03) == 0x00) || isIsdbCTSTLVAutoOutput) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC4, &data, 1) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        switch (data & 0x88) {
        case 0x80:
            tsDataMask = 0x01;
            break;
        case 0x88:
            tsDataMask = 0x80;
            break;
        case 0x08:
        case 0x00:
        default:
            if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
                if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
                }

                if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE4, &data, 1) != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
                }

                if (data & 0x01) {
                    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE5, &data, 1) != CXDREF_RESULT_OK) {
                        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
                    }

                    tsDataMask = (0x01 << (data & 0x07));
                    tsDataMask |= (0x01 << ((data >> 4) & 0x07));
                } else {
                    tsDataMask = 0xFF;
                }
            } else if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
                tsDataMask = 0xFF;
            } else {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
            }
            break;
        }

        tsDataMaskAll |= tsDataMask;
    }

register_write:

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x81, (uint8_t) (enable ? 0xFF : 0x00), tsDataMaskAll) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_SetStreamOutput (cxdref_demod_t * pDemod, uint8_t enable)
{
    uint8_t data = 0;
    uint8_t isIsdbCTSTLVAutoOutput = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_SetStreamOutput");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
    if ((pDemod->system == CXDREF_DTV_SYSTEM_ISDBC) && (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ISDBC)
        && (pDemod->isdbcChBondOutput == CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TS_TLV_AUTO)) {
        isIsdbCTSTLVAutoOutput = 1;
    }
#endif

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

#ifdef CXDREF_DEMOD_SUPPORT_TLV
    if (((data & 0x03) == 0x01) || isIsdbCTSTLVAutoOutput) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC0, enable ? 0x00 : 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ALP
    if ((data & 0x03) == 0x02) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC0, enable ? 0x00 : 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }
#endif

    if (((data & 0x03) == 0x00) || isIsdbCTSTLVAutoOutput) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xC3, enable ? 0x00 : 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t XtoSL(cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("XtoSL");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x02, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        CXDREF_SLEEP (4);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x1D, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        switch (pDemod->xtalFreq) {
        case CXDREF_DEMOD_XTAL_16000KHz:
        case CXDREF_DEMOD_XTAL_24000KHz:
        case CXDREF_DEMOD_XTAL_32000KHz:
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x14, (uint8_t)pDemod->xtalFreq) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
            break;

        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        CXDREF_SLEEP (1);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x50, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->atscCoreDisable) {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x90, 0x00) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        CXDREF_SLEEP (1);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x10, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->atscCoreDisable) {
            CXDREF_SLEEP (1);
        } else {
            CXDREF_SLEEP (21);
        }

        if (pDemod->chipId == CXDREF_DEMOD_CHIP_ID_CXD6802) {
            const uint8_t data[] = {0x00, 0x00, 0x00, 0x00};

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9C) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        } else {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x95) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
			
        	/* customization from original reference code */
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x23, 0x33) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }
    } else if (CXDREF_DEMOD_CHIP_ID_2856_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x10, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x18, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x28, 0x13) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x1D, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        switch (pDemod->xtalFreq) {
        case CXDREF_DEMOD_XTAL_16000KHz:
        case CXDREF_DEMOD_XTAL_24000KHz:
        case CXDREF_DEMOD_XTAL_32000KHz:
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x14, (uint8_t)pDemod->xtalFreq) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
            break;

        default:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x1C, 0x03) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        CXDREF_SLEEP (4);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x50, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        CXDREF_SLEEP (1);

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x10, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        CXDREF_SLEEP (1);
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t SLtoSD(cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("SLtoSD");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x10, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x1C, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t SDtoSL(cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("SDtoSL");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x1C, 0x03) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_SLEEP (4);

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x10, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->atscCoreDisable) {
        CXDREF_SLEEP (1);
    } else {
        CXDREF_SLEEP (21);
    }

    if (pDemod->chipId == CXDREF_DEMOD_CHIP_ID_CXD6802) {
        const uint8_t data[] = {0x00, 0x00, 0x00, 0x00};

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9C) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x10, data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else if (CXDREF_DEMOD_CHIP_ID_2878_FAMILY (pDemod->chipId)) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x95) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

    	/* customization from original reference code */
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x23, 0x33) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t loadConfigMemory (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t i;

    CXDREF_TRACE_ENTER ("loadConfigMemory");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for (i = 0; i < pDemod->configMemoryLastEntry; i++) {
        result = setRegisterBitsForConfig (pDemod,
                                           pDemod->configMemory[i].slaveAddress,
                                           pDemod->configMemory[i].bank,
                                           pDemod->configMemory[i].registerAddress,
                                           pDemod->configMemory[i].value,
                                           pDemod->configMemory[i].bitMask);

        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t setConfigMemory (cxdref_demod_t * pDemod,
                                                 uint8_t slaveAddress,
                                                 uint8_t bank,
                                                 uint8_t registerAddress,
                                                 uint8_t value,
                                                 uint8_t bitMask)
{
    uint8_t i;
    uint8_t valueStored = 0;

    CXDREF_TRACE_ENTER ("setConfigMemory");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for (i = 0; i < pDemod->configMemoryLastEntry; i++){
        if ((valueStored == 0) &&
            (pDemod->configMemory[i].slaveAddress == slaveAddress) &&
            (pDemod->configMemory[i].bank == bank) &&
            (pDemod->configMemory[i].registerAddress == registerAddress)) {

            pDemod->configMemory[i].value &= ~bitMask;
            pDemod->configMemory[i].value |= (value & bitMask);

            pDemod->configMemory[i].bitMask |= bitMask;

            valueStored = 1;
        }
    }

    if (valueStored == 0) {
        if (pDemod->configMemoryLastEntry < CXDREF_DEMOD_MAX_CONFIG_MEMORY_COUNT) {
            pDemod->configMemory[pDemod->configMemoryLastEntry].slaveAddress = slaveAddress;
            pDemod->configMemory[pDemod->configMemoryLastEntry].bank = bank;
            pDemod->configMemory[pDemod->configMemoryLastEntry].registerAddress = registerAddress;
            pDemod->configMemory[pDemod->configMemoryLastEntry].value = (value & bitMask);
            pDemod->configMemory[pDemod->configMemoryLastEntry].bitMask = bitMask;
            pDemod->configMemoryLastEntry++;
        }
        else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OVERFLOW);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t setRegisterBitsForConfig (cxdref_demod_t * pDemod,
                                               uint8_t slaveAddress,
                                               uint8_t bank,
                                               uint8_t registerAddress,
                                               uint8_t value,
                                               uint8_t bitMask)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("setRegisterBitsForConfig");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (slaveAddress == pDemod->i2cAddressSLVR) {
#ifdef CXDREF_DEMOD_SUPPORT_ATSC
        result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, bank, registerAddress, value, bitMask);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
#else
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
#endif
    } else {
        result = pDemod->pI2c->WriteOneRegister (pDemod->pI2c, slaveAddress, 0x00, bank);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_i2c_SetRegisterBits (pDemod->pI2c, slaveAddress, registerAddress, value, bitMask);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
