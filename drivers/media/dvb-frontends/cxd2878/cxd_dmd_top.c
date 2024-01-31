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
#include <linux/gpio.h>
#include "cxd_dmd.h"
#include "cxd_dmd_common.h"
#include "cxd_dmd_priv.h"
#include "cfm.h"
#include "mdrv_adapter.h"

//extern struct dvb_adapter *mdrv_get_dvb_adapter(void);

#define CXD_DMD_UNIQUE_ID 0x39A
#define CXD_DMD_TS_RATE 109330000 //fix me
#define GPIO_A 187

unsigned int nUniqueId = 0;
static const char *pinctrl_ts = "ext1_ts_para_demod_pad_mux";
static const char *decouple_demod = "atsc3_sony";
static u16 demod_unique_id = CXD_DMD_UNIQUE_ID;
static bool lock_ever_flag;
static struct cfm_properties cfm_props_status = { 0 };
static struct cfm_properties cfm_props_configuration = { 0 };
#define MAX_PARAM_NUM    11
static struct cfm_property cfm_prop_status[MAX_PARAM_NUM];
static struct cfm_property cfm_prop_configuration[MAX_PARAM_NUM];
static bool is_demod_inited = false;

static struct cxd_dmd_config dmd_cfg = {
    .xtal_freq_khz = 24000, // Crystal frequency of demodulator
    .i2c_address_slvx = 0xDC, // I2C slave address of system part of demodulator
};

#define PARALLEL_BIT    8
#define SERIAL_BIT      1
#define TS_PACKET_BIT   188
static u8 delivery_sys_rec;

static void (*demod_notify)(int, enum cfm_notify_event_type_t, struct cfm_properties*);
static void demod_status_monitor_n_notify(struct dvb_frontend *fe, enum fe_status *status);
static int cxd_dmd_get_frontend(struct dvb_frontend *fe, struct dtv_frontend_properties *props);


static int gpio_reset_demod()
{
    int ret, val;

    ret = gpio_request(GPIO_A, "demod_rst");
    if ((ret != -EBUSY) && (ret < 0))
        goto gpio_err;

    ret = gpio_direction_output(GPIO_A, 0);
    if (ret < 0)
        goto gpio_err_release;

    gpio_set_value(GPIO_A, 0);
    msleep(10);
    gpio_set_value(GPIO_A, 1);
    msleep(10);

    val = gpio_get_value(GPIO_A);
    if (val < 0)
        goto gpio_err_release;

    pr_info("gpio_reset_demod gpio %d's value is %d\n", GPIO_A, val);

    gpio_free(GPIO_A);
    return(0);

gpio_err_release:
    gpio_free(GPIO_A);

gpio_err:
    return -1;
}

static int cxd_dmd_init(struct dvb_frontend *fe)
{
	int ret = 0;

	if (!fe)
		return -EINVAL;

    if( is_demod_inited )
    {
        pr_info("[%s][%d] demod has been init, return!\n", __func__, __LINE__);
        return ret;
    }
    pr_info("[%s][%d]demod reset and init\n", __func__, __LINE__);
    ret = gpio_reset_demod();
    if(ret < 0){
        pr_err("[%s][%d] demod reset failed\n", __func__, __LINE__);
        return ret;
    }

	ret = cxd_dmd_common_initialize_demod(fe, 0);
	if(ret)
		return ret;

    is_demod_inited = true;

	return 0;
}


static int cxd_dmd_sleep(struct dvb_frontend *fe)
{
    int ret = 0;

    if(!fe)
        return -EINVAL;

    ret = cxd_dmd_common_sleep(fe);
    if (ret)
        return ret;

    return 0;
}

static int cxd_dmd_set_tone(struct dvb_frontend *fe, enum fe_sec_tone_mode tone)
{
	return 0;
}


static int cxd_dmd_get_frontend(struct dvb_frontend *fe, struct dtv_frontend_properties *props)
{
	int ret = 0;
    
	if(!fe || !props)
		return -EINVAL;

	pr_info(":---------------------- Monitor (cxd_dmd_get_frontend) ----------\n");
	pr_info(":---------------------- --------------------- -------------------\n");
	ret = cxd_dmd_common_monitor_strength(fe);
	if (ret) {
		pr_info(": Signal Level (Tuner) | error result        | %d \n",ret);
		pr_info(":---------------------- --------------------- -------------------\n");
	}

	ret = cxd_dmd_common_monitor_cnr(fe);
	if (ret) {
		pr_info(": cnr                  | error result        | %d \n",ret);
		pr_info(":---------------------- --------------------- -------------------\n");
	}

	ret = cxd_dmd_common_monitor_bit_error(fe);
	if (ret) {
		pr_info(": PER                  | error result        | %d \n",ret);
		pr_info(":---------------------- --------------------- -------------------\n");
	}

	ret = cxd_dmd_common_monitor_IFAGCOut(fe);
	if (ret) {
		pr_info(": IFAGC OUT            | error result        | %d \n",ret);
		pr_info(":---------------------- --------------------- -------------------\n");
	}

	props = &fe->dtv_property_cache;

	return 0;
}


static int cxd_dmd_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	int ret = 0;
	u32 pll_lock_status;

	if(!fe || !status)
		return -EINVAL;

	pr_info(":---------------------- Monitor (cxd_dmd_read_status) -----------\n");
	pr_info(":---------------------- --------------------- -------------------\n");

	if (fe->ops.tuner_ops.get_status) {
		ret = fe->ops.tuner_ops.get_status(fe, &pll_lock_status);
		if (ret) {
			pr_info(": PLL Lock (Tuner)     | error result        | %d \n",ret);
			pr_info(":---------------------- --------------------- -------------------\n");
		}
	}

	ret = cxd_dmd_common_monitor_sync_stat(fe, status);
	if (ret) {
		pr_info(": Sync State           | error result        | %d \n",ret);
		pr_info(":---------------------- --------------------- -------------------\n");
		return ret;
	}
    demod_status_monitor_n_notify(fe ,status);

	return 0;
}

static void demod_status_monitor_n_notify(struct dvb_frontend *fe, enum fe_status *status)
{
	//int ret;
	u32 ts_rate;

	if ((((*status) & FE_HAS_LOCK) != 0) && (lock_ever_flag == 0)) {
		cfm_prop_status[0].cmd = CFM_STREAMING_STATUS;
		cfm_prop_status[0].u.data = 1;
		cfm_props_status.num = 1;
		cfm_props_status.props = &cfm_prop_status[0];
		demod_notify(demod_unique_id, STREAMING_STATUS_CHANGED, &cfm_props_status);

		//ret = mtk_demod_ts_get_clk_rate(fe, &ts_rate);
		ts_rate = CXD_DMD_TS_RATE;
		cfm_prop_configuration[0].cmd = CFM_STREAM_CLOCK_RATE;
		cfm_prop_configuration[0].u.data = ts_rate;
		cfm_props_configuration.num = 1;
		cfm_props_configuration.props = &cfm_prop_configuration[0];
		demod_notify(demod_unique_id,
			STREAM_CONFIGURATION_CHANGED,
			&cfm_props_configuration);

		lock_ever_flag = 1;
		pr_info("[%s][%d] demod lock!, ts clock rate = %d\n", __func__, __LINE__, ts_rate);
	}

	if ((((*status) & FE_HAS_LOCK) == 0) && (lock_ever_flag == 1)) {
		cfm_prop_status[0].cmd = CFM_STREAMING_STATUS;
		cfm_prop_status[0].u.data = 0;
		cfm_props_status.num = 1;
		cfm_props_status.props = &cfm_prop_status[0];
		demod_notify(demod_unique_id,
			STREAMING_STATUS_CHANGED,
			&cfm_props_status);

		cfm_prop_configuration[0].cmd = CFM_STREAM_CLOCK_RATE;
		cfm_prop_configuration[0].u.data = 0;
		cfm_props_configuration.num = 1;
		cfm_props_configuration.props = &cfm_prop_configuration[0];
		demod_notify(demod_unique_id,
			STREAM_CONFIGURATION_CHANGED,
			&cfm_props_configuration);

		lock_ever_flag = 0;
		pr_info("[%s][%d] demod lost lock!\n", __func__, __LINE__);
	}
}


static int cxd_dmd_tune(struct dvb_frontend *fe, bool re_tune, unsigned int mode_flags,
	unsigned int *delay, enum fe_status *status)
{
	int ret = 0;
	struct dtv_frontend_properties *c;

	if (!fe || !delay || !status)
		return -EINVAL;

	c = &fe->dtv_property_cache;

    if (re_tune) {
    	c->pre_bit_error.len = 1;
    	c->pre_bit_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
    	c->pre_bit_error.stat[0].uvalue = 0;
    	c->pre_bit_count.len = 1;
    	c->pre_bit_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
    	c->pre_bit_count.stat[0].uvalue = 0;
    	c->post_bit_error.len = 1;
    	c->post_bit_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
    	c->post_bit_error.stat[0].uvalue = 0;
    	c->post_bit_count.len = 1;
    	c->post_bit_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
    	c->post_bit_count.stat[0].uvalue = 0;
    	c->block_error.len = 1;
    	c->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
    	c->block_error.stat[0].uvalue = 0;
    	c->block_count.len = 1;
    	c->block_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
    	c->block_count.stat[0].uvalue = 0;

    	if (fe->ops.tuner_ops.set_params) {
    		ret = fe->ops.tuner_ops.set_params(fe);
    		if (ret)
    			return ret;
    	}

    	ret = cxd_dmd_common_tune(fe);
    	if (ret)
    		return ret;

    	msleep(1000);
    	
    	ret = cxd_dmd_common_monitor_sync_stat(fe, status);
    	if (ret) 
            return ret;
    }

    delivery_sys_rec = c->delivery_system;
	return 0;
}

static enum dvbfe_algo cxd_dmd_get_frontend_algo(struct dvb_frontend *fe)
{
    return DVBFE_ALGO_HW;
}

static int cxd_dmd_suspend(struct dvb_frontend *fe)
{
	int ret = 0;

	if(!fe)
		return -EINVAL;

	ret = cxd_dmd_common_sleep(fe);
	if (ret)
		return ret;

	return 0;
}
static int cxd_dmd_resume(struct dvb_frontend *fe)
{
	int ret = 0;
	struct cxd_dmd_state *state;

	if (!fe)
		return -EINVAL;

	state = fe->demodulator_priv;

	ret = cxd_dmd_common_initialize_demod(fe, 0);
	if(ret)
		return ret;

	return 0;
}

static int cxd_dmd_reset(unsigned int nGpio)
{
    int nRet = 0;
    int nRetCode = 0;
    int nVal = 0;

    do
    {
        nRetCode = gpio_request(nGpio, "demod_reset");
        if ((nRetCode != -EBUSY) && (nRetCode < 0))
        {
            pr_err("[%s][%d] gpio_request: %d failed\n", __func__, __LINE__,nGpio);
            nRet = -1;
            break;
        }

        nRetCode = gpio_direction_output(nGpio, 0);
        if (nRetCode < 0)
        {
            pr_err("[%s][%d] gpio_direction_output: %d failed\n", __func__, __LINE__,nGpio);
            nRet = -2;
            break;
        }

        gpio_set_value(nGpio, 0);
        msleep(200);
        gpio_set_value(nGpio, 1);
        msleep(10);

        nVal = gpio_get_value(nGpio);
        if (nVal < 0)
        {
            pr_err("[%s][%d] gpio_get_value: %d failed\n", __func__, __LINE__,nGpio);
            nRet = -2;
            break;
        }
        else
        {
            pr_info("[%s][%d] gpio_get_value: %d value:%d\n", __func__, __LINE__,nGpio,nVal);
        }
    }
    while (0);
    
    if((nRet == -2) || (nRet == 0))
    {
        gpio_free(nGpio);
    }
    
    return nRet;
}


static int cfm_property_process_get(struct cfm_property *tvp)
{
	int ret = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	if (!tvp) {
		pr_err("[%s][%d] tvp is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	switch (tvp->cmd) {
	case CFM_STREAM_CLOCK_INV:
		tvp->u.data = 0;
		pr_info("[%s][%d]CFM_TS_CLOCK_INV!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_STREAM_EXT_SYNC:
		tvp->u.data = 1;
		pr_info("[%s][%d]CFM_TS_EXT_SYNC!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_STREAM_DATA_BITS_NUM:
		tvp->u.data = PARALLEL_BIT;
		pr_info("[%s][%d]CFM_TS_DATA_BITS_NUM!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_STREAM_BIT_SWAP:
		tvp->u.data = 0;
		pr_info("[%s][%d]CMF_TS_BIT_SWAP!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_TS_SOURCE_PACKET_SIZE:
		tvp->u.data = TS_PACKET_BIT;
		pr_info("[%s][%d]CMF_TS_SOURCE_PACKET_MODE!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_DECOUPLE_DMD:
		if (delivery_sys_rec == SYS_ATSC30)
			decouple_demod = "atsc3_sony";
		memcpy(tvp->u.buffer.data, decouple_demod, strlen(decouple_demod) + 1);
		pr_info("[%s][%d]CFM_DECOUPLE_DMD!, value = %s\n",
			__func__, __LINE__, tvp->u.buffer.data);
		break;
	case CFM_STREAM_PIN_MUX_INFO:
		if (delivery_sys_rec == SYS_ATSC30)
			pinctrl_ts = "ext1_ts_para_demod_pad_mux";

		memcpy(tvp->u.buffer.data, pinctrl_ts, strlen(pinctrl_ts) + 1);
		pr_info("[%s][%d]CFM_STREAM_PIN_MUX_INFO!, value = %s\n",
			__func__, __LINE__, tvp->u.buffer.data);
	    break;
	default:
		pr_err("[%s][%d]FE property %d doesn't exist\n",
		 __func__, __LINE__, tvp->cmd);
		pr_info("[%s][%d]FE property %d doesn't exist\n",
			__func__, __LINE__, tvp->cmd);
		return -EINVAL;
	}
	return ret;
}

int cxd_dmd_get_param(struct dvb_frontend *fe, struct cfm_properties *demodProps)
{
	int ret = 0;
	int i;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < demodProps->num; i++) {
		ret = cfm_property_process_get((demodProps->props)+i);
		if (ret < 0)
			goto out;
	}
out:
	return (ret > 0) ? 0 : ret;
}

int cxd_dmd_set_notify(int device_unique_id,
	void (*dd_notify)(int, enum cfm_notify_event_type_t, struct cfm_properties *))
{
	demod_notify = dd_notify;

	return 0;
}

int cxd_dmd_dvb_frontend_registered(int device_unique_id, struct dvb_frontend *fe)
{
	// save the mapping between device_unique_id and fe to handle multiple frontend cases
	return 0;
}


static struct dvb_frontend_ops cxd_dmd_ops = {
    .delsys = {
        SYS_ATSC30, SYS_ATSC, SYS_DVBC_ANNEX_B
    },
    .info = {
        .name = "CXD demodulator",
        .frequency_min_hz =  57 * MHz,
        .frequency_max_hz = 866 * MHz,
        .caps =
            FE_CAN_INVERSION_AUTO |
            FE_CAN_FEC_AUTO |
            FE_CAN_QAM_AUTO |
            FE_CAN_TRANSMISSION_MODE_AUTO |
            FE_CAN_GUARD_INTERVAL_AUTO
    },
    .init = cxd_dmd_init,
    .sleep = cxd_dmd_sleep,
    .tune = cxd_dmd_tune,
    .get_frontend = cxd_dmd_get_frontend,
    .read_status = cxd_dmd_read_status,
    .get_frontend_algo = cxd_dmd_get_frontend_algo,
};


static struct cfm_demod_ops cxd_cfm_dmd_ops = {
    .chip_delsys.size = 1,
    .chip_delsys.element = {SYS_ATSC30},
    .info = {
        .name = "CXD demodulator",
        .frequency_min_hz =  57 * MHz,
        .frequency_max_hz = 866 * MHz,
        .caps =
            FE_CAN_INVERSION_AUTO |
            FE_CAN_FEC_AUTO |
            FE_CAN_QAM_AUTO |
            FE_CAN_TRANSMISSION_MODE_AUTO |
            FE_CAN_GUARD_INTERVAL_AUTO
    },
    .init = cxd_dmd_init,
    .sleep = cxd_dmd_sleep,
    .tune = cxd_dmd_tune,
    .get_demod = cxd_dmd_get_frontend,
    .read_status = cxd_dmd_read_status,
    .get_frontend_algo = cxd_dmd_get_frontend_algo,
    .set_tone = cxd_dmd_set_tone,
    .device_unique_id = 0x39A,//CXDREF_DEMOD_CHIP_ID_CXD2878A,
    .suspend = cxd_dmd_suspend,
    .resume = cxd_dmd_resume,
    .get_param = cxd_dmd_get_param,
    .set_notify = cxd_dmd_set_notify,
    .dvb_frontend_registered = cxd_dmd_dvb_frontend_registered,
};

extern struct dvb_adapter *mdrv_get_dvb_adapter(void);
static int cxd_dmd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    unsigned int nXtalFreq = 0,nI2cAddr = 0,nResetGpio=0;
    //struct cxd_dmd_config *config = NULL;
    struct cxd_dmd_state *state = NULL;
    struct cfm_demod_ops *cfm_cxd_ops = NULL;
    struct dvb_adapter *adapter = NULL;
    pr_info("[%s][%d] cxd_dmd_probe enter..\n", __func__, __LINE__);

    if (!client)
    {
        return -EINVAL;
    }

    ret = of_property_read_u32(client->dev.of_node, "unique_id", &nUniqueId);
    if(ret == 0)
    {
        pr_info("[%s][%d] get unique_id: %d\n", __func__, __LINE__,nUniqueId);
    }
    else
    {
        pr_err("[%s][%d] get unique_id failed\n", __func__, __LINE__);
    }

    ret = of_property_read_u32(client->dev.of_node, "i2c_slave_id", &nI2cAddr);
    if(ret == 0)
    {
        pr_info("[%s][%d] get i2c_slave_id: 0x%x\n", __func__, __LINE__,nI2cAddr);
    }
    else
    {
        pr_err("[%s][%d] get i2c_slave_id failed\n", __func__, __LINE__);
    }

    ret = of_property_read_u32(client->dev.of_node, "xtal_cap", &nXtalFreq);
    if(ret == 0)
    {
        pr_info("[%s][%d] get xtal_cap: %d\n", __func__, __LINE__,nXtalFreq);
    }
    else
    {
        pr_err("[%s][%d] get xtal_cap failed\n", __func__, __LINE__);
    }

    ret = of_property_read_u32(client->dev.of_node, "reset_gpio", &nResetGpio);
    if(ret == 0)
    {
        pr_info("[%s][%d] get reset_gpio: %d\n", __func__, __LINE__,nResetGpio);
    }
    else
    {
        pr_err("[%s][%d] get reset_gpio failed\n", __func__, __LINE__);
    }

    ret = cxd_dmd_reset(nResetGpio);
    if(ret < 0)
    {
        pr_err("[%s][%d] demod reset failed\n", __func__, __LINE__);
        return ret;
    }

    /*config = client->dev.platform_data;
    if (!config)
        return -EINVAL;*/
    
    state = kzalloc(sizeof(struct cxd_dmd_state), GFP_KERNEL);
    if (!state)
    {
        pr_err("[%s][%d] kzalloc failed\n", __func__, __LINE__);
        return -ENOMEM;
    }

    //memcpy(&state->config, config, sizeof(struct cxd_dmd_config));
    state->config.i2c_address_slvx = nI2cAddr + 4;//nI2cAddr;//nI2cAddr;//0xcc;//0xc8;//slvx:system addr
    state->config.xtal_freq_khz = nXtalFreq;

    pr_info("[%s][%d] addr:0x%x xtal:%d\n", __func__, __LINE__,state->config.i2c_address_slvx,state->config.xtal_freq_khz);

    state->i2c = client->adapter;
    state->fe.demodulator_priv = state;

    memcpy(&state->fe.ops, &cxd_dmd_ops, sizeof(struct dvb_frontend_ops));
    
    i2c_set_clientdata(client, state);
    
    ret = cxd_dmd_common_initialize_i2c_device(client);
    if (ret < 0)
    {
        pr_err("[%s][%d] cxd_dmd_common_initialize_i2c_device failed ret:%d\n", __func__, __LINE__,ret);
        kfree(state);
        return ret;
    }
    
    ret = cxd_dmd_common_get_dmd_chip_id(client);
    if (ret < 0)
    {
        pr_err("[%s][%d] 11 cxd_dmd_common_get_dmd_chip_id failed ret:%d\n", __func__, __LINE__,ret);
        kfree(state);
        return ret;
    }

    adapter = mdrv_get_dvb_adapter();

    if (!adapter) {
        pr_err("[%s][%d] get dvb adapter fail\n", __func__, __LINE__);
        return -ENODEV;
    }
    
    cfm_cxd_ops = kzalloc(sizeof(struct cfm_demod_ops), GFP_KERNEL);
    if(!cfm_cxd_ops){
        pr_err("[%s][%d] kzalloc cfm_cxd_ops failed!\n", __func__, __LINE__);
        return -ENODEV;
    }
    
    memcpy(cfm_cxd_ops, &cxd_cfm_dmd_ops, sizeof(struct cfm_demod_ops));
    cfm_cxd_ops->demodulator_priv = state->fe.demodulator_priv;
    cfm_register_demod_device(adapter, cfm_cxd_ops);
    
    return 0;
}

static int cxd_dmd_remove(struct i2c_client *client)
{
    struct cxd_dmd_state *state;

    if (!client)
        return -EINVAL;

    cxd_dmd_common_finalize_i2c_device(client);

    state = i2c_get_clientdata(client);

    if (state) kfree(state);

    return 0;
}

static const struct i2c_device_id cxd_dmd_id_table[] = {
    {"cxd2878", 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, cxd_dmd_id_table);


static const struct of_device_id cxd_demod_dt_match[] = {
    { .compatible = "cxd2878" },
    {},
};

MODULE_DEVICE_TABLE(of, cxd_demod_dt_match);


static struct i2c_driver cxd_dmd_driver = {
    .driver = {
        .owner  = THIS_MODULE,
        .name = "cxd2878",
        .of_match_table = of_match_ptr(cxd_demod_dt_match),
    },
    .probe    = cxd_dmd_probe,
    .remove   = cxd_dmd_remove,
    .id_table = cxd_dmd_id_table,
};
/*
static int __init cxd2878_init(void)
{
    pr_info("%s is called\n", __func__);
    pr_emerg("cxd2878_init\n");
    return i2c_add_driver(&cxd_dmd_driver);
}
*/
//module_init(cxd2878_init);

module_i2c_driver(cxd_dmd_driver);

MODULE_DESCRIPTION("CXD demodulator driver");
MODULE_AUTHOR("Sony Semiconductor Solutions Corporation");
MODULE_LICENSE("GPL v2");
