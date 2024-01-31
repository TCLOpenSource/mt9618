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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <media/dvb_frontend.h>

#include "cxd_dmd_i2c_device.h"
#include "cxd_dmd_monitor_bit_error.h"

#include "cxdref_integ.h"
#include "cxdref_demod_driver_version.h"

#include "cxdref_demod_atsc_monitor.h"
#include "cxdref_demod_atsc3_monitor.h"
#include "cxdref_demod_dvbc_monitor.h"
#include "cxdref_demod_dvbs_s2_monitor.h"
#include "cxdref_demod_dvbt_monitor.h"
#include "cxdref_demod_dvbt2_monitor.h"
#include "cxdref_demod_isdbc_monitor.h"
#include "cxdref_demod_isdbs_monitor.h"
#include "cxdref_demod_isdbs3_monitor.h"
#include "cxdref_demod_isdbt_monitor.h"
#include "cxdref_demod_j83b_monitor.h"

#include "cxdref_integ_atsc.h"
#include "cxdref_integ_atsc3.h"
#include "cxdref_integ_dvbc.h"
#include "cxdref_integ_dvbs_s2.h"
#include "cxdref_integ_dvbt.h"
#include "cxdref_integ_dvbt2.h"
#include "cxdref_integ_isdbc.h"
#include "cxdref_integ_isdbs.h"
#include "cxdref_integ_isdbs3.h"
#include "cxdref_integ_isdbt.h"
#include "cxdref_integ_j83b.h"

#include "cxd_dmd_priv.h"
#include "cxd_dmd.h"

static int convert_return_result (cxdref_result_t result);
static int convert_bandwidth_param (u32 bandwidth_hz);

int cxd_dmd_common_initialize_i2c_device (struct i2c_client *client)
{
    cxdref_result_t result = 0;
    struct cxd_dmd_state *state;
    struct cxd_dmd_driver_instance *driver;
    
    if (!client)
        return -EINVAL;
    
    state = i2c_get_clientdata(client);
    if (!state)
        return -EINVAL;
    
    driver = &state->driver;
    
    result = cxd_dmd_i2c_device_initialize (&driver->i2c_device, state->i2c);
    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);

    result = cxd_dmd_i2c_device_create_i2c (&driver->i2c, &driver->i2c_device);
    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);
    
    return 0;
}

int cxd_dmd_common_finalize_i2c_device (struct i2c_client *client)
{
    cxdref_result_t result = 0;
    struct cxd_dmd_state *state;
    struct cxd_dmd_driver_instance *driver;
    
    if (!client)
        return -EINVAL;
    
    state = i2c_get_clientdata(client);
    if (!state)
        return -EINVAL;
    
    driver = &state->driver;
    
    result = cxd_dmd_i2c_device_finalize(&driver->i2c_device);
    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);
    
    return 0;
}

int cxd_dmd_common_get_dmd_chip_id (struct i2c_client *client)
{
	cxdref_result_t result = 0;
    struct cxd_dmd_state *state;
    struct cxd_dmd_driver_instance *driver;
    cxdref_i2c_t *i2c;
    cxdref_demod_chip_id_t chip_id;
    u8 i2c_address_slvx;
    u8 data[2];
    
    if (!client)
        return -EINVAL;
    
    state = i2c_get_clientdata(client);
    if (!state)
        return -EINVAL;
    
    driver = &state->driver;
    i2c = &driver->i2c;
    i2c_address_slvx = state->config.i2c_address_slvx;
    chip_id = CXDREF_DEMOD_CHIP_ID_UNKNOWN;

    
    result = i2c->WriteOneRegister (i2c, i2c_address_slvx, 0x00, 0x00);
    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);
    
    result = i2c->ReadRegister (i2c, i2c_address_slvx, 0xFB, &data[0], 1);
    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);

    result = i2c->ReadRegister (i2c, i2c_address_slvx, 0xFD, &data[1], 1);
    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);

    chip_id = (cxdref_demod_chip_id_t) (((data[0] & 0x03) << 8) | data[1]);

    pr_info(" Chip ID (Demod)      | Chip ID             | 0x%X \n",chip_id);
    pr_info("---------------------- --------------------- -------------------\n");
    
    switch (chip_id) {
    case CXDREF_DEMOD_CHIP_ID_CXD2856:
    case CXDREF_DEMOD_CHIP_ID_CXD2857:
    case CXDREF_DEMOD_CHIP_ID_CXD2878:
    case CXDREF_DEMOD_CHIP_ID_CXD2879:
    case CXDREF_DEMOD_CHIP_ID_CXD6802:
    case CXDREF_DEMOD_CHIP_ID_CXD2878A:
        break;
    default:
        return -ENODEV;
    }
    
    return 0;
}

int cxd_dmd_common_initialize_demod (struct dvb_frontend *fe, u8 enable_lnbc)
{
	cxdref_result_t result = 0;
    cxdref_demod_create_param_t create_param;
    u8 enable_tuner = 0;
    struct cxd_dmd_state *state;
    struct cxd_dmd_driver_instance *driver;

    if (!fe || !fe->demodulator_priv)
        return -EINVAL;

    state = fe->demodulator_priv;
    driver = &state->driver;
    if (state->config.xtal_freq_khz == 16000)
        create_param.xtalFreq = CXDREF_DEMOD_XTAL_16000KHz;
    else if (state->config.xtal_freq_khz == 24000)
        create_param.xtalFreq = CXDREF_DEMOD_XTAL_24000KHz;
    else if (state->config.xtal_freq_khz == 32000)
        create_param.xtalFreq = CXDREF_DEMOD_XTAL_32000KHz;
    else
        return -EINVAL;

    create_param.i2cAddressSLVT = state->config.i2c_address_slvx - 4;
    create_param.tunerI2cConfig = CXDREF_DEMOD_TUNER_I2C_CONFIG_DISABLE;
    create_param.atscCoreDisable = 0;

    pr_info(":---------------------- --------------------- -------------------\n");
    pr_info(":----------------------    INIT (Demod)       -------------------\n");
    pr_info(":---------------------- --------------------- -------------------\n");
    pr_info(":                      | xtal Frequency      | %d   \n",create_param.xtalFreq);
    pr_info(":                      | i2c Address SLVT    | 0x%X \n",create_param.i2cAddressSLVT);
    pr_info(":---------------------- --------------------- -------------------\n");

    result = cxdref_integ_Create (&driver->integ, &driver->demod, &create_param, &driver->i2c,
        (enable_tuner ? &driver->tuner : NULL), (enable_lnbc ? &driver->lnbc : NULL));
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_integ_Create failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_integ_Initialize (&driver->integ);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_integ_Initialize failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_IFAGCNEG, 0);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_TSCLK_CURRENT, 2);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_TS_CURRENT, 2);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_ATSC3_PLPID_PACKET_SPLP, 0);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_OUTPUT_ATSC3, CXDREF_DEMOD_OUTPUT_ATSC3_ALP);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_ALP_PARALLEL_SEL, 1);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_ALP_SER_DATA_ON_MSB, 0);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_ALP_SYNC_LENGTH, 1);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_PARALLEL_SEL, 1);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_SER_DATA_ON_MSB, 0);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
    
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_ATSC3_AUTO_PLP_ID_SPLP, 0);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
/*
    result = cxdref_demod_SetConfig (&driver->demod,
        CXDREF_DEMOD_CONFIG_RF_SPECTRUM_INV, CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV);
    if (result != CXDREF_RESULT_OK) {
        pr_err("[%s][%d]cxdref_demod_SetConfig failed result:%d\n",__func__,__LINE__,result);
        return convert_return_result(result);
    }
*/
    pr_info("[%s][%d]initialize demod success\n",__func__,__LINE__);
    return 0;
}

static int convert_return_result (cxdref_result_t result)
{
    switch (result) {
    case CXDREF_RESULT_OK:
        return 0;
    case CXDREF_RESULT_ERROR_ARG:
        return -EINVAL;
    case CXDREF_RESULT_ERROR_I2C:
        return -EIO;
    case CXDREF_RESULT_ERROR_SW_STATE:
        return -EINVAL;
    case CXDREF_RESULT_ERROR_HW_STATE:
        return -EINVAL;
    case CXDREF_RESULT_ERROR_TIMEOUT:
        return -ETIMEDOUT;
    case CXDREF_RESULT_ERROR_UNLOCK:
        return -EINVAL;
    case CXDREF_RESULT_ERROR_RANGE:
        return -EINVAL;
    case CXDREF_RESULT_ERROR_NOSUPPORT:
        return -EINVAL;
    case CXDREF_RESULT_ERROR_CANCEL:
        return -ECANCELED;
    case CXDREF_RESULT_ERROR_OTHER:
        return -EINVAL;
    case CXDREF_RESULT_ERROR_OVERFLOW:
        return -ENOMEM;
    case CXDREF_RESULT_OK_CONFIRM: 
        return -EINVAL;
    default:
        return -EINVAL;
    }
}

static int convert_bandwidth_param (u32 bandwidth_hz)
{
    switch (bandwidth_hz) {
    case 1700000:
        return CXDREF_DTV_BW_1_7_MHZ;
    case 5000000:
        return CXDREF_DTV_BW_5_MHZ;
    case 6000000:
        return CXDREF_DTV_BW_6_MHZ;
    case 7000000:
        return CXDREF_DTV_BW_7_MHZ;
    case 8000000:
        return CXDREF_DTV_BW_8_MHZ;
    case 5060000:
        return CXDREF_DTV_BW_J83B_5_06_5_36_MSPS;
    case 5360000:
        return CXDREF_DTV_BW_J83B_5_06_5_36_MSPS;
    case 5600000:
        return CXDREF_DTV_BW_J83B_5_60_MSPS;
    default:
        return CXDREF_DTV_BW_UNKNOWN;
    }
}

int cxd_dmd_common_tune (struct dvb_frontend *fe)
{
    struct cxd_dmd_state *state;
    struct dtv_frontend_properties *c;
    struct cxd_dmd_driver_instance *driver;

	cxdref_result_t result = 0;

    u32 center_freq, bandwidth, symbol_rate_ksps;
    u16 stream_id;

    if (!fe || !fe->demodulator_priv)
        return -EINVAL;
        
    state = fe->demodulator_priv;
    c = &fe->dtv_property_cache;
    driver = &state->driver;

    center_freq = c->frequency;
    bandwidth = convert_bandwidth_param(c->bandwidth_hz);
    symbol_rate_ksps = (c->symbol_rate)/1000;
    stream_id = (u16)c->stream_id;

	pr_info(":---------------------- --------------------- -------------------\n");
	pr_info(":----------------------     TUNE (Demod)      -------------------\n");
	pr_info(":---------------------- --------------------- -------------------\n");
	pr_info(":                      | delivery System(dvb)| %d \n", c->delivery_system);

	switch (c->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		{
		cxdref_dvbc_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq/1000;
		tuneParam.bandwidth = CXDREF_DTV_BW_8_MHZ;
		pr_info(":                      | frequency           | %d kHz \n", (center_freq/1000));
		pr_info(":                      | bandwidth           | 8 MHz \n");
		result = cxdref_integ_dvbc_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_DVBC_ANNEX_B:
		{
		cxdref_j83b_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq/1000;
		tuneParam.bandwidth = CXDREF_DTV_BW_J83B_5_06_5_36_MSPS;
		pr_info(":                      | frequency           |  %d kHz \n", (center_freq/1000));
		pr_info(":                      | bandwidth           |  %d MHz \n", bandwidth);
		pr_info(":                      | bandwidth           |  (only support 5060000/5360000 or 5600000) \n");
		result = cxdref_demod_j83b_Tune (&driver->demod, &tuneParam);
		if (result == CXDREF_RESULT_OK)
			result = cxdref_demod_TuneEnd (&driver->demod);
		return convert_return_result(result);
		}
	case SYS_DVBT:
		{
		cxdref_dvbt_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq/1000;
		tuneParam.bandwidth = bandwidth;
		tuneParam.profile = CXDREF_DVBT_PROFILE_HP;
		pr_info(":                      | frequency           | %d kHz \n", (center_freq/1000));
		pr_info(":                      | bandwidth           | %d MHz \n", bandwidth);
		result = cxdref_integ_dvbt_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_DSS:
		return -EINVAL;
	case SYS_DVBS:
		{
		cxdref_dvbs_s2_tune_param_t tuneParam;
		tuneParam.system = CXDREF_DTV_SYSTEM_DVBS;
		tuneParam.centerFreqKHz = center_freq;
		tuneParam.symbolRateKSps = symbol_rate_ksps;
		pr_info(":                      | frequency           | %d kHz \n", center_freq);
		pr_info(":                      | symbol rate         | %d ksps \n", symbol_rate_ksps);
		result = cxdref_integ_dvbs_s2_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_DVBS2:
		{
		cxdref_dvbs_s2_tune_param_t tuneParam;
		tuneParam.system = CXDREF_DTV_SYSTEM_DVBS2;
		tuneParam.centerFreqKHz = center_freq;
		tuneParam.symbolRateKSps = symbol_rate_ksps;
		pr_info(":                      | frequency           | %d kHz \n", center_freq);
		pr_info(":                      | symbol rate         | %d ksps \n", symbol_rate_ksps);
		result = cxdref_integ_dvbs_s2_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_DVBH:
		return -EINVAL;
	case SYS_ISDBT:
		{
		cxdref_isdbt_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq/1000;
		tuneParam.bandwidth = bandwidth;
		pr_info(":                      | frequency           | %d kHz \n", (center_freq/1000));
		pr_info(":                      | bandwidth           | %d MHz \n", bandwidth);
		result = cxdref_integ_isdbt_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_ISDBS:
		{
		cxdref_isdbs_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq;
		if (stream_id == 0x0000)
			stream_id = 0x4010;
		else
			tuneParam.tsid = stream_id;
		tuneParam.tsidType = CXDREF_ISDBS_TSID_TYPE_TSID;
		pr_info(":                      | frequency           | %d kHz \n", center_freq);
		pr_info(":                      | stream id           | %u  \n", stream_id);
		result = cxdref_integ_isdbs_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_ISDBC:
		{
		cxdref_isdbc_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq/1000;
		tuneParam.tsmfMode = CXDREF_DEMOD_ISDBC_TSMF_MODE_AUTO;
		tuneParam.tsidType = CXDREF_ISDBC_TSID_TYPE_TSID;
		tuneParam.tsid = stream_id;
		tuneParam.networkId = 0x0004;
		pr_info(":                      | frequency           | %d kHz \n", (center_freq/1000));
		pr_info(":                      | stream id           | %u  \n", stream_id);
		result = cxdref_integ_isdbc_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_ATSC:
		{
		cxdref_atsc_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq/1000;
		pr_info(":                      | frequency           | %d kHz \n", (center_freq/1000));
		result = cxdref_demod_atsc_Tune (&driver->demod, &tuneParam);
		if (result == CXDREF_RESULT_OK)
			result = cxdref_demod_TuneEnd (&driver->demod);
		return convert_return_result(result);
		}
	case SYS_ATSCMH:
		return -EINVAL;
	case SYS_DTMB:
		return -EINVAL;
	case SYS_CMMB:
		return -EINVAL;
	case SYS_DAB:
		return -EINVAL;
	case SYS_DVBT2:
		{
		cxdref_dvbt2_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq/1000;
		tuneParam.bandwidth = bandwidth;
		tuneParam.dataPlpId = stream_id;
		tuneParam.profile = CXDREF_DVBT2_PROFILE_BASE;
		tuneParam.tuneInfo = CXDREF_DEMOD_DVBT2_TUNE_INFO_OK;
		pr_info(":                      | frequency           | %d kHz \n", (center_freq/1000));
		pr_info(":                      | bandwidth           | %d MHz \n", bandwidth);
		result = cxdref_integ_dvbt2_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_TURBO:
		return -EINVAL;
	case SYS_DVBC_ANNEX_C:
		{
		cxdref_isdbc_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq/1000;
		tuneParam.tsmfMode = CXDREF_DEMOD_ISDBC_TSMF_MODE_AUTO;
		tuneParam.tsidType = CXDREF_ISDBC_TSID_TYPE_TSID;
		tuneParam.tsid = stream_id;
		tuneParam.networkId = 0x0004;
		pr_info(":                      | frequency           | %d kHz \n", (center_freq/1000));
		result = cxdref_integ_isdbc_Tune (&driver->integ, &tuneParam);
		return convert_return_result(result);
		}
	case SYS_ISDBS3:
		{
		cxdref_isdbs3_tune_param_t tuneParam;
		tuneParam.centerFreqKHz = center_freq;
		if (stream_id == 0x0000)
			tuneParam.streamid = 0xB110;
		else
			tuneParam.streamid = stream_id;
		tuneParam.streamidType = CXDREF_ISDBS3_STREAMID_TYPE_STREAMID;
		result = cxdref_integ_isdbs3_Tune (&driver->integ, &tuneParam);
		pr_info(":                      | frequency           | %d kHz \n", center_freq);
		pr_info(":                      | stream id           | %x  \n", stream_id);
		return convert_return_result(result);
		}
	case SYS_ATSC30:
		{
		cxdref_atsc3_tune_param_t tuneParam;
		u8 cnt, i;
		u64 tmp;

        tuneParam.centerFreqKHz = center_freq/1000;
        tuneParam.bandwidth = bandwidth;
        pr_info(":                      | frequency           | %d kHz \n", (center_freq/1000));
        pr_info(":                      | bandwidth (fixed)   | %d MHz \n", bandwidth);

        tmp = *((u64 *)&c->plp_array[0]);
        cnt = 0;
        for (i = 0; i < 64; i++) {
            if (tmp & ((u64)1 << i)) {
                tuneParam.plpID[cnt] = i;
                pr_info("tuneParam.plpID[%d]=%d\n", cnt, i);
                cnt++;
                if (cnt >= 4)
                    break;
            }
        }
		tuneParam.plpIDNum = cnt;
		pr_info("tuneParam.plpIDNum=%d\n", cnt);
        
        result = cxdref_demod_atsc3_SetPLPConfig (&driver->demod, tuneParam.plpIDNum, tuneParam.plpID);
        
        if (result == CXDREF_RESULT_OK) {
            result = cxdref_demod_atsc3_Tune (&driver->demod, &tuneParam);        
            if (result == CXDREF_RESULT_OK)
                result = cxdref_demod_TuneEnd (&driver->demod);
        }

        return convert_return_result(result);
        }
    default:
        return -EINVAL;
    }

    return 0;
}

int cxd_dmd_common_sleep (struct dvb_frontend *fe)
{
    cxdref_result_t result = 0;
    struct cxd_dmd_state *state;
    struct cxd_dmd_driver_instance *driver;

    if(!fe || !fe->demodulator_priv)
        return -EINVAL;

    state = fe->demodulator_priv;
    driver = &state->driver;

    result = cxdref_integ_Sleep(&driver->integ);
    if (result != CXDREF_RESULT_OK) 
        return convert_return_result(result);  
    
    return 0;
}

int cxd_dmd_common_monitor_sync_stat (struct dvb_frontend *fe, enum fe_status *status)
{
    cxdref_result_t result = 0;

    struct cxd_dmd_state *state;
    struct dtv_frontend_properties *c;
    struct cxd_dmd_driver_instance *driver;

    uint8_t arLocked = 0, tsLock = 0, earlyUnlock = 0;
    uint8_t syncState = 0, dmdLock = 0, agcLocked =0, tmccLock = 0, tstlvLock = 0;
    uint8_t vqLockState =0, agcLockState =0, tsLockState = 0, unlockDetected = 0;

    if(!fe || !fe->demodulator_priv || !status)
        return -EINVAL;

    state = fe->demodulator_priv;
    c = &fe->dtv_property_cache;
    driver = &state->driver;

    *status = FE_NONE;

	switch (c->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		result = cxdref_demod_dvbc_monitor_SyncStat (&driver->demod, &arLocked, &tsLock, &earlyUnlock);
		break;
	case SYS_DVBC_ANNEX_B:
		result = cxdref_demod_j83b_monitor_SyncStat (&driver->demod, &arLocked, &tsLock, &earlyUnlock);
		break;
	case SYS_DVBT:
		result = cxdref_demod_dvbt_monitor_SyncStat (&driver->demod, &syncState, &tsLock, &earlyUnlock);
		break;
	case SYS_DSS:
		return -EINVAL;
	case SYS_DVBS:
		result = cxdref_demod_dvbs_s2_monitor_SyncStat (&driver->demod, &tsLock);
		break;
	case SYS_DVBS2:
		result = cxdref_demod_dvbs_s2_monitor_SyncStat (&driver->demod, &tsLock);
		break;
	case SYS_DVBH:
		return -EINVAL;
	case SYS_ISDBT:
		result = cxdref_demod_isdbt_monitor_SyncStat (&driver->demod, &dmdLock, &tsLock, &earlyUnlock);
		break;
	case SYS_ISDBS:
		result = cxdref_demod_isdbs_monitor_SyncStat (&driver->demod, &agcLocked, &tsLock, &tmccLock);
		break;
	case SYS_ISDBC:
		result = cxdref_demod_isdbc_monitor_SyncStat (&driver->demod, &arLocked, &tsLock, &earlyUnlock);
		break;
	case SYS_ATSC:
		result = cxdref_demod_atsc_monitor_SyncStat (&driver->demod, &vqLockState, &agcLockState,
			&tsLockState, &unlockDetected);
		break;
	case SYS_DTMB:
		return -EINVAL;
	case SYS_CMMB:
		return -EINVAL;
	case SYS_DAB:
		return -EINVAL;
	case SYS_DVBT2:
		result = cxdref_demod_dvbt2_monitor_SyncStat (&driver->demod, &syncState, &tsLock, &earlyUnlock);
		break;
	case SYS_TURBO:
		return -EINVAL;
	case SYS_DVBC_ANNEX_C:
		result = cxdref_demod_isdbc_monitor_SyncStat (&driver->demod, &arLocked, &tsLock, &earlyUnlock);
		break;
	case SYS_ISDBS3:
		result = cxdref_demod_isdbs3_monitor_SyncStat (&driver->demod, &agcLocked, &tstlvLock, &tmccLock);
		break;
	case SYS_ATSC30:
		{
		uint8_t alpLockState[4] = {0};
		uint8_t alpLockAll = 0;
		uint8_t alpUnlockDetected = 0;
		uint8_t j = 0;
		uint8_t flag = 0;
		cxdref_atsc3_plp_list_entry_t plpList[CXDREF_ATSC3_NUM_PLP_MAX];
		uint8_t numPLPs = 0;

        result = cxdref_demod_atsc3_monitor_SignalChange(&driver->demod, &flag);
        if (result != CXDREF_RESULT_OK)
            break;
        
        result = cxdref_demod_atsc3_monitor_SyncStat (&driver->demod, &syncState, &alpLockState[0],
                                                        &alpLockAll, &alpUnlockDetected);
        if (result != CXDREF_RESULT_OK)
            break;

        if (alpLockAll==1) {
            tsLockState = 1;
        } else {
            for (j = 0; j < 4; j++) {
                if (alpLockState[j])
                    tsLockState = 1;
            }
        }
        unlockDetected = alpUnlockDetected;
		c->plpId_bitmap.bitmap = 0;
		c->llsFlag_bitmap.bitmap = 0;
		for (j = 0; j < 4; j++) {
			c->atsc3_plp_info[j].plp_id = 0xFF;
			c->atsc3_plp_info[j].plp_lock_status = FE_NONE;
		}

		if (syncState == 0x06) {
			result = cxdref_demod_atsc3_monitor_PLPList (&driver->demod, &plpList[0], &numPLPs);
			if (result != CXDREF_RESULT_OK)
				break;
		}
			
		pr_info("numPLPs=%d\n", numPLPs);
			
		for (j = 0; j < 4; j++) {
			if (j + 1 > numPLPs)
				break;
			
			c->plpId_bitmap.bitmap |= 1 << plpList[j].id;
			c->atsc3_plp_info[j].plp_id = plpList[j].id;
			if (alpLockState[j])
				c->atsc3_plp_info[j].plp_lock_status = FE_HAS_CARRIER|FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_SYNC|FE_HAS_VITERBI;
			
			if (plpList[j].lls_flg)
				c->llsFlag_bitmap.bitmap |= 1 << plpList[j].id;
			
		    pr_info("plpId_bitmap.bitmap=0x%08llx\n", c->plpId_bitmap.bitmap);
			pr_info("atsc3_plp_info[%d].plp_id=%d\n", j, c->atsc3_plp_info[j].plp_id);
			pr_info("atsc3_plp_info[%d].plp_lock_status=0x%02x\n", j, c->atsc3_plp_info[j].plp_lock_status);
			pr_info("llsFlag_bitmap.bitmap=0x%08llx\n", c->llsFlag_bitmap.bitmap);
		}
        }
        break;
    default:
        return -EINVAL;
    }

    if (result != CXDREF_RESULT_OK) {
        if (result == CXDREF_RESULT_ERROR_TIMEOUT)
            *status |= FE_TIMEDOUT;
        return convert_return_result(result);
    }

    if (earlyUnlock==1 || unlockDetected==1)
        *status |= FE_TIMEDOUT;

    if (tmccLock==1 || vqLockState==1)
        *status |= FE_HAS_CARRIER;

    if (agcLocked==1 || agcLockState==1)
        *status |= FE_HAS_SIGNAL;

    if (arLocked==1 || dmdLock==1 || syncState==0x06)
        *status |= FE_HAS_CARRIER|FE_HAS_SIGNAL;

    if (tsLock==1 || tsLockState==1 || tstlvLock==1)
        *status |= FE_HAS_CARRIER|FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_SYNC|FE_HAS_VITERBI;

    return 0;
}

int cxd_dmd_common_monitor_cnr (struct dvb_frontend *fe)
{
	cxdref_result_t result = 0;

    struct cxd_dmd_state *state;
    struct dtv_frontend_properties *c;
    struct cxd_dmd_driver_instance *driver;
    struct dtv_fe_stats *stats_cnr;

    int32_t cnr=0;
    
	if(!fe || !fe->demodulator_priv)
        return -EINVAL;
        
    state = fe->demodulator_priv;
    driver = &state->driver;
    c = &fe->dtv_property_cache;
	stats_cnr = &c->cnr;
	
	switch (c->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		result = cxdref_demod_dvbc_monitor_SNR (&driver->demod, &cnr);
		break;
	case SYS_DVBC_ANNEX_B:
		result = cxdref_demod_j83b_monitor_SNR (&driver->demod, &cnr);
		break;
	case SYS_DVBT:
		result = cxdref_demod_dvbt_monitor_SNR (&driver->demod, &cnr);
		break;
	case SYS_DSS:
		return -EINVAL;
	case SYS_DVBS:
		result = cxdref_demod_dvbs_s2_monitor_CNR (&driver->demod, &cnr);
		break;
	case SYS_DVBS2:
		result = cxdref_demod_dvbs_s2_monitor_CNR (&driver->demod, &cnr);
		break;
	case SYS_DVBH:
		return -EINVAL;
	case SYS_ISDBT:
		result = cxdref_demod_isdbt_monitor_SNR (&driver->demod, &cnr);
		break;
	case SYS_ISDBS:
		result = cxdref_demod_isdbs_monitor_CNR (&driver->demod, &cnr);
		break;
	case SYS_ISDBC:
		result = cxdref_demod_isdbc_monitor_SNR (&driver->demod, &cnr);
		break;
	case SYS_ATSC:
		result = cxdref_demod_atsc_monitor_SNR (&driver->demod, &cnr);
		break;
	case SYS_DTMB:
		return -EINVAL;
	case SYS_CMMB:
		return -EINVAL;
	case SYS_DAB:
		return -EINVAL;
	case SYS_DVBT2:
		result = cxdref_demod_dvbt2_monitor_SNR (&driver->demod, &cnr);
		break;
	case SYS_TURBO:
		return -EINVAL;
	case SYS_DVBC_ANNEX_C:
		result = cxdref_demod_isdbc_monitor_SNR (&driver->demod, &cnr);
		break;
	case SYS_ISDBS3:
		result = cxdref_demod_isdbs3_monitor_CNR (&driver->demod, &cnr);
		break;
	case SYS_ATSC30:
		result = cxdref_demod_atsc3_monitor_SNR (&driver->demod, &cnr);
		break;
	default:
		return -EINVAL;
	}

    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);

    stats_cnr->len=1;
    stats_cnr->stat[0].scale = FE_SCALE_DECIBEL;
    stats_cnr->stat[0].svalue = cnr;
    
    pr_info(": CNR                  | CNR                 |  %d x 10^-3 dB\n", cnr);
    pr_info(":---------------------- --------------------- -------------------\n");

    return 0;
}

int cxd_dmd_common_monitor_bit_error (struct dvb_frontend *fe)
{
	cxdref_result_t result = 0;

    struct cxd_dmd_state *state;
    struct dtv_frontend_properties *c;
    struct cxd_dmd_driver_instance *driver;

    struct dtv_fe_stats *stats_pre_bit_error;
    struct dtv_fe_stats *stats_pre_bit_count;
    struct dtv_fe_stats *stats_post_bit_error;
    struct dtv_fe_stats *stats_post_bit_count;
    struct dtv_fe_stats *stats_block_error;
    struct dtv_fe_stats *stats_block_count;

    uint32_t pre_bit_error_count, pre_bit_count;
    uint32_t post_bit_error_count, post_bit_count;
    uint32_t block_error_count, block_count;

	if(!fe || !fe->demodulator_priv)
		return -EINVAL;
		
	state= fe->demodulator_priv;
	driver = &state->driver;
	c = &fe->dtv_property_cache;

	stats_pre_bit_error = &c->pre_bit_error;
	stats_pre_bit_count = &c->pre_bit_count;
	stats_post_bit_error = &c->post_bit_error;
	stats_post_bit_count = &c->post_bit_count;
	stats_block_error = &c->block_error;
	stats_block_count = &c->block_count;
	
	switch (c->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		result = cxdref_demod_dvbc_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
		result = cxdref_demod_dvbc_monitor_block_error(&driver->demod, &block_error_count, &block_count);
		break;
	case SYS_DVBC_ANNEX_B:
		result = cxdref_demod_j83b_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
		result = cxdref_demod_j83b_monitor_block_error(&driver->demod, &block_error_count, &block_count);
		break;
	case SYS_DVBT:
		result = cxdref_demod_dvbt_monitor_pre_bit_error(&driver->demod, &pre_bit_error_count, &pre_bit_count);
		result = cxdref_demod_dvbt_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
		result = cxdref_demod_dvbt_monitor_block_error(&driver->demod, &block_error_count, &block_count);
		break;
	case SYS_DSS:
		return -EINVAL;
	case SYS_DVBS:
		result = cxdref_demod_dvbs_monitor_pre_bit_error(&driver->demod, &pre_bit_error_count, &pre_bit_count);
		result = cxdref_demod_dvbs_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
		result = cxdref_demod_dvbs_monitor_block_error(&driver->demod, &block_error_count, &block_count);
		break;
	case SYS_DVBS2:
		result = cxdref_demod_dvbs2_monitor_pre_bit_error(&driver->demod, &pre_bit_error_count, &pre_bit_count);
		result = cxdref_demod_dvbs2_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
		result = cxdref_demod_dvbs2_monitor_block_error(&driver->demod, &block_error_count, &block_count);
		break;
	case SYS_DVBH:
		return -EINVAL;
	case SYS_ISDBT:
		{
			uint32_t post_bit_error_count_a, post_bit_error_count_b, post_bit_error_count_c;
			uint32_t post_bit_count;

            uint32_t block_error_count_a, block_error_count_b, block_error_count_c;
            uint32_t block_count;

            pr_info(": pre_bit_error_count  | pre_bit_error_count | (Not Available) \n");
            pr_info(": pre_bit_count        | pre_bit_count       | (Not Available) \n");
            pr_info(":---------------------- --------------------- -------------------\n");

            result = cxdref_demod_isdbt_monitor_post_bit_error (&driver->demod,
                &post_bit_error_count_a, &post_bit_error_count_b,
                &post_bit_error_count_c, &post_bit_count);

            if (result != CXDREF_RESULT_OK)
                return convert_return_result(result);

            stats_post_bit_error->len=4;
            stats_post_bit_count->len=4;

            if (post_bit_error_count_a == CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID) {
                pr_info(": post_bit_error_count | Layer A             | Unused \n");
                pr_info(": post_bit__count      | Layer A             | Unused \n");
                stats_post_bit_error->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_error->stat[1].uvalue = 0;
                stats_post_bit_count->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_count->stat[1].uvalue = 0;
            } else {
                stats_post_bit_error->stat[1].scale = FE_SCALE_COUNTER;
                stats_post_bit_error->stat[1].uvalue = post_bit_error_count_a;
                stats_post_bit_count->stat[1].scale = FE_SCALE_COUNTER;
                stats_post_bit_count->stat[1].uvalue = post_bit_count;
                pr_info(": post_bit_error_count | Layer A             | %d \n",post_bit_error_count_a);
                pr_info(": post_bit_count       | Layer A             | %d \n",post_bit_count);
            }

            if (post_bit_error_count_b == CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID) {
                stats_post_bit_error->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_error->stat[2].uvalue = 0;
                stats_post_bit_count->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_count->stat[2].uvalue = 0;
                pr_info(": post_bit_error_count | Layer B             | Unused \n");
                pr_info(": post_bit_count       | Layer B             | Unused \n");
            } else{
                stats_post_bit_error->stat[2].scale = FE_SCALE_COUNTER;
                stats_post_bit_error->stat[2].uvalue = post_bit_error_count_b;
                stats_post_bit_count->stat[2].scale = FE_SCALE_COUNTER;
                stats_post_bit_count->stat[2].uvalue = post_bit_count;
                pr_info(": post_bit_error_count | Layer B             | %d \n",post_bit_error_count_b);
                pr_info(": post_bit_count       | Layer B             | %d \n",post_bit_count);
            }

            if (post_bit_error_count_c == CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID) {
                stats_post_bit_error->stat[3].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_error->stat[3].uvalue = 0;
                stats_post_bit_count->stat[3].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_count->stat[3].uvalue = 0;
                pr_info(": post_bit_error_count | Layer C             | Unused \n");
                pr_info(": post_bit_count       | Layer C             | Unused \n");
            } else {
                stats_post_bit_error->stat[3].scale = FE_SCALE_COUNTER;
                stats_post_bit_error->stat[3].uvalue = post_bit_error_count_c;
                stats_post_bit_count->stat[3].scale = FE_SCALE_COUNTER;
                stats_post_bit_count->stat[3].uvalue = post_bit_count;
                pr_info(": post_bit_error_count | Layer C             | %d \n",post_bit_error_count_c);
                pr_info(": post_bit_count       | Layer C             | %d \n",post_bit_count);
            }
            
            pr_info("---------------------- --------------------- -------------------\n");

            result = cxdref_demod_isdbt_monitor_block_error (&driver->demod,
                &block_error_count_a, &block_error_count_b,
                &block_error_count_c, &block_count);

            if (result != CXDREF_RESULT_OK)
                return convert_return_result(result);

            stats_block_error->len=4;
            stats_block_count->len=4;

            if (block_error_count_a == CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID) {
                stats_block_error->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_error->stat[1].uvalue = 0;
                stats_block_count->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_count->stat[1].uvalue = 0;
                pr_info(": error_count          | Layer A             | Unused \n");
                pr_info(": block_count          | Layer A             | Unused \n");
                pr_info(":---------------------- --------------------- -------------------\n");

            } else {
                stats_block_error->stat[1].scale = FE_SCALE_COUNTER;
                stats_block_error->stat[1].uvalue = block_error_count_a;
                stats_block_count->stat[1].scale = FE_SCALE_COUNTER;
                stats_block_count->stat[1].uvalue = block_count;
                pr_info(": error_count          | Layer A             | %d \n",block_error_count_a);
                pr_info(": block_count          | Layer A             | %d \n",block_count);
                pr_info(":---------------------- --------------------- -------------------\n");
            }

            if (block_error_count_b == CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID) {
                stats_block_error->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_error->stat[2].uvalue = 0;
                stats_block_count->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_count->stat[2].uvalue = 0;
                pr_info(": error_count          | Layer B             | Unused \n");
                pr_info(": block_count          | Layer B             | Unused \n");
                pr_info(":---------------------- --------------------- -------------------\n");
            } else {
                stats_block_error->stat[2].scale = FE_SCALE_COUNTER;
                stats_block_error->stat[2].uvalue = block_error_count_b;
                stats_block_count->stat[2].scale = FE_SCALE_COUNTER;
                stats_block_count->stat[2].uvalue = block_count;
                pr_info(": error_count          | Layer B             | %d \n",block_error_count_b);
                pr_info(": block_count          | Layer B             | %d \n",block_count);
                pr_info(":---------------------- --------------------- -------------------\n");
            }

            if (block_error_count_c == CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID) {
                stats_block_error->stat[3].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_error->stat[3].uvalue = 0;
                stats_block_count->stat[3].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_count->stat[3].uvalue = 0;
                pr_info(": error_count          | Layer C             | Unused \n");
                pr_info(": block_count          | Layer C             | Unused \n");
                pr_info(":---------------------- --------------------- -------------------\n");
            } else {
                stats_block_error->stat[3].scale = FE_SCALE_COUNTER;
                stats_block_error->stat[3].uvalue = block_error_count_c;
                stats_block_count->stat[3].scale = FE_SCALE_COUNTER;
                stats_block_count->stat[3].uvalue = block_count;
                pr_info(": error_count          | Layer C             | %d \n",block_error_count_c);
                pr_info(": block_count          | Layer C             | %d \n",block_count);
                pr_info(":---------------------- --------------------- -------------------\n");
            }

            if ((block_error_count_a != CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID) && (block_error_count_a > 0)) {
                stats_block_error->stat[1].uvalue = 1;
            } else {
                stats_block_error->stat[1].uvalue = 0;
            }
            if ((block_error_count_b != CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID) && (block_error_count_b > 0)) {
                stats_block_error->stat[2].uvalue = 1;
            } else {
                stats_block_error->stat[2].uvalue = 0;
            }
            if ((block_error_count_c != CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID) && (block_error_count_c > 0)) {
                stats_block_error->stat[3].uvalue = 1;
            } else {
                stats_block_error->stat[3].uvalue = 0;
            }
        }
        return 0;
    case SYS_ISDBS:
        {
            uint32_t post_bit_error_count_h, post_bit_count_h;
            uint32_t post_bit_error_count_l, post_bit_count_l;
            uint32_t post_bit_error_count_tmcc, post_bit_count_tmcc;

            uint32_t block_error_count_h, block_count_h;
            uint32_t block_error_count_l, block_count_l;
            result = cxdref_demod_isdbs_monitor_post_bit_error (&driver->demod,
                    &post_bit_error_count_h, &post_bit_count_h,
                    &post_bit_error_count_l, &post_bit_count_l,
                    &post_bit_error_count_tmcc, &post_bit_count_tmcc);

            if (result != CXDREF_RESULT_OK)
                return convert_return_result(result);

            stats_post_bit_error->len=4;
            stats_post_bit_count->len=4;

            if (post_bit_error_count_l == CXDREF_DEMOD_ISDBS_MONITOR_PRERSBER_INVALID) {
                pr_info(": post_bit_error_count | (Low)               | Unused \n");
                pr_info(": post_bit_count       | (Low)               | Unused \n");
                stats_post_bit_error->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_error->stat[1].uvalue = 0;
                stats_post_bit_count->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_count->stat[1].uvalue = 0;
            } else {
                stats_post_bit_error->stat[1].scale = FE_SCALE_COUNTER;
                stats_post_bit_error->stat[1].uvalue = post_bit_error_count_l;
                stats_post_bit_count->stat[1].scale = FE_SCALE_COUNTER;
                stats_post_bit_count->stat[1].uvalue = post_bit_count_l;
                pr_info(": post_bit_error_count | (Low)               | %d \n",post_bit_error_count_l);
                pr_info(": post_bit_count       | (Low)               | %d \n",post_bit_count_l);
            }

            if (post_bit_error_count_h == CXDREF_DEMOD_ISDBS_MONITOR_PRERSBER_INVALID) {
                stats_post_bit_error->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_error->stat[2].uvalue = 0;
                stats_post_bit_count->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_count->stat[2].uvalue = 0;
                pr_info(": post_bit_error_count | (High)              | Unused \n");
                pr_info(": post_bit_count       | (High)              | Unused \n");
            } else {
                stats_post_bit_error->stat[2].scale = FE_SCALE_COUNTER;
                stats_post_bit_error->stat[2].uvalue = post_bit_error_count_h;
                stats_post_bit_count->stat[2].scale = FE_SCALE_COUNTER;
                stats_post_bit_count->stat[2].uvalue = post_bit_count_h;
                pr_info(": post_bit_error_count | (High)              | %d \n",post_bit_error_count_h);
                pr_info(": post_bit_count       | (High)              | %d \n",post_bit_count_h);
            }

            if (post_bit_error_count_tmcc == CXDREF_DEMOD_ISDBS_MONITOR_PRERSBER_INVALID) {
                stats_post_bit_error->stat[3].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_error->stat[3].uvalue = 0;
                stats_post_bit_count->stat[3].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_count->stat[3].uvalue = 0;
                pr_info(": post_bit_error_count | (TMCC)              | Unused \n");
                pr_info(": post_bit_count       | (TMCC)              | Unused \n");
            } else {
                stats_post_bit_error->stat[3].scale = FE_SCALE_COUNTER;
                stats_post_bit_error->stat[3].uvalue = post_bit_error_count_tmcc;
                stats_post_bit_count->stat[3].scale = FE_SCALE_COUNTER;
                stats_post_bit_count->stat[3].uvalue = post_bit_count_tmcc;
                pr_info(": post_bit_error_count | (TMCC)              | %d \n",post_bit_error_count_tmcc);
                pr_info(": post_bit_count       | (TMCC)              | %d \n",post_bit_count_tmcc);
            }
            
            pr_info("---------------------- --------------------- -------------------\n");

            result = cxdref_demod_isdbs_monitor_block_error (&driver->demod,
                    &block_error_count_h, &block_count_h,
                    &block_error_count_l, &block_count_l);

            if (result != CXDREF_RESULT_OK)
                return convert_return_result(result);

            stats_block_error->len=3;
            stats_block_count->len=3;

            if (block_error_count_l == CXDREF_DEMOD_ISDBS_MONITOR_PER_INVALID) {
                stats_block_error->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_error->stat[1].uvalue = 0;
                stats_block_count->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_count->stat[1].uvalue = 0;
                pr_info(": error_count          | (Low)               | Unused \n");
                pr_info(": block_count          | (Low)               | Unused \n");
                pr_info(":---------------------- --------------------- -------------------\n");
            } else {
                stats_block_error->stat[1].scale = FE_SCALE_COUNTER;
                stats_block_error->stat[1].uvalue = block_error_count_l;
                stats_block_count->stat[1].scale = FE_SCALE_COUNTER;
                stats_block_count->stat[1].uvalue = block_count_l;
                pr_info(": error_count          | (Low)               | %d \n",block_error_count_l);
                pr_info(": block_count          | (Low)               | %d \n",block_count_l);
                pr_info(":---------------------- --------------------- -------------------\n");
            }

            if (block_error_count_h == CXDREF_DEMOD_ISDBS_MONITOR_PER_INVALID) {
                stats_block_error->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_error->stat[2].uvalue = 0;
                stats_block_count->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_count->stat[2].uvalue = 0;
                pr_info(": error_count          | (High)              | Unused \n");
                pr_info(": block_count          | (High)              | Unused \n");
                pr_info(":---------------------- --------------------- -------------------\n");
            } else {
                stats_block_error->stat[2].scale = FE_SCALE_COUNTER;
                stats_block_error->stat[2].uvalue = block_error_count_h;
                stats_block_count->stat[2].scale = FE_SCALE_COUNTER;
                stats_block_count->stat[2].uvalue = block_count_h;
                pr_info(": error_count          | (High)              | %d \n",block_error_count_h);
                pr_info(": block_count          | (High)              | %d \n",block_count_h);
                pr_info(":---------------------- --------------------- -------------------\n");
            }

            if ((block_error_count_l != CXDREF_DEMOD_ISDBS_MONITOR_PER_INVALID) && (block_error_count_l > 0)) {
                stats_block_error->stat[1].uvalue = 1;
            } else {
                stats_block_error->stat[1].uvalue = 0;
            }
            if ((block_error_count_h != CXDREF_DEMOD_ISDBS_MONITOR_PER_INVALID) && (block_error_count_h > 0)) {
                stats_block_error->stat[2].uvalue = 1;
            } else {
                stats_block_error->stat[2].uvalue = 0;
            }
        }
        return 0;
    case SYS_ISDBC:
        result = cxdref_demod_isdbc_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
        result = cxdref_demod_isdbc_monitor_block_error(&driver->demod, &block_error_count, &block_count);
        break;
    case SYS_ATSC:
        result = cxdref_demod_atsc_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
        result = cxdref_demod_atsc_monitor_block_error(&driver->demod, &block_error_count, &block_count);
        break;
    case SYS_DTMB:
        return -EINVAL;
    case SYS_CMMB:
        return -EINVAL;
    case SYS_DAB:
        return -EINVAL;
    case SYS_DVBT2:
        result = cxdref_demod_dvbt2_monitor_pre_bit_error(&driver->demod, &pre_bit_error_count, &pre_bit_count);
        result = cxdref_demod_dvbt2_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
        result = cxdref_demod_dvbt2_monitor_block_error(&driver->demod, &block_error_count, &block_count);
        break;
    case SYS_TURBO:
        return -EINVAL;
    case SYS_DVBC_ANNEX_C:
        result = cxdref_demod_isdbc_monitor_post_bit_error(&driver->demod, &post_bit_error_count, &post_bit_count);
        result = cxdref_demod_isdbc_monitor_block_error(&driver->demod, &block_error_count, &block_count);
        break;
    case SYS_ISDBS3:
        {
            uint32_t pre_bit_error_count_h, pre_bit_count_h;
            uint32_t pre_bit_error_count_l, pre_bit_count_l;
            uint32_t post_bit_error_count_h, post_bit_count_h;
            uint32_t post_bit_error_count_l, post_bit_count_l;
            uint32_t block_error_count_h, block_count_h;
            uint32_t block_error_count_l, block_count_l;

            result = cxdref_demod_isdbs3_monitor_pre_bit_error(&driver->demod,
                    &pre_bit_error_count_h, &pre_bit_count_h,
                    &pre_bit_error_count_l, &pre_bit_count_l);

            if (result != CXDREF_RESULT_OK)
                    return convert_return_result(result);

            stats_pre_bit_error->len=3;
            stats_pre_bit_count->len=3;

            if (pre_bit_error_count_l == CXDREF_DEMOD_ISDBS3_MONITOR_PRELDPCBER_INVALID) {
                stats_pre_bit_error->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_pre_bit_error->stat[1].uvalue = 0;
                stats_pre_bit_count->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_pre_bit_count->stat[1].uvalue = 0;
                pr_info(": pre_bit_error_count  | (Low)               | Unused \n");
                pr_info(": pre_bit_count        | (Low)               | Unused \n");
            } else {
                stats_pre_bit_error->stat[1].scale = FE_SCALE_COUNTER;
                stats_pre_bit_error->stat[1].uvalue = pre_bit_error_count_l;
                stats_pre_bit_count->stat[1].scale = FE_SCALE_COUNTER;
                stats_pre_bit_count->stat[1].uvalue = pre_bit_count_l;
                pr_info(": pre_bit_error_count  | (Low)               | %d \n",pre_bit_error_count_l);
                pr_info(": pre_bit_count        | (Low)               | %d \n",pre_bit_count_l);
            }

            if (pre_bit_error_count_h == CXDREF_DEMOD_ISDBS3_MONITOR_PRELDPCBER_INVALID) {
                stats_pre_bit_error->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_pre_bit_error->stat[2].uvalue = 0;
                stats_pre_bit_count->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_pre_bit_count->stat[2].uvalue = 0;
                pr_info(": pre_bit_error_count  | (High)              | Unused \n");
                pr_info(": pre_bit_count        | (High)              | Unused \n");
            } else {
                stats_pre_bit_error->stat[2].scale = FE_SCALE_COUNTER;
                stats_pre_bit_error->stat[2].uvalue = pre_bit_error_count_h;
                stats_pre_bit_count->stat[2].scale = FE_SCALE_COUNTER;
                stats_pre_bit_count->stat[2].uvalue = pre_bit_count_h;
                pr_info(": pre_bit_error_count  | (High)              | %d \n",pre_bit_error_count_h);
                pr_info(": pre_bit_count        | (High)              | %d \n",pre_bit_count_h);
            }
            pr_info(":---------------------- --------------------- -------------------\n");

            result = cxdref_demod_isdbs3_monitor_post_bit_error(&driver->demod,
                    &post_bit_error_count_h, &post_bit_count_h,
                    &post_bit_error_count_l, &post_bit_count_l);
            if (result != CXDREF_RESULT_OK)
                    return convert_return_result(result);

            stats_post_bit_error->len=3;
            stats_post_bit_count->len=3;

            if (post_bit_error_count_l == CXDREF_DEMOD_ISDBS3_MONITOR_PREBCHBER_INVALID) {
                stats_post_bit_error->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_error->stat[1].uvalue = 0;
                stats_post_bit_count->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_count->stat[1].uvalue = 0;
                pr_info(": post_bit_error_count | (Low)               | Unused \n");
                pr_info(": post_bit_count       | (Low)               | Unused \n");
            } else {
                stats_post_bit_error->stat[1].scale = FE_SCALE_COUNTER;
                stats_post_bit_error->stat[1].uvalue = post_bit_error_count_l;
                stats_post_bit_count->stat[1].scale = FE_SCALE_COUNTER;
                stats_post_bit_count->stat[1].uvalue = post_bit_count_l;
                pr_info(": post_bit_error_count | (Low)               | %d \n",post_bit_error_count_l);
                pr_info(": post_bit_count       | (Low)               | %d \n",post_bit_count_l);
            }

            if (post_bit_error_count_h == CXDREF_DEMOD_ISDBS3_MONITOR_PREBCHBER_INVALID) {
                stats_post_bit_error->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_error->stat[2].uvalue = 0;
                stats_post_bit_count->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_post_bit_count->stat[2].uvalue = 0;
                pr_info(": post_bit_error_count | (High)              | Unused \n");
                pr_info(": post_bit_count       | (High)              | Unused \n");
            } else {
                stats_post_bit_error->stat[2].scale = FE_SCALE_COUNTER;
                stats_post_bit_error->stat[2].uvalue = post_bit_error_count_h;
                stats_post_bit_count->stat[2].scale = FE_SCALE_COUNTER;
                stats_post_bit_count->stat[2].uvalue = post_bit_count_h;
                pr_info(": post_bit_error_count | (High)              | %d \n",post_bit_error_count_h);
                pr_info(": post_bit_count       | (High)              | %d \n",post_bit_count_h);
            }
            pr_info("---------------------- --------------------- -------------------\n");

            result = cxdref_demod_isdbs3_monitor_block_error (&driver->demod,
                &block_error_count_h, &block_count_h,
                &block_error_count_l, &block_count_l);
            if (result != CXDREF_RESULT_OK)
                    return convert_return_result(result);

            stats_block_error->len=3;
            stats_block_count->len=3;

            if (block_error_count_l == CXDREF_DEMOD_ISDBS3_MONITOR_POSTBCHFER_INVALID) {
                stats_block_error->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_error->stat[1].uvalue = 0;
                stats_block_count->stat[1].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_count->stat[1].uvalue = 0;
                pr_info(": error_count          | (Low)               | Unused \n");
                pr_info(": block_count          | (Low)               | Unused \n");
            } else {
                stats_block_error->stat[1].scale = FE_SCALE_COUNTER;
                stats_block_error->stat[1].uvalue = block_error_count_l;
                stats_block_count->stat[1].scale = FE_SCALE_COUNTER;
                stats_block_count->stat[1].uvalue = block_count_l;
                pr_info(": error_count          | (Low)               | %d \n",block_error_count_l);
                pr_info(": block_count          | (Low)               | %d \n",block_count_l);
            }

            if (block_error_count_h == CXDREF_DEMOD_ISDBS3_MONITOR_POSTBCHFER_INVALID) {
                stats_block_error->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_error->stat[2].uvalue = 0;
                stats_block_count->stat[2].scale = FE_SCALE_NOT_AVAILABLE;
                stats_block_count->stat[2].uvalue = 0;
                pr_info(": error_count          | (High)              | Unused \n");
                pr_info(": block_count          | (High)              | Unused \n");
            } else {
                stats_block_error->stat[2].scale = FE_SCALE_COUNTER;
                stats_block_error->stat[2].uvalue = block_error_count_h;
                stats_block_count->stat[2].scale = FE_SCALE_COUNTER;
                stats_block_count->stat[2].uvalue = block_count_h;
                pr_info(": error_count          | (High)              | %d \n",block_error_count_h);
                pr_info(": block_count          | (High)              | %d \n",block_count_h);
            }

            pr_info(":---------------------- --------------------- -------------------\n");

            if ((block_error_count_l != CXDREF_DEMOD_ISDBS3_MONITOR_POSTBCHFER_INVALID) && (block_error_count_l > 0)) {
                stats_block_error->stat[1].uvalue = 1;
            } else {
                stats_block_error->stat[1].uvalue = 0;
            }

            if ((block_error_count_h != CXDREF_DEMOD_ISDBS3_MONITOR_POSTBCHFER_INVALID) && (block_error_count_h > 0)) {
                stats_block_error->stat[2].uvalue = 1;
            } else {
                stats_block_error->stat[2].uvalue = 0;
            }
        }
        return 0;
    case SYS_ATSC30:
        {
            int j;
            uint32_t pre_bit_error_count_a[4], pre_bit_count_a[4];
            uint32_t post_bit_error_count_a[4], post_bit_count_a[4];
            uint32_t block_error_count_a[4], block_count_a[4];

            result = cxdref_demod_atsc3_monitor_pre_bit_error(&driver->demod, &pre_bit_error_count_a[0], &pre_bit_count_a[0]);
            if (result != CXDREF_RESULT_OK)
                return convert_return_result(result);

            stats_pre_bit_error->len=4;
            stats_pre_bit_count->len=4;

            stats_pre_bit_error->stat[0].scale = FE_SCALE_COUNTER;
            stats_pre_bit_error->stat[0].uvalue = pre_bit_error_count_a[0];
            stats_pre_bit_count->stat[0].scale = FE_SCALE_COUNTER;
            stats_pre_bit_count->stat[0].uvalue = pre_bit_count_a[0];
            pr_info(": pre_bit_error_count  | PLP ID [0]          | %d \n",pre_bit_error_count_a[0]);
            pr_info(": pre_bit_count        | PLP ID [0]          | %d \n",pre_bit_count_a[0]);
            
            for(j = 1; j <4 ; j++){
                if (pre_bit_error_count_a[j] == CXDREF_DEMOD_ATSC3_MONITOR_PRELDPCBER_INVALID) {
                    pr_info(": pre_bit_error_count  | PLP ID [%d]         | Unused \n",j);
                    pr_info(": pre_bit_count        | PLP ID [%d]         | Unused \n",j);
                } else {
                    stats_pre_bit_error->stat[j].scale = FE_SCALE_COUNTER;
                    stats_pre_bit_error->stat[j].uvalue = pre_bit_error_count_a[j];
                    stats_pre_bit_count->stat[j].scale = FE_SCALE_COUNTER;
                    stats_pre_bit_count->stat[j].uvalue = pre_bit_count_a[j];
                    pr_info(": pre_bit_error_count  | PLP ID [%d]         | %d \n",j, pre_bit_error_count_a[j]);
                    pr_info(": pre_bit_count        | PLP ID [%d]         | %d \n",j, pre_bit_count_a[j]);
                }
            }
            
            pr_info(":---------------------- --------------------- -------------------\n");

            result = cxdref_demod_atsc3_monitor_post_bit_error(&driver->demod, &post_bit_error_count_a[0], &post_bit_count_a[0]);
            if (result != CXDREF_RESULT_OK)
                return convert_return_result(result);

            stats_post_bit_error->len=4;
            stats_post_bit_count->len=4;

             stats_post_bit_error->stat[0].scale = FE_SCALE_COUNTER;
            stats_post_bit_error->stat[0].uvalue = post_bit_error_count_a[0];
            stats_post_bit_count->stat[0].scale = FE_SCALE_COUNTER;
            stats_post_bit_count->stat[0].uvalue = post_bit_count_a[0];
            pr_info(": post_bit_error_count | PLP ID [0]          | %d \n",post_bit_error_count_a[0]);
            pr_info(": post_bit_count       | PLP ID [0]          | %d \n",post_bit_count_a[0]);

            for(j = 1; j <4 ; j++){
                if (post_bit_error_count_a[j] == CXDREF_DEMOD_ATSC3_MONITOR_PREBCHBER_INVALID) {
                    pr_info(": post_bit_error_count | PLP ID [%d]         | Unused \n",j);
                    pr_info(": post_bit_count       | PLP ID [%d]         | Unused \n",j);
                } else {
                    stats_post_bit_error->stat[j].scale = FE_SCALE_COUNTER;
                    stats_post_bit_error->stat[j].uvalue = post_bit_error_count_a[j];
                    stats_post_bit_count->stat[j].scale = FE_SCALE_COUNTER;
                    stats_post_bit_count->stat[j].uvalue = post_bit_count_a[j];
                    pr_info(": post_bit_error_count | PLP ID [%d]         | %d \n",j, post_bit_error_count_a[j]);
                    pr_info(": post_bit_count       | PLP ID [%d]         | %d \n",j, post_bit_count_a[j]);
                }
            }
            
            pr_info("---------------------- --------------------- -------------------\n");

            result = cxdref_demod_atsc3_monitor_block_error(&driver->demod, &block_error_count_a[0], &block_count_a[0]);
            if (result != CXDREF_RESULT_OK)
                return convert_return_result(result);

            stats_block_error->len=4;
            stats_block_count->len=4;

            stats_block_error->stat[0].scale = FE_SCALE_COUNTER;
            stats_block_error->stat[0].uvalue = block_error_count_a[0];
            stats_block_count->stat[0].scale = FE_SCALE_COUNTER;
            stats_block_count->stat[0].uvalue = block_count_a[0];
            pr_info(": error_count          | PLP ID [0]          | %d \n",block_error_count_a[0]);
            pr_info(": block_count          | PLP ID [0]          | %d \n",block_count_a[0]);
            
            for(j = 1; j <4 ; j++){
                if (block_error_count_a[j] == CXDREF_DEMOD_ATSC3_MONITOR_POSTBCHFER_INVALID) {
                    pr_info(": error_count          | PLP ID [%d]         | Unused \n",j);
                    pr_info(": block_count          | PLP ID [%d]         | Unused \n",j);
                } else{
                    stats_block_error->stat[j].scale = FE_SCALE_COUNTER;
                    stats_block_error->stat[j].uvalue = block_error_count_a[j];
                    stats_block_count->stat[j].scale = FE_SCALE_COUNTER;
                    stats_block_count->stat[j].uvalue = block_count_a[j];
                    pr_info(": error_count          | PLP ID [%d]         | %d \n",j, block_error_count_a[j]);
                    pr_info(": block_count          | PLP ID [%d]         | %d \n",j, block_count_a[j]);
                }
            }
            
            pr_info(":---------------------- --------------------- -------------------\n");
        }
        return 0;

    default:
        return -EINVAL;
    }

    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);

	switch (c->delivery_system) {
	case SYS_DVBT:
	case SYS_DVBS:
	case SYS_DVBS2:
	case SYS_DVBT2:
		stats_pre_bit_error->len=1;
		stats_pre_bit_error->stat[0].scale = FE_SCALE_COUNTER;
		stats_pre_bit_error->stat[0].uvalue = pre_bit_error_count;
		stats_pre_bit_count->len=1;
		stats_pre_bit_count->stat[0].scale = FE_SCALE_COUNTER;
		stats_pre_bit_count->stat[0].uvalue = pre_bit_count;
		pr_info(": pre_bit_error_count  | pre_bit_error_count | %d \n",pre_bit_error_count);
		pr_info(": pre_bit_count        | pre_bit_count       | %d \n",pre_bit_count);
		pr_info(":---------------------- --------------------- -------------------\n");
		break;
	default:
		pr_info(": pre_bit_error_count  | pre_bit_error_count | (Not Available) \n");
		pr_info(": pre_bit_count        | pre_bit_count       | (Not Available) \n");
		pr_info(":---------------------- --------------------- -------------------\n");
	}

    stats_post_bit_error->len=1;
    stats_post_bit_error->stat[0].scale = FE_SCALE_COUNTER;
    stats_post_bit_error->stat[0].uvalue = post_bit_error_count;
    stats_post_bit_count->len=1;
    stats_post_bit_count->stat[0].scale = FE_SCALE_COUNTER;
    stats_post_bit_count->stat[0].uvalue = post_bit_count;
    pr_info(": post_bit_error_count | post_bit_error_count| %d \n",post_bit_error_count);
    pr_info(": post_bit_count       | post_bit_count      | %d \n",post_bit_count);
    pr_info(":---------------------- --------------------- -------------------\n");

    stats_block_error->len=1;
    stats_block_error->stat[0].scale = FE_SCALE_COUNTER;
    stats_block_error->stat[0].uvalue = block_error_count;
    stats_block_count->len=1;
    stats_block_count->stat[0].scale = FE_SCALE_COUNTER;
    stats_block_count->stat[0].uvalue = block_count;
    pr_info(": error_count          | block_error_count   | %d \n",block_error_count);
    pr_info(": block_count          | block_count         | %d \n",block_count);
    pr_info(":---------------------- --------------------- -------------------\n");

    return 0;
}

int cxd_dmd_common_monitor_strength (struct dvb_frontend *fe)
{
	struct cxd_dmd_state *state;
	struct dtv_frontend_properties *c;
	struct cxd_dmd_driver_instance *driver;

	enum fe_delivery_system dtv_system;

	if(!fe || !fe->demodulator_priv)
		return -EINVAL;
		
	state = fe->demodulator_priv;
	driver = &state->driver;
	c = &fe->dtv_property_cache;
	dtv_system = c->delivery_system;

	if (dtv_system == SYS_DVBS || dtv_system == SYS_DVBS2 || dtv_system == SYS_ISDBS ||
	    dtv_system == SYS_ISDBS3) {
		{
		u16 signal_strength = 0;
		s32 rf_level = 0;
		if (fe->ops.tuner_ops.get_rf_strength){
			fe->ops.tuner_ops.get_rf_strength(fe, &signal_strength);
		}
		rf_level = c->strength.stat[0].svalue;

		pr_info(": Signal Level (Sat)   | signal_strength     | %d x 10^-3 dBm \n", rf_level);
		pr_info(":---------------------- --------------------- -------------------\n");
		}
	} else if (dtv_system == SYS_DVBC_ANNEX_A || dtv_system == SYS_DVBC_ANNEX_B || 
		   dtv_system == SYS_DVBT || dtv_system == SYS_ISDBT || dtv_system == SYS_ISDBC ||
		   dtv_system == SYS_ATSC || dtv_system == SYS_DVBT2 || dtv_system == SYS_DVBC_ANNEX_C ||
		   dtv_system == SYS_ATSC30) {
		{
		u16 signal_strength = 0;
		s32 rf_level = 0;
		if (fe->ops.tuner_ops.get_rf_strength){
			fe->ops.tuner_ops.get_rf_strength(fe, &signal_strength);
		}
		rf_level = c->strength.stat[0].svalue;

		pr_info(": Signal Level (Terr)  | signal_strength     | %d x 10^-3 dBm \n", rf_level);
		pr_info(":---------------------- --------------------- -------------------\n");
		}
	} else {
		return -EINVAL;
	}

    return 0;
}

int cxd_dmd_common_monitor_IFAGCOut (struct dvb_frontend *fe)
{
	cxdref_result_t result = 0;

    struct cxd_dmd_state *state;
    struct dtv_frontend_properties *c;
    struct cxd_dmd_driver_instance *driver;

    uint32_t ifAGCOut = 0;

	if(!fe || !fe->demodulator_priv)
		return -EINVAL;
			
	state = fe->demodulator_priv;
	driver = &state->driver;
	c = &fe->dtv_property_cache;

	switch (c->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		result = cxdref_demod_dvbc_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_DVBC_ANNEX_B:
		result = cxdref_demod_j83b_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_DVBT:
		result = cxdref_demod_dvbt_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_DSS:
		return -EINVAL;
	case SYS_DVBS:
		result = cxdref_demod_dvbs_s2_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_DVBS2:
		result = cxdref_demod_dvbs_s2_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_DVBH:
		return -EINVAL;
	case SYS_ISDBT:
		result = cxdref_demod_isdbt_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_ISDBS:
		result = cxdref_demod_isdbs_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_ISDBC:
		result = cxdref_demod_isdbc_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_ATSC:
		result = cxdref_demod_atsc_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_DTMB:
		return -EINVAL;
	case SYS_CMMB:
		return -EINVAL;
	case SYS_DAB:
		return -EINVAL;
	case SYS_DVBT2:
		result = cxdref_demod_dvbt2_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_TURBO:
		return -EINVAL;
	case SYS_DVBC_ANNEX_C:
		result = cxdref_demod_isdbc_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	case SYS_ISDBS3:
		result = cxdref_demod_isdbs3_monitor_IFAGCOut (&driver->demod, &ifAGCOut);
		break;
	case SYS_ATSC30:
		result = cxdref_demod_atsc3_monitor_IFAGCOut(&driver->demod, &ifAGCOut);
		break;
	default:
		return -EINVAL;
	}

    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);  

    pr_info(": AGC                  | (IFAGCOut)          | %d  \n", ifAGCOut);
    pr_info(":---------------------- --------------------- -------------------\n");

    return 0;
}

int cxd_dmd_common_monitor_quality (struct dvb_frontend *fe)
{
	cxdref_result_t result = 0;

    struct cxd_dmd_state *state;
    struct dtv_frontend_properties *c;
    struct cxd_dmd_driver_instance *driver;
    struct dtv_fe_stats *stats_quality;

    uint8_t quality = 0;

	if(!fe || !fe->demodulator_priv)
		return -EINVAL;
			
	state = fe->demodulator_priv;
	driver = &state->driver;
	c = &fe->dtv_property_cache;
	stats_quality = &c->quality;

	switch (c->delivery_system) {
	case SYS_ATSC30:
		result = cxdref_demod_atsc3_monitor_Quality(&driver->demod, &quality);
		break;
	default:
		return -EINVAL;
	}

    if (result != CXDREF_RESULT_OK)
        return convert_return_result(result);
    
    stats_quality->len=1;
    stats_quality->stat[0].scale = FE_SCALE_RELATIVE;
    stats_quality->stat[0].uvalue = quality;

    pr_info(": Signal Quality                  | Signal Quality          | %d  \n", quality);
    pr_info(":---------------------- --------------------- -------------------\n");

    return 0;
}
