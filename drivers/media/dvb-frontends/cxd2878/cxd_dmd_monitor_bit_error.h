/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXD_DMD_MONITOR_BIT_ERROR_H
#define CXD_DMD_MONITOR_BIT_ERROR_H

#include "cxdref_integ.h"

cxdref_result_t cxdref_demod_atsc_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_atsc_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count, uint32_t *block_count);

cxdref_result_t cxdref_demod_atsc3_monitor_pre_bit_error (cxdref_demod_t * pDemod,
		uint32_t pre_bit_error_count[4], uint32_t pre_bit_count[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t post_bit_error_count[4], uint32_t post_bit_count[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t block_error_count[4], uint32_t block_count[4]);

cxdref_result_t cxdref_demod_dvbc_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_dvbc_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count, uint32_t *block_count);

cxdref_result_t cxdref_demod_j83b_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_j83b_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count, uint32_t *block_count);

cxdref_result_t cxdref_demod_dvbs2_monitor_pre_bit_error (cxdref_demod_t * pDemod,
		uint32_t *pre_bit_error_count, uint32_t *pre_bit_count);

cxdref_result_t cxdref_demod_dvbs2_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_dvbs2_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count, uint32_t *block_count);

cxdref_result_t cxdref_demod_dvbs_monitor_pre_bit_error (cxdref_demod_t * pDemod,
		uint32_t *pre_bit_error_count, uint32_t *pre_bit_count);

cxdref_result_t cxdref_demod_dvbs_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_dvbs_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count, uint32_t *block_count);

cxdref_result_t cxdref_demod_dvbt_monitor_pre_bit_error (cxdref_demod_t * pDemod,
		uint32_t *pre_bit_error_count, uint32_t *pre_bit_count);

cxdref_result_t cxdref_demod_dvbt_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_dvbt_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count, uint32_t *block_count);

cxdref_result_t cxdref_demod_dvbt2_monitor_pre_bit_error (cxdref_demod_t * pDemod,
		uint32_t *pre_bit_error_count, uint32_t *pre_bit_count);

cxdref_result_t cxdref_demod_dvbt2_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_dvbt2_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count, uint32_t *block_count);

cxdref_result_t cxdref_demod_isdbc_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_isdbc_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count, uint32_t *block_count);

cxdref_result_t cxdref_demod_isdbs_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count_h, uint32_t *post_bit_count_h,
		uint32_t *post_bit_error_count_l, uint32_t *post_bit_count_l,
		uint32_t *post_bit_error_count_tmcc, uint32_t *post_bit_count_tmcc);

cxdref_result_t cxdref_demod_isdbs_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count_h, uint32_t *block_count_h,
		uint32_t *block_error_count_l, uint32_t *block_count_l);

cxdref_result_t cxdref_demod_isdbs3_monitor_pre_bit_error (cxdref_demod_t * pDemod,
		uint32_t *pre_bit_error_count_h, uint32_t *pre_bit_count_h,
		uint32_t *pre_bit_error_count_l, uint32_t *pre_bit_count_l);

cxdref_result_t cxdref_demod_isdbs3_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count_h, uint32_t *post_bit_count_h,
		uint32_t *post_bit_error_count_l, uint32_t *post_bit_count_l);

cxdref_result_t cxdref_demod_isdbs3_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count_h, uint32_t *block_count_h,
		uint32_t *block_error_count_l, uint32_t *block_count_l);

cxdref_result_t cxdref_demod_isdbt_monitor_post_bit_error (cxdref_demod_t * pDemod,
		uint32_t *post_bit_error_count_a, uint32_t *post_bit_error_count_b,
		uint32_t *post_bit_error_count_c, uint32_t *post_bit_count);

cxdref_result_t cxdref_demod_isdbt_monitor_block_error (cxdref_demod_t * pDemod,
		uint32_t *block_error_count_a, uint32_t *block_error_count_b,
		uint32_t *block_error_count_c, uint32_t *block_count);
#endif
