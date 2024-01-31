/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * MediaTek	Inc. (C) 2020. All rights	reserved.
 */
#ifndef _CFM_PRIV_H
#define _CFM_PRIV_H
#include  "cfm.h"

#define MAX_DEMOD_NUMBER_FOR_FRONTEND 5
#define MAX_TUNER_NUMBER_FOR_FRONTEND 5

#define MAX_VIRTUAL_FE_NUM 4
static int reg_fe_number;
static bool reg_sysfs_node;
static struct mutex cfm_tuner_mutex;
static struct mutex cfm_demod_mutex;
static struct mutex cfm_dish_mutex;
static struct mutex cfm_notify_mutex;

#define	transfer_unit	1000
#define NULL_INDEX  0xff
#define DISH_INDEX_USE  0
#define default_value 0xffffffff

struct cfm_info_table {
	struct dvb_frontend *fe;
	bool fenode_set_al;
	// demod
	u16 demod_unique_id[MAX_DEMOD_NUMBER_FOR_FRONTEND];
	struct cfm_demod_ops *reg_demod_ops[MAX_DEMOD_NUMBER_FOR_FRONTEND];
	u8 demod_device_number;
	u8 sel_demod_index;
	u8 act_demod_index;
	u8 del_sys_num;
	struct dvb_adapter *adapter[MAX_DEMOD_NUMBER_FOR_FRONTEND];
	// tuner
	u16 tuner_unique_id[MAX_TUNER_NUMBER_FOR_FRONTEND];
	struct cfm_tuner_ops *reg_tuner_ops[MAX_TUNER_NUMBER_FOR_FRONTEND];
	struct cfm_tuner_pri_data tuner_pri_data[MAX_TUNER_NUMBER_FOR_FRONTEND];
	u8 tuner_device_number;
	u8 sel_tuner_index;
	u8 act_tuner_index;
	// dish
	u16 dish_unique_id;
	struct cfm_dish_ops *reg_dish_ops;
	u8 act_dish_index;
};

struct cfm_notify notify_0;

#endif
