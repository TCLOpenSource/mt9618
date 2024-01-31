/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_alsa_pcm_atv.h  --  Mediatek pcm atv header
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */
#ifndef _ATV_PLATFORM_HEADER
#define _ATV_PLATFORM_HEADER

#define ATV_MONO_MODE	0x00
#define ATV_HIDEV_MODE	0x10
#define ATV_A2_MODE	0x20
#define ATV_NICAM_MODE	0x40

/// A2/FM/_SOUND_MOD Report info
#define _STATUS_MONO_EXIST		(1 << 0)
#define _STATUS_STEREO_EXIST		(1 << 1)
#define _STATUS_DUAL_EXIST		(1 << 2)
#define _STATUS_A2_PILOT_EXIST		(1 << 3)
#define _STATUS_A2_PRIMARY_EXIST	(1 << 4)
#define _STATUS_A2_SECONDARY_EXIST	(1 << 5)
#define _STATUS_A2_DK2_EXIST		(1 << 6)
#define _STATUS_A2_DK3_EXIST		(1 << 7)

#define _MASK_NICAM_STATUS_INFO		0x0F
#define _MASK_NICAM_SOUND_MODE_INFO	0xF0
#define _NICAM_LOCK_STATE		0x05
#define _NICAM_UNLOCK_STATE		0x00

/// NICAM_SOUND_MOD Report info
#define _FM_MONO_NOT_EXIST		0x00
#define _NICAM_MONO			0x10
#define _NICAM_STEREO			0x20
#define _NICAM_DUAL			0x30
#define _FM_MONO_EXIST			0x80
#define _FM_MONO_n_NICAM_MONO		0x90
#define _FM_MONO_n_NICAM_STEREO		0xA0
#define _FM_MONO_n_NICAM_DUAL		0xB0

#define DSP_PM_SEG3_ADDR		0x2040

#define ATV_PAL_THR_BASE_ADDR		(DSP_PM_SEG3_ADDR + 0x31)
#define ATV_BTSC_THR_BASE_ADDR		(DSP_PM_SEG3_ADDR + 0x212)

#define ATV_PAL_TYPE_GAIN_BASE_ADDR	(DSP_PM_SEG3_ADDR + 0x717)
#define ATV_BTSC_TOTAL_GAIN_BASE_ADDR	(DSP_PM_SEG3_ADDR + 0x223)
#define ATV_BTSC_MTS_GAIN_BASE_ADDR	(DSP_PM_SEG3_ADDR + 0x578)

#define PRESCALE_STEP_ONE_DB		0x4
#define PRESCALE_UPPER_BOUND		100
#define PRESCALE_LOWER_BOUND		-100
#define PRESCALE_NICAM_I		18

// Palsum DSP debug cmd
#define PAL_SC1_AMP_DBG_CMD		0x3000
#define PAL_SC1_NSR_DBG_CMD		0x3C00
#define PAL_SC2_AMP_DBG_CMD		0x3200
#define PAL_SC2_NSR_DBG_CMD		0x3D00
#define PAL_PILOT_AMP_DBG_CMD		0x4000
#define PAL_PILOT_PHASE_DBG_CMD		0x3700
#define PAL_STEREO_DUAL_ID_DBG_CMD	0x3F00
#define PAL_NICAM_PHASE_ERR_DBG_CMD	0x4100
#define PAL_BAND_STRENGTH_DBG_CMD	0x4500
// BTSC DSP debug cmd
#define BTSC_SC1_AMP_DBG_CMD		0x8000
#define BTSC_SC1_NSR_DBG_CMD		0x8B00
#define BTSC_STEREO_PILOT_AMP_DBG_CMD	0x8700
#define BTSC_SAP_AMP_DBG_CMD		0x8400
#define BTSC_SAP_NSR_DBG_CMD		0x8C00

#define SIF_DSP_DET_TIME	1  // ms

#define SHIFT_16	16

enum atv_decode_system {
	ATV_SYS_PALSUM,
	ATV_SYS_BTSC,
	ATV_SYS_NUM,
};

enum atv_sub_std {
	ATV_SUB_STD_MONO,
	ATV_SUB_STD_HIDEV,
	ATV_SUB_STD_A2,
	ATV_SUB_STD_NICAM,
	ATV_SUB_STD_INVALID		= 0xFF,
};

enum atv_kcontrol_cmd {
	ATV_HIDEV,
	ATV_PRESCALE,
	ATV_STANDARD,
	ATV_SOUNDMODE,
	ATV_THRESHOLD,
	ATV_CARRIER_STATUS,
	ATV_AUTO_DET_STD_RESULT,
	ATV_SYS_CAPABILITY,

	// Palsum audio signal level
	ATV_PAL_SC1_AMP,
	ATV_PAL_SC1_NSR,
	ATV_PAL_SC2_AMP,
	ATV_PAL_SC2_NSR,
	ATV_PAL_PILOT_AMP,
	ATV_PAL_PILOT_PHASE,
	ATV_PAL_STEREO_DUAL_ID,
	ATV_PAL_NICAM_PHASE_ERR,
	ATV_PAL_BAND_STRENGTH,

	// Btsc audio signal level
	ATV_BTSC_SC1_AMP,
	ATV_BTSC_SC1_NSR,
	ATV_BTSC_PILOT_AMP,
	ATV_BTSC_SAP_AMP,
	ATV_BTSC_SAP_NSR,
};

enum atv_standard_type {
	ATV_STD_NOT_READY		= 0x0,
	ATV_STD_M_BTSC			= 0x1,
	ATV_STD_M_EIAJ			= 0x2,
	ATV_STD_M_A2			= 0x3,
	ATV_STD_BG_A2			= 0x4,
	ATV_STD_DK1_A2			= 0x5,
	ATV_STD_DK2_A2			= 0x6,
	ATV_STD_DK3_A2			= 0x7,
	ATV_STD_BG_NICAM		= 0x8,
	ATV_STD_DK_NICAM		= 0x9,
	ATV_STD_I_NICAM			= 0xA,
	ATV_STD_L_NICAM			= 0xB,
	ATV_STD_FM_RADIO		= 0xC,
	ATV_STD_BUSY			= 0x80,
	ATV_STD_SEL_AUTO		= 0xE0,
	ATV_STD_MAX,
};

enum {
	ATV_MAIN_STANDARD,
	ATV_SUB_STANDARD,
};

enum atv_carrier_status {
	ATV_NO_CARRIER			= 0x00, ///< No carrier detect
	ATV_PRIMARY_CARRIER		= 0x01, ///< Carrier 1 exist
	ATV_SECONDARY_CARRIER		= 0x02, ///< Carrier 2 exist
	ATV_NICAM			= 0x04, ///< Nicam lock state
	ATV_STEREO			= 0x08, ///< A2 Stereo exist
	ATV_BILINGUAL			= 0x10, ///< A2 Dual exist
	ATV_PILOT			= 0x20, ///< A2 Pilot exist
	ATV_DK2				= 0x40, ///< Sound standard is DK2
	ATV_DK3				= 0x80  ///< Sound standard is DK3
};

enum atv_sound_mode_detect {
	ATV_SOUND_MODE_DET_UNKNOWN,		///< unknown
	ATV_SOUND_MODE_DET_MONO,		///< MTS/A2/EIAJ/FM-Mono mono
	ATV_SOUND_MODE_DET_STEREO,		///< MTS/A2/EIAJ stereo
	ATV_SOUND_MODE_DET_MONO_SAP,		///< MTS mono + SAP
	ATV_SOUND_MODE_DET_STEREO_SAP,		///< MTS stereo + SAP
	ATV_SOUND_MODE_DET_DUAL,		///< A2/EIAJ dual 1
	ATV_SOUND_MODE_DET_NICAM_MONO,		///< NICAM mono (with FM/AM mono)
	ATV_SOUND_MODE_DET_NICAM_STEREO,	///< NICAM stereo (with FM/AM mono)
	ATV_SOUND_MODE_DET_NICAM_DUAL,		///< NICAM dual (with FM/AM mono)
};

enum atv_sound_mode {
	ATV_SOUND_MODE_UNKNOWN,			///< unknown
	ATV_SOUND_MODE_MONO,			///< MTS/A2/EIAJ/FM-Mono mono
	ATV_SOUND_MODE_STEREO,			///< MTS/A2/EIAJ stereo
	ATV_SOUND_MODE_SAP,			///< MTS SAP
	ATV_SOUND_MODE_A2_LANG_A,		///< A2/EIAJ dual A
	ATV_SOUND_MODE_A2_LANG_B,		///< A2/EIAJ dual B
	ATV_SOUND_MODE_A2_LANG_AB,		///< A2/EIAJ L:dual A, R:dual B
	ATV_SOUND_MODE_NICAM_MONO,		///< NICAM mono
	ATV_SOUND_MODE_NICAM_STEREO,		///< NICAM stereo
	ATV_SOUND_MODE_NICAM_DUAL_A,		///< NICAM L:dual A, R:dual A
	ATV_SOUND_MODE_NICAM_DUAL_B,		///< NICAM L:dual B, R:dual B
	ATV_SOUND_MODE_NICAM_DUAL_AB,		///< NICAM L:dual A, R:dual B
	ATV_SOUND_MODE_AUTO		= 0x80,	///< auto sound mode
};

enum atv_comm_cmd {
	ATV_SET_START,
	ATV_SET_STOP,
	ATV_ENABLE_HIDEV,
	ATV_SET_HIDEV_BW,
	ATV_ENABLE_AGC,
	ATV_RESET_AGC,
	ATV_ENABLE_FC_TRACKING,
	ATV_RESET_FC_TRACKING,

	// Palsum audio signal level
	ATV_PAL_GET_SC1_AMP,
	ATV_PAL_GET_SC1_NSR,
	ATV_PAL_GET_SC2_AMP,
	ATV_PAL_GET_SC2_NSR,
	ATV_PAL_GET_PILOT_AMP,
	ATV_PAL_GET_PILOT_PHASE,
	ATV_PAL_GET_STEREO_DUAL_ID,
	ATV_PAL_GET_NICAM_PHASE_ERR,
	ATV_PAL_GET_BAND_STRENGTH,

	// Btsc audio signal level
	ATV_BTSC_GET_SC1_AMP,
	ATV_BTSC_GET_SC1_NSR,
	ATV_BTSC_GET_STEREO_PILOT_AMP,
	ATV_BTSC_GET_SAP_AMP,
	ATV_BTSC_GET_SAP_NSR,

	ATV_COMM_CMD_NUM,
};

enum atv_gain_type {
	ATV_PAL_GAIN_A2_FM_M,
	ATV_PAL_GAIN_HIDEV_M,
	ATV_PAL_GAIN_A2_FM_X,
	ATV_PAL_GAIN_HIDEV_X,
	ATV_PAL_GAIN_NICAM,
	ATV_PAL_GAIN_AM,

	ATV_BTSC_GAIN_TOTAL,
	ATV_BTSC_GAIN_MONO,
	ATV_BTSC_GAIN_STEREO,
	ATV_BTSC_GAIN_SAP,
	ATV_GAIN_TYPE_NUM,
};

enum atv_hidev_mode {
	ATV_HIDEV_OFF,
	ATV_HIDEV_L1,
	ATV_HIDEV_L2,
	ATV_HIDEV_L3,
	ATV_HIDEV_NUM,
};

enum atv_hidev_filter_bw {
	ATV_HIDEV_BW_DEFAULT	= 0x00,
	ATV_HIDEV_BW_L1		= 0x10,
	ATV_HIDEV_BW_L2		= 0x20,
	ATV_HIDEV_BW_L3		= 0x30,
};

enum atv_thr_type {
	A2_M_CARRIER1_ON_AMP,
	A2_M_CARRIER1_OFF_AMP,
	A2_M_CARRIER1_ON_NSR,
	A2_M_CARRIER1_OFF_NSR,
	A2_M_CARRIER2_ON_AMP,
	A2_M_CARRIER2_OFF_AMP,
	A2_M_CARRIER2_ON_NSR,
	A2_M_CARRIER2_OFF_NSR,
	A2_M_A2_PILOT_ON_AMP,
	A2_M_A2_PILOT_OFF_AMP,
	A2_M_PILOT_PHASE_ON_THD,
	A2_M_PILOT_PHASE_OFF_THD,
	A2_M_POLIT_MODE_VALID_RATIO,
	A2_M_POLIT_MODE_INVALID_RATIO,
	A2_M_EXTENSION,

	A2_BG_CARRIER1_ON_AMP,
	A2_BG_CARRIER1_OFF_AMP,
	A2_BG_CARRIER1_ON_NSR,
	A2_BG_CARRIER1_OFF_NSR,
	A2_BG_CARRIER2_ON_AMP,
	A2_BG_CARRIER2_OFF_AMP,
	A2_BG_CARRIER2_ON_NSR,
	A2_BG_CARRIER2_OFF_NSR,
	A2_BG_A2_PILOT_ON_AMP,
	A2_BG_A2_PILOT_OFF_AMP,
	A2_BG_PILOT_PHASE_ON_THD,
	A2_BG_PILOT_PHASE_OFF_THD,
	A2_BG_POLIT_MODE_VALID_RATIO,
	A2_BG_POLIT_MODE_INVALID_RATIO,
	A2_BG_EXTENSION,

	A2_DK_CARRIER1_ON_AMP,
	A2_DK_CARRIER1_OFF_AMP,
	A2_DK_CARRIER1_ON_NSR,
	A2_DK_CARRIER1_OFF_NSR,
	A2_DK_CARRIER2_ON_AMP,
	A2_DK_CARRIER2_OFF_AMP,
	A2_DK_CARRIER2_ON_NSR,
	A2_DK_CARRIER2_OFF_NSR,
	A2_DK_A2_PILOT_ON_AMP,
	A2_DK_A2_PILOT_OFF_AMP,
	A2_DK_PILOT_PHASE_ON_THD,
	A2_DK_PILOT_PHASE_OFF_THD,
	A2_DK_POLIT_MODE_VALID_RATIO,
	A2_DK_POLIT_MODE_INVALID_RATIO,
	A2_DK_EXTENSION,

	FM_I_CARRIER1_ON_AMP,
	FM_I_CARRIER1_OFF_AMP,
	FM_I_CARRIER1_ON_NSR,
	FM_I_CARRIER1_OFF_NSR,

	AM_CARRIER1_ON_AMP,
	AM_CARRIER1_OFF_AMP,
	AM_EXTENSION,

	HIDEV_M_CARRIER1_ON_AMP,
	HIDEV_M_CARRIER1_OFF_AMP,
	HIDEV_M_CARRIER1_ON_NSR,
	HIDEV_M_CARRIER1_OFF_NSR,

	HIDEV_BG_CARRIER1_ON_AMP,
	HIDEV_BG_CARRIER1_OFF_AMP,
	HIDEV_BG_CARRIER1_ON_NSR,
	HIDEV_BG_CARRIER1_OFF_NSR,

	HIDEV_DK_CARRIER1_ON_AMP,
	HIDEV_DK_CARRIER1_OFF_AMP,
	HIDEV_DK_CARRIER1_ON_NSR,
	HIDEV_DK_CARRIER1_OFF_NSR,

	HIDEV_I_CARRIER1_ON_AMP,
	HIDEV_I_CARRIER1_OFF_AMP,
	HIDEV_I_CARRIER1_ON_NSR,
	HIDEV_I_CARRIER1_OFF_NSR,

	NICAM_BG_NICAM_ON_SIGERR,
	NICAM_BG_NICAM_OFF_SIGERR,
	NICAM_BG_EXTENSION,

	NICAM_I_NICAM_ON_SIGERR,
	NICAM_I_NICAM_OFF_SIGERR,
	NICAM_I_EXTENSION,

	PAL_THR_TYPE_END,

	BTSC_MONO_ON_NSR,
	BTSC_MONO_OFF_NSR,
	BTSC_PILOT_ON_AMPLITUDE,
	BTSC_PILOT_OFF_AMPLITUDE,
	BTSC_SAP_ON_NSR,
	BTSC_SAP_OFF_NSR,
	BTSC_STEREO_ON,
	BTSC_STEREO_OFF,
	BTSC_SAP_ON_AMPLITUDE,
	BTSC_SAP_OFF_AMPLITUDE,

	BTSC_THR_TYPE_END,
};

enum atv_input_type {
	ATV_INPUT_VIF_INTERNAL	= 0,	// vif mode
	ATV_INPUT_SIF_EXTERNAL	= 1,	// sif mode
};

enum dsp_memory_type {
	DSP_MEM_TYPE_PM,
	DSP_MEM_TYPE_DM,
};

struct atv_thr_tbl {
	u32 a2_m_carrier1_on_amp;
	u32 a2_m_carrier1_off_amp;
	u32 a2_m_carrier1_on_nsr;
	u32 a2_m_carrier1_off_nsr;
	u32 a2_m_carrier2_on_amp;
	u32 a2_m_carrier2_off_amp;
	u32 a2_m_carrier2_on_nsr;
	u32 a2_m_carrier2_off_nsr;
	u32 a2_m_a2_pilot_on_amp;
	u32 a2_m_a2_pilot_off_amp;
	u32 a2_m_pilot_phase_on_thd;
	u32 a2_m_pilot_phase_off_thd;
	u32 a2_m_polit_mode_valid_ratio;
	u32 a2_m_polit_mode_invalid_ratio;
	u32 a2_m_carrier2_output_thr;

	u32 a2_bg_carrier1_on_amp;
	u32 a2_bg_carrier1_off_amp;
	u32 a2_bg_carrier1_on_nsr;
	u32 a2_bg_carrier1_off_nsr;
	u32 a2_bg_carrier2_on_amp;
	u32 a2_bg_carrier2_off_amp;
	u32 a2_bg_carrier2_on_nsr;
	u32 a2_bg_carrier2_off_nsr;
	u32 a2_bg_a2_pilot_on_amp;
	u32 a2_bg_a2_pilot_off_amp;
	u32 a2_bg_pilot_phase_on_thd;
	u32 a2_bg_pilot_phase_off_thd;
	u32 a2_bg_polit_mode_valid_ratio;
	u32 a2_bg_polit_mode_invalid_ratio;
	u32 a2_bg_carrier2_output_thr;

	u32 a2_dk_carrier1_on_amp;
	u32 a2_dk_carrier1_off_amp;
	u32 a2_dk_carrier1_on_nsr;
	u32 a2_dk_carrier1_off_nsr;
	u32 a2_dk_carrier2_on_amp;
	u32 a2_dk_carrier2_off_amp;
	u32 a2_dk_carrier2_on_nsr;
	u32 a2_dk_carrier2_off_nsr;
	u32 a2_dk_a2_pilot_on_amp;
	u32 a2_dk_a2_pilot_off_amp;
	u32 a2_dk_pilot_phase_on_thd;
	u32 a2_dk_pilot_phase_off_thd;
	u32 a2_dk_polit_mode_valid_ratio;
	u32 a2_dk_polit_mode_invalid_ratio;
	u32 a2_dk_carrier2_output_thr;

	u32 fm_i_carrier1_on_amp;
	u32 fm_i_carrier1_off_amp;
	u32 fm_i_carrier1_on_nsr;
	u32 fm_i_carrier1_off_nsr;

	u32 am_carrier1_on_amp;
	u32 am_carrier1_off_amp;
	u32 am_agc_ref;

	u32 hidev_m_carrier1_on_amp;
	u32 hidev_m_carrier1_off_amp;
	u32 hidev_m_carrier1_on_nsr;
	u32 hidev_m_carrier1_off_nsr;

	u32 hidev_bg_carrier1_on_amp;
	u32 hidev_bg_carrier1_off_amp;
	u32 hidev_bg_carrier1_on_nsr;
	u32 hidev_bg_carrier1_off_nsr;

	u32 hidev_dk_carrier1_on_amp;
	u32 hidev_dk_carrier1_off_amp;
	u32 hidev_dk_carrier1_on_nsr;
	u32 hidev_dk_carrier1_off_nsr;

	u32 hidev_i_carrier1_on_amp;
	u32 hidev_i_carrier1_off_amp;
	u32 hidev_i_carrier1_on_nsr;
	u32 hidev_i_carrier1_off_nsr;

	u32 nicam_bg_nicam_on_sigerr;
	u32 nicam_bg_nicam_off_sigerr;
	u32 nicam_bg_reset_hw_siger;

	u32 nicam_i_nicam_on_sigerr;
	u32 nicam_i_nicam_off_sigerr;
	u32 nicam_i_reset_hw_siger;

	u32 pal_thr_type_end;

	u32 btsc_mono_on_nsr;
	u32 btsc_mono_off_nsr;
	u32 btsc_pilot_on_amplitude;
	u32 btsc_pilot_off_amplitude;
	u32 btsc_sap_on_nsr;
	u32 btsc_sap_off_nsr;
	u32 btsc_stereo_on;
	u32 btsc_stereo_off;
	u32 btsc_sap_on_amplitude;
	u32 btsc_sap_off_amplitude;

	u32 btsc_thr_type_end;
};

struct atv_gain_tbl {
	//[-12 ~ +10]dB, 0.25dB/step
	//eg:  Fm + 5dB---->s32Fm = 5dB/0.25dB
	s32 pal_a2_fm_m;
	s32 pal_hidev_m;
	s32 pal_a2_fm_x;
	s32 pal_hidev_x;
	s32 pal_nicam;
	s32 pal_am;

	s32 btsc_total;
	s32 btsc_mono;
	s32 btsc_stereo;
	s32 btsc_sap;
};

#endif /* _ATV_PLATFORM_HEADER */
