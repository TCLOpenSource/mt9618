// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
	* MediaTek Inc. (C) 2020. All rights reserved.
	*/
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <media/media-device.h>
#include "cfm_priv.h"
#include <linux/sysfs.h>
#include <linux/device.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "mtk_cfm: " fmt

static struct cfm_info_table _cfm_info_table[MAX_VIRTUAL_FE_NUM] = { {0} };
static u8 reg_demod_num, reg_tuner_num;
static u8 reg_demod_times, reg_tuner_times;
static u8 Unicable_blindscan;
static u16 user_band_freq;
static bool Unicable_flag;
static bool JESS_flag;

#define SEL_DEMOD_IND(idx) ((_cfm_info_table[idx].act_demod_index != NULL_INDEX) ? \
	(_cfm_info_table[idx].act_demod_index) : (_cfm_info_table[idx].sel_demod_index))
#define SEL_TUNER_IND(idx) ((_cfm_info_table[idx].act_tuner_index != NULL_INDEX) ? \
	(_cfm_info_table[idx].act_tuner_index) : (_cfm_info_table[idx].sel_tuner_index))

void _i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	int ret = 0;
	int dmd_idx_use;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]
		->i2c_gate_ctrl) {
		ret = _cfm_info_table[(unsigned int)(fe->id)]
		.reg_demod_ops[(unsigned int)(dmd_idx_use)]->i2c_gate_ctrl(fe, enable);
		if (ret != 0)
			pr_err("[%s][%d] fe[%d] set i2c gate control fail!! enable= %d\n",
			__func__, __LINE__, fe->id, enable);
	} else
		pr_info("[%s][%d] fe[%d] i2c_gate_ctrl function has not implemented!! enable= %d\n",
		__func__, __LINE__, fe->id, enable);
}

int _debug_info_show(int index, ssize_t *buf_len, char *buf)
{
	// eof: to imply the data termination
	// 0: still have data, 1: all data is read
	int eof;
	int buf_offset;
	ssize_t size_demod = 0;
	ssize_t size_tuner = 0;
	ssize_t size_dish = 0;
	int	ret	= 0;
	int tuner_idx_use;
	int dmd_idx_use;

	pr_info("[%s][%d] enter demod\n", __func__, __LINE__);

	//check null ptr
	if (index < 0)
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND(index);
	dmd_idx_use = SEL_DEMOD_IND(index);
	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[index].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[index].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	buf_offset = 0;
	//demod dump
	if (_cfm_info_table[index].reg_demod_ops[dmd_idx_use]->dump_debug_info) {
		do {
			size_demod += _cfm_info_table[index].reg_demod_ops[dmd_idx_use]
			->dump_debug_info(buf+(buf_offset*PAGE_SIZE), PAGE_SIZE, &eof,
			buf_offset, _cfm_info_table[index].fe);

			buf_offset++;
		} while (eof == 0);
	}

	pr_info("[%s][%d] enter tuner\n", __func__, __LINE__);
	buf_offset = 0;
	// tuner dump
	if (_cfm_info_table[index].reg_tuner_ops[tuner_idx_use]->dump_debug_info) {
		_i2c_gate_ctrl(_cfm_info_table[index].fe, 1);
		do {
			size_tuner += _cfm_info_table[index].reg_tuner_ops[tuner_idx_use]
			->dump_debug_info(buf+size_demod+(buf_offset*PAGE_SIZE), PAGE_SIZE,
			&eof, buf_offset, _cfm_info_table[index].fe);

			buf_offset++;
		} while (eof == 0);
		_i2c_gate_ctrl(_cfm_info_table[index].fe, 0);
	}

	pr_info("[%s][%d] enter dish\n", __func__, __LINE__);
	buf_offset = 0;
	// dish dump
	if (_cfm_info_table[index].reg_dish_ops) {
		if (_cfm_info_table[index].reg_dish_ops->dump_debug_info) {
			do {
				size_dish += _cfm_info_table[index].reg_dish_ops
				->dump_debug_info(buf+size_tuner+size_demod+(buf_offset*PAGE_SIZE),
				PAGE_SIZE, &eof, buf_offset, _cfm_info_table[index].fe);

				buf_offset++;
			} while (eof == 0);
		}
	}

	pr_info("[%s][%d] dump debug info end!, PAGE_SIZE = %ld, size_demod = %zd, size_tuner = %zd, size_dish = %zd\n",
	__func__, __LINE__, PAGE_SIZE, size_demod, size_tuner, size_dish);

	*buf_len = (size_demod+size_tuner+size_dish);

	return ret;
}

static ssize_t debug_info_0_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	ssize_t buf_len = 0;
	const int index = 0;
	int result = 0;

	pr_info("[%s][%d] enter\n", __func__, __LINE__);

	result = _debug_info_show(index, &buf_len, buf);

	pr_info("[%s][%d] buf = %s\n", __func__, __LINE__, buf);
	pr_info("[%s][%d] buf_len = %zd\n", __func__, __LINE__, buf_len);

	return buf_len;
}

static ssize_t debug_info_1_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	ssize_t buf_len = 0;
	const int index = 1;
	int result = 0;

	pr_info("[%s][%d] enter\n", __func__, __LINE__);

	result = _debug_info_show(index, &buf_len, buf);

	pr_info("[%s][%d] buf = %s\n", __func__, __LINE__, buf);
	pr_info("[%s][%d] buf_len = %zd\n", __func__, __LINE__, buf_len);

	return buf_len;
}

static ssize_t debug_info_2_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	ssize_t buf_len = 0;
	const int index = 2;
	int result = 0;

	pr_info("[%s][%d] enter\n", __func__, __LINE__);

	result = _debug_info_show(index, &buf_len, buf);

	pr_info("[%s][%d] buf = %s\n", __func__, __LINE__, buf);
	pr_info("[%s][%d] buf_len = %zd\n", __func__, __LINE__, buf_len);

	return buf_len;
}

static ssize_t debug_info_3_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	ssize_t buf_len = 0;
	const int index = 3;
	int result = 0;

	pr_info("[%s][%d] enter\n", __func__, __LINE__);

	result = _debug_info_show(index, &buf_len, buf);

	pr_info("[%s][%d] buf = %s\n", __func__, __LINE__, buf);
	pr_info("[%s][%d] buf_len = %zd\n", __func__, __LINE__, buf_len);

	return buf_len;
}

static struct device_attribute dev_attr_debug_info_0 =
	__ATTR(debug_info, 0444, debug_info_0_show, NULL);
static struct device_attribute dev_attr_debug_info_1 =
	__ATTR(debug_info, 0444, debug_info_1_show, NULL);
static struct device_attribute dev_attr_debug_info_2 =
	__ATTR(debug_info, 0444, debug_info_2_show, NULL);
static struct device_attribute dev_attr_debug_info_3 =
	__ATTR(debug_info, 0444, debug_info_3_show, NULL);

static struct attribute *frontend0_attrs[] = {
	&dev_attr_debug_info_0.attr,
	NULL,
};

static struct attribute *frontend1_attrs[] = {
	&dev_attr_debug_info_1.attr,
	NULL,
};

static struct attribute *frontend2_attrs[] = {
	&dev_attr_debug_info_2.attr,
	NULL,
};

static struct attribute *frontend3_attrs[] = {
	&dev_attr_debug_info_3.attr,
	NULL,
};

static struct attribute_group mtk_frontend_attr_group0 = {
	.name = "dvb0.frontend0",
	.attrs = frontend0_attrs,
};

static struct attribute_group mtk_frontend_attr_group1 = {
	.name = "dvb0.frontend1",
	.attrs = frontend1_attrs,
};

static struct attribute_group mtk_frontend_attr_group2 = {
	.name = "dvb0.frontend2",
	.attrs = frontend2_attrs,
};

static struct attribute_group mtk_frontend_attr_group3 = {
	.name = "dvb0.frontend3",
	.attrs = frontend3_attrs,
};

static const struct attribute_group *mtk_frontend_attr_groups[] = {
	&mtk_frontend_attr_group0,
	&mtk_frontend_attr_group1,
	&mtk_frontend_attr_group2,
	&mtk_frontend_attr_group3,
	NULL,
};

u32 _convert_array_to_u16(struct cfm_ext_array delsys)
{
	u32 value = 0;
	struct cfm_ext_array testValue = delsys;
	int i;

	for (i = 0; i < (testValue.size); i++) {
		if ((testValue.element[i]) > MAX_SIZE_DEL_SYSTEM) {
			pr_err("Failed to get cfm_ext_array element\r\n");
			return -ENONET;
		}
		value |= ((u32)1 << (testValue.element[i]));
		pr_info("[%s][%d], testValue.element[i = %d] = [0x%x], value = [0x%x]\n",
			__func__, __LINE__, i, (testValue.element[i]), value);
	}
	return value;
}

int _confirm_which_tuner_use(struct dvb_frontend *fe, int *tuner_idx)
{
	int tuner_index = 0;
	int tuner_number;
	int i;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	u32 board_del_sys_capability = 0;
	// Now chip_del_sys_capability don't implement
	u32 chip_del_sys_capability = default_value;
	u32 act_del_sys_capability = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	mutex_lock(&cfm_tuner_mutex);

	//check null ptr
	if (!(fe)) {
		mutex_unlock(&cfm_tuner_mutex);
		return FALSE;
	}

	tuner_number = _cfm_info_table[(unsigned int)(fe->id)].tuner_device_number;

	for (i = 0; i < tuner_number; i++) {
		if (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[i])) {
			mutex_unlock(&cfm_tuner_mutex);
			return FALSE;
		}

		board_del_sys_capability = _convert_array_to_u16
		(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[i]->board_delsys);

		pr_info("[%s][%d], board_del_sys_capability = [0x%x]\n",
			__func__, __LINE__, board_del_sys_capability);

//  chip_del_sys_capability = _convert_array_to_u16
// (_cfm_info_table[fe->id].reg_tuner_ops[i]->chip_delsys);

		act_del_sys_capability = board_del_sys_capability
			& chip_del_sys_capability;
		pr_info("[%s][%d], act_del_sys_capability = [0x%x]\n",
		__func__, __LINE__, act_del_sys_capability);

		// compare delivery system of fe and tuner's capability
		if (((u32)1 << (c->delivery_system)) & act_del_sys_capability) {
			tuner_index = i;
			pr_info("[%s][%d], Correct to selected tuner index = [%d]\n",
				__func__, __LINE__, tuner_index);
			break;
		}
	}
	if (i == tuner_number) {
		// set to default value first !!
		tuner_index = 0; //0xff;
		pr_info("[%s][%d] Undefined delivery system\r\n", __func__, __LINE__);
	}

	_cfm_info_table[(unsigned int)(fe->id)].sel_tuner_index = tuner_index;

	mutex_unlock(&cfm_tuner_mutex);
	*tuner_idx = tuner_index;
	return TRUE;
}

int _confirm_which_demod_use(struct dvb_frontend *fe, int *demod_idx)
{
	int demod_index = 0;
	int demod_number;
	int i, j;
	bool found_match = false;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	//u32 board_del_sys_capability = 0;
	// Now chip_del_sys_capability don't implement
	//u32 chip_del_sys_capability = 0xffffffff;
	//u32 act_del_sys_capability = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);
	mutex_lock(&cfm_demod_mutex);
	//check null ptr
	if (!(fe)) {
		mutex_unlock(&cfm_demod_mutex);
		return FALSE;
	}

	demod_number = _cfm_info_table[(unsigned int)(fe->id)].demod_device_number;

	for (i = 0; i < demod_number; i++) {
		if (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[i])) {
			mutex_unlock(&cfm_demod_mutex);
			return FALSE;
		}
		for (j = 0; j < (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[i]
			->chip_delsys.size); j++) {
			if ((c->delivery_system)
				== (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[i]
				->chip_delsys.element[j])) {
				found_match = true;
				break;
			}
		}

		if (found_match)
			break;
	}

	if (i == demod_number) {
		// set to default value first !!
		demod_index = 0; //0xff;
		pr_info("[%s][%d] Undefined delivery system\r\n", __func__, __LINE__);
	} else {
		demod_index = i;
	}

	_cfm_info_table[(unsigned int)(fe->id)].sel_demod_index = demod_index;

	mutex_unlock(&cfm_demod_mutex);
	*demod_idx = demod_index;
	return TRUE;
}

int _check_and_do_demod_init(int dmd_idx_use, struct dvb_frontend *fe)
{
	int	ret = 0;
	int pri_demod_index;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe)) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]fe is null!\n", __func__, __LINE__);
		return FALSE;
	}

	// save correct demod private to fe
	fe->demodulator_priv = _cfm_info_table[(unsigned int)(fe->id)]
				.reg_demod_ops[(unsigned int)(dmd_idx_use)]
				->demodulator_priv;

	pri_demod_index = _cfm_info_table[(unsigned int)(fe->id)].act_demod_index;

	// if demod change and previous demod is not released,
	// need to release previous demod first
	if ((pri_demod_index != NULL_INDEX)
		&& (pri_demod_index != dmd_idx_use)) {
		if (_cfm_info_table[(unsigned int)(fe->id)]
			.reg_demod_ops[(unsigned int)(pri_demod_index)]
			->sleep) {
			_cfm_info_table[(unsigned int)(fe->id)]
			.reg_demod_ops[(unsigned int)(pri_demod_index)]
			->sleep(fe);
			_cfm_info_table[(unsigned int)(fe->id)].act_demod_index
				= NULL_INDEX;
		}
	}

	//demod init
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]
		->init) {
		ret = _cfm_info_table[(unsigned int)(fe->id)]
		.reg_demod_ops[(unsigned int)(dmd_idx_use)]->init(fe);
		if (ret == 0)
			_cfm_info_table[(unsigned int)(fe->id)].act_demod_index
				= dmd_idx_use;
		else
			_cfm_info_table[(unsigned int)(fe->id)].act_demod_index
				= NULL_INDEX; //init fail
	}
	return ret;
}

int _check_and_do_tuner_init(int tuner_idx_use, struct dvb_frontend *fe)
{
	int	ret = 0;
	int pri_tuner_index;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe)) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]fe is null!\n", __func__, __LINE__);
		return FALSE;
	}

	// save correct tuner private to fe
	fe->tuner_priv = _cfm_info_table[(unsigned int)(fe->id)]
			.tuner_pri_data[(unsigned int)(tuner_idx_use)].vendor_tuner_priv;

	pri_tuner_index	= _cfm_info_table[(unsigned int)(fe->id)].act_tuner_index;
    //need lock tuner-mutex
    mutex_lock(&cfm_tuner_mutex);

	// if tuner change and previous tuner is not released,
	// need to release previous tuner first
	if ((pri_tuner_index != NULL_INDEX)
		&& (pri_tuner_index != tuner_idx_use)) {
		if (_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(pri_tuner_index)]
			->device_tuner_ops.release) {
			_i2c_gate_ctrl(fe, 1);
			_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(pri_tuner_index)]
			->device_tuner_ops.release(fe);
			_i2c_gate_ctrl(fe, 0);


			_cfm_info_table[(unsigned int)(fe->id)].act_tuner_index
				= NULL_INDEX;
		}
	}

	//tuner init
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[(unsigned int)(tuner_idx_use)]
		->device_tuner_ops.init) {
		_i2c_gate_ctrl(fe, 1);
		ret = _cfm_info_table[(unsigned int)(fe->id)]
		.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
		->device_tuner_ops.init(fe);
		_i2c_gate_ctrl(fe, 0);
		if (ret	== 0)
			_cfm_info_table[(unsigned int)(fe->id)].act_tuner_index
				= tuner_idx_use;
		else
			_cfm_info_table[(unsigned int)(fe->id)].act_tuner_index
				= NULL_INDEX;	//init fail
	}
    mutex_unlock(&cfm_tuner_mutex);
    //need unlock tuner-mutex

	return ret;
}

int _check_and_do_dish_init(struct dvb_frontend *fe)
{
	int	ret = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe)) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]fe is null!\n", __func__, __LINE__);
		return FALSE;
	}

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops) {
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->init) {
			ret = _cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->init(fe);
			if (ret	== 0)
				_cfm_info_table[(unsigned int)(fe->id)].act_dish_index
					= DISH_INDEX_USE;
			else
				_cfm_info_table[(unsigned int)(fe->id)].act_dish_index
					= NULL_INDEX;	//init fail
		}
	}
	return ret;
}

int _check_and_do_frontend_init(struct dvb_frontend *fe, int tuner_idx_use, int dmd_idx_use)
{
	int	ret = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	// tuner init
	ret = _check_and_do_tuner_init(tuner_idx_use, fe);
	if (ret)
		pr_err("[%s][%d] tuner init fail\n", __func__, __LINE__);

	// demod init
	ret = _check_and_do_demod_init(dmd_idx_use, fe);
	if (ret)
		pr_err("[%s][%d] demod init fail\n", __func__, __LINE__);

	return ret;
}

int cfm_init(struct dvb_frontend *fe)
{
	int ret = 0;
	int tuner_idx_use;
	int dmd_idx_use;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	if (fe->exit != DVB_FE_DEVICE_RESUME) {
		if (_confirm_which_tuner_use(fe, &tuner_idx_use) == FALSE)
			return -EINVAL;
		if (_confirm_which_demod_use(fe, &dmd_idx_use) == FALSE)
			return -EINVAL;
	} else {
		tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
		dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	}

	if (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[(unsigned int)(tuner_idx_use)]))
		return -EINVAL;
	if (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]))
		return -EINVAL;

	// to fix priv not is NULL bug
	fe->tuner_priv = _cfm_info_table[(unsigned int)(fe->id)]
			.tuner_pri_data[(unsigned int)(tuner_idx_use)].vendor_tuner_priv;

	fe->demodulator_priv = _cfm_info_table[(unsigned int)(fe->id)]
		.reg_demod_ops[(unsigned int)(dmd_idx_use)]
		->demodulator_priv;
	pr_info("[%s][%d] fe->tuner_priv:%p fe->id:%d tuner_idx_use:%d\n",
		__func__, __LINE__, fe->tuner_priv, fe->id, tuner_idx_use);

	//tuner init
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[(unsigned int)(tuner_idx_use)]
		->device_tuner_ops.init) {
		_i2c_gate_ctrl(fe, 1);
		ret = _cfm_info_table[(unsigned int)(fe->id)]
		.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
		->device_tuner_ops.init(fe);
		_i2c_gate_ctrl(fe, 0);
		if (ret	== 0)
			_cfm_info_table[(unsigned int)(fe->id)].act_tuner_index
				= tuner_idx_use;
		else
			_cfm_info_table[(unsigned int)(fe->id)].act_tuner_index
				= NULL_INDEX;	//init fail
	}

	//demod init
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]
		->init) {
		ret = _cfm_info_table[(unsigned int)(fe->id)]
			.reg_demod_ops[(unsigned int)(dmd_idx_use)]->init(fe);
		if (ret == 0)
			_cfm_info_table[(unsigned int)(fe->id)].act_demod_index
				= dmd_idx_use;
		else
			_cfm_info_table[(unsigned int)(fe->id)].act_demod_index
				= NULL_INDEX; //init fail
	}

	// dish init
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops) {
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->init) {
			ret = _cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->init(fe);
			if (ret == 0)
				_cfm_info_table[(unsigned int)(fe->id)].act_dish_index
					= DISH_INDEX_USE;
			else
				_cfm_info_table[(unsigned int)(fe->id)].act_dish_index
					= NULL_INDEX; //init fail
		}
	}
	return ret;
}

int cfm_set_lna(struct dvb_frontend *fe)
{
	int ret = 0;
	//cfm_device_ext_cmd_param *ext_para;
	int tuner_idx_use;

	//check null ptr
	if (!(fe))
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;

	if (fe->dtv_property_cache.delivery_system == SYS_UNDEFINED) {
		pr_info("[%s][%d] SYS_UNDEFINED.Assign as system DVBT!!\n", __func__, __LINE__);
		fe->dtv_property_cache.delivery_system = SYS_DVBT;
	}

	pr_info("[%s][%d] lna %d\n", __func__, __LINE__,
		fe->dtv_property_cache.lna);
	// tuner set_lna
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]->set_lna) {

		//call to tuner driver with lna setting fe->dtv_property_cache->lna
		_i2c_gate_ctrl(fe, 1);
		ret = _cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
		->set_lna(fe);
		_i2c_gate_ctrl(fe, 0);
	} else {
		pr_emerg("[%s][%d] there is no set_lna in tuner driver\n"
			, __func__, __LINE__);
	}

	return ret;
}

int cfm_tune(struct dvb_frontend *fe,
			bool re_tune,
			unsigned int mode_flags,
			unsigned int *delay,
			enum fe_status *status)
{
	int ret = 0;
	struct dtv_frontend_properties *c;
	/*fake para*/
	struct analog_parameters a_para	= {0};
	int tuner_idx_use;
	int dmd_idx_use;
	struct tuner_common_parameter common_param;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	c = &fe->dtv_property_cache;
	pr_info("[%s][%d] c->stream_id: 0x%x\n", __func__, __LINE__, c->stream_id);

	if (re_tune) {
		ret = _confirm_which_tuner_use(fe, &tuner_idx_use);
		pr_info("[%s][%d],	tuner_idx_use = [%d]\n",
			__func__, __LINE__, tuner_idx_use);

		if ((ret == FALSE) ||
			(!(_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)])))
			return -EINVAL;

		//demod&tuner init
		ret = _check_and_do_frontend_init(fe, tuner_idx_use, dmd_idx_use);
		if (ret)
			pr_err("[%s][%d] frontend init fail\n", __func__, __LINE__);

		//if it is unicable blindscan, then we should set user band frequency to tuner
		if (Unicable_blindscan)
			c->frequency = user_band_freq*transfer_unit;

		// tuner set parameters
		if ((c->delivery_system == SYS_ANALOG) &&
			(_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
			->device_tuner_ops.set_analog_params)) {
			_i2c_gate_ctrl(fe, 1);
			_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
			->device_tuner_ops.set_analog_params(fe, &a_para);
			_i2c_gate_ctrl(fe, 0);
		}
		else if (_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
			->device_tuner_ops.set_params) {
			_i2c_gate_ctrl(fe, 1);
			_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
			->device_tuner_ops.set_params(fe);
			_i2c_gate_ctrl(fe, 0);
		}

		// tuner get parameters
		if (_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
			->get_params) {
			_i2c_gate_ctrl(fe, 1);
			_cfm_info_table[(unsigned int)(fe->id)]
				.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
				->get_params(fe, &common_param);
			_i2c_gate_ctrl(fe, 0);
			memcpy(&_cfm_info_table[(unsigned int)(fe->id)]
				.tuner_pri_data[(unsigned int)(tuner_idx_use)].common_parameter
				, &common_param, sizeof(struct tuner_common_parameter));
		}

		// dish init and tune
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops) {
			// dish init
			ret = _check_and_do_dish_init(fe);
			if (ret)
				pr_err("[%s][%d] dish init fail\n", __func__, __LINE__);

			// dish tune
			if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->tune)
				_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->tune(fe);
		}

		ret = _confirm_which_demod_use(fe, &dmd_idx_use);
		pr_info("[%s][%d], dmd_idx_use = [%d]\n",
			__func__, __LINE__, dmd_idx_use);

		if ((ret == FALSE)
			|| (!(_cfm_info_table[(unsigned int)(fe->id)]
			.reg_demod_ops[(unsigned int)(dmd_idx_use)])))
			return -EINVAL;

		// demod tune
		if (_cfm_info_table[(unsigned int)(fe->id)]
			.reg_demod_ops[(unsigned int)(dmd_idx_use)]->tune)
			_cfm_info_table[(unsigned int)(fe->id)]
			.reg_demod_ops[(unsigned int)(dmd_idx_use)]->tune(fe,
				re_tune, mode_flags, delay, status);
	}

	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)]
		.reg_demod_ops[(unsigned int)(dmd_idx_use)])))
		return -EINVAL;


	if (mode_flags & FE_TUNE_MODE_STOPTUNE)
	{
		pr_info("[%s][%d] mode_flags & FE_TUNE_MODE_STOPTUNE is TRUE\n", __func__, __LINE__);
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]->stop_tune)
			_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]->stop_tune(fe, status);
	}
	else
	{
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]
			->read_status)
			_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]
			->read_status(fe, status);
	}

	return ret;
}

int cfm_set_frontend(struct dvb_frontend *fe)
{
	int ret = 0;
	struct dtv_frontend_properties *c;
	/*fake para*/
	struct analog_parameters a_para	= {0};
	int tuner_idx_use;
	int dmd_idx_use;
	struct tuner_common_parameter common_param;

	bool re_tune = true;
	unsigned int dummy_mode_flags = 0;
	unsigned int *delay, dummy_delay = 0;
	enum fe_status *status, dummy_status = 0;
	delay = &dummy_delay;
	status = &dummy_status;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	c = &fe->dtv_property_cache;

	if (_confirm_which_tuner_use(fe, &tuner_idx_use) == FALSE)
		return -EINVAL;
	pr_info("[%s][%d],	tuner_idx_use = [%d]\n", __func__, __LINE__, tuner_idx_use);

	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;

	// tuner init
	ret = _check_and_do_tuner_init(tuner_idx_use, fe);
	if (ret)
		pr_err("[%s][%d] tuner init fail\n", __func__, __LINE__);

	// tuner set parameters
	if ((c->delivery_system == SYS_ANALOG) &&
		(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
		->device_tuner_ops.set_analog_params)) {
		_i2c_gate_ctrl(fe, 1);
		_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
		->device_tuner_ops.set_analog_params(fe, &a_para);
		_i2c_gate_ctrl(fe, 0);
	} else {
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
		->device_tuner_ops.set_params) {
			_i2c_gate_ctrl(fe, 1);
			_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
			->device_tuner_ops.set_params(fe);
			_i2c_gate_ctrl(fe, 0);
		}
	}

	// tuner get parameters
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]->get_params) {
		_i2c_gate_ctrl(fe, 1);
		_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
			->get_params(fe, &common_param);
		_i2c_gate_ctrl(fe, 0);
		memcpy(&_cfm_info_table[(unsigned int)(fe->id)]
			.tuner_pri_data[(unsigned int)(tuner_idx_use)].common_parameter
			, &common_param, sizeof(struct tuner_common_parameter));
	}
	// dish init
	ret = _check_and_do_dish_init(fe);
	if (ret)
		pr_err("[%s][%d] dish init fail\n", __func__, __LINE__);

	if (_confirm_which_demod_use(fe, &dmd_idx_use) == FALSE)
		return -EINVAL;
	pr_info("[%s][%d], dmd_idx_use = [%d]\n",
		__func__, __LINE__, dmd_idx_use);

	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	// demod init
	ret = _check_and_do_demod_init(dmd_idx_use, fe);
	if (ret)
		pr_err("[%s][%d] demod init fail\n", __func__, __LINE__);

	// demod config
	//if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->set_demod)
	//	_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->set_demod(fe);

	//ckang test use tune to set
	// demod tune
	if (_cfm_info_table[(unsigned int)(fe->id)]
		.reg_demod_ops[(unsigned int)(dmd_idx_use)]->tune)
		_cfm_info_table[(unsigned int)(fe->id)]
		.reg_demod_ops[(unsigned int)(dmd_idx_use)]->tune(fe,
			re_tune, dummy_mode_flags, delay, status);
/*
	if (mode_flags & FE_TUNE_MODE_STOPTUNE)
	{
		pr_info("[%s][%d] mode_flags & FE_TUNE_MODE_STOPTUNE is TRUE\n", __func__, __LINE__);
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]->stop_tune)
			_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]->stop_tune(fe, status);
	}
	else
	{
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]
			->read_status)
			_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[(unsigned int)(dmd_idx_use)]
			->read_status(fe, status);
	}
*/
	return ret;
}

int cfm_get_frontend(struct dvb_frontend *fe,
					struct dtv_frontend_properties *props)
{
	int ret = 0;
	int dmd_idx_use;
	int tuner_idx_use;
	struct dtv_fe_stats *stats;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe) || !(props))
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;
	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;

	stats = &props->strength;
	stats->stat[0].svalue = 0xFFFF;
	stats->stat[0].scale = FE_SCALE_DECIBEL;

	//2022.09.23, need to update tuner signal strength first and then get right SSI.
	// get tuner's related information
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]->get_tuner) {
		_i2c_gate_ctrl(fe, 1);
		_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
		->get_tuner(fe, props);
		_i2c_gate_ctrl(fe, 0);
	}

	// demod get frontend
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->get_demod)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->get_demod(fe, props);

	// get dish's related information
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops) {
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->get_dish)
			_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->get_dish(fe, props);
	}

	return ret;
}

int cfm_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	int ret = 0;
	int dmd_idx_use;

	//pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	// demod get status
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->read_status)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->read_status(fe, status);

	return ret;
}

static enum dvbfe_algo cfm_get_frontend_algo(struct dvb_frontend *fe)
{
	enum dvbfe_algo frontend_algo = 0;
	int dmd_idx_use;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	// demod get frontend
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->get_frontend_algo)
		frontend_algo = _cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->get_frontend_algo(fe);

	return frontend_algo;
}

int cfm_sleep(struct dvb_frontend *fe)
{
	int ret = 0;
	int tuner_idx_use;
	int dmd_idx_use;
	pr_info("[%s][%d]\n", __func__,	__LINE__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	// to prevent priv not is NULL bug
	fe->tuner_priv = _cfm_info_table[(unsigned int)(fe->id)].tuner_pri_data[tuner_idx_use]
					.vendor_tuner_priv;
	fe->demodulator_priv = _cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->demodulator_priv;

	// tuner sleep
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
		->device_tuner_ops.release) {
		_i2c_gate_ctrl(fe, 1);
		_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
			->device_tuner_ops.release(fe);
		_i2c_gate_ctrl(fe, 0);
		_cfm_info_table[(unsigned int)(fe->id)].act_tuner_index = NULL_INDEX;
	}

	// demod sleep
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->sleep) {
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->sleep(fe);
		_cfm_info_table[(unsigned int)(fe->id)].act_demod_index = NULL_INDEX;
	}

	// dish sleep
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops) {
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->sleep) {
			_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->sleep(fe);
			_cfm_info_table[(unsigned int)(fe->id)].act_dish_index = NULL_INDEX;
		}
	}

	Unicable_blindscan = 0;
	Unicable_flag  = FALSE;
	JESS_flag = FALSE;
	return ret;
}

int cfm_get_tune_settings(struct dvb_frontend *fe,
	struct dvb_frontend_tune_settings *settings)
{
	int ret = 0;
	int dmd_idx_use;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	// demod get frontend
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->get_tune_settings)
		ret = _cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->get_tune_settings(fe, settings);

	return ret;
}

int cfm_set_tone(struct dvb_frontend *fe, enum fe_sec_tone_mode tone)
{
	int ret = 0;
	int dmd_idx_use = 0;
	int tuner_idx_use = 0;
	struct tuner_common_parameter common_param;

	pr_info("[%s][%d] %d\n", __func__, __LINE__, tone);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));

	if (_confirm_which_tuner_use(fe, &tuner_idx_use) == FALSE)
		return -EINVAL;

	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;


	// tuner get parameters(agc_slope)
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]->get_params) {
		if (_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
			->get_params(fe, &common_param) != 0) {
			// tuner init
			ret = _check_and_do_tuner_init(tuner_idx_use, fe);
			_cfm_info_table[(unsigned int)(fe->id)]
						.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
						->get_params(fe, &common_param);
		}

		memcpy(&_cfm_info_table[(unsigned int)(fe->id)]
			.tuner_pri_data[(unsigned int)(tuner_idx_use)].common_parameter
			, &common_param, sizeof(struct tuner_common_parameter));
	}

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->set_tone)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->set_tone(fe, tone);

	return ret;
}

int cfm_diseqc_send_burst(struct dvb_frontend *fe,
					enum fe_sec_mini_cmd minicmd)
{
	int ret = 0;
	int dmd_idx_use;
	pr_info("[%s][%d] %d\n", __func__, __LINE__, minicmd);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->diseqc_send_burst)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->diseqc_send_burst(fe, minicmd);

	return ret;
}

int cfm_diseqc_send_master_cmd(struct dvb_frontend *fe,
	struct dvb_diseqc_master_cmd *cmd)
{
	int ret = 0;
	int dmd_idx_use;
	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	//extra command to set user band frequency for unicable blindscan
	if ((cmd->msg[0] == 0xF0)) {
		user_band_freq = cmd->msg[2];
		user_band_freq <<= 8;
		user_band_freq |= cmd->msg[3];
		pr_info("user_band_freq: %d\n", user_band_freq);
		return ret;
	} else if ((cmd->msg[0]&0xF0) == 0x70) {
		JESS_flag = TRUE;
		pr_info("[%s][%d] lnbtype eLNBTYPE_JESS", __func__, __LINE__);
	} else if (cmd->msg_len >= 3 && ((cmd->msg[2]&0xF0) == 0x50)) {
		Unicable_flag  = TRUE;
		pr_info("[%s][%d] lnbtype eLNBTYPE_UNICABLE", __func__, __LINE__);
	} else {
		Unicable_flag  = FALSE;
		JESS_flag = FALSE;
	}
	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->diseqc_send_master_cmd)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->diseqc_send_master_cmd(fe, cmd);

	return ret;
}

static int cfm_blindscan_start(struct dvb_frontend *fe,
				struct DMD_DVBS_BlindScan_Start_param *Start_param)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct tuner_common_parameter common_param;
	int ret = 0;
	int tuner_idx_use;
	int dmd_idx_use;

	pr_info("[%s] is called\n", __func__);
	pr_info("delivery_system%d\n", c->delivery_system);

	//check null ptr
	if (!(fe))
		return -EINVAL;
	if (JESS_flag || Unicable_flag)
		Unicable_blindscan = 1;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));

	c =	&fe->dtv_property_cache;

	if (_confirm_which_tuner_use(fe, &tuner_idx_use) == FALSE)
		return -EINVAL;

	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;

	// tuner init
	ret = _check_and_do_tuner_init(tuner_idx_use, fe);

	// tuner get parameters(agc_slope)
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]->get_params) {
		_cfm_info_table[(unsigned int)(fe->id)]
			.reg_tuner_ops[(unsigned int)(tuner_idx_use)]
			->get_params(fe, &common_param);
		memcpy(&_cfm_info_table[(unsigned int)(fe->id)]
			.tuner_pri_data[(unsigned int)(tuner_idx_use)].common_parameter
			, &common_param, sizeof(struct tuner_common_parameter));
	}

	if (_confirm_which_demod_use(fe, &dmd_idx_use) == FALSE)
		return -EINVAL;

	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->blindscan_start)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
			->blindscan_start(fe, Start_param);

	return ret;
}

static int cfm_blindscan_get_tuner_freq(struct dvb_frontend *fe,
				struct DMD_DVBS_BlindScan_GetTunerFreq_param *GetTunerFreq_param)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int ret = 0;
	int tuner_idx_use;
	int dmd_idx_use;
	pr_info("[%s] is called\n", __func__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	//get tuner frequency from demod DD
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->blindscan_get_tuner_freq)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->blindscan_get_tuner_freq(
			fe, GetTunerFreq_param);

	c->symbol_rate = GetTunerFreq_param->TunerCutOffFreq*transfer_unit;
	c->frequency = (GetTunerFreq_param->TunerCenterFreq)*transfer_unit;

	return ret;
}

static int cfm_blindscan_next_freq(struct dvb_frontend *fe,
				struct DMD_DVBS_BlindScan_NextFreq_param *NextFreq_param)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int ret = 0;
	int tuner_idx_use;
	int dmd_idx_use;
	pr_info("[%s] is called\n", __func__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));
	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));

	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;

	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (JESS_flag || Unicable_flag)
		Unicable_blindscan = 1;

	//if it is unicable blindscan, then we should set user band frequency to tuner
	if (Unicable_blindscan)
		c->frequency = user_band_freq*transfer_unit;

	//set tuner parameter
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
		->device_tuner_ops.set_params) {
		_i2c_gate_ctrl(fe, 1);
		_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
			->device_tuner_ops.set_params(fe);
		_i2c_gate_ctrl(fe, 0);
		}

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->blindscan_next_freq)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->blindscan_next_freq(fe,
			NextFreq_param);

	return ret;
}

static int cfm_blindscan_wait_curfreq_finished(struct dvb_frontend *fe,
		struct DMD_DVBS_BlindScan_WaitCurFreqFinished_param *WaitCurFreqFinished_param)
{
	int ret = 0;
	int dmd_idx_use;
	pr_info("[%s] is called\n", __func__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->blindscan_wait_curfreq_finished)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
			->blindscan_wait_curfreq_finished(fe, WaitCurFreqFinished_param);

	return ret;
}

static int cfm_blindscan_get_channel(struct dvb_frontend *fe,
				struct DMD_DVBS_BlindScan_GetChannel_param *GetChannel_param)
{
	int ret = 0;
	int dmd_idx_use;
	pr_info("[%s] is called\n", __func__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->blindscan_get_channel)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->blindscan_get_channel(fe,
			GetChannel_param);

	return ret;
}

static int cfm_blindscan_end(struct dvb_frontend *fe)
{
	int ret = 0;
	int dmd_idx_use;
	pr_info("[%s] is called\n", __func__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->blindscan_end)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->blindscan_end(fe);

	Unicable_blindscan = 0;
	//user_band_freq = 0xFFFF;
	Unicable_flag  = FALSE;
	JESS_flag = FALSE;
	return ret;
}

static int cfm_blindscan_cancel(struct dvb_frontend *fe)
{
	int ret = 0;
	int dmd_idx_use;
	pr_info("[%s] is called\n", __func__);

	//check null ptr
	if (!(fe))
		return -EINVAL;

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));
	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->blindscan_cancel)
		_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
		->blindscan_cancel(fe);

	Unicable_blindscan = 0;
	user_band_freq = 0xFFFF;
	return ret;
}

int cfm_set_voltage(struct dvb_frontend *fe,
				enum fe_sec_voltage voltage)
{
	struct dtv_frontend_properties *c;
	int ret = 0;

	//check null ptr
	if (!(fe))
		return -EINVAL;
	c = &fe->dtv_property_cache;

	pr_info("[%s][%d]\n", __func__, __LINE__);
	pr_info("[%d]voltage= %d \r\n", __LINE__, (int)voltage);
	c->voltage = voltage;
	// dish set voltage
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops) {
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->set_voltage)
			_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->set_voltage(fe);
	}
	return ret;
}
enum lnb_overload_status cfm_get_lnb_overload_status(struct dvb_frontend *fe)
{
	enum lnb_overload_status overload = 0;

	if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops) {
		if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->lnb_short_status)
			overload = _cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops
			->lnb_short_status(fe);
	}

	return overload;
}

struct dvb_frontend_ops _common_FE_ops = {
	.info = {
		.name = "common frontend manager",
//  .frequency_min_hz  = 42 * MHz,
//  .frequency_max_hz  = 2150 * MHz,
	},
	.init = cfm_init,
	.tune = cfm_tune,
	.set_frontend = cfm_set_frontend,
	.get_frontend = cfm_get_frontend,
	.read_status = cfm_read_status,
	.get_frontend_algo = cfm_get_frontend_algo,
	.sleep = cfm_sleep,
	.get_tune_settings = cfm_get_tune_settings,
	.set_tone = cfm_set_tone,
	.diseqc_send_burst = cfm_diseqc_send_burst,
	.diseqc_send_master_cmd = cfm_diseqc_send_master_cmd,
	.set_voltage = cfm_set_voltage,
	.set_lna = cfm_set_lna,
	.blindscan_start = cfm_blindscan_start,
	.blindscan_get_tuner_freq = cfm_blindscan_get_tuner_freq,
	.blindscan_next_freq = cfm_blindscan_next_freq,
	.blindscan_wait_curfreq_finished = cfm_blindscan_wait_curfreq_finished,
	.blindscan_get_channel = cfm_blindscan_get_channel,
	.blindscan_end = cfm_blindscan_end,
	.blindscan_cancel = cfm_blindscan_cancel,
	.get_lnb_overload_status  = cfm_get_lnb_overload_status,
};

int cfm_probe(struct platform_device *pdev)
{
	struct device_node *cfm_nodes_list;
	int ret = 0;
	int i, j;
	int dev_num = 0;

	// parse group information from DTB
	int virtual_fe_num = 0;
	int readOutValue = 0xffff;

		// create fe
	struct dvb_frontend *fe_dev;
//	struct dvb_adapter *adapter;

	pr_info("[%s][%d]\r\n", __func__, __LINE__);
	reg_demod_num = 0;
	reg_demod_times = 0;
	reg_tuner_times = 0;

	while (1) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
		"vritual_fe_id", virtual_fe_num, &readOutValue);

		if (ret == 0) {
			virtual_fe_num++;
			pr_info("[%s][%d] is called, fe id = [%d]\n",
				__func__, __LINE__, readOutValue);
		} else {
			pr_info("[%s][%d] is called fail\n",
				__func__, __LINE__);
			break;
		}
	}
	readOutValue = 0xffff;
	reg_fe_number = virtual_fe_num;

	if (virtual_fe_num > MAX_VIRTUAL_FE_NUM) {
		pr_err("[%s][%d] Over the maximal FE number\n", __func__, __LINE__);
		return -ENODEV;
	}
	pr_info("[%s][%d] virtual_fe_num = [%d]\n",
		__func__, __LINE__, reg_fe_number);

	// parse demod, tuner, lnb from device tree
	for (i = 0; i < virtual_fe_num; i++) {
		cfm_nodes_list = of_parse_phandle(pdev->dev.of_node,
			"cfm_group", i);

		if (!cfm_nodes_list)
			pr_info("cfm_node %d not defined in device tree\n", i);
		else {
			dev_num = of_property_count_u32_elems(cfm_nodes_list, "demods");
			dev_num = (dev_num < MAX_DEMOD_NUMBER_FOR_FRONTEND) ? dev_num
				: MAX_DEMOD_NUMBER_FOR_FRONTEND;
			//demod
			for (j = 0; j < dev_num; j++) {
				ret = of_property_read_u32_index(cfm_nodes_list,
					"demods", j, &readOutValue);

				pr_emerg("[%d]fe group [%d], demod %d id = [%d]\n",
				__LINE__, i, j, readOutValue);

				if (ret != 0)
					pr_emerg("[%d] cfm parse fail\n", __LINE__);
				else {
					_cfm_info_table[i].demod_unique_id[j] = readOutValue;

					if (readOutValue != 0xffff)
						_cfm_info_table[i].demod_device_number++;
				}
			}
			// total demod number
			reg_demod_num += _cfm_info_table[i].demod_device_number;

			dev_num = of_property_count_u32_elems(cfm_nodes_list, "tuners");
			dev_num = (dev_num < MAX_TUNER_NUMBER_FOR_FRONTEND) ? dev_num
				: MAX_TUNER_NUMBER_FOR_FRONTEND;
			//tuner
			for (j = 0; j < dev_num; j++) {
				ret = of_property_read_u32_index(cfm_nodes_list,
					"tuners", j, &readOutValue);

				pr_emerg("[%d]fe group [%d], tuner %d id = [%d]\n",
				__LINE__, i, j, readOutValue);

				if (ret != 0)
					pr_emerg("[%d] cfm parse fail\n", __LINE__);
				else {
					_cfm_info_table[i].tuner_unique_id[j] = readOutValue;

					if (readOutValue != 0xffff)
						_cfm_info_table[i].tuner_device_number++;
				}
			}
			// total tuner number
			reg_tuner_num += _cfm_info_table[i].tuner_device_number;

			//dev_num = of_property_count_u32_elems(cfm_nodes_list, "lnbs");
			//lnb
			//for (j = 0; j < dev_num; j++) {
				ret = of_property_read_u32_index(cfm_nodes_list,
					"lnbs", 0, &readOutValue);

				//pr_emerg("[%d]fe group [%d], lnb %d id = [%d]\n",
				//__LINE__, i, j, readOutValue);

				if (ret != 0)
					pr_emerg("[%d] cfm parse fail\n", __LINE__);
				else
					_cfm_info_table[i].dish_unique_id = readOutValue;
			//}
		}
		_cfm_info_table[i].reg_dish_ops = NULL;
	}

	for (i = 0; i < virtual_fe_num; i++) {
		fe_dev = devm_kzalloc(&(pdev->dev), sizeof(struct dvb_frontend), GFP_ATOMIC);
		if (!fe_dev)
			return -ENOMEM;
		_common_FE_ops.set_voltage = NULL;

		memcpy(&fe_dev->ops, &_common_FE_ops, sizeof(struct dvb_frontend_ops));

		// set frontend id
		fe_dev->id = i;

		// save fe
		_cfm_info_table[i].fe = fe_dev;
	}

	mutex_init(&cfm_demod_mutex);
	mutex_init(&cfm_tuner_mutex);
	mutex_init(&cfm_dish_mutex);
	mutex_init(&cfm_notify_mutex);
	INIT_LIST_HEAD(&notify_0.list);
	return ret;
}

int cfm_remove(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("[%s] is called\n", __func__);

// demod_dev = platform_get_drvdata(pdev);
// dvb_unregister_frontend(fe);

	return ret;
}

// device_unique_id: device unique id for which device notify
// event_type: event type for device notify
// props: parameters for device notify
void dd_notify(int device_unique_id,
							enum cfm_notify_event_type_t event_type,
							struct cfm_properties *props)
{

	struct list_head *pos;
	struct cfm_notify *pNotify;
	int i, j;
	bool bMatch = 0;
	struct dvb_frontend *fe = NULL;

	mutex_lock(&cfm_notify_mutex);
	pr_info("[%s][%d]\n", __func__, __LINE__);

	// check if device_unique_id is valid and find out the according fe
	for (i = 0; i < reg_fe_number; i++) {
		// search demod
		for (j = 0; j < _cfm_info_table[i].demod_device_number; j++) {
			if (device_unique_id == _cfm_info_table[i].demod_unique_id[j]) {
				fe = _cfm_info_table[i].fe;
				bMatch = 1;
				goto searchdone;
			}
		}
		// search tuner
		for (j = 0; j < _cfm_info_table[i].tuner_device_number; j++) {
			if (device_unique_id == _cfm_info_table[i].tuner_unique_id[j]) {
				fe = _cfm_info_table[i].fe;
				bMatch = 1;
				goto searchdone;
			}
		}
		// search dish
		if (device_unique_id == _cfm_info_table[i].dish_unique_id) {
			fe = _cfm_info_table[i].fe;
			bMatch = 1;
			goto searchdone;
		}
	}
searchdone:

	if (bMatch == 0) {
		pr_emerg("[%s][%d]unique id[0x%x] is not found!\r\n",
		__func__, __LINE__, device_unique_id);
//		return -ENODEV;
	}

	if (fe == NULL) {
		pr_emerg("[%s][%d]fe is a null pointer!\r\n",
		__func__, __LINE__);
	}
	// broadcast to external cb
	list_for_each(pos, &notify_0.list) {
		pNotify = list_entry(pos, struct cfm_notify, list);
		pNotify->notify(fe, event_type, props, pNotify->notify_data);
	}
	mutex_unlock(&cfm_notify_mutex);
//	return 0;
}

int cfm_register_notify(struct cfm_notify *nptr)
{
	int ret = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);
	list_add(&nptr->list, &notify_0.list);

	return ret;
}
EXPORT_SYMBOL(cfm_register_notify);

int cfm_unregister_notify(struct cfm_notify *nptr)
{
	int ret = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);
	list_del(&nptr->list);

	return ret;
}
EXPORT_SYMBOL(cfm_unregister_notify);

int cfm_register_demod_device(struct dvb_adapter *adapter,
				struct cfm_demod_ops *demod_ops)
{
	int ret = 0;
	int i, j, k, m;
	int known_sys_num = 0;
	bool bMatch = 0;

	mutex_lock(&cfm_demod_mutex);

	pr_info("[%s][%d] reg_fe_number:%d device_unique_id:0x%x\n",
		__func__, __LINE__, reg_fe_number, demod_ops->device_unique_id);

	//get unique id to compare with unique id saved in cfm_info_table
	for (i = 0; i < reg_fe_number; i++) {
		known_sys_num = _cfm_info_table[i].del_sys_num;
		pr_info("[%s][%d] i:%d known_sys_num:%d\n",
			__func__, __LINE__, i, known_sys_num);
		for (j = 0; j < _cfm_info_table[i].demod_device_number; j++) {
			pr_info("[%s][%d] i:%d j:%d unique_id:0x%x\n",
				__func__, __LINE__, i, j, _cfm_info_table[i].demod_unique_id[j]);
			if ((demod_ops->device_unique_id)
					== _cfm_info_table[i].demod_unique_id[j]) {

				// save demod ops to _cfm_info_table
				_cfm_info_table[i].reg_demod_ops[j] = demod_ops;

				// save adapter for register
				_cfm_info_table[i].adapter[j] = adapter;

				// emerg, known_sys_num+j may exceed MAX
				for (k = 0; k < (demod_ops->chip_delsys.size); k++) {
					_cfm_info_table[i].fe->ops.delsys[known_sys_num+k]
						= (demod_ops->chip_delsys.element[k]);
				}
				_cfm_info_table[i].del_sys_num = _cfm_info_table[i].del_sys_num
					+ demod_ops->chip_delsys.size;

				if (_cfm_info_table[i].reg_demod_ops[j]->set_notify)
					_cfm_info_table[i].reg_demod_ops[j]
					->set_notify(demod_ops->device_unique_id, dd_notify);

				// register sysfs node
				if ((!reg_sysfs_node) && (adapter != NULL)) {
					for (k = 0; k < reg_fe_number; k++)
						sysfs_create_group(&adapter->device->kobj,
							mtk_frontend_attr_groups[k]);

					reg_sysfs_node = true;
					pr_info("[%s][%d]cfm register sysfs nodes to dvbcore\n",
						__func__, __LINE__);
				}

				pr_info("[%s][%d]demod register to cfm successfully,fe[%d], demod[%d], unique id[0x%x]\n"
				, __func__, __LINE__, i, j, demod_ops->device_unique_id);

				// increment reg_demod_times
				// since demod register to cfm successfully
				if (_cfm_info_table[i].reg_demod_ops[j] != NULL)
					reg_demod_times++;
				else
					pr_err("[%s][%d]demod_ops[%d][%d] is NULL\n",
					__func__, __LINE__, i, j);

				bMatch = 1;
				//break;
			}
			//break;
		}
	}

	// create frontend node if both tuner and demod ops are exists
	if (reg_demod_times + reg_tuner_times == reg_demod_num + reg_tuner_num) {
		for (m = 0; m < reg_fe_number; m++) {
			if ((!_cfm_info_table[m].fenode_set_al)
				&& (_cfm_info_table[m].adapter[0] != NULL)) {
				// register to dvb core to generate frontend node
				ret = dvb_register_frontend(_cfm_info_table[m].adapter[0],
				_cfm_info_table[m].fe);

				_cfm_info_table[m].fenode_set_al = true;

				pr_info("[%s][%d]cfm register frontend[%d] to dvbcore\n",
						__func__, __LINE__, m);
			}
		}
	}

	if (bMatch == 0) {
		pr_info("[%s][%d]demod register to cfm failed, unique id[0x%x]\r\n",
		__func__, __LINE__, demod_ops->device_unique_id);
	}
	mutex_unlock(&cfm_demod_mutex);
	return ret;
}
EXPORT_SYMBOL(cfm_register_demod_device);

int cfm_register_tuner_device(struct dvb_adapter *adapter,
				struct cfm_tuner_ops *tuner_ops)
{

	int ret = 0;
	int i, j, m;
	bool bMatch = 0;

	mutex_lock(&cfm_tuner_mutex);

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//get unique id to compare with unique id saved in cfm_info_table
	for (i = 0; i < reg_fe_number; i++) {
		for (j = 0; j < _cfm_info_table[i].tuner_device_number; j++) {
			if ((tuner_ops->device_unique_id)
				== _cfm_info_table[i].tuner_unique_id[j]) {

				// save tuner ops to _cfm_info_table
				_cfm_info_table[i].reg_tuner_ops[j] = tuner_ops;

				// save tuner private data
				_cfm_info_table[i].tuner_pri_data[j].vendor_tuner_priv
				= tuner_ops->tuner_priv;

				if (_cfm_info_table[i].reg_tuner_ops[j]->set_notify)
					_cfm_info_table[i].reg_tuner_ops[j]
					->set_notify(tuner_ops->device_unique_id, dd_notify);

				pr_info("[%s][%d]tuner register to cfm successfully, fe[%d], tuner[%d], unique id[0x%x]\n"
				, __func__, __LINE__, i, j, tuner_ops->device_unique_id);

				if (_cfm_info_table[i].reg_tuner_ops[j] != NULL)
					reg_tuner_times++;
				else
					pr_err("[%s][%d]tuner_ops[%d][%d] is NULL\n",
					__func__, __LINE__, i, j);

				bMatch = 1;
				//i = reg_fe_number+1;
				//break;
			}
		}
	}

	// create frontend node if both tuner and demod ops are exists
	if (reg_demod_times + reg_tuner_times == reg_demod_num + reg_tuner_num) {
		for (m = 0; m < reg_fe_number; m++) {
			if ((!_cfm_info_table[m].fenode_set_al)
				&& (_cfm_info_table[m].adapter[0] != NULL)) {
				// register to dvb core to generate frontend node
				ret = dvb_register_frontend(_cfm_info_table[m].adapter[0],
				_cfm_info_table[m].fe);

				_cfm_info_table[m].fenode_set_al = true;

				pr_info("[%s][%d]cfm register frontend[%d] to dvbcore\n",
						__func__, __LINE__, m);
			}
		}
	}

	if (bMatch == 0) {
		pr_info("[%s][%d]tuner register to cfm failed, unique id[0x%x]\r\n",
		__func__, __LINE__, tuner_ops->device_unique_id);
	}
	mutex_unlock(&cfm_tuner_mutex);
	return ret;
}
EXPORT_SYMBOL(cfm_register_tuner_device);

int cfm_register_dish_device(struct dvb_adapter *adapter,
				struct cfm_dish_ops *dish_ops)
{

	int ret = 0;
	int i;
	bool bMatch = 0;

	mutex_lock(&cfm_dish_mutex);

	pr_info("[%s][%d]dish_ops->device_unique_id = [%d]\n", __func__,
		__LINE__, dish_ops->device_unique_id);

	//get unique id to compare with unique id saved in cfm_info_table
	for (i = 0; i < reg_fe_number; i++) {
		if ((dish_ops->device_unique_id) ==
			_cfm_info_table[i].dish_unique_id) {

			// save dish ops to _cfm_info_table
			_cfm_info_table[i].reg_dish_ops = dish_ops;
			// save dish private data
			_cfm_info_table[i].fe->sec_priv = dish_ops->sec_priv;

			if (_cfm_info_table[i].reg_dish_ops->set_notify)
				_cfm_info_table[i].reg_dish_ops
				->set_notify(dish_ops->device_unique_id, dd_notify);

			_cfm_info_table[i].fe->ops.set_voltage = cfm_set_voltage;

			pr_info("[%s][%d]dish register to cfm successfully, fe[%d], unique id[0x%x]\n",
				__func__, __LINE__, i, dish_ops->device_unique_id);

			bMatch = 1;
			//break;
		}
	}

	if (bMatch == 0) {
		pr_info("[%s][%d]dish register to cfm failed, unique id[0x%x]\r\n",
		__func__, __LINE__, dish_ops->device_unique_id);
	}
	mutex_unlock(&cfm_dish_mutex);
	return ret;
}
EXPORT_SYMBOL(cfm_register_dish_device);

int cfm_get_demod_property(struct dvb_frontend *fe, struct cfm_properties *demodProps)
{

	int ret = 0;
	u8 dmd_idx_use = 0;

	mutex_lock(&cfm_demod_mutex);
	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe) || !(demodProps)) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]fe or demodProps is null!\n",
		__func__, __LINE__);
		return FALSE;
	}

	dmd_idx_use = SEL_DEMOD_IND((unsigned int)(fe->id));

	if ((dmd_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use])))
		return -EINVAL;

	// cfm get data from demod get parameter API
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]->get_param) {
		ret = _cfm_info_table[(unsigned int)(fe->id)].reg_demod_ops[dmd_idx_use]
				->get_param(fe, demodProps);
	}
	mutex_unlock(&cfm_demod_mutex);
	return ret;
}
EXPORT_SYMBOL(cfm_get_demod_property);

int cfm_get_tuner_property(struct dvb_frontend *fe, struct cfm_properties *tunerProps)
{

	int ret = 0;
	u8 tuner_idx_use = 0;

	mutex_lock(&cfm_tuner_mutex);
	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe) || !(tunerProps)) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]fe or tunerProps is null!\n",
		__func__, __LINE__);
		return FALSE;
	}

	tuner_idx_use = SEL_TUNER_IND((unsigned int)(fe->id));

	if ((tuner_idx_use < 0)
		|| (!(_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use])))
		return -EINVAL;

	// cfm get data from tuner get parameter API
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]->get_param) {
		_i2c_gate_ctrl(fe, 1);
		ret = _cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[tuner_idx_use]
				->get_param(fe, tunerProps);
		_i2c_gate_ctrl(fe, 0);
	}

	mutex_unlock(&cfm_tuner_mutex);
	return ret;
}
EXPORT_SYMBOL(cfm_get_tuner_property);

int cfm_get_dish_property(struct dvb_frontend *fe, struct cfm_properties *dishProps)
{
	int ret = 0;

	mutex_lock(&cfm_dish_mutex);
	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(fe) || !(dishProps)) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]fe or dishProps is null!\n",
		__func__, __LINE__);
		return FALSE;
	}

	if (!(_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops))
		return -EINVAL;

	// cfm get data from dish get parameter API
	if (_cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops->get_param) {
		ret = _cfm_info_table[(unsigned int)(fe->id)].reg_dish_ops
				->get_param(fe, dishProps);
	}

	mutex_unlock(&cfm_dish_mutex);
	return ret;
}
EXPORT_SYMBOL(cfm_get_dish_property);

// order of suspend action: demod -> tuner -> dish
static int cfm_suspend(struct device *dev)
{
	int ret = 0;
	int i, j;
	struct dvb_frontend *fe = NULL;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(dev)) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]dev is null!\n", __func__, __LINE__);
		return FALSE;
	}

	for (i = 0; i < reg_fe_number; i++) {
		fe = _cfm_info_table[i].fe;

		// demod
		pr_info("[%s][%d]demod suspend!\n", __func__, __LINE__);
		for (j = 0; j < _cfm_info_table[i].demod_device_number; j++) {
			if (_cfm_info_table[i].reg_demod_ops[j]) {
				if (_cfm_info_table[i].reg_demod_ops[j]->suspend)
					_cfm_info_table[i].reg_demod_ops[j]->suspend(fe);
			}
		}

		// tuner
		pr_info("[%s][%d]tuner suspend!\n", __func__, __LINE__);
		for (j = 0; j < _cfm_info_table[i].tuner_device_number; j++) {
			if (_cfm_info_table[i].reg_tuner_ops[j]) {
				if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[j]
					->device_tuner_ops.suspend) {
					_i2c_gate_ctrl(fe, 1);
					_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[j]
						->device_tuner_ops.suspend(fe);
					_i2c_gate_ctrl(fe, 0);
				}
			}
		}

		// dish
		pr_info("[%s][%d]dish suspend!\n", __func__, __LINE__);
		if (_cfm_info_table[i].reg_dish_ops) {
			if (_cfm_info_table[i].reg_dish_ops->suspend)
				_cfm_info_table[i].reg_dish_ops->suspend(fe);
		}
	}

	return ret;
}

// order of resume action: dish -> tuner -> demod
static int cfm_resume(struct device *dev)
{
	int ret = 0;
	int i, j;
	struct dvb_frontend *fe = NULL;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	//check null ptr
	if (!(dev)) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]dev is null!\n", __func__, __LINE__);
		return FALSE;
	}

	for (i = 0; i < reg_fe_number; i++) {
		fe = _cfm_info_table[i].fe;

		// dish
		pr_info("[%s][%d]dish resume!\n", __func__, __LINE__);
		if (_cfm_info_table[i].reg_dish_ops) {
			if (_cfm_info_table[i].reg_dish_ops->resume)
				_cfm_info_table[i].reg_dish_ops->resume(fe);
		}

		// tuner
		pr_info("[%s][%d]tuner resume!\n", __func__, __LINE__);
		for (j = 0; j < _cfm_info_table[i].tuner_device_number; j++) {
			if (_cfm_info_table[i].reg_tuner_ops[j]) {
				if (_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[j]
					->device_tuner_ops.resume) {
					_i2c_gate_ctrl(fe, 1);
					_cfm_info_table[(unsigned int)(fe->id)].reg_tuner_ops[j]
						->device_tuner_ops.resume(fe);
					_i2c_gate_ctrl(fe, 0);
				}
			}
		}

		// demod
		pr_info("[%s][%d]demod resume!\n", __func__, __LINE__);
		for (j = 0; j < _cfm_info_table[i].demod_device_number; j++) {
			if (_cfm_info_table[i].reg_demod_ops[j]) {
				if (_cfm_info_table[i].reg_demod_ops[j]->resume)
					_cfm_info_table[i].reg_demod_ops[j]->resume(fe);
			}
		}
	}
	return ret;
}

static const struct dev_pm_ops cfm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cfm_suspend,
							cfm_resume)
};

static const struct of_device_id cfm_dt_match[] = {
	{ .compatible = CFM_DRV_NAME },
	{},
};

MODULE_DEVICE_TABLE(of, cfm_dt_match);

static struct platform_driver cfm_driver = {
		.probe  = cfm_probe,
		.remove  = cfm_remove,
		.driver  = {
		.name  = CFM_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cfm_dt_match,
		.pm = &cfm_pm_ops,
//  .groups  = cfm_groups
		},
};

module_platform_driver(cfm_driver);

MODULE_DESCRIPTION("Common Frontend Manager");
MODULE_AUTHOR("Mediatek Author");
MODULE_LICENSE("GPL");
