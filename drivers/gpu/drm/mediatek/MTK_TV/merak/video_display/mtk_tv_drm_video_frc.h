/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_VIDEO_FRC_H_
#define _MTK_TV_DRM_VIDEO_FRC_H_

#include "ext_command_frc_if.h"
#include "mtk_tv_drm_video_display.h"
#include "hwreg_render_video_frc.h"

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------
#define  MEMC_BLOCK_H_ALIGN 16
#define  MEMC_BLOCK_V_ALIGN 16

#define RV55_CMD_LENGTH 40
#define MAX_FRC_SMI_MIU2GMC_NUM 48
#define MAX_FRC_SMI_PORT_NUM 48
#define MAX_FRC_MAX_FRAME_CNT (20)

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------
enum MEMC_Version_Index {
	Version_Major                = 0x00,
	Version_Minor                = 0x01,
};

enum Frc_command_list {
	//Get command
	FRC_MB_CMD_GET_SW_VERSION                       = 0x02,
	FRC_MB_CMD_GET_RWDIFF                           = 0x03,

	//Set command
	FRC_MB_CMD_INIT                                 = 0x04,
	FRC_MB_CMD_SET_TIMING                           = 0x05,
	FRC_MB_CMD_SET_INPUT_FRAME_SIZE                 = 0x06,
	FRC_MB_CMD_SET_OUTPUT_FRAME_SIZE                = 0x07,
	FRC_MB_CMD_SET_FPLL_LOCK_DONE                   = 0x08,
	FRC_MB_CMD_SET_FRC_MODE                         = 0x0F,
	FRC_MB_CMD_SET_MFC_MODE                         = 0x10,
	FRC_MB_CMD_SET_FRC_GAME_MODE                    = 0x11,
	FRC_MB_CMD_SET_MFC_DEMO_MODE	                = 0x12,
	FRC_MB_CMD_60_REALCINEMA_MODE                   = 0x13,
	FRC_MB_CMD_SET_MFC_DECODE_IDX_DIFF              = 0x20,
	FRC_MB_CMD_SET_MFC_FRC_DS_SETTINGS              = 0x21,
	FRC_MB_CMD_SET_MFC_CROPWIN_CHANGE               = 0x22,
	FRC_MB_CMD_MEMC_MASK_WINDOWS_H                  = 0x30,
	FRC_MB_CMD_MEMC_MASK_WINDOWS_V                  = 0x31,
	FRC_MB_CMD_MEMC_ACTIVE_WINDOWS_H                = 0x32,
	FRC_MB_CMD_MEMC_ACTIVE_WINDOWS_V                = 0x33,
	FRC_MB_CMD_FILM_CADENCE_CTRL                    = 0x34,
	FRC_MB_CMD_INIT_DONE                            = 0x35,
	FRC_MB_CMD_SET_SHARE_MEMORY_ADDR                = 0x36,
	FRC_MB_CMD_SET_QoS                            = 0x37,
	FRC_MB_CMD_GET_QoS                            = 0x38,

	//SONY Command
	FRC_MB_CMD_SET_MEMC_DEBUG_CADENCE               = 0x40,
	FRC_MB_CMD_SET_MEMC_DEBUG_OUTSR                 = 0x41,
	FRC_MB_CMD_SET_MEMC_DEBUG_FALLBACKW3            = 0x42,
	FRC_MB_CMD_SET_MEMC_DEBUG_FALLBACKW0            = 0x43,
	FRC_MB_CMD_SET_MEMC_DEBUG_MOTION                = 0x44,
	FRC_MB_CMD_SET_MEMC_DEBUG_SHOW_FILM_ERROR       = 0x45,
	FRC_MB_CMD_SET_MEMC_DEBUG_AUTOMODE              = 0x46,
	FRC_MB_CMD_SET_MEMC_DEBUG_AUTOMODE_FINAL        = 0x47,
	FRC_MB_CMD_SET_MEMC_DEBUG_OSDMASK               = 0x48,
	FRC_MB_CMD_SET_MEMC_DEBUG_HALO_REDUCTION        = 0x49,
	FRC_MB_CMD_SET_MEMC_DEBUG_SEARCH_RANGE          = 0x4A,
	FRC_MB_CMD_SET_MEMC_DEBUG_PERIODICAL            = 0x4B,
	FRC_MB_CMD_SET_MEMC_DEBUG_BADEDIT               = 0x4C,
	FRC_MB_CMD_SET_MEMC_DEBUG_MVSTICKING            = 0x4D,
	FRC_MB_CMD_SET_MEMC_DEBUG_SMALL_OBJECT          = 0x4E,
	FRC_MB_CMD_SET_MEMC_DEBUG_SPOTLIGHT             = 0x4F,
	FRC_MB_CMD_SET_MEMC_DEBUG_BOUNDARY_HALO         = 0x50,
	FRC_MB_CMD_SET_MEMC_DEBUG_POSITION_RATIO_TABLE  = 0x51,
	FRC_MB_CMD_SET_MEMC_DEBUG_IMPREMENTATION_FRAME  = 0x52,
	FRC_MB_CMD_SET_MEMC_RELPOS_RATIO_INDEX          = 0x53,

	//Send burst command
	FRC_MB_CMD_SET_MECM_LEVEL                       = 0x80,

	//Get command for MEMC alg LTP
	FRC_MB_CMD_GET_MECM_ALG_STATUS                  = 0xF0,
};

enum mtk_video_plane_memc_initstate {
	MEMC_INIT_CHECKRV55,
	MEMC_INIT_SENDDONE,
	MEMC_INIt_FINISH,

	MEMC_INT_STATE_NUM
};

enum mtk_video_frc_pattern {
	MEMC_PAT_OFF,
	MEMC_PAT_OPM,
	MEMC_PAT_IPM,
	MEMC_PAT_IPM_DYNAMIC, // Debug Only
	MEMC_PAT_IPM_STATIC, // Debug Only
	MEMC_PAT_IPM_FORCE_TIMING,
};

enum mtk_video_frc_pq_scene {
	MEMC_PQ_SCENE_NORMAL = 0,
	MEMC_PQ_SCENE_GAME,
	MEMC_PQ_SCENE_GAME_CROP,
	MEMC_PQ_SCENE_PC,
	MEMC_PQ_SCENE_PHOTO,
	MEMC_PQ_SCENE_FORCE_P,
	MEMC_PQ_SCENE_HFR,
	MEMC_PQ_SCENE_GAME_WITH_MEMC,
	MEMC_PQ_SCENE_3D,
	MEMC_PQ_SCENE_SLICED_GAME,
	MEMC_PQ_SCENE_GAME_WITH_PC,
	MEMC_PQ_SCENE_MAX,
};

struct mtk_frc_context {

	uint8_t frc_ip_version;
	bool frcopmMaskEnable;
	bool bIspre_memc_en_Status[MAX_WINDOW_NUM];
	bool bIsmemc_1stInit;
	uint16_t frc_latency;
	uint16_t frc_rwdiff;
	uint16_t frc_user_rwdiff;
	uint16_t frc_dts_rwdiff;

	bool frc_pattern_enable;
	enum mtk_video_frc_pattern frc_pattern_sel;
	uint8_t frc_pattern_trig_cnt;
	enum mtk_video_plane_memc_initstate memc_InitState;

	uint64_t u64frcpts;
	uint16_t frc_frame_count;
	uint32_t frc_shm_size;
	uint64_t frc_shm_adr;
	unsigned long frc_shm_virAdr;

	uint8_t video_memc_mode;
	uint8_t video_frc_mode;
	uint8_t frc_alg_run_on;
	uint8_t frc_frame_cnt;
	uint8_t frc_max_frame_cnt;

	uint32_t frc_vgsync_shm_size;
	uint64_t frc_vgsync_shm_adr;
	unsigned long frc_vgsync_shm_virAdr;

	uint8_t frc_as_hse;

	// smi
	uint8_t frc_smi_number;
	uint8_t frc_smi_miu2gmc[MAX_FRC_SMI_MIU2GMC_NUM];
	uint8_t frc_smi_port[MAX_FRC_SMI_PORT_NUM];

	uint16_t frc_ipm_hsize;
	uint16_t frc_ipm_vsize;

	uint8_t frc_photo_enable;
	uint8_t frc_photo_rw_diff;

	bool bSkipGameMemc;
	enum mtk_video_frc_pq_scene frc_scene;
};

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------
struct mtk_frc_timinginfo_tbl {
	uint32_t input_freq_L;
	uint32_t input_freq_H;
	uint32_t frc_memc_latency;
	uint32_t frc_game_latency;
};

struct mtk_frc_shm_info {
	uint64_t pts;
	uint16_t frame_count;
	uint8_t extra_delay_num;
};

struct mtk_frc_vgsync_buf_info {
	uint8_t u8CurBufIdx;
	struct window_info stCurWinInfo;
	uint8_t u8BufCnt;
	uint8_t u8PqOutCurBufIdx;
	struct window_info astWinInfo[MAX_FRC_MAX_FRAME_CNT];
	enum E_DRV_FRC_LBW_F2F3_PATH eLbwPath;
};

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
struct mtk_tv_kms_context;

const char *SenceToString(enum mtk_video_frc_pq_scene scene);
uint32_t mtk_video_display_frc_set_Qos(uint8_t QosMode);
uint32_t mtk_video_display_frc_get_Qos(uint8_t *pQosMode);

uint32_t mtk_video_display_frc_set_rv55_burst_cmd(uint16_t *u16Data, uint16_t u16Length);
uint32_t mtk_video_display_frc_set_rv55_cmd(uint16_t *u16Data, uint16_t u16Length,
	struct mtk_frc_context *pctx_frc);

int mtk_video_display_frc_get_memc_level(uint8_t *memc_level, uint16_t u16Length);
void _mtk_video_frc_get_latency_preplay(uint32_t input_freq, struct mtk_tv_kms_context *pctx);

void mtk_video_display_frc_suspend(struct device *dev, struct mtk_tv_kms_context *pctx);
void mtk_video_display_frc_resume(struct device *dev, struct mtk_tv_kms_context *pctx);

void mtk_video_display_frc_init(
	struct device *dev,
	struct mtk_tv_kms_context *pctx);

void mtk_video_display_set_frc_opm_path(struct mtk_plane_state *state,
						 bool enable);
void mtk_video_display_set_frc_property(
	struct video_plane_prop *plane_props,
	enum ext_video_plane_prop prop);
void mtk_video_display_get_frc_property(
	struct video_plane_prop *plane_props,
	enum ext_video_plane_prop prop);

void mtk_video_display_frc_set_pattern(
		struct mtk_tv_kms_context *pctx,
		enum mtk_video_frc_pattern sel, bool enable);

void mtk_video_display_set_frc_freeze(
		struct mtk_tv_kms_context *pctx,
		unsigned int plane_inx,
		bool enable);

bool mtk_video_display_check_frc_freeze(
		bool memc_enable,
		bool memc_freeze);

E_DRV_MFC_UNMASK mtk_video_frc_get_unmask(struct mtk_video_context *pctx_video);

int mtk_frc_get_all_value_from_string(char *buf, char *delim,
	unsigned int len, uint16_t *value, uint16_t *TokenCnt);

int mtk_video_display_frc_set_extra_latency(
	struct mtk_tv_kms_context *pctx,
	struct drm_mtk_tv_video_extra_latency *latency);

extern int mtk_miu2gmc_mask(u32 miu2gmc_id, u32 port_id, bool enable);

int mtk_video_display_frc_get_algo_info(struct device *dev, char *buf);

void mtk_video_display_set_timing(unsigned int plane_inx, struct mtk_tv_kms_context *pctx,
	uint32_t input_Vfreq, uint32_t output_Vfreq);

void mtk_video_display_frc_set_reset_aid(
	struct mtk_tv_kms_context *pctx);

int mtk_video_display_frc_get_algo_version(uint32_t *pVer);
#endif
