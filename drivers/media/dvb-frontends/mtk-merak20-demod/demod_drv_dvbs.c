// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <media/dvb_frontend.h>
#include "demod_drv_dvbs.h"
#include "demod_hal_dvbs.h"
#include "demod_common.h"

int mdrv_dmd_dvbs_init(struct dvb_frontend *fe)
{
	int ret = 0;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	intern_dvbs_init_clk(fe);
	ret = intern_dvbs_load_code();

	return ret;
}

int mdrv_dmd_dvbs_config(struct dvb_frontend *fe)
{
	return intern_dvbs_config(fe);
}

int mdrv_dmd_dvbs_exit(struct dvb_frontend *fe)
{
	return intern_dvbs_exit(fe);
}

int mdrv_dmd_dvbs_get_ts_rate(u32 *clock_rate)
{
	u64  CLK_RATE = 0;
	int bRet = 0;

	bRet = intern_dvbs_get_ts_rate(&CLK_RATE);
	*clock_rate = (u32)(CLK_RATE);
	return bRet;
}

int mdrv_dmd_dvbs_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	int ret = 0;
	s32 FreqOffset;
	u32 abs_FreqOff;
	u32 symbol_rate;

	ret = intern_dvbs_read_status(fe, status);
	if ((*status&0x10) == 0x10) {
		ret = intern_dvbs_get_symbol_rate(&symbol_rate);
		ret = intern_dvbs_get_freqoffset(&FreqOffset);

		if (FreqOffset < 0)
			abs_FreqOff = (u32)((-1)*FreqOffset);
		else
			abs_FreqOff = (u32)(FreqOffset);

		if (symbol_rate <= 6000) {
			if (abs_FreqOff > (symbol_rate + 500)) {
				*status &= ~0x10;
				*status |= FE_TIMEDOUT;
			}
		} else if (symbol_rate <= 10000) {
			if (abs_FreqOff > ((symbol_rate + 20000)/4)) {
				*status &= ~0x10;
				*status |= FE_TIMEDOUT;
			}
		} else {
			if (abs_FreqOff > ((symbol_rate*7/10)+1000)) {
				*status &= ~0x10;
				*status |= FE_TIMEDOUT;
			}
		}
		pr_info("status = 0x%x", *status);
	}
	return ret;
}

int mdrv_dmd_dvbs_get_frontend(struct dvb_frontend *fe,
				 struct dtv_frontend_properties *p)
{
	int bRet = 0;

	bRet = intern_dvbs_get_frontend(fe, p);
	return bRet;
}

int mdrv_dmd_dvbs_blindScan_start(struct DMD_DVBS_BlindScan_Start_param *Start_param)
{
	u8 ret = 0;

	ret = intern_dvbs_BlindScan_Start(Start_param);
	return ret;
}

int mdrv_dmd_dvbs_blindScan_next_freq(struct DMD_DVBS_BlindScan_NextFreq_param *NextFreq_param)
{
	u8 bRet = 0;

	bRet = intern_dvbs_BlindScan_NextFreq(NextFreq_param);
	return bRet;
}

int mdrv_dmd_dvbs_blindScan_cancel(void)
{
	u8 bRet = 0;

	bRet = intern_dvbs_BlindScan_Cancel();
	return bRet;
}

int mdrv_dmd_dvbs_blindScan_end(void)
{
	u8 bRet = 0;

	bRet = intern_dvbs_BlindScan_End();
	return bRet;
}

int mdrv_dmd_dvbs_blindScan_get_channel(
	struct DMD_DVBS_BlindScan_GetChannel_param *GetChannel_param)
{
	return intern_dvbs_BlindScan_GetChannel(GetChannel_param);
}

int mdrv_dmd_dvbs_blindScan_wait_finished(
	struct DMD_DVBS_BlindScan_WaitCurFreqFinished_param *WaitCurFreqFinished_param)
{
	return intern_dvbs_BlindScan_WaitCurFreqFinished(WaitCurFreqFinished_param);
}

int mdrv_dmd_dvbs_blindScan_get_tuner_freq(
	struct DMD_DVBS_BlindScan_GetTunerFreq_param *GetTunerFreq_param)
{
	return intern_dvbs_BlindScan_GetTunerFreq(GetTunerFreq_param);
}

int mdrv_dmd_dvbs_blindscan_config(struct dvb_frontend *fe)
{
	u32 ret = 0;

	ret = intern_dvbs_blindscan_config(fe);

	return ret;
}

int mdrv_dmd_dvbs_diseqc_set_22k(enum fe_sec_tone_mode tone)
{
	return intern_dvbs_diseqc_set_22k(tone);
}

int mdrv_dmd_dvbs_diseqc_send_cmd(struct dvb_diseqc_master_cmd *cmd)
{
	return intern_dvbs_diseqc_send_cmd(cmd);
}

int mdrv_dmd_dvbs_diseqc_set_tone(enum fe_sec_mini_cmd minicmd)
{
	return intern_dvbs_diseqc_set_tone(minicmd);
}

int mdrv_dmd_dvbs_diseqc_set_envelope_mode(bool b_enable_envelope)
{
	return intern_dvbs_diseqc_set_envelope_mode(b_enable_envelope);
}

ssize_t dvbs_get_information_show(struct device_driver *driver, char *buf)
{
	return demod_hal_dvbs_get_information(driver, buf);
}

ssize_t dvbs_get_information(char *buf)
{
	struct device_driver driver;

	return demod_hal_dvbs_get_information(&driver, buf);
}

