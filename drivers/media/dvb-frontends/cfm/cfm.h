/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * MediaTek	Inc. (C) 2020. All rights	reserved.
 */
#ifndef _CFM_H
#define _CFM_H

#include <linux/printk.h>
#include <media/dvb_frontend.h>
#define CFM_DRV_NAME    "mediatek,cfm"

#define	MAX_SIZE_DEL_SYSTEM	32

// Old method.
struct tuner_common_parameter {
	u16 IF_frequency; //KHz
	bool spectrum_inversion;
	bool AGC_inversion;
};

struct cfm_tuner_pri_data {
	void *vendor_tuner_priv;//vendor
	struct tuner_common_parameter  common_parameter;
};

// New method
/* cfm property Commands */
#define CFM_UNDEFINED       0
#define CFM_IF_FREQ         1
#define CFM_AGC_INV         2
#define CFM_SPECTRUM_INV    3
#define CFM_POWER_LEVEL     4

#define CFM_STREAM_CLOCK_INV	5
#define CFM_STREAM_EXT_SYNC	6
#define CFM_STREAM_DATA_BITS_NUM	7
#define CFM_STREAM_BIT_SWAP	8
#define CFM_TS_SOURCE_PACKET_SIZE	9
#define CFM_STREAM_CLOCK_RATE	10
#define CFM_STREAMING_STATUS	11
#define CFM_DECOUPLE_DMD	12
#define CFM_STREAM_PIN_MUX_INFO	13

// Similar as dtv_property definition of dvb core
struct cfm_property {
	u32 cmd;
	union {
		u32 data;
		s32 data1;
		struct {
			u8 data[32];
			u32 len;
			u32 reserved1[3];
			void *reserved2;
		} buffer;
	} u;
	int result;
};

struct cfm_properties {
	u32 num;
	struct cfm_property *props;
};

// callback method for cfm
enum cfm_notify_event_type_t {
	STREAMING_STATUS_CHANGED = 0,
	STREAM_CONFIGURATION_CHANGED = 1,
};

// @list: List head
// @notify_data: Input data to invoke the callback
// @notify: Callback function pointer
struct cfm_notify {
	struct list_head list;
	void *notify_data;
	void (*notify)(struct dvb_frontend *fe,
				enum cfm_notify_event_type_t event_type,
				struct cfm_properties *props,
				void *notify_data);
};

struct cfm_ext_array {
	int size;
	u8 element[MAX_SIZE_DEL_SYSTEM];
};

typedef struct {
	u32 u32Param1;
	u32 u32Param2;
	void *pParam;
} cfm_device_ext_cmd_param;

typedef enum {
	CFM_TUNER_EXT_CMD_TYPE_LNA_CONTROL,
} cfm_device_ext_cmd_type;

// ops for demod
struct cfm_demod_ops {
	int (*init)(struct dvb_frontend *fe);
	int (*tune)(struct dvb_frontend *fe, bool re_tune, unsigned int mode_flags,
		unsigned int *delay, enum fe_status *status);
	int (*get_demod)(struct dvb_frontend *fe, struct dtv_frontend_properties *props);
	int (*set_demod)(struct dvb_frontend *fe);
	int (*read_status)(struct dvb_frontend *fe, enum fe_status *status);
	enum dvbfe_algo (*get_frontend_algo)(struct dvb_frontend *fe);
	int (*sleep)(struct dvb_frontend *fe);
	int (*get_tune_settings)(struct dvb_frontend *fe,
		struct dvb_frontend_tune_settings *settings);
	int (*diseqc_send_burst)(struct dvb_frontend *fe, enum fe_sec_mini_cmd minicmd);
	int (*diseqc_send_master_cmd)(struct dvb_frontend *fe, struct dvb_diseqc_master_cmd *cmd);
	u16 device_unique_id;
	struct cfm_ext_array chip_delsys;
	void *demodulator_priv;
	int (*set_tone)(struct dvb_frontend *fe, enum fe_sec_tone_mode tone);
	struct dvb_frontend_internal_info info;
	int (*dump_debug_info)(char *buffer, int buffer_size, int *eof, int offset,
						struct dvb_frontend *fe);
	int (*blindscan_start)(struct dvb_frontend *fe,
		struct DMD_DVBS_BlindScan_Start_param *Start_param);
	int (*blindscan_get_tuner_freq)(struct dvb_frontend *fe,
		struct DMD_DVBS_BlindScan_GetTunerFreq_param *GetTunerFreq_param);
	int (*blindscan_next_freq)(struct dvb_frontend *fe,
		struct DMD_DVBS_BlindScan_NextFreq_param *NextFreq_param);
	int (*blindscan_wait_curfreq_finished)(struct dvb_frontend *fe,
		struct DMD_DVBS_BlindScan_WaitCurFreqFinished_param *WaitCurFreqFinished_param);
	int (*blindscan_get_channel)(struct dvb_frontend *fe,
		struct DMD_DVBS_BlindScan_GetChannel_param *GetChannel_param);
	int (*blindscan_end)(struct dvb_frontend *fe);
	int (*blindscan_cancel)(struct dvb_frontend *fe);
	int (*suspend)(struct dvb_frontend *fe);
	int (*resume)(struct dvb_frontend *fe);
	int (*get_param)(struct dvb_frontend *fe, struct cfm_properties *demodProps);
	int (*dvb_frontend_registered)(int device_unique_id, struct dvb_frontend *fe);
	int (*set_notify)(int device_unique_id,
					void (*notify)(int device_unique_id,
						enum cfm_notify_event_type_t event_type,
						struct cfm_properties *props));
	int (*i2c_gate_ctrl)(struct dvb_frontend *fe, int enable);
	int (*stop_tune)(struct dvb_frontend *fe, enum fe_status *status);
};

// ops for tuner
struct cfm_tuner_ops {
	struct dvb_tuner_ops device_tuner_ops;
	int (*get_params)(struct dvb_frontend *fe, struct tuner_common_parameter *data);
	int (*tune)(struct dvb_frontend *fe);
	int (*ext_func)(struct dvb_frontend *fe, cfm_device_ext_cmd_type cmd_type,
		 cfm_device_ext_cmd_param *para);
	u16 device_unique_id;
	struct cfm_ext_array board_delsys;
	struct cfm_ext_array chip_delsys;
	void *tuner_priv;
	int (*dump_debug_info)(char *buffer, int buffer_size, int *eof, int offset,
						struct dvb_frontend *fe);
	int (*get_param)(struct dvb_frontend *fe, struct cfm_properties *tunerProps);
	int (*get_tuner)(struct dvb_frontend *fe, struct dtv_frontend_properties *props);
	int (*set_lna)(struct dvb_frontend *fe);
	int (*dvb_frontend_registered)(int device_unique_id, struct dvb_frontend *fe);
	int (*set_notify)(int device_unique_id,
					void (*notify)(int device_unique_id,
						enum cfm_notify_event_type_t event_type,
						struct cfm_properties *props));
};

// ops for dish
struct cfm_dish_ops {
	int (*init)(struct dvb_frontend *fe);
	int (*set_voltage)(struct dvb_frontend *fe);
	enum lnb_overload_status (*lnb_short_status)(struct dvb_frontend *fe);
	int (*tune)(struct dvb_frontend *fe);
	int (*sleep)(struct dvb_frontend *fe);
	u16 device_unique_id;
	struct cfm_ext_array chip_delsys;
	void *sec_priv;
	int (*dump_debug_info)(char *buffer, int buffer_size, int *eof, int offset,
						struct dvb_frontend *fe);
	int (*suspend)(struct dvb_frontend *fe);
	int (*resume)(struct dvb_frontend *fe);
	int (*get_param)(struct dvb_frontend *fe, struct cfm_properties *dishProps);
	int (*get_dish)(struct dvb_frontend *fe, struct dtv_frontend_properties *props);
	int (*dvb_frontend_registered)(int device_unique_id, struct dvb_frontend *fe);
	int (*set_notify)(int device_unique_id,
					void (*notify)(int device_unique_id,
						enum cfm_notify_event_type_t event_type,
						struct cfm_properties *props));
};

// register demod/tuner/dish device to cfm
int cfm_register_demod_device(struct dvb_adapter *adapter, struct cfm_demod_ops *demod_ops);
int cfm_register_tuner_device(struct dvb_adapter *adapter, struct cfm_tuner_ops *tuner_ops);
int cfm_register_dish_device(struct dvb_adapter *adapter, struct cfm_dish_ops *dish_ops);
// get demod/tuner/dish information
int cfm_get_demod_property(struct dvb_frontend *fe, struct cfm_properties *props);
int cfm_get_tuner_property(struct dvb_frontend *fe, struct cfm_properties *props);
int cfm_get_dish_property(struct dvb_frontend *fe, struct cfm_properties *props);
// notify callback
int cfm_register_notify(struct cfm_notify *nptr);
int cfm_unregister_notify(struct cfm_notify *nptr);
#endif