// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/math64.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/dma-buf.h>

#include <drm/mtk_tv_drm.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>

#include "../mtk_tv_drm_plane.h"
#include "../mtk_tv_drm_gem.h"
#include "../mtk_tv_drm_kms.h"
#include "mtk_tv_drm_common.h"
#include "mtk_tv_drm_log.h"
#include "drv_scriptmgt.h"
#include "hwreg_render_video_display.h"
#include "mtk_tv_drm_video_frc.h"
#include "mtk_tv_drm_atrace.h"
#include "mtk-reserved-memory.h"
#include "pqu_msg.h"

#include "clk-dtv.h"

#define free kfree
#define malloc(size) kmalloc((size), GFP_KERNEL)

#define FRC_IP_VERSION_1       (0x01) // M6
#define FRC_IP_VERSION_2       (0x02) // M6L
#define FRC_IP_VERSION_1_E3    (0x03)
#define FRC_IP_VERSION_4       (0x04) // mokona
#define FRC_IP_VERSION_5       (0x05)
#define FRC_IP_VERSION_6       (0x06)


#define LOCK                    (1)
#define VFRQRATIO_10            (10)
#define VFRQRATIO_1000          (1000)
#define SIZERATIO_16            (16)
#define LATENCYRATIO_10         (10)
#define LATENCYRATIO_1000       (1000)

#define FRC_MODE_CNT            (2)
#define MEMC_MODE_CNT           (4)
#define MEMC_LEVLE_MAX_CNT      (39)
#define DEMO_MODE_CNT           (2)
#define REALCINEMA_MODE_CNT     (4)
#define CADENCE_CNT             (4)

#define DATA_D0                 (0)
#define DATA_D1                 (1)
#define DATA_D2                 (2)
#define DATA_D3                 (3)

//CMD DEFAULT VALUE
#define DEFAULT_VALUE           (0x00)
#define DEFAULT_MEMC_MODE       (0x01)
#define DEFAULT_FRC_MODE        (0x00)
#define DEFAULT_DEMO_MODE       (0x00)
#define DEFAULT_PARTITION_MODE  (0x00)
#define DEFAULT_CADENCE_CTRL    (0xFF)
#define DEFAULT_FPLL_LOCK       (0xFF)
#define DEFAULT_PRE_VALUE       (0xFF)

//CMD EXPEND VALUE
#define CMD_D4_VGSNYC_BUF_INDEX (0x01)

#define LIMIT_MEMC_LEVEL        (0xFF)

#define MEMC_LEVEL_OFF          (0x00)
#define MEMC_LEVEL_LOW          (0x07)
#define MEMC_LEVEL_MIDDLE       (0x0E)
#define MEMC_LEVEL_HIGH         (0x15)

#define INIT_FREQ               (0x3C) // 60Hz
#define INIT_H_SIZE             (0x1E00) // 7680
#define INIT_V_SIZE             (0x10E0) // 4320

#define INIT_VALUE_FAIL         (0x00)
#define INIT_VALUE_PASS         (0xFF)

#define DEFAULT_CMD_STATUS      (0xFF)


#define NO_UPDATE               (0)
#define UPDATE                  (1)

//For LTP
#define CMD_STATUS             (0x00000000FFFFFFFF)
#define INPUT_SIZE_CMD_STATUS  (0x0000000000000001)
#define OUTPUT_SIZE_CMD_STATUS (0x0000000000000002)
#define FPLL_LOCK_CMD_STATUS   (0x0000000000000004)
#define TIMING_CMD_STATUS      (0x0000000000000008)
#define FRC_MODE_CMD_STATUS    (0x0000000000000010)
#define MFC_MODE_CMD_STATUS    (0x0000000000000020)
#define MFC_LEVEL_CMD_STATUS   (0x0000000000000040)
#define MFC_DEMO_CMD_STATUS    (0x0000000000000080)
#define MFC_ALG_STATUS         (0x0000000000000100)
#define MFC_PW_STATUS          (0x0000000000000200)
#define GAME_MODE_CMD_STATUS   (0x0000000000000400)
#define MFC_BYPASS_CMD_STATUS  (0x0000000000000800)
#define REALCINEMA_CMD_STATUS  (0x0000000000001000)
#define CADENCE_CMD_STATUS     (0x0000000000002000)

#define MEMC_STATUS            (0x0000000700000000)
#define MEMC_ON_STATUS         (0x0000000100000000)
#define MEMC_OFF_STATUS        (0x0000000200000000)
#define MEMC_AUTO_STATUS       (0x0000000400000000)

#define DEMO_STATUS            (0x0000007800000000)
#define DEMO_ON_ALL_STATUS     (0x0000000800000000)
#define DEMO_ON_LEFT_STATUS    (0x0000001000000000)
#define DEMO_ON_RIGHT_STATUS   (0x0000002000000000)
#define DEMO_OFF_STATUS        (0x0000004000000000)

#define MEMC_LV_STATUS         (0x0000078000000000)
#define MEMC_LV_OFF_STATUS     (0x0000008000000000)
#define MEMC_LV_LOW_STATUS     (0x0000010000000000)
#define MEMC_LV_MID_STATUS     (0x0000020000000000)
#define MEMC_LV_HIGH_STATUS    (0x0000040000000000)

#define GAMING_LV_STATUS       (0x0000080000000000)
#define DEJUDDER_STATUS        (0x0000100000000000)
#define DEBLUR_STATUS          (0x0000200000000000)
#define MEMC_USER_STATUS       (0x0000400000000000)
#define MEMC_BYPASS_STATUS     (0x0000800000000000)

#define REALCINEMA_STATUS      (0x0003000000000000)
#define REALCINEMA_OFF_STATUS  (0x0001000000000000)
#define REALCINEMA_ON_STATUS   (0x0002000000000000)

#define CADENCE_STATUS         (0x000C000000000000)
#define CADENCE_OFF_STATUS     (0x0004000000000000)
#define CADENCE_ON_STATUS      (0x0008000000000000)
#define CADENCE_D1_STATUS      (0x0010000000000000)
#define CADENCE_D2_STATUS      (0x0020000000000000)
#define CADENCE_D3_STATUS      (0x0040000000000000)

//----------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_FRC

//FRC mode setting
#define MEMC_MODE_OFF     (0)
#define MEMC_MODE_ON      (1)
#define MEMC_MODE_AUTO    (2)
#define MEMC_MODE_USER    (0x0A)

//Real Cinema Mode setting
#define REAL_CINEMA_OFF   (0)
#define REAL_CINEMA_ON    (1)

//MMAP index
#define FRC_SHM_INDEX_V4     (0)
#define FRC_SHM_INDEX        (1)
#define FRC_VGSYNC_SHM_INDEX (2)

//Demo mode setting
#define DEMO_MODE_OFF            (0)
#define DEMO_MODE_ON_LEFT_RIGHT  (1)
#define DEMO_MODE_ON_TOP_DOWN    (2)

//Demo PARTITION setting
#define DEMO_PARTITION_LEFT_OR_TOP      (0)
#define DEMO_PARTITION_RIGHT_OR_DOWN    (1)

//Coprocess define
#define FRC_ALG_PROCESSOR_OFF    (0)
#define FRC_ALG_PROCESSOR_RV55   (1)
#define FRC_ALG_PROCESSOR_R2     (2)
#define FRC_ALG_PROCESSOR_ARM    (3)

//GAME Mode
#define GAME_OFF                   (0)
#define GAME_ON_GAMING_LV_OFF      (1)
#define GAME_ON_GAMING_LV_ON       (2)

//Gaming level
#define GAMING_LEVLE_INVALID  (0xFF)
#define GAMING_LEVLE_MIN      (0)
#define GAMING_LEVLE_MAX      (10)

//MJC_Dejudder
#define DEJUDDER_LEVLE_INVALID  (0xFF)
#define DEJUDDER_LEVLE_MIN      (0)
#define DEJUDDER_LEVLE_MAX      (10)

//MJC_Deblur
#define DEBLUR_LEVLE_INVALID  (0xFF)
#define DEBLUR_LEVLE_MIN      (0)
#define DEBLUR_LEVLE_MAX      (10)

//Cadence_On/Off
#define CADENCE_OFF   (0)
#define CADENCE_ON    (1)

//Cadence_D1/D2/D3
#define CADENCE_DATA_DEFAULT_VALUE    (0x00)
#define CADENCE_DATA_MAX_VALUE        (0xFF)

#define _24HZ 24000
#define _25HZ 25000
#define _30HZ 30000
#define _48HZ 48000
#define _50HZ 50000
#define _60HZ 60000
#define _0HZ 0
#define _100HZ 100000
#define _120HZ 120000
#define _144HZ 144000
#define _288HZ 288000
#define _tHZ 500

#define _LATENCY_2_0 0x20
#define _LATENCY_3_0 0x30
#define _LATENCY_4_0 0x40
#define _LATENCY_4_5 0x45
#define _LATENCY_5_0 0x50
#define _60HZ_LOW_LATENCY 0x01
#define _120HZ_LOW_LATENCY 0x05
#define _MAX_LATENCY 5
#define _VG_SYNC_OFF (0x3)
#define _VG_SYNC_ON  (0x0)
#define _VG_SYNC_BYPASS_BY_DS (0x2)
#define _VG_SYNC_OFF_LATE   "VG sync turn off warning! Bypass-timing is too late, recover by DS.\n"
#define _VG_SYNC_OFF_EARLY  "VG sync turn off error!!! Bypass-timing is too eraly!\n"
#define _VG_SYNC_FRAME_INDEX_FROM_SHM "Frame index->SHM"

#define IS_INPUT_DTV(x)	((x >= META_PQ_INPUTSRC_DTV_DIS) && (x < META_PQ_INPUTSRC_DTV_DIS_MAX))
#define IS_INPUT_MM(x)	((x >= META_PQ_INPUTSRC_STORAGE_DIS) && \
				(x < META_PQ_INPUTSRC_STORAGE_DIS_MAX))
#define IS_INPUT_B2R(x)	(IS_INPUT_DTV(x) || IS_INPUT_MM(x))

#define MTK_DRM_TV_FRC_SMI_COMPATIBLE "mediatek,frc-smi"

#define INVALID_BUF_INDEX 0xFF

struct mtk_frc_timinginfo_tbl frc_timing_60Hz_table[] = {
		{_24HZ-_tHZ, _24HZ+_tHZ,   _LATENCY_2_0, _60HZ_LOW_LATENCY}, //24Hz
		{_25HZ-_tHZ, _25HZ+_tHZ,   _LATENCY_2_0, _60HZ_LOW_LATENCY}, //25Hz
		{_30HZ-_tHZ, _30HZ+_tHZ,   _LATENCY_3_0, _60HZ_LOW_LATENCY}, //30Hz
		{_48HZ-_tHZ, _48HZ+_tHZ,   _LATENCY_4_0, _60HZ_LOW_LATENCY}, //48Hz
		{_50HZ-_tHZ, _50HZ+_tHZ,   _LATENCY_4_0, _60HZ_LOW_LATENCY}, //50Hz
		{_60HZ-_tHZ, _60HZ+_tHZ,   _LATENCY_4_0, _60HZ_LOW_LATENCY}, //60Hz
		{_100HZ-_tHZ, _100HZ+_tHZ, _LATENCY_4_0, _60HZ_LOW_LATENCY}, //100Hz
		{_120HZ-_tHZ, _120HZ+_tHZ, _LATENCY_4_0, _60HZ_LOW_LATENCY}, //120Hz
		{      _0HZ,     _100HZ,   _LATENCY_4_0, _60HZ_LOW_LATENCY}, //Others
		{      _0HZ,     _100HZ,   _LATENCY_4_0, _60HZ_LOW_LATENCY}

};

struct mtk_frc_timinginfo_tbl frc_timing_120Hz_table[] = {
		{_24HZ-_tHZ, _24HZ+_tHZ,   _LATENCY_4_0,   _120HZ_LOW_LATENCY}, //24Hz
		{_25HZ-_tHZ, _25HZ+_tHZ,   _LATENCY_4_0,   _120HZ_LOW_LATENCY}, //25Hz
		{_30HZ-_tHZ, _30HZ+_tHZ,   _LATENCY_3_0,   _120HZ_LOW_LATENCY}, //30Hz
		{_48HZ-_tHZ, _48HZ+_tHZ,   _LATENCY_4_0,   _120HZ_LOW_LATENCY}, //48Hz
		{_50HZ-_tHZ, _50HZ+_tHZ,   _LATENCY_4_0,   _120HZ_LOW_LATENCY}, //50Hz
		{_60HZ-_tHZ, _60HZ+_tHZ,   _LATENCY_4_0,   _120HZ_LOW_LATENCY}, //60Hz
		{_100HZ-_tHZ, _100HZ+_tHZ, _LATENCY_4_0,   _120HZ_LOW_LATENCY}, //100Hz
		{_120HZ-_tHZ, _120HZ+_tHZ, _LATENCY_4_0,   _120HZ_LOW_LATENCY}, //120Hz
		{      _0HZ,     _100HZ,   _LATENCY_4_0,   _120HZ_LOW_LATENCY}, //Others
		{      _0HZ,     _100HZ,   _LATENCY_4_0,   _120HZ_LOW_LATENCY}

};

static uint64_t _u64VgsyncBufAddr;
static uint32_t _u32VgsyncBufSize;

#define _FREQ_70HZ 70
#define BIT_RIGHT_RIGHT_32 (32)
#define IS_OUTPUT_120HZ(x) ((x) > _FREQ_70HZ)


static uint64_t memc_init_value_for_rv55 = DEFAULT_VALUE;
static uint64_t memc_rv55_info = DEFAULT_VALUE;
static uint32_t frc_alg_run_on = FRC_ALG_PROCESSOR_OFF;

//MFCMode = MFCMode,DeJudderLevel,DeBlurLevel,FRC_Mode
static uint8_t MFCMode[MEMC_MODE_CNT] = {0x01, 0x00, 0x00, 0x00};

//RealCinemaMode
static uint8_t RealCinemaMode[REALCINEMA_MODE_CNT] = {0x00, 0x00, 0x00, 0x00};

//Cadence (bCadence_onoff/Cadence_D1/Cadence_D2/Cadence_D3)
static uint8_t Cadence_cmd[CADENCE_CNT] = {0x00, 0x00, 0x00, 0x00};

//For LTP test
//MFClevel 0~17 = Off, 18~39 = High
static uint8_t LTP_MFCLevel_OFF[MEMC_LEVLE_MAX_CNT] = {
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH
};

//MFClevel 0~17 = Low, 18~39 = High
static uint8_t LTP_MFCLevel_Low[MEMC_LEVLE_MAX_CNT] = {
MEMC_LEVEL_LOW, MEMC_LEVEL_LOW, MEMC_LEVEL_LOW,
MEMC_LEVEL_LOW, MEMC_LEVEL_LOW, MEMC_LEVEL_LOW,
MEMC_LEVEL_LOW, MEMC_LEVEL_LOW, MEMC_LEVEL_LOW,
MEMC_LEVEL_LOW, MEMC_LEVEL_LOW, MEMC_LEVEL_LOW,
MEMC_LEVEL_LOW, MEMC_LEVEL_LOW, MEMC_LEVEL_LOW,
MEMC_LEVEL_LOW, MEMC_LEVEL_LOW, MEMC_LEVEL_LOW,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH
};

//MFClevel 0~17 = Middle,18~39 = High,
static uint8_t LTP_MFCLevel_Mid[MEMC_LEVLE_MAX_CNT] = {
MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE,
MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE,
MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE,
MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE,
MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE,
MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE, MEMC_LEVEL_MIDDLE,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH
};

//MFClevel 0~39 = High
static uint8_t LTP_MFCLevel_High[MEMC_LEVLE_MAX_CNT] = {
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH
};
//---------------------------------------------------

//MFClevel 0~39 = default:21
static uint8_t MFCLevel[MEMC_LEVLE_MAX_CNT] = {
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH,
MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH, MEMC_LEVEL_HIGH
};

//DEMO_Mode = DEMO_MODE,PARTITION_MODE
static uint8_t DEMOMode[FRC_MODE_CNT] = {DEFAULT_DEMO_MODE, DEFAULT_PARTITION_MODE};

//Gaming_level = MFCMode,DeJudderLevel,DeBlurLevel,FRC_Mode
static uint8_t Gaming_level[MEMC_MODE_CNT] = {0x01, 0x00, 0x00, 0x00};
static uint8_t Gaming_MJC_Lvl;

//VRRorGame_MJC_lv_Off = MFCMode,DeJudderLevel,DeBlurLevel,FRC_Mode
static uint8_t _VRRorGame_MJC_lv_Off[MEMC_MODE_CNT] = {0x00, 0x00, 0x00, 0x00};

//_VRRorGame_MFCLevel_OFF 0~39 = Off
static uint8_t _VRRorGame_MFCLevel_OFF[MEMC_LEVLE_MAX_CNT] = {
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF,
MEMC_LEVEL_OFF, MEMC_LEVEL_OFF, MEMC_LEVEL_OFF
};

static bool bBypassMEMC = DEFAULT_VALUE;
static void *_vaddr_frc;
static void *_vaddr_vgsync;
static bool bIsnode_1stInit = TRUE;

static int _mtk_video_display_frc_check_ipm_hv_size(struct mtk_tv_kms_context *pctx);
static int mtk_video_display_frc_vsync_callback(void *priv_data);

static bool _mtk_video_display_frc_is_variable_updated(
	uint64_t oldVar,
	uint64_t newVar)
{
	bool update = NO_UPDATE;

	if (newVar != oldVar)
		update = UPDATE;

	return update;
}

static bool _mtk_video_display_frc_is_skip_game_memc(unsigned int plane_inx,
				struct mtk_tv_kms_context *pctx)
{
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct video_plane_prop *plane_props = NULL;

	enum mtk_video_frc_pq_scene scene = pctx_frc->frc_scene;
	uint32_t inputfreq = 0;
	uint8_t framesync = 0;
	bool bSkip = false;

	if ((crtc_props == NULL) || (pctx_video == NULL)) {
		DRM_ERROR("[%s, %d] pointer is null!\n", __func__, __LINE__);
		return false;
	}

	plane_props = (pctx_video->video_plane_properties + plane_inx);
	framesync = (uint8_t)crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE];
	inputfreq = div64_u64((plane_props->prop_val[EXT_VPLANE_PROP_INPUT_VFREQ] +
				_tHZ), VFRQRATIO_1000) * VFRQRATIO_1000; // +500 for round

	bSkip |= (inputfreq == _144HZ);
	bSkip |= (inputfreq == _288HZ);
	bSkip |= (framesync == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK);
	bSkip |= ((scene != MEMC_PQ_SCENE_GAME_CROP) &&
				(scene != MEMC_PQ_SCENE_GAME_WITH_MEMC));

	if (_mtk_video_display_frc_is_variable_updated(pctx_frc->bSkipGameMemc, bSkip)) {
		DRM_INFO("[%s,%d] fsync[%d], infreq[%d], scene[%d], bSkip[%d|%d]\n",
			__func__, __LINE__, framesync, inputfreq, scene,
			pctx_frc->bSkipGameMemc, bSkip);
		pctx_frc->bSkipGameMemc = bSkip;
	}

	return bSkip;
}

static void _mtk_video_display_set_init_done_for_rv55(void)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	int ret = 0;

	//FRC_MB_CMD_INIT_DONE
	send.CmdID = FRC_MB_CMD_INIT_DONE;
	send.D1    = DEFAULT_VALUE;
	send.D2    = DEFAULT_VALUE;
	send.D3    = DEFAULT_VALUE;
	send.D4    = DEFAULT_VALUE;
	ret = pqu_send_cmd(&send, &callback_info);
	if (ret != 0)
		DRM_ERROR("[%s %d]CMD FRC_MB_CMD_INIT_DONE Fail\n",
			__func__, __LINE__);
}

static void _mtk_video_display_set_init_value_for_rv55(
	struct mtk_tv_kms_context *pctx)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};

	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	int ret = 0;

	memc_init_value_for_rv55 = INIT_VALUE_FAIL;
	//FRC_MB_CMD_INIT
	send.CmdID = FRC_MB_CMD_INIT;
	send.D1    = DEFAULT_VALUE;
	send.D2    = DEFAULT_VALUE;
	send.D3    = DEFAULT_VALUE;
	send.D4    = DEFAULT_VALUE;
	ret = pqu_send_cmd(&send, &callback_info);
	if (ret != 0)
		DRM_ERROR("[%s %d]CMD FRC_MB_CMD_INIT Fail\n",
			__func__, __LINE__);


    //FRC_MB_CMD_SET_TIMING
	send.CmdID = FRC_MB_CMD_SET_TIMING;
	send.D1    = (uint32_t)INIT_FREQ;  //60Hz
	send.D2    = (uint32_t)INIT_FREQ;  //60Hz
	send.D3    = DEFAULT_VALUE;
	send.D4    = DEFAULT_VALUE;
	ret = pqu_send_cmd(&send, &callback_info);
	if (ret != 0)
		DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_TIMING Fail\n",
			__func__, __LINE__);


	//FRC_MB_CMD_SET_INPUT_FRAME_SIZE
	send.CmdID = FRC_MB_CMD_SET_INPUT_FRAME_SIZE;
	send.D1    = (uint32_t)INIT_H_SIZE;  // 7680
	send.D2    = (uint32_t)INIT_V_SIZE;  // 4320
	send.D3    = DEFAULT_VALUE;
	send.D4    = DEFAULT_VALUE;
	ret = pqu_send_cmd(&send, &callback_info);
	if (ret != 0)
		DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_INPUT_FRAME_SIZE Fail\n",
			__func__, __LINE__);


	//FRC_MB_CMD_SET_OUTPUT_FRAME_SIZE
	send.CmdID = FRC_MB_CMD_SET_OUTPUT_FRAME_SIZE;
	send.D1    = (uint32_t)INIT_H_SIZE;  // 7680
	send.D2    = (uint32_t)INIT_V_SIZE;  // 4320
	send.D3    = DEFAULT_VALUE;
	send.D4    = DEFAULT_VALUE;
	ret = pqu_send_cmd(&send, &callback_info);
	if (ret != 0)
		DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_OUTPUT_FRAME_SIZE Fail\n",
			__func__, __LINE__);


    //FRC_MB_CMD_SET_FRC_MODE
	send.CmdID = FRC_MB_CMD_SET_FRC_MODE;
	send.D1    = E_MEMC_LBW;
	send.D2    = E_FRC_LBW_NORMAL;
	send.D3    = DEFAULT_VALUE;
	send.D4    = DEFAULT_VALUE;
	ret = pqu_send_cmd(&send, &callback_info);
	if (ret != 0)
		DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_FRC_MODE Fail\n",
			__func__, __LINE__);

	//FRC_MB_CMD_SET_SHARE_MEMORY_ADDR
	if (pctx_video) {
		send.CmdID = FRC_MB_CMD_SET_SHARE_MEMORY_ADDR;
		send.D1    = (uint32_t)(pctx_frc->frc_shm_adr >> BIT_RIGHT_RIGHT_32);
		send.D2    = (uint32_t)pctx_frc->frc_shm_adr;
		send.D3    = pctx_frc->frc_shm_size;
		send.D4    = DEFAULT_VALUE;
		ret = pqu_send_cmd(&send, &callback_info);
		if (ret != 0)
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_SHARE_MEMORY_ADDR Fail\n",
				__func__, __LINE__);
	}

	//FRC_MB_CMD_SET_SHARE_MEMORY_ADDR
	if ((_u64VgsyncBufAddr != 0) &&
		(_u32VgsyncBufSize != 0)) {
		send.CmdID = FRC_MB_CMD_SET_SHARE_MEMORY_ADDR;
		send.D1    = (uint32_t)(_u64VgsyncBufAddr >> BIT_RIGHT_RIGHT_32);
		send.D2    = (uint32_t)_u64VgsyncBufAddr;
		send.D3    = _u32VgsyncBufSize;
		send.D4    = CMD_D4_VGSNYC_BUF_INDEX; // 0X01
		ret = pqu_send_cmd(&send, &callback_info);
		if (ret != 0)
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_SHARE_MEMORY_ADDR Fail\n",
				__func__, __LINE__);
	}

	memc_init_value_for_rv55 = INIT_VALUE_PASS;
}

//set memc input size
static void _mtk_video_display_set_input_frame_size(unsigned int plane_inx,
struct mtk_tv_kms_context *pctx, struct mtk_plane_state *state)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};

	static uint32_t pre_width  = DEFAULT_VALUE;
	static uint32_t pre_height = DEFAULT_VALUE;
	static uint32_t cmdstatus  = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_inx);
	enum drm_mtk_video_plane_type video_plane_type =
	plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
		pre_width  = DEFAULT_VALUE;
		pre_height = DEFAULT_VALUE;
	}

	if (pctx->stub)
		force = TRUE;

	if ((force == FALSE) && (video_plane_type != MTK_VPLANE_TYPE_MEMC))
		return;

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_width, (state->base.src_w>>SIZERATIO_16))) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_height, (state->base.src_h>>SIZERATIO_16)))
	){
		DRM_INFO("[%s,%d] pre_width = 0x%x, pre_height = 0x%x\n",
			__func__, __LINE__, pre_width, pre_height);
		send.CmdID = FRC_MB_CMD_SET_INPUT_FRAME_SIZE;
		send.D1    = (state->base.src_w>>SIZERATIO_16);
		send.D2    = (state->base.src_h>>SIZERATIO_16);
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus = pqu_send_cmd(&send, &callback_info);
		pre_width  = (state->base.src_w>>SIZERATIO_16);
		pre_height = (state->base.src_h>>SIZERATIO_16);
		DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);
		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|INPUT_SIZE_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~INPUT_SIZE_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_INPUT_FRAME_SIZE Fail\n",
				__func__, __LINE__);
		}
	}
}

//set memc output size
static void _mtk_video_display_set_output_frame_size(unsigned int plane_inx,
struct mtk_tv_kms_context *pctx, struct mtk_plane_state *state)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};

	static uint32_t pre_width  = DEFAULT_VALUE;
	static uint32_t pre_height = DEFAULT_VALUE;
	static uint32_t cmdstatus  = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_inx);
	enum drm_mtk_video_plane_type video_plane_type =
	plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
		pre_width  = DEFAULT_VALUE;
		pre_height = DEFAULT_VALUE;
	}

	if (pctx->stub)
		force = TRUE;

	if ((force == FALSE) && (video_plane_type != MTK_VPLANE_TYPE_MEMC))
		return;

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_width, state->base.crtc_w)) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_height, state->base.crtc_h))
	){
		DRM_INFO("[%s,%d] pre_width = 0x%x, pre_height = 0x%x\n",
			__func__, __LINE__, pre_width, pre_height);
		send.CmdID = FRC_MB_CMD_SET_MFC_FRC_DS_SETTINGS;
		send.D1    = state->base.crtc_w;
		send.D2    = state->base.crtc_h;
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus  = pqu_send_cmd(&send, &callback_info);

		pre_width  = state->base.crtc_w;
		pre_height = state->base.crtc_h;
		DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);
		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|OUTPUT_SIZE_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~OUTPUT_SIZE_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_MFC_FRC_DS_SETTINGS Fail\n",
				__func__, __LINE__);
		}
	}
}
#if (0) //Remove to PQ DD
static void _mtk_video_display_frc_set_rv55_pts(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	if (pctx_frc) {
		struct mtk_frc_shm_info *frc_shm_info =
		 (struct mtk_frc_shm_info *)pctx_frc->frc_shm_virAdr;

		pctx_frc->frc_frame_count++;
		if (frc_shm_info != NULL) {
			frc_shm_info->pts = pctx_frc->u64frcpts;
			frc_shm_info->frame_count = pctx_frc->frc_frame_count;
			DRM_INFO("[%s,%d]Current pts = %lld, frame_count = %d.\n",
				__func__, __LINE__, frc_shm_info->pts, frc_shm_info->frame_count);
		}
	}
}
#endif
static void _mtk_video_display_set_fpll_lock_done(struct mtk_tv_kms_context *pctx)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	static bool fpll_lock     = DEFAULT_VALUE;
	static bool pre_fpll_lock = DEFAULT_FPLL_LOCK;
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE)
		pre_fpll_lock = DEFAULT_FPLL_LOCK;

	if (pctx->stub) {
		force = TRUE;
		pctx_pnl->outputTiming_info.locked_flag = TRUE;
	}
	fpll_lock = pctx_pnl->outputTiming_info.locked_flag;

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_fpll_lock, fpll_lock))
	){
		DRM_INFO("[%s,%d] pre_fpll_lock = 0x%x\n",
			__func__, __LINE__, pre_fpll_lock);
		send.CmdID = FRC_MB_CMD_SET_FPLL_LOCK_DONE;
		send.D1    = (uint32_t)fpll_lock;
		send.D2    = DEFAULT_VALUE;
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus = pqu_send_cmd(&send, &callback_info);
		pre_fpll_lock = fpll_lock;
		DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);
		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|FPLL_LOCK_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~FPLL_LOCK_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_FPLL_LOCK_DONE Fail\n",
				__func__, __LINE__);
		}
	}
}

//set timing
void mtk_video_display_set_timing(unsigned int plane_inx, struct mtk_tv_kms_context *pctx,
	uint32_t input_Vfreq, uint32_t output_Vfreq)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	//struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_inx);
	enum drm_mtk_video_plane_type video_plane_type =
	plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];

	static uint32_t input_freq      = DEFAULT_VALUE;
	static uint32_t pre_input_freq  = DEFAULT_VALUE;
	static uint32_t output_freq     = DEFAULT_VALUE;
	static uint32_t pre_output_freq = DEFAULT_VALUE;
	static uint32_t cmdstatus       = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
		pre_input_freq  = DEFAULT_VALUE;
		pre_output_freq = DEFAULT_VALUE;
	}

	if (pctx->stub)
		force = TRUE;

	if ((force == FALSE) && (video_plane_type != MTK_VPLANE_TYPE_MEMC))
		return;

	input_freq  = ((input_Vfreq + _tHZ)/VFRQRATIO_1000);
	output_freq = ((output_Vfreq+ _tHZ)/VFRQRATIO_1000);

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_input_freq, input_freq)) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_output_freq, output_freq))
	){
		DRM_INFO("[%s,%d] pre_input_freq = 0x%x, pre_output_freq = 0x%x\n",
			__func__, __LINE__, pre_input_freq, pre_output_freq);

		send.CmdID = FRC_MB_CMD_SET_TIMING;
		send.D1    = (uint32_t)input_freq;
		send.D2    = (uint32_t)output_freq;
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus  = pqu_send_cmd(&send, &callback_info);
		pre_input_freq  = input_freq;
		pre_output_freq = output_freq;
		DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);

	}
}

//set timing
static void _mtk_video_display_set_timing(unsigned int plane_inx, struct mtk_tv_kms_context *pctx)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_inx);
	enum drm_mtk_video_plane_type video_plane_type =
	plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];


	static uint32_t input_freq      = DEFAULT_VALUE;
	static uint32_t pre_input_freq  = DEFAULT_VALUE;
	static uint32_t output_freq     = DEFAULT_VALUE;
	static uint32_t pre_output_freq = DEFAULT_VALUE;
	static uint32_t cmdstatus       = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
		pre_input_freq  = DEFAULT_VALUE;
		pre_output_freq = DEFAULT_VALUE;
	}

	if (pctx->stub)
		force = TRUE;

	if ((force == FALSE) && (video_plane_type != MTK_VPLANE_TYPE_MEMC))
		return;

	input_freq  = div64_u64((plane_props->prop_val[EXT_VPLANE_PROP_INPUT_VFREQ] +
		_tHZ), VFRQRATIO_1000); // +500 for round
	output_freq = ((pctx_pnl->outputTiming_info.u32OutputVfreq +
		_tHZ)/VFRQRATIO_1000); // +500 for round

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_input_freq, input_freq)) ||
		(_mtk_video_display_frc_is_variable_updated(
				pre_output_freq, output_freq))
	){
		DRM_INFO("[%s,%d] pre_input_freq = 0x%x, pre_output_freq = 0x%x\n",
			__func__, __LINE__, pre_input_freq, pre_output_freq);

		send.CmdID = FRC_MB_CMD_SET_TIMING;
		send.D1    = (uint32_t)input_freq;
		send.D2    = (uint32_t)output_freq;
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus  = pqu_send_cmd(&send, &callback_info);
		pre_input_freq  = input_freq;
		pre_output_freq = output_freq;
		DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);

		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|TIMING_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~TIMING_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_TIMING Fail\n",
				__func__, __LINE__);
		}
	}
}

struct mtk_frc_timinginfo_tbl *_mtk_video_frc_find_in_timing_mappinginfo(uint32_t output_freq,
	uint32_t input_freq)
{
	uint16_t tablenum;
	struct mtk_frc_timinginfo_tbl *pinfotable = NULL;
	uint16_t index;

	if (IS_OUTPUT_120HZ(output_freq)) {
		tablenum = (sizeof(frc_timing_120Hz_table) /
			sizeof(struct mtk_frc_timinginfo_tbl));
		pinfotable = frc_timing_120Hz_table;
	} else {
		tablenum = (sizeof(frc_timing_60Hz_table) /
			sizeof(struct mtk_frc_timinginfo_tbl));
		pinfotable = frc_timing_60Hz_table;
	}

	for (index = 0; index < (tablenum - 1); index++) {
		if ((input_freq > pinfotable->input_freq_L) &&
			(input_freq < pinfotable->input_freq_H))
			break;
		else
			pinfotable++;
	}
	return pinfotable;
}

static int _mtk_video_display_check_photo_mode(struct mtk_tv_kms_context *pctx,
	bool *bIsPhotoMode)
{
	struct mtk_frc_context *pctx_frc = NULL;

	if ((pctx == NULL) || (bIsPhotoMode == NULL)) {
		DRM_ERROR("[%s, %d] pointer is null\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	*bIsPhotoMode  = false;
	pctx_frc = &pctx->frc_priv;

	if (pctx_frc == NULL) {
		DRM_ERROR("[%s, %d] pctx_frc is null\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	if ((pctx_frc->frc_photo_enable == TRUE) &&
		(pctx->trigger_gen_info.eSourceType == MTK_SOURCE_TYPE_PHOTO)) {
		*bIsPhotoMode  = true;
	}

	return 0;
}


#define _MSB(x) (x>>4)
#define _LSB(x) (x&0x0F)
#define _HEX2DEC(x) ((_MSB(x)*10)+_LSB(x))
#define _CHANGE_2_RWDIFF(x) (x>>4)
#define _DIV10 10
#define _1_SECOND 1000000ul
#define _RW_DIFF_BUF 0x10

void _mtk_video_frc_get_latency(unsigned int plane_inx, struct mtk_tv_kms_context *pctx,
	bool enable)
{
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_inx);

	struct mtk_frc_timinginfo_tbl *pinfotable = NULL;

	static uint32_t input_freq      = DEFAULT_VALUE;
	static uint32_t pre_input_freq  = DEFAULT_VALUE;
	static uint32_t output_freq     = DEFAULT_VALUE;
	bool force = FALSE;
	bool pre_enable = FALSE;
	uint8_t u8latency = 0;
	bool bIsPhotoMode = FALSE;
	int ret = 0;

	DRM_INFO("\n plane_inx=%d", plane_inx);
	input_freq  = plane_props->prop_val[EXT_VPLANE_PROP_INPUT_VFREQ];
	output_freq = (pctx_pnl->outputTiming_info.u32OutputVfreq/VFRQRATIO_1000);
	DRM_INFO("\n[PH]input_freq=%d, output_freq=%d", input_freq, output_freq);

	u8latency = IS_OUTPUT_120HZ(output_freq) ? _120HZ_LOW_LATENCY : _60HZ_LOW_LATENCY;

	API_FRC_UpdateSetting(pctx_video->reg_num, pctx->panel_priv.info.de_height * output_freq);

	if ((force == TRUE) || (pre_enable != enable) ||
			(_mtk_video_display_frc_is_variable_updated(
					pre_input_freq, input_freq))) {
		pinfotable = _mtk_video_frc_find_in_timing_mappinginfo(output_freq, input_freq);
		if (pinfotable)
			u8latency = pinfotable->frc_memc_latency;

		u8latency = _HEX2DEC(u8latency);
		if (enable) {
			if (input_freq)
				pctx_frc->frc_latency = _1_SECOND/input_freq*u8latency/_DIV10;
			else
				pctx_frc->frc_latency = 0;
		} else {
			pctx_frc->frc_latency = 0;
		}
		DRM_INFO("[%s,%d] frc_latency= %d\n",
					__func__, __LINE__, pctx_frc->frc_latency);

		ret = _mtk_video_display_check_photo_mode(pctx, &bIsPhotoMode);
		if (ret < 0) {
			DRM_ERROR("[%s, %d] check_photo_mode failed\n",
				__func__, __LINE__);
			return;
		}

		pre_input_freq  = input_freq;
		pre_enable = enable;
		if (pctx_frc->frc_user_rwdiff)
			pctx_frc->frc_rwdiff = pctx_frc->frc_user_rwdiff;
		else {
			if (bIsPhotoMode == TRUE)
				pctx_frc->frc_rwdiff = pctx_frc->frc_photo_rw_diff;
			else
				pctx_frc->frc_rwdiff = pctx_frc->frc_dts_rwdiff;
		}
	}
}

void _mtk_video_frc_get_latency_preplay(uint32_t input_freq, struct mtk_tv_kms_context *pctx)
{
	struct mtk_panel_context *pctx_pnl = &(pctx->panel_priv);
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	struct mtk_frc_timinginfo_tbl *pinfotable = NULL;

	static uint32_t pre_input_freq  = DEFAULT_VALUE;
	static uint32_t output_freq     = DEFAULT_VALUE;
	uint8_t u8latency = 0;
	uint8_t u8RWdiff = 0;
	uint8_t utemp8latency = 0;
	bool bIsPhotoMode = false;
	int ret = 0;

	output_freq = (pctx_pnl->outputTiming_info.u32OutputVfreq/VFRQRATIO_1000);
	DRM_INFO("\n input_freq=%d, output_freq=%d", input_freq, output_freq);

	utemp8latency = IS_OUTPUT_120HZ(output_freq) ? _120HZ_LOW_LATENCY : _60HZ_LOW_LATENCY;

	if (_mtk_video_display_frc_is_variable_updated(pre_input_freq, input_freq)) {
		pinfotable = _mtk_video_frc_find_in_timing_mappinginfo(output_freq, input_freq);
		if (pinfotable)
			utemp8latency = pinfotable->frc_memc_latency;

		u8latency = _HEX2DEC(utemp8latency);

		u8RWdiff = _CHANGE_2_RWDIFF((utemp8latency + _RW_DIFF_BUF)); //Add 1 frame BUFF

		if (u8RWdiff > _MAX_LATENCY)
			u8RWdiff = _MAX_LATENCY;

		DRM_INFO("[%s,%d] input_freq = %d u8latency = %d u8RWdiff = %d\n",
				__func__, __LINE__, input_freq, u8latency, u8RWdiff);

		ret = _mtk_video_display_check_photo_mode(pctx, &bIsPhotoMode);
		if (ret < 0) {
			DRM_ERROR("[%s, %d] check_photo_mode failed\n",
					__func__, __LINE__);
			return;
		}

		if (input_freq) {
			pctx_frc->frc_latency = _1_SECOND/input_freq*u8latency/_DIV10;
			pctx_frc->frc_rwdiff = u8RWdiff;
		} else {
			pctx_frc->frc_latency = 0;
			pctx_frc->frc_rwdiff = pctx_frc->frc_dts_rwdiff;
		}

		if (bIsPhotoMode == true)
			pctx_frc->frc_rwdiff = pctx_frc->frc_photo_rw_diff;
		else {
			if (pctx_frc->frc_user_rwdiff) //user input
				pctx_frc->frc_rwdiff = pctx_frc->frc_user_rwdiff;
		}

		DRM_INFO("[%s,%d] frc_latency_preplay= %d frc_rwdiff = %d\n",
			__func__, __LINE__, pctx_frc->frc_latency, pctx_frc->frc_rwdiff);

		pre_input_freq  = input_freq;
	}
}

uint32_t mtk_video_display_frc_set_Qos(uint8_t QosMode)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	//FRC_MB_CMD_INIT_DONE
	send.CmdID = FRC_MB_CMD_SET_QoS;
	send.D1    = QosMode;
	send.D2    = DEFAULT_VALUE;
	send.D3    = DEFAULT_VALUE;
	send.D4    = DEFAULT_VALUE;

	cmdstatus = pqu_send_cmd(&send, &callback_info);

	if (cmdstatus == 0) {
		// None
	} else {
		memc_rv55_info = (memc_rv55_info&(~FPLL_LOCK_CMD_STATUS));
		DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_QoS Fail\n",
			__func__, __LINE__);
	}

	return cmdstatus;
}

static uint32_t _mtk_video_display_frc_get_Qos(uint8_t *pQosMode)
{
	struct pqu_frc_get_cmd_param send = {DEFAULT_VALUE};
	struct pqu_frc_get_cmd_reply_param reply = {DEFAULT_VALUE};

	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	if (frc_alg_run_on == FRC_ALG_PROCESSOR_RV55) {

		send.CmdID = FRC_MB_CMD_GET_QoS;
		send.D1    = DEFAULT_VALUE;
		send.D2    = DEFAULT_VALUE;
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus = pqu_get_cmd(&send, &reply);
		if (cmdstatus == 0) {
			// None
		} else {
			memc_rv55_info = (memc_rv55_info&(~FPLL_LOCK_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_GET_QoS Fail\n",
				__func__, __LINE__);
		}

		DRM_INFO("[%s,%d] [%x]D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.CmdID, reply.D1, reply.D2, reply.D3, reply.D4);
		*pQosMode = reply.D1;
		}
	return cmdstatus;
}

uint32_t mtk_video_display_frc_get_Qos(uint8_t *pQosMode)
{
	return _mtk_video_display_frc_get_Qos(pQosMode);
}

static void _mtk_video_display_update_Qos(struct mtk_tv_kms_context *pctx)
{
	uint8_t PreQosMode, QosMode;
	static uint32_t cmdstatus   = DEFAULT_CMD_STATUS;
	bool force = FALSE;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	if (pctx_frc->frc_alg_run_on == FRC_ALG_PROCESSOR_RV55) {

		if (pctx->stub)
			force = TRUE;

		if (force == TRUE) {
			PreQosMode = true;
			cmdstatus = mtk_video_display_frc_set_Qos(PreQosMode);
			if (cmdstatus != 0) {
				memc_rv55_info = (memc_rv55_info&(~MFC_PW_STATUS));
				DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_QoS Fail\n",
					__func__, __LINE__);
				return;
			}

			cmdstatus = mtk_video_display_frc_get_Qos(&QosMode);
			if (cmdstatus != 0) {
				memc_rv55_info = (memc_rv55_info&(~MFC_PW_STATUS));
				DRM_ERROR("[%s %d]CMD FRC_MB_CMD_GET_QoS Fail\n",
					__func__, __LINE__);
				return;
			}

			if (PreQosMode == QosMode)
				memc_rv55_info = (memc_rv55_info|MFC_PW_STATUS);
			else
				memc_rv55_info = (memc_rv55_info&(~MFC_PW_STATUS));
			}
		}
	else {

		memc_rv55_info = (memc_rv55_info|MFC_PW_STATUS);
	}
}


// Parsing and update memc mode
static void _mtk_video_display_parsing_and_update_memc_mode(char *memc_mode)
{
	char Mode_off[]  = "Off;";
	char Mode_on[]   = "On;";
	char Mode_auto[] = "Auto;";
	char Mode_user[] = "User;";
	char Mode_bypass[] = "Bypass;";

	if (memc_mode == NULL) {
		DRM_ERROR("[%s %d] memc_mode = NULL\n", __func__, __LINE__);
		return;
	}

	memc_mode += strlen("MEMC_ONOFF:");
	if (strncmp(Mode_off, memc_mode, strlen(Mode_off)) == 0) {
		MFCMode[DATA_D0] = MEMC_MODE_OFF;
		bBypassMEMC = FALSE;
		DRM_INFO("[%s,%d] MFCMode[0]= 0x%x\n", __func__, __LINE__, MFCMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|MEMC_OFF_STATUS);
	} else if (strncmp(Mode_on, memc_mode, strlen(Mode_on)) == 0) {
		MFCMode[DATA_D0] = MEMC_MODE_ON;
		bBypassMEMC = FALSE;
		DRM_INFO("[%s,%d] MFCMode[0]= 0x%x\n", __func__, __LINE__, MFCMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|MEMC_ON_STATUS);
	} else if (strncmp(Mode_auto, memc_mode, strlen(Mode_auto)) == 0) {
		MFCMode[DATA_D0] = MEMC_MODE_AUTO;
		bBypassMEMC = FALSE;
		DRM_INFO("[%s,%d] MFCMode[0]= 0x%x\n", __func__, __LINE__, MFCMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|MEMC_AUTO_STATUS);
	} else if (strncmp(Mode_user, memc_mode, strlen(Mode_user)) == 0) {
		MFCMode[DATA_D0] = MEMC_MODE_USER;
		bBypassMEMC = FALSE;
		DRM_INFO("[%s,%d] MFCMode[0]= 0x%x\n", __func__, __LINE__, MFCMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|MEMC_USER_STATUS);
	} else if (strncmp(Mode_bypass, memc_mode, strlen(Mode_bypass)) == 0) {
		//MEMC OFF + Low delay
		MFCMode[DATA_D0] = MEMC_MODE_OFF;
		bBypassMEMC = TRUE;
		DRM_INFO("[%s,%d] MFCMode[0]= 0x%x\n", __func__, __LINE__, MFCMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|MEMC_BYPASS_STATUS);
	} else {
		DRM_ERROR("[%s %d] unsupport exception memc mode\n", __func__, __LINE__);
		memc_rv55_info = (memc_rv55_info&(~(MEMC_STATUS|
			MEMC_USER_STATUS|MEMC_BYPASS_STATUS)));
	}
}

// Parsing and update memc demo mode
static void _mtk_video_display_parsing_and_update_demo_mode(
			char *memc_demo,
			char *demo_partition)
{
	char Demo_off[]  = "Off;";
	char Demo_on[]   = "On;";
	char partition_left[]  = "Left;";
	char partition_right[] = "Right;";
	char partition_top[]   = "Top;";
	char partition_dwon[]  = "Down;";
	char partition_all[]  = "All;";

	if (memc_demo == NULL) {
		DRM_ERROR("[%s %d] memc_demo = NULL\n", __func__, __LINE__);
		return;
	}

	if (demo_partition == NULL) {
		DRM_ERROR("[%s %d] demo_partition = NULL\n", __func__, __LINE__);
		return;
	}

	memc_demo += strlen("MJC_Demo:");
	demo_partition += strlen("MJC_Demo_Partition:");
	if (strncmp(Demo_off, memc_demo, strlen(Demo_off)) == 0) {
		DEMOMode[DATA_D0] = MEMC_MODE_OFF;
		DRM_INFO("[%s,%d] DEMOMode[0]= 0x%x\n", __func__, __LINE__, DEMOMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|DEMO_OFF_STATUS);
	} else if ((strncmp(Demo_on, memc_demo, strlen(Demo_on)) == 0) &&
			(strncmp(partition_left, demo_partition, strlen(partition_left)) == 0)
	 ){
		DEMOMode[DATA_D0] = DEMO_MODE_ON_LEFT_RIGHT;
		DEMOMode[DATA_D1] = DEMO_PARTITION_LEFT_OR_TOP;
		DRM_INFO("[%s,%d] DEMOMode[0]= 0x%x,DEMOMode[1]= 0x%x\n", __func__, __LINE__,
			DEMOMode[DATA_D0], DEMOMode[DATA_D1]);
		memc_rv55_info = (memc_rv55_info|DEMO_ON_LEFT_STATUS);
	} else if ((strncmp(Demo_on, memc_demo, strlen(Demo_on)) == 0) &&
			(strncmp(partition_right, demo_partition, strlen(partition_right)) == 0)
	 ){
		DEMOMode[DATA_D0] = DEMO_MODE_ON_LEFT_RIGHT;
		DEMOMode[DATA_D1] = DEMO_PARTITION_RIGHT_OR_DOWN;
		DRM_INFO("[%s,%d] DEMOMode[0]= 0x%x,DEMOMode[1]= 0x%x\n", __func__, __LINE__,
			DEMOMode[DATA_D0], DEMOMode[DATA_D1]);
		memc_rv55_info = (memc_rv55_info|DEMO_ON_RIGHT_STATUS);
	} else if ((strncmp(Demo_on, memc_demo, strlen(Demo_on)) == 0) &&
			(strncmp(partition_top, demo_partition, strlen(partition_top)) == 0)
	 ){
		DEMOMode[DATA_D0] = DEMO_MODE_ON_TOP_DOWN;
		DEMOMode[DATA_D1] = DEMO_PARTITION_LEFT_OR_TOP;
		DRM_INFO("[%s,%d] DEMOMode[0]= 0x%x,DEMOMode[1]= 0x%x\n", __func__, __LINE__,
			DEMOMode[DATA_D0], DEMOMode[DATA_D1]);
	} else if ((strncmp(Demo_on, memc_demo, strlen(Demo_on)) == 0) &&
			(strncmp(partition_dwon, demo_partition, strlen(partition_dwon)) == 0)
	 ){
		DEMOMode[DATA_D0] = DEMO_MODE_ON_TOP_DOWN;
		DEMOMode[DATA_D1] = DEMO_PARTITION_RIGHT_OR_DOWN;
		DRM_INFO("[%s,%d] DEMOMode[0]= 0x%x,DEMOMode[1]= 0x%x\n", __func__, __LINE__,
			DEMOMode[DATA_D0], DEMOMode[DATA_D1]);
	} else if ((strncmp(Demo_on, memc_demo, strlen(Demo_on)) == 0) &&
			(strncmp(partition_all, demo_partition, strlen(partition_all)) == 0)
	 ){
		DEMOMode[DATA_D0] = MEMC_MODE_OFF;
		DRM_INFO("[%s,%d] DEMOMode[0]= 0x%x\n", __func__, __LINE__, DEMOMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|DEMO_ON_ALL_STATUS);
	} else {
		DRM_ERROR("[%s %d] unsupport exception demo mode\n",	__func__, __LINE__);
		memc_rv55_info = (memc_rv55_info&(~(DEMO_STATUS)));
	}
}

// Parsing and update memc level
static void _mtk_video_display_parsing_and_update_memc_level(char *memc_level)
{
	char memc_level_start[]  = "[";
	char memc_level_comma[]  = ",";
	char memc_level_end[]    = "];";
	char *p_one_level_end = NULL;
	unsigned long ul_one_memc_level = 0;
	uint8_t memc_level_value = 0;
	uint8_t data_index = 0;

	if (memc_level == NULL) {
		DRM_ERROR("[%s %d] memc_level = NULL\n", __func__, __LINE__);
		return;
	}

	memc_level += strlen("MEMC_Level:");
	if ((*memc_level) == memc_level_start[DATA_D0]) {
		memc_level += strlen(memc_level_start);
		for (data_index = 0; data_index < MEMC_LEVLE_MAX_CNT; data_index++) {
			ul_one_memc_level = strtoul(memc_level, &p_one_level_end, 0);

			if (ul_one_memc_level < LIMIT_MEMC_LEVEL)
				memc_level_value = (uint8_t)(ul_one_memc_level);
			else
				DRM_ERROR("[%s %d] unsupport exception memc_level_value\n",
				__func__, __LINE__);

			MFCLevel[data_index] = memc_level_value;
			DRM_INFO("[%s,%d] MFCLevel[%d]= 0x%x\n", __func__, __LINE__,
				data_index, MFCLevel[data_index]);

			if (strncmp(memc_level_end, p_one_level_end,
								strlen(memc_level_end)) == 0) {
				break;
			} else if ((*p_one_level_end) == memc_level_comma[DATA_D0]) {
				memc_level = p_one_level_end;
				memc_level += strlen(memc_level_comma);
			} else {
				DRM_ERROR("[%s %d] unsupport exception memc_level\n",
									__func__, __LINE__);
			}
		}
	} else {
		DRM_ERROR("[%s %d] unsupport exception memc_level\n",
			__func__, __LINE__);
	}

	//For LTP
	if (memcmp(MFCLevel, LTP_MFCLevel_OFF, sizeof(MFCLevel)) == 0)
		memc_rv55_info = (memc_rv55_info|MEMC_LV_OFF_STATUS);
	else if (memcmp(MFCLevel, LTP_MFCLevel_Low, sizeof(MFCLevel)) == 0)
		memc_rv55_info = (memc_rv55_info|MEMC_LV_LOW_STATUS);
	else if (memcmp(MFCLevel, LTP_MFCLevel_Mid, sizeof(MFCLevel)) == 0)
		memc_rv55_info = (memc_rv55_info|MEMC_LV_MID_STATUS);
	else if (memcmp(MFCLevel, LTP_MFCLevel_High, sizeof(MFCLevel)) == 0)
		memc_rv55_info = (memc_rv55_info|MEMC_LV_HIGH_STATUS);
	else
		memc_rv55_info = (memc_rv55_info&(~MEMC_LV_STATUS));

}

// Parsing and update Gaming_MJC_Lvl
static void _mtk_video_display_parsing_and_update_gaming_level(char *gaming_level)
{
	unsigned long ul_one_gaming_level = 0;
	uint8_t gaming_level_value = GAMING_LEVLE_INVALID;
	char *p_one_level_end = NULL;

	if (gaming_level == NULL) {
		DRM_ERROR("[%s %d] gaming_level = NULL\n", __func__, __LINE__);
		return;
	}

	gaming_level += strlen("Gaming_MJC_Lvl:");
	ul_one_gaming_level = strtoul(gaming_level, &p_one_level_end, 0);

	if (ul_one_gaming_level <= GAMING_LEVLE_MAX) {
		gaming_level_value = (uint8_t)(ul_one_gaming_level);
		Gaming_level[DATA_D0] = MEMC_MODE_USER;
		Gaming_MJC_Lvl = gaming_level_value;
		//Use dejudder_level/deblur_level value
		//Gaming_level[DATA_D1] = gaming_level_value;
		//Gaming_level[DATA_D2] = gaming_level_value;
	} else
		DRM_ERROR("[%s %d] unsupport exception ul_one_gaming_level\n",
		__func__, __LINE__);

	//For LTP
	if (gaming_level_value <= GAMING_LEVLE_MAX)
		memc_rv55_info = (memc_rv55_info|GAMING_LV_STATUS);
	else
		memc_rv55_info = (memc_rv55_info&(~GAMING_LV_STATUS));

}

// Parsing and update dejudder_level
static void _mtk_video_display_parsing_and_update_p_dejudder_level(char *dejudder_level)
{
	unsigned long ul_one_dejudder_level = 0;
	uint8_t dejudder_level_value = DEJUDDER_LEVLE_INVALID;
	char *p_one_level_end = NULL;

	if (dejudder_level == NULL) {
		DRM_ERROR("[%s %d] dejudder_level = NULL\n", __func__, __LINE__);
		return;
	}

	dejudder_level += strlen("MJC_Dejudder:");
	ul_one_dejudder_level = strtoul(dejudder_level, &p_one_level_end, 0);

	if (ul_one_dejudder_level <= DEBLUR_LEVLE_MAX) {
		dejudder_level_value = (uint8_t)(ul_one_dejudder_level);
		MFCMode[DATA_D1] = dejudder_level_value;
		Gaming_level[DATA_D1] = dejudder_level_value;
	} else
		DRM_ERROR("[%s %d] unsupport exception ul_one_dejudder_level\n",
		__func__, __LINE__);

	//For LTP
	if (dejudder_level_value <= DEJUDDER_LEVLE_MAX)
		memc_rv55_info = (memc_rv55_info|DEJUDDER_STATUS);
	else
		memc_rv55_info = (memc_rv55_info&(~DEJUDDER_STATUS));

}

// Parsing and update mjc_deblur
static void _mtk_video_display_parsing_and_update_p_deblur_level(char *deblur_level)
{
	unsigned long ul_one_deblur_level = 0;
	uint8_t deblur_level_value = DEBLUR_LEVLE_INVALID;
	char *p_one_level_end = NULL;

	if (deblur_level == NULL) {
		DRM_ERROR("[%s %d] deblur_level = NULL\n", __func__, __LINE__);
		return;
	}

	deblur_level += strlen("MJC_Deblur:");
	ul_one_deblur_level = strtoul(deblur_level, &p_one_level_end, 0);

	if (ul_one_deblur_level <= DEBLUR_LEVLE_MAX) {
		deblur_level_value = (uint8_t)(ul_one_deblur_level);
		MFCMode[DATA_D2] = deblur_level_value;
		Gaming_level[DATA_D2] = deblur_level_value;
	} else
		DRM_ERROR("[%s %d] unsupport exception ul_one_mjc_deblur\n",
		__func__, __LINE__);

	//For LTP
	if (deblur_level_value <= DEBLUR_LEVLE_MAX)
		memc_rv55_info = (memc_rv55_info|DEBLUR_STATUS);
	else
		memc_rv55_info = (memc_rv55_info&(~DEBLUR_STATUS));

}

// Parsing and update real_cinema_mode
static void _mtk_video_display_parsing_and_update_p_real_cinema_mode(char *real_cinema_mode)
{
	char Mode_off[]  = "Off;";
	char Mode_on[]   = "On;";

	if (real_cinema_mode == NULL) {
		DRM_ERROR("[%s %d] real_cinema_mode = NULL\n", __func__, __LINE__);
		return;
	}

	real_cinema_mode += strlen("Real_Cinema:");
	if (strncmp(Mode_off, real_cinema_mode, strlen(Mode_off)) == 0) {
		RealCinemaMode[DATA_D0] = REAL_CINEMA_OFF;
		DRM_INFO("[%s,%d] RealCinemaMode[DATA_D0]= 0x%x\n",
			__func__, __LINE__, RealCinemaMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|REALCINEMA_OFF_STATUS);
	} else if (strncmp(Mode_on, real_cinema_mode, strlen(Mode_on)) == 0) {
		RealCinemaMode[DATA_D0] = REAL_CINEMA_ON;
		DRM_INFO("[%s,%d] RealCinemaMode[DATA_D0]= 0x%x\n",
			__func__, __LINE__, RealCinemaMode[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|REALCINEMA_ON_STATUS);
	} else {
		DRM_ERROR("[%s %d] unsupport exception real_cinema_mode\n",
			__func__, __LINE__);
		memc_rv55_info = (memc_rv55_info&(~(REALCINEMA_STATUS)));
	}

}

// Parsing and update cadence_onoff
static void _mtk_video_display_parsing_and_update_p_cadence_onoff(char *cadence_onoff)
{
	char Mode_off[]  = "Off;";
	char Mode_on[]   = "On;";

	if (cadence_onoff == NULL) {
		DRM_ERROR("[%s %d] cadence_onoff = NULL\n", __func__, __LINE__);
		return;
	}

	cadence_onoff += strlen("Cadence:");
	if (strncmp(Mode_off, cadence_onoff, strlen(Mode_off)) == 0) {
		Cadence_cmd[DATA_D0] = CADENCE_OFF;
		DRM_INFO("[%s,%d] Cadence_cmd[0]= 0x%x\n",
			__func__, __LINE__, Cadence_cmd[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|CADENCE_OFF_STATUS);
	} else if (strncmp(Mode_on, cadence_onoff, strlen(Mode_on)) == 0) {
		Cadence_cmd[DATA_D0] = CADENCE_ON;
		DRM_INFO("[%s,%d] Cadence_cmd[0]= 0x%x\n",
			__func__, __LINE__, Cadence_cmd[DATA_D0]);
		memc_rv55_info = (memc_rv55_info|CADENCE_ON_STATUS);
	} else {
		DRM_ERROR("[%s %d] unsupport exception cadence_onoff\n",
			__func__, __LINE__);
		memc_rv55_info = (memc_rv55_info&(~(CADENCE_STATUS)));
	}

}

// Parsing and update cadence_D1
static void _mtk_video_display_parsing_and_update_p_cadence_D1(char *cadence_D1)
{
	unsigned long ul_one_cadence_D1 = 0;
	uint8_t cadence_D1_value = CADENCE_DATA_DEFAULT_VALUE;
	char *p_one_level_end = NULL;

	if (cadence_D1 == NULL) {
		DRM_ERROR("[%s %d] cadence_D1 = NULL\n", __func__, __LINE__);
		return;
	}

	cadence_D1 += strlen("Cadence_Data1:");
	ul_one_cadence_D1 = strtoul(cadence_D1, &p_one_level_end, 0);

	if (ul_one_cadence_D1 <= CADENCE_DATA_MAX_VALUE) {
		cadence_D1_value = (uint8_t)(ul_one_cadence_D1);
		Cadence_cmd[DATA_D1] = cadence_D1_value;
	} else
		DRM_ERROR("[%s %d] unsupport exception ul_one_cadence_D1\n",
		__func__, __LINE__);

	//For LTP
	if (cadence_D1_value <= CADENCE_DATA_MAX_VALUE)
		memc_rv55_info = (memc_rv55_info|CADENCE_D1_STATUS);
	else
		memc_rv55_info = (memc_rv55_info&(~CADENCE_D1_STATUS));

}

// Parsing and update cadence_D2
static void _mtk_video_display_parsing_and_update_p_cadence_D2(char *cadence_D2)
{
	unsigned long ul_one_cadence_D2 = 0;
	uint8_t cadence_D2_value = CADENCE_DATA_DEFAULT_VALUE;
	char *p_one_level_end = NULL;

	if (cadence_D2 == NULL) {
		DRM_ERROR("[%s %d] cadence_D2 = NULL\n", __func__, __LINE__);
		return;
	}

	cadence_D2 += strlen("Cadence_Data2:");
	ul_one_cadence_D2 = strtoul(cadence_D2, &p_one_level_end, 0);

	if (ul_one_cadence_D2 <= CADENCE_DATA_MAX_VALUE) {
		cadence_D2_value = (uint8_t)(ul_one_cadence_D2);
		Cadence_cmd[DATA_D2] = cadence_D2_value;
	} else
		DRM_ERROR("[%s %d] unsupport exception ul_one_cadence_D2\n",
		__func__, __LINE__);

	//For LTP
	if (cadence_D2_value <= CADENCE_DATA_MAX_VALUE)
		memc_rv55_info = (memc_rv55_info|CADENCE_D2_STATUS);
	else
		memc_rv55_info = (memc_rv55_info&(~CADENCE_D2_STATUS));

}

// Parsing and update cadence_D3
static void _mtk_video_display_parsing_and_update_p_cadence_D3(char *cadence_D3)
{
	unsigned long ul_one_cadence_D3 = 0;
	uint8_t cadence_D3_value = CADENCE_DATA_DEFAULT_VALUE;
	char *p_one_level_end = NULL;

	if (cadence_D3 == NULL) {
		DRM_ERROR("[%s %d] cadence_D3 = NULL\n", __func__, __LINE__);
		return;
	}

	cadence_D3 += strlen("Cadence_Data3:");
	ul_one_cadence_D3 = strtoul(cadence_D3, &p_one_level_end, 0);

	if (ul_one_cadence_D3 <= CADENCE_DATA_MAX_VALUE) {
		cadence_D3_value = (uint8_t)(ul_one_cadence_D3);
		Cadence_cmd[DATA_D3] = cadence_D3_value;
	} else
		DRM_ERROR("[%s %d] unsupport exception ul_one_cadence_D3\n",
		__func__, __LINE__);

	cadence_D3_value = (uint8_t)(ul_one_cadence_D3);
	Cadence_cmd[DATA_D3] = cadence_D3_value;

	//For LTP
	if (cadence_D3_value <= CADENCE_DATA_MAX_VALUE)
		memc_rv55_info = (memc_rv55_info|CADENCE_D3_STATUS);
	else
		memc_rv55_info = (memc_rv55_info&(~CADENCE_D3_STATUS));

}

//Parsing json file
static void _mtk_video_display_parsing_json_file(unsigned int plane_inx,
							struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct video_plane_prop *plane_props = (pctx_video->video_plane_properties + plane_inx);
	char *pq_string = (char *)plane_props->prop_val[EXT_VPLANE_PROP_WINDOW_PQ];

	char *p_memc_onoff = NULL;
	char *p_memc_demo = NULL;
	char *p_memc_demo_Partition = NULL;
	char *p_memc_level = NULL;
	char *p_gaming_level = NULL;
	char *p_dejudder_level = NULL;
	char *p_deblur_level = NULL;
	char *p_real_cinema_mode = NULL;
	char *p_cadence_onoff = NULL;
	char *p_cadence_D1 = NULL;
	char *p_cadence_D2 = NULL;
	char *p_cadence_D3 = NULL;

	if (pq_string == NULL) {
		DRM_ERROR("[%s %d] pq_string = NULL\n", __func__, __LINE__);
	} else if (plane_props->prop_update[EXT_VPLANE_PROP_WINDOW_PQ] ||
				plane_props->prop_update[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE] ||
				plane_props->prop_update[EXT_VPLANE_PROP_FREEZE]) {
		DRM_INFO("[%s,%d]pq_string:%s\n", __func__, __LINE__, pq_string);

		// Parsing and update memc mode
		p_memc_onoff = strstr(pq_string, "MEMC_ONOFF:");
		if (p_memc_onoff == NULL) {
			DRM_ERROR("\n[%s,%d]p_memc_onoff is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_memc_mode(p_memc_onoff);

		// Parsing and update memc demo mode
		p_memc_demo = strstr(pq_string, "MJC_Demo:");
		if (p_memc_demo == NULL) {
			DRM_ERROR("\n[%s,%d]p_memc_demo is NULL", __func__, __LINE__);
			return;
		}

		p_memc_demo_Partition = strstr(pq_string, "MJC_Demo_Partition:");
		if (p_memc_demo_Partition == NULL) {
			DRM_ERROR("\n[%s,%d]p_memc_demo_Partition is NULL", __func__, __LINE__);
			return;
		}
		_mtk_video_display_parsing_and_update_demo_mode(p_memc_demo, p_memc_demo_Partition);

		// Parsing and update memc level
		p_memc_level = strstr(pq_string, "MEMC_Level:");
		if (p_memc_level == NULL) {
			DRM_ERROR("\n[%s,%d]p_memc_level is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_memc_level(p_memc_level);

		// Parsing and update gaming_level
		p_gaming_level = strstr(pq_string, "Gaming_MJC_Lvl:");
		if (p_gaming_level == NULL) {
			DRM_ERROR("\n[%s,%d]p_gaming_level is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_gaming_level(p_gaming_level);

		// Parsing and update dejudder_level
		p_dejudder_level = strstr(pq_string, "MJC_Dejudder:");
		if (p_dejudder_level == NULL) {
			DRM_ERROR("\n[%s,%d]p_dejudder_level is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_p_dejudder_level(p_dejudder_level);

		// Parsing and update deblur_level
		p_deblur_level = strstr(pq_string, "MJC_Deblur:");
		if (p_deblur_level == NULL) {
			DRM_ERROR("\n[%s,%d]p_deblur_level is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_p_deblur_level(p_deblur_level);

		// Parsing and update real_cinema_mode
		p_real_cinema_mode = strstr(pq_string, "Real_Cinema:");
		if (p_real_cinema_mode == NULL) {
			DRM_ERROR("\n[%s,%d]p_real_cinema_mode is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_p_real_cinema_mode(p_real_cinema_mode);

		// Parsing and update Cadence
		p_cadence_onoff = strstr(pq_string, "Cadence:");
		if (p_cadence_onoff == NULL) {
			DRM_ERROR("\n[%s,%d]p_cadence_onoff is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_p_cadence_onoff(p_cadence_onoff);

		// Parsing and update Cadence_Data1
		p_cadence_D1 = strstr(pq_string, "Cadence_Data1:");
		if (p_cadence_D1 == NULL) {
			DRM_ERROR("\n[%s,%d]p_cadence_D1 is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_p_cadence_D1(p_cadence_D1);

		// Parsing and update Cadence_Data2
		p_cadence_D2 = strstr(pq_string, "Cadence_Data2:");
		if (p_cadence_D2 == NULL) {
			DRM_ERROR("\n[%s,%d]p_cadence_D2 is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_p_cadence_D2(p_cadence_D2);

		// Parsing and update Cadence_Data3
		p_cadence_D3 = strstr(pq_string, "Cadence_Data3:");
		if (p_cadence_D3 == NULL) {
			DRM_ERROR("\n[%s,%d]p_cadence_D3 is NULL", __func__, __LINE__);
			return;
		}

		_mtk_video_display_parsing_and_update_p_cadence_D3(p_cadence_D3);
	}
}

#define _MFC_LBW_MODE 0x0F
static void _mtk_video_display_set_frc_mode(unsigned int plane_inx, struct mtk_tv_kms_context *pctx,
				struct mtk_plane_state *state)
{
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};

	static uint8_t pre_MFCMode       = DEFAULT_PRE_VALUE;
	static uint8_t pre_FRC_Mode      = DEFAULT_PRE_VALUE;
	static uint32_t cmdstatus        = DEFAULT_CMD_STATUS;
	uint32_t output_freq     = DEFAULT_VALUE;
	uint8_t mfc_mode = E_FRC_LBW_NORMAL;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
		pre_MFCMode = DEFAULT_PRE_VALUE;
		pre_FRC_Mode = DEFAULT_PRE_VALUE;
	}

	if (pctx->stub)
		force = TRUE;

	output_freq = (pctx_pnl->outputTiming_info.u32OutputVfreq/VFRQRATIO_1000);

	if (IS_OUTPUT_120HZ(output_freq)) {
		switch (pctx_frc->video_frc_mode) {
		case E_MEMC_MFC_MODE_1X:
			mfc_mode = E_FRC_LBW_LOWBANDWIDTH;
			break;
		case E_MEMC_MFC_MODE_REPEAT:
			mfc_mode = E_FRC_LBW_REPEAT;
			break;
		case E_MEMC_MFC_MODE_2X:
		case E_MEMC_MFC_MODE_4X:
		case E_MEMC_MFC_MODE_SINGLE_NORMAL_8K:
		case E_MEMC_MFC_MODE_SINGLE_LOW_8K:
		case E_MEMC_MFC_MODE_MULTI_8K:
		default:
			mfc_mode = E_FRC_LBW_NORMAL;
			break;
		}
	} else {
		switch (pctx_frc->video_frc_mode) {
		case E_MEMC_MFC_MODE_1X:
		case E_MEMC_MFC_MODE_2X:
		case E_MEMC_MFC_MODE_REPEAT_AUTO:
			mfc_mode = E_FRC_LBW_LOWBANDWIDTH;
			break;
		case E_MEMC_MFC_MODE_REPEAT:
			mfc_mode = E_FRC_LBW_REPEAT;
			break;
		case E_MEMC_MFC_MODE_4X:
		case E_MEMC_MFC_MODE_SINGLE_NORMAL_8K:
		case E_MEMC_MFC_MODE_SINGLE_LOW_8K:
		case E_MEMC_MFC_MODE_MULTI_8K:
		default:
			mfc_mode = E_FRC_LBW_NORMAL;
			break;
		}
	}

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_MFCMode, pctx_frc->video_memc_mode)) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_FRC_Mode, pctx_frc->video_frc_mode))
	){
		DRM_INFO("[%s,%d] Host send CPUIF to FRC RV55\n", __func__, __LINE__);
		send.CmdID = FRC_MB_CMD_SET_FRC_MODE;//FRC_MB_CMD_SET_MFC_MODE;
		send.D1    = (uint32_t)pctx_frc->video_memc_mode;
		send.D2    = (uint32_t)mfc_mode;
		send.D3    = (uint32_t)DEFAULT_VALUE;
		send.D4    = (uint32_t)DEFAULT_VALUE;
		cmdstatus = pqu_send_cmd(&send, &callback_info);
		pre_MFCMode       = pctx_frc->video_memc_mode;
		pre_FRC_Mode      = pctx_frc->video_frc_mode;

		DRM_INFO("[%s,%d][0x%x] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.CmdID, send.D1, send.D2, send.D3, send.D4);

		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|FRC_MODE_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~FRC_MODE_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_MFC_MODE Fail\n",
				__func__, __LINE__);
		}
	}
}

static void _mtk_video_display_set_mfc_mode_level(unsigned int plane_inx,
				struct mtk_tv_kms_context *pctx, uint8_t *u8Data)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};

	static uint8_t pre_MFCMode       = DEFAULT_PRE_VALUE;
	static uint8_t pre_DeJudderLevel = DEFAULT_PRE_VALUE;
	static uint8_t pre_DeBlurLevel   = DEFAULT_PRE_VALUE;
	static uint32_t cmdstatus        = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
		pre_MFCMode = DEFAULT_PRE_VALUE;
		pre_DeJudderLevel = DEFAULT_PRE_VALUE;
		pre_DeBlurLevel = DEFAULT_PRE_VALUE;
	}

	if (pctx->stub)
		force = TRUE;


	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_MFCMode, u8Data[DATA_D0])) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_DeJudderLevel, u8Data[DATA_D1])) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_DeBlurLevel, u8Data[DATA_D2]))
	){
		DRM_INFO("[%s,%d] Host send CPUIF to FRC RV55\n", __func__, __LINE__);
		send.CmdID = FRC_MB_CMD_SET_MFC_MODE;
		send.D1    = (uint32_t)u8Data[DATA_D0];
		send.D2    = (uint32_t)u8Data[DATA_D1];
		send.D3    = (uint32_t)u8Data[DATA_D2];
		send.D4    = (uint32_t)DEFAULT_VALUE;
		cmdstatus = pqu_send_cmd(&send, &callback_info);
		pre_MFCMode       = u8Data[DATA_D0];
		pre_DeJudderLevel = u8Data[DATA_D1];
		pre_DeBlurLevel   = u8Data[DATA_D2];

		DRM_INFO("[%s,%d][0x%x] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.CmdID, send.D1, send.D2, send.D3, send.D4);

		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|MFC_MODE_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~MFC_MODE_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_MFC_MODE(Level) Fail\n",
				__func__, __LINE__);
		}
	}
}

#define _MFC_BYPASS_CMD_INDEX (0x04)
static void _mtk_video_display_set_bypass_mfc_mode(struct mtk_tv_kms_context *pctx)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};

	static uint8_t pre_BypassEn   = DEFAULT_PRE_VALUE;
	static uint32_t cmdstatus     = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE)
		pre_BypassEn = DEFAULT_PRE_VALUE;


	if (pctx->stub)
		force = TRUE;

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_BypassEn, bBypassMEMC))
	){
		DRM_INFO("[%s,%d] Host send CPUIF to FRC RV55\n", __func__, __LINE__);
		send.CmdID = FRC_MB_CMD_SET_MFC_MODE;
		send.D1    = (uint32_t)_MFC_BYPASS_CMD_INDEX;
		send.D2    = (uint32_t)DEFAULT_VALUE;
		send.D3    = (uint32_t)DEFAULT_VALUE;
		send.D4    = (uint32_t)bBypassMEMC;
		cmdstatus = pqu_send_cmd(&send, &callback_info);
		pre_BypassEn   = bBypassMEMC;

		DRM_INFO("[%s,%d][0x%x] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.CmdID, send.D1, send.D2, send.D3, send.D4);

		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|MFC_BYPASS_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~MFC_BYPASS_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_MFC_MODE(BYPASS) Fail\n",
				__func__, __LINE__);
		}
	}
}

//set mfc level
static void _mtk_video_display_set_memc_level_update(struct mtk_tv_kms_context *pctx,
							uint8_t *u8Data)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct pqu_frc_send_burstcmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info      = {DEFAULT_VALUE};
	uint8_t data_size = 0;
	static uint8_t pre_MFCLeve[MEMC_LEVLE_MAX_CNT] = {DEFAULT_PRE_VALUE};
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx->stub)
		force = TRUE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE)
		memset(pre_MFCLeve, DEFAULT_PRE_VALUE, (sizeof(uint8_t)*MEMC_LEVLE_MAX_CNT));

	if ((force == TRUE) ||
		(memcmp(pre_MFCLeve, u8Data, sizeof(pre_MFCLeve)) != 0)) {
		send.CmdID = FRC_MB_CMD_SET_MECM_LEVEL;
		for (data_size = 0; data_size < MEMC_LEVLE_MAX_CNT; data_size++) {
			send.Data[data_size] = u8Data[data_size];
			pre_MFCLeve[data_size] = u8Data[data_size];
			DRM_INFO("[%s,%d] send.Data[%d] = %d\n",
				__func__, __LINE__, data_size, send.Data[data_size]);
		}

		cmdstatus = pqu_send_burst_cmd(&send, &callback_info);
		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|MFC_LEVEL_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~MFC_LEVEL_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_MECM_LEVEL Fail\n",
				__func__, __LINE__);
		}
	}
}

uint32_t mtk_video_display_frc_set_rv55_burst_cmd(uint16_t *u16Data, uint16_t u16Length)
{
	struct pqu_frc_send_burstcmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info      = {DEFAULT_VALUE};
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	uint8_t data_size = 0;

	if ((u16Data == NULL) || (u16Length == 0) || (u16Length > RV55_CMD_LENGTH)) {
		DRM_ERROR("[%s, %d]Pointer or Length is Error!\n", __func__, __LINE__);
		return -1;
	}

	send.CmdID = *u16Data;
	u16Data++;
	for (data_size = 0; data_size < (u16Length-1); data_size++) {
		send.Data[data_size] = *u16Data;
		u16Data++;
	}
	cmdstatus = pqu_send_burst_cmd(&send, &callback_info);
	return cmdstatus;
}

const char *SenceToString(enum mtk_video_frc_pq_scene scene)
{
	switch (scene) {
	case MEMC_PQ_SCENE_NORMAL:
		return "Normal";
	case MEMC_PQ_SCENE_GAME:
		return "Game";
	case MEMC_PQ_SCENE_GAME_CROP:
		return "GameCrop";
	case MEMC_PQ_SCENE_PC:
		return "PC";
	case MEMC_PQ_SCENE_PHOTO:
		return "Photo";
	case MEMC_PQ_SCENE_FORCE_P:
		return "ForceP";
	case MEMC_PQ_SCENE_HFR:
		return "HFR";
	case MEMC_PQ_SCENE_GAME_WITH_MEMC:
		return "GameMEMC";
	case MEMC_PQ_SCENE_3D:
		return "3D";
	case MEMC_PQ_SCENE_SLICED_GAME:
		return "SlicedGame";
	case MEMC_PQ_SCENE_GAME_WITH_PC:
		return "GamePC";
	case MEMC_PQ_SCENE_MAX:
		return "MAX";
	default:
		return "Not Defined";
	}
}

static void _mtk_video_display_set_mfc_mode_cmd(unsigned int plane_inx,
				struct mtk_tv_kms_context *pctx)
{
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_inx);

	bool bLowLatencyEn = false;
	uint32_t inputfreq = 0;
	uint8_t framesync_mode = 0;
	uint8_t _MFCMode[MEMC_MODE_CNT] = {0};
	uint8_t _MFCLevel[MEMC_LEVLE_MAX_CNT] = {0};

	framesync_mode = (uint8_t)crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE];
	bLowLatencyEn = (bool)crtc_props->prop_val[E_EXT_CRTC_PROP_LOW_LATENCY_MODE];
	inputfreq = div64_u64((plane_props->prop_val[EXT_VPLANE_PROP_INPUT_VFREQ] +
		_tHZ), VFRQRATIO_1000) * VFRQRATIO_1000; // +500 for round

	if ((framesync_mode == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK) ||
		(inputfreq == _144HZ) || (inputfreq == _288HZ)) {
		//VRR ON && 144Hz && 288Hz
		memcpy(_MFCMode, _VRRorGame_MJC_lv_Off, sizeof(_VRRorGame_MJC_lv_Off));
		memcpy(_MFCLevel, _VRRorGame_MFCLevel_OFF, sizeof(_VRRorGame_MFCLevel_OFF));
	} else {
		if ((bLowLatencyEn == TRUE) && (Gaming_MJC_Lvl >= 1)) {
			//VRR OFF, Game ON, Gaming level>=1
			memcpy(_MFCMode, Gaming_level, sizeof(Gaming_level));
			memcpy(_MFCLevel, MFCLevel, sizeof(MFCLevel));
		} else if (bLowLatencyEn == TRUE) {
			//VRR OFF, Game ON
			memcpy(_MFCMode, _VRRorGame_MJC_lv_Off, sizeof(_VRRorGame_MJC_lv_Off));
			memcpy(_MFCLevel, _VRRorGame_MFCLevel_OFF, sizeof(_VRRorGame_MFCLevel_OFF));
		} else {
			//VRR OFF, Game OFF, Gaming level==X
			memcpy(_MFCMode, MFCMode, sizeof(MFCMode));
			memcpy(_MFCLevel, MFCLevel, sizeof(MFCLevel));
		}
	}

	_mtk_video_display_set_mfc_mode_level(plane_inx, pctx, _MFCMode);
	if (_MFCMode[DATA_D0] != MEMC_MODE_USER)
		_mtk_video_display_set_memc_level_update(pctx, _MFCLevel);

}

#define _CMD_LEN 5
uint32_t mtk_video_display_frc_set_rv55_cmd(uint16_t *u16Data,
	uint16_t u16Length, struct mtk_frc_context *pctx_frc)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;

	if ((u16Data == NULL) || (u16Length == 0) || (u16Length > _CMD_LEN)
		|| (pctx_frc == NULL)) {
		DRM_ERROR("[%s, %d]Pointer or Length is Error!\n", __func__, __LINE__);
		return -1;
	}

	//command ID
	send.CmdID = *u16Data;
	u16Data++;
	send.D1 = *u16Data;
	u16Data++;
	send.D2 = *u16Data;
	u16Data++;
	send.D3 = *u16Data;
	u16Data++;
	send.D4 = *u16Data;

	if ((pctx_frc->frc_ip_version == FRC_IP_VERSION_1_E3)
		&& (pctx_frc->video_frc_mode == E_MEMC_MFC_MODE_MULTI_8K)) {
		DRM_INFO("[%s,%d][Patch] can't use ML mode, UseMEMC off.\n",
			__func__, __LINE__);
		if ((send.CmdID == FRC_MB_CMD_SET_FRC_MODE)
			&& (send.D1 == _MFC_LBW_MODE)
			&& (send.D2 == E_FRC_LBW_REPEAT)
		) {
			send.CmdID = FRC_MB_CMD_SET_MFC_MODE;
			send.D1 = DEFAULT_VALUE;
			send.D2 = DEFAULT_VALUE;
			send.D3 = DEFAULT_VALUE;
			send.D4 = DEFAULT_VALUE;
		}

		if ((send.CmdID == FRC_MB_CMD_SET_FRC_MODE)
			&& (send.D1 == _MFC_LBW_MODE)
			&& (send.D2 == E_FRC_LBW_NORMAL)
		) {
			send.CmdID = FRC_MB_CMD_SET_MFC_MODE;
			send.D1 = MEMC_MODE_ON;
			send.D2 = DEFAULT_VALUE;
			send.D3 = DEFAULT_VALUE;
			send.D4 = DEFAULT_VALUE;
		}
	}

	if ((send.CmdID == FRC_MB_CMD_SET_MFC_MODE)
		&& (send.D1 == MEMC_MODE_ON)
	) {
		send.CmdID = FRC_MB_CMD_SET_MFC_MODE;
		send.D1 = MFCMode[DATA_D0];
		send.D2 = MFCMode[DATA_D1];
		send.D3 = MFCMode[DATA_D2];
		send.D4 = MFCMode[DATA_D3];
	}

	DRM_INFO("[%s,%d] [%x]D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
		__func__, __LINE__, send.CmdID, send.D1, send.D2, send.D3, send.D4);

	cmdstatus = pqu_send_cmd(&send, &callback_info);
	if (cmdstatus != 0)
		DRM_ERROR("[%s %d]CMD frc_set_rv55_cmd Fail\n",
			__func__, __LINE__);


	return cmdstatus;
}

int mtk_video_display_frc_get_memc_level(uint8_t *memc_level, uint16_t u16Length)
{
	int ret = 0;
	int i;

	if (memc_level == NULL) {
		DRM_ERROR("[%s, %d]Pointer is NULL!\n", __func__, __LINE__);
		return -1;
	}

	for (i = 0; i < u16Length; i++)
		memc_level[i] = MFCLevel[i];


	return ret;
}

#define RV55_magic_status 0x55667788

static void _mtk_video_display_get_mfc_alg_status(struct mtk_tv_kms_context *pctx)
{
	struct pqu_frc_get_cmd_param send = {DEFAULT_VALUE};
	struct pqu_frc_get_cmd_reply_param reply = {DEFAULT_VALUE};
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;


	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->frc_alg_run_on == FRC_ALG_PROCESSOR_RV55) {

		if (pctx->stub)
			force = TRUE;
		if (force == TRUE) {
			send.CmdID = FRC_MB_CMD_GET_MECM_ALG_STATUS;
			send.D1    = DEFAULT_VALUE;
			send.D2    = DEFAULT_VALUE;
			send.D3    = DEFAULT_VALUE;
			send.D4    = DEFAULT_VALUE;
			cmdstatus = pqu_get_cmd(&send, &reply);

			DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
				__func__, __LINE__, reply.D1, reply.D2, reply.D3, reply.D4);

			if (cmdstatus != 0)
				DRM_ERROR("[%s %d]CMD FRC_MB_CMD_GET_MECM_ALG_STATUS Fail\n",
				 __func__, __LINE__);

			if (reply.D1 == RV55_magic_status)
				memc_rv55_info = (memc_rv55_info|MFC_ALG_STATUS);
			else
				memc_rv55_info = (memc_rv55_info&(~MFC_ALG_STATUS));
		}
	} else {
		memc_rv55_info = (memc_rv55_info|MFC_ALG_STATUS);
	}


}

//set demo mode
static void _mtk_video_display_set_demo_mode(struct mtk_tv_kms_context *pctx,
							uint8_t *u8Data)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	static uint8_t pre_DemoMode  = DEFAULT_PRE_VALUE;
	static uint8_t pre_Partition = DEFAULT_PRE_VALUE;
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
		pre_DemoMode = DEFAULT_PRE_VALUE;
		pre_Partition = DEFAULT_PRE_VALUE;
	}


	if (pctx->stub)
		force = TRUE;

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_DemoMode, u8Data[DATA_D0])) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_Partition, u8Data[DATA_D1]))
	){
		DRM_INFO("[%s,%d] pre_DemoMode = 0x%x, pre_Partition = 0x%x\n",
			__func__, __LINE__, pre_DemoMode, pre_Partition);
		send.CmdID = FRC_MB_CMD_SET_MFC_DEMO_MODE;
		send.D1    = (uint32_t)u8Data[DATA_D0];
		send.D2    = (uint32_t)u8Data[DATA_D1];
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus = pqu_send_cmd(&send, &callback_info);
		pre_DemoMode  = u8Data[DATA_D0];
		pre_Partition = u8Data[DATA_D1];
		DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);
		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|MFC_DEMO_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~MFC_DEMO_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_MFC_DEMO_MODE Fail\n",
				__func__, __LINE__);
		}
	}
}

static void _mtk_video_display_set_game_mode(unsigned int plane_inx,
				struct mtk_tv_kms_context *pctx)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	bool bLowLatencyEn = false;
	bool bSkipGameMemc = false;
	static uint8_t pre_GameMode = DEFAULT_PRE_VALUE;
	static uint8_t GameMode = DEFAULT_PRE_VALUE;
	bool force = FALSE;

	if ((crtc_props == NULL) || (pctx_video == NULL)) {
		DRM_ERROR("[%s, %d] pointer is null!\n", __func__, __LINE__);
		return;
	}

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE)
		pre_GameMode = DEFAULT_PRE_VALUE;

	if (pctx->stub)
		force = TRUE;

	bLowLatencyEn = (bool) crtc_props->prop_val[E_EXT_CRTC_PROP_LOW_LATENCY_MODE];
	bSkipGameMemc = _mtk_video_display_frc_is_skip_game_memc(plane_inx, pctx);

	if ((bLowLatencyEn == TRUE) && (Gaming_MJC_Lvl >= 1) && (bSkipGameMemc == false))
		GameMode = GAME_ON_GAMING_LV_ON;
	else if (bLowLatencyEn == TRUE)
		GameMode = GAME_ON_GAMING_LV_OFF;
	else
		GameMode = GAME_OFF;

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(pre_GameMode, GameMode))
		) {
		DRM_INFO("[%s,%d] pre_GameMode = 0x%x, GameMode = 0x%x\n",
			__func__, __LINE__, pre_GameMode, GameMode);
		send.CmdID = FRC_MB_CMD_SET_FRC_GAME_MODE;
		send.D1    = (uint32_t)GameMode;
		send.D2    = DEFAULT_VALUE;
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus  = pqu_send_cmd(&send, &callback_info);
		pre_GameMode  = GameMode;
		DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);

		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|GAME_MODE_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~GAME_MODE_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_SET_FRC_GAME_MODE Fail\n",
				__func__, __LINE__);
		}
	}
}

//set real_cinema_mode
static void _mtk_video_display_set_real_cinema_mode(struct mtk_tv_kms_context *pctx,
							uint8_t *u8Data)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	static uint8_t pre_RealCinemaMode  = DEFAULT_PRE_VALUE;
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE)
		pre_RealCinemaMode = DEFAULT_PRE_VALUE;

	if (pctx->stub)
		force = TRUE;

	if ((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_RealCinemaMode, u8Data[DATA_D0]))
	){
		DRM_INFO("[%s,%d] pre_RealCinemaMode = 0x%x\n",
			__func__, __LINE__, pre_RealCinemaMode);
		send.CmdID = FRC_MB_CMD_60_REALCINEMA_MODE;
		send.D1    = (uint32_t)u8Data[DATA_D0];
		send.D2    = DEFAULT_VALUE;
		send.D3    = DEFAULT_VALUE;
		send.D4    = DEFAULT_VALUE;
		cmdstatus = pqu_send_cmd(&send, &callback_info);
		pre_RealCinemaMode  = u8Data[DATA_D0];

		DRM_INFO("[%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);

		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|REALCINEMA_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~REALCINEMA_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_60_REALCINEMA_MODE Fail\n",
				__func__, __LINE__);
		}
	}
}

static void _mtk_video_display_set_photo_mode(struct mtk_tv_kms_context *pctx)
{
	bool bIsPhotoMode = false;
	int ret = 0;

	if (pctx == NULL) {
		DRM_ERROR("[%s, %d] pctx is null\n",
			__func__, __LINE__);
		return;
	}

	ret = _mtk_video_display_check_photo_mode(pctx, &bIsPhotoMode);
	if (ret < 0) {
		DRM_ERROR("[%s, %d] check_photo_mode failed\n",
			__func__, __LINE__);
		return;
	}

	// photo cmd: memc off and low latency
	if (bIsPhotoMode == true)
		bBypassMEMC = true;
}

//set cadence
static void _mtk_video_display_set_cadence(struct mtk_tv_kms_context *pctx,
							uint8_t *u8Data)
{
	struct pqu_frc_send_cmd_param send = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	static uint8_t pre_cadence_D1  = DEFAULT_VALUE;
	static uint8_t pre_cadence_D2  = DEFAULT_VALUE;
	static uint8_t pre_cadence_D3  = DEFAULT_VALUE;
	static uint32_t cmdstatus = DEFAULT_CMD_STATUS;
	bool force = FALSE;

	if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
		pre_cadence_D1 = DEFAULT_VALUE;
		pre_cadence_D2 = DEFAULT_VALUE;
		pre_cadence_D3 = DEFAULT_VALUE;
	}

	if (pctx->stub)
		force = TRUE;

	if ((u8Data[DATA_D0] == CADENCE_ON) &&
		((force == TRUE) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_cadence_D1, u8Data[DATA_D1])) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_cadence_D2, u8Data[DATA_D2])) ||
		(_mtk_video_display_frc_is_variable_updated(
					pre_cadence_D3, u8Data[DATA_D3])))
	){
		DRM_INFO("[%s,%d] u8Data[DATA_D0] = 0x%x\n",
			__func__, __LINE__, u8Data[DATA_D0]);
		DRM_INFO("[%s,%d] pre_cadence_D1 = 0x%x\n",
			__func__, __LINE__, pre_cadence_D1);
		DRM_INFO("[%s,%d] pre_cadence_D2 = 0x%x\n",
			__func__, __LINE__, pre_cadence_D2);
		DRM_INFO("[%s,%d] pre_cadence_D3 = 0x%x\n",
			__func__, __LINE__, pre_cadence_D3);

		send.CmdID = FRC_MB_CMD_FILM_CADENCE_CTRL;
		send.D1    = (uint32_t)u8Data[DATA_D1];
		send.D2    = (uint32_t)u8Data[DATA_D2];
		send.D3    = (uint32_t)u8Data[DATA_D3];
		send.D4    = DEFAULT_VALUE;
		cmdstatus = pqu_send_cmd(&send, &callback_info);
		pre_cadence_D1  = u8Data[DATA_D1];
		pre_cadence_D2  = u8Data[DATA_D2];
		pre_cadence_D3  = u8Data[DATA_D3];

		DRM_INFO("[CAD][%s,%d] D1 = 0x%x, D2 = 0x%x, D3 = 0x%x, D4 = 0x%x\n",
			__func__, __LINE__, send.D1, send.D2, send.D3, send.D4);

		if (cmdstatus == 0)
			memc_rv55_info = (memc_rv55_info|CADENCE_CMD_STATUS);
		else {
			memc_rv55_info = (memc_rv55_info&(~CADENCE_CMD_STATUS));
			DRM_ERROR("[%s %d]CMD FRC_MB_CMD_FILM_CADENCE_CTRL Fail\n",
				__func__, __LINE__);
		}
	}
}


//init clock
static void _mtk_video_display_set_frc_clock(struct mtk_video_context *pctx_video,
							bool enable)
{
	struct hwregFrcClockIn paramFrcClockIn;
	struct hwregOut paramOut;
	uint16_t reg_num = pctx_video->reg_num;

	memset(&paramFrcClockIn, 0, sizeof(struct hwregFrcClockIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = malloc(sizeof(struct reg_info) * reg_num);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*reg_num);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return;
	}

	paramFrcClockIn.RIU = 1;
	paramFrcClockIn.bEnable = enable;

	drv_hwreg_render_frc_set_clock(
		paramFrcClockIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
}

#define _8K_HSIZE 7680
#define _8K_VSIZE 4320
#define _4K_HSIZE 3840
#define _4K_VSIZE 2160

int _mtk_video_display_frc_getSize(uint8_t frc_ip_version,
	uint16_t *Hsize, uint16_t *Vsize)
{
	if ((Hsize == NULL) || (Vsize == NULL)) {
		DRM_ERROR("[%s, %d]frc getSize failed!\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	switch (frc_ip_version) {
	case FRC_IP_VERSION_2:
	case FRC_IP_VERSION_4:
	case FRC_IP_VERSION_5:
	case FRC_IP_VERSION_6:
		*Hsize = _4K_HSIZE;
		*Vsize = _4K_VSIZE;
		break;
	default:
		*Hsize = _8K_HSIZE;
		*Vsize = _8K_VSIZE;
		break;
	}

	return 0;
}

void _mtk_video_display_frc_set_vs_trig(
		struct mtk_tv_kms_context *pctx,
		bool bEn)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	uint16_t reg_num = pctx_video->reg_num;
	struct hwregFrcPatIn paramFrcPatIn;
	struct hwregOut paramOut;
	int ret = 0;

	memset(&paramFrcPatIn, 0, sizeof(struct hwregFrcPatIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = malloc(sizeof(struct reg_info) * reg_num);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*reg_num);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return;
	}

	paramFrcPatIn.RIU = 1;
	paramFrcPatIn.bEn = bEn;

	ret = _mtk_video_display_frc_getSize(pctx_frc->frc_ip_version,
		&paramFrcPatIn.Hsize, &paramFrcPatIn.Vsize);
	if (ret < 0)
		DRM_ERROR("[%s, %d]frc getSize failed!\n",
			__func__, __LINE__);

	drv_hwreg_render_frc_set_vs_sw_trig(
		paramFrcPatIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
}

void _mtk_video_display_frc_vs_trig_ctl(struct mtk_tv_kms_context *pctx)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

		if (((pctx_frc->frc_pattern_sel == MEMC_PAT_IPM_DYNAMIC) &&
			(pctx_frc->frc_pattern_enable)) ||
		((pctx_frc->frc_pattern_sel == MEMC_PAT_IPM_STATIC) &&
		(pctx_frc->frc_pattern_trig_cnt))
		) {
			_mtk_video_display_frc_set_vs_trig(pctx, true);
			if (pctx_frc->frc_pattern_trig_cnt)
				pctx_frc->frc_pattern_trig_cnt--;
		}
}

static int _mtk_video_display_frc_get_ipm_hv_size(uint16_t *HSize, uint16_t *VSize)
{
	struct hwregFrcDebugInfo stFrcDebugInfo;

	if (HSize == NULL) {
		DRM_ERROR("%s[%d]HSize is nullptr!\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (VSize == NULL) {
		DRM_ERROR("%s[%d]VSize is nullptr!\n", __func__, __LINE__);
		return -EINVAL;
	}

	memset(&stFrcDebugInfo, 0x0, sizeof(struct hwregFrcDebugInfo));
	drv_hwreg_render_frc_get_debug_info(&stFrcDebugInfo);

	*HSize = stFrcDebugInfo.IpmSrcHSize;
	*VSize = stFrcDebugInfo.IpmSrcVSize;

	return 0;
}

static int _mtk_video_display_frc_get_wr_idx(uint8_t *WriteIdx, uint8_t *ReadIdx)
{
	struct hwregFrcDebugInfo stFrcDebugInfo;

	if (!gMtkDrmDbg.atrace_enable) {
		return 0;
	}

	if (WriteIdx == NULL) {
		DRM_ERROR("%s[%d]IpmWriteIdx is nullptr!\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (ReadIdx == NULL) {
		DRM_ERROR("%s[%d]OpmF2ReadIdx is nullptr!\n", __func__, __LINE__);
		return -EINVAL;
	}

	memset(&stFrcDebugInfo, 0x0, sizeof(struct hwregFrcDebugInfo));
	drv_hwreg_render_frc_get_debug_info(&stFrcDebugInfo);

	*WriteIdx = stFrcDebugInfo.IpmWriteIdx;
	*ReadIdx = stFrcDebugInfo.OpmF2ReadIdx;

	return 0;
}

void _mtk_video_display_frc_set_ipm_pattern(
		struct mtk_tv_kms_context *pctx,
		bool bEn, bool bForce)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;

	uint16_t reg_num = pctx_video->reg_num;

	struct hwregFrcPatIn paramFrcPatIn;
	struct hwregOut paramOut;
	int ret = 0;

	memset(&paramFrcPatIn, 0, sizeof(struct hwregFrcPatIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = malloc(sizeof(struct reg_info) * reg_num);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*reg_num);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return;
	}

	paramFrcPatIn.RIU = 1;
	paramFrcPatIn.bEn = bEn;
	paramFrcPatIn.bForce = bForce;

	ret = _mtk_video_display_frc_get_ipm_hv_size(
		&paramFrcPatIn.Hsize, &paramFrcPatIn.Vsize);
	if (ret != 0)
		DRM_ERROR("[%s, %d]frc get IPM H/V size failed!\n", __func__, __LINE__);

	drv_hwreg_render_frc_set_ipmpattern(
	paramFrcPatIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
}

void _mtk_video_display_frc_set_opm_pattern(
		struct mtk_tv_kms_context *pctx,
		bool bEn, bool bForce)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	struct hwregFrcPatIn paramFrcPatIn;
	struct hwregOut paramOut;
	int ret = 0;

	memset(&paramFrcPatIn, 0, sizeof(struct hwregFrcPatIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = malloc(sizeof(struct reg_info)*REG_NUM_MAX);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*REG_NUM_MAX);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return;
	}

	paramFrcPatIn.RIU = 1;
	paramFrcPatIn.bEn = bEn;
	paramFrcPatIn.bForce = bForce;

	ret = _mtk_video_display_frc_getSize(pctx_frc->frc_ip_version,
		&paramFrcPatIn.Hsize, &paramFrcPatIn.Vsize);
	if (ret < 0)
		DRM_ERROR("[%s, %d]frc getSize failed!\n",
			__func__, __LINE__);

	drv_hwreg_render_frc_set_opmpattern(
	paramFrcPatIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
}

void _mtk_video_frc_sw_reset(struct mtk_tv_kms_context *pctx, bool bEnable)
{
	struct hwregFrcSwReset paramInitIn;
	struct hwregOut paramOut;
	struct reg_info *regTbl;

	regTbl = malloc(sizeof(struct reg_info)*REG_NUM_MAX);
	if (regTbl == NULL) {
		DRM_ERROR("[%s,%5d] NULL POINTER\n", __func__, __LINE__);
		return;
	}

	memset(&paramInitIn, 0, sizeof(struct hwregFrcSwReset));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	memset(regTbl, 0, sizeof(struct reg_info)*REG_NUM_MAX);

	paramOut.reg = regTbl;
	paramInitIn.RIU = 1;
	paramInitIn.bEnable = bEnable;

	//frc sw reset
	drv_hwreg_render_frc_sw_reset(
		paramInitIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
}

void _mtk_video_frc_preultra_ultra(struct mtk_tv_kms_context *pctx, bool bEnable)
{
	struct hwregFrcSetMFCPreUltraUltra paramInitIn;
	struct hwregOut paramOut;
	struct reg_info *regTbl;

	regTbl = malloc(sizeof(struct reg_info)*REG_NUM_MAX);
	if (regTbl == NULL) {
		DRM_ERROR("[%s,%5d] NULL POINTER\n", __func__, __LINE__);
		return;
	}

	memset(&paramInitIn, 0, sizeof(struct hwregFrcSetMFCPreUltraUltra));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	memset(regTbl, 0, sizeof(struct reg_info)*REG_NUM_MAX);

	paramOut.reg = regTbl;
	paramInitIn.RIU = 1;
	paramInitIn.bEnable = bEnable;

	//frc sw reset
	drv_hwreg_render_frc_set_preultra_ultra(
		paramInitIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);

}
void _mtk_video_frc_init_reg_1(struct mtk_tv_kms_context *pctx)
{

	struct hwregFrcInitIn paramInitIn;
	struct hwregFrctriggenIn paramTriggergenIn;
	struct hwregOut paramOut;

	memset(&paramInitIn, 0, sizeof(struct hwregFrcInitIn));
	memset(&paramTriggergenIn, 0, sizeof(struct hwregFrctriggenIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = malloc(sizeof(struct reg_info)*REG_NUM_MAX);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*REG_NUM_MAX);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return;
	}

	paramInitIn.RIU = 1;

	//init frc
	drv_hwreg_render_frc_init_1(
		paramInitIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
}

void _mtk_video_frc_init_reg_2(struct mtk_tv_kms_context *pctx)
{

	struct hwregFrcInitIn paramInitIn;
	struct hwregFrctriggenIn paramTriggergenIn;
	struct hwregOut paramOut;

	memset(&paramInitIn, 0, sizeof(struct hwregFrcInitIn));
	memset(&paramTriggergenIn, 0, sizeof(struct hwregFrctriggenIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = malloc(sizeof(struct reg_info)*REG_NUM_MAX);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*REG_NUM_MAX);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return;
	}

	paramInitIn.RIU = 1;

	//init frc
	drv_hwreg_render_frc_init_2(
		paramInitIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
}

void _mtk_video_frc_init_reg(struct mtk_tv_kms_context *pctx)
{
#if (1) //(MEMC_CONFIG == 1)
	struct hwregFrcInitIn paramInitIn;
	struct hwregFrctriggenIn paramTriggergenIn;
	struct hwregOut paramOut;
	uint16_t reg_size;

	memset(&paramInitIn, 0, sizeof(struct hwregFrcInitIn));
	memset(&paramTriggergenIn, 0, sizeof(struct hwregFrctriggenIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	reg_size = sizeof(struct reg_info)*REG_NUM_MAX;
	paramOut.reg = malloc(reg_size);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*REG_NUM_MAX);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return;
	}

	paramInitIn.RIU = 1;

	//init frc
	drv_hwreg_render_frc_init(
		paramInitIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);

	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramTriggergenIn.RIU = 1;
	paramOut.reg = malloc(reg_size);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*REG_NUM_MAX);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return;
	}
	//init trigger gen
	drv_hwreg_render_frc_trig_gen(
		paramTriggergenIn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
#endif
}

#define NODE_VR_BASE 0x1C000000
void _mtk_video_frc_set_RiuAdr(uint64_t adr, uint64_t adr_size)
{
	drv_hwreg_common_setRIUaddr(adr, adr_size,
				(uint64_t)ioremap(adr+NODE_VR_BASE, adr_size));
}

void _mtk_video_frc_unused_MaskEn(struct mtk_tv_kms_context *pctx)
{
	struct hwregFrcUnusedMaskEn paramUnusedMaskEn;
	struct hwregOut paramOut;
	struct reg_info *regTbl;

	regTbl = malloc(sizeof(struct reg_info)*REG_NUM_MAX);
	if (regTbl == NULL) {
		DRM_ERROR("[%s,%5d] NULL POINTER\n", __func__, __LINE__);
		return;
	}

	memset(&paramUnusedMaskEn, 0, sizeof(struct hwregFrcUnusedMaskEn));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	memset(regTbl, 0, sizeof(struct reg_info)*REG_NUM_MAX);

	paramOut.reg = regTbl;
	paramUnusedMaskEn.RIU = 1;
	paramUnusedMaskEn.enable = true; // If it's unused then mask it.

	drv_hwreg_render_frc_unused_MaskEn(
		paramUnusedMaskEn, &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);
}

#define REG_BK_SIZE 0x200
#define BK_8001 0x1000200
#define BK_1030 0x206000
#define BK_A4F0 0x149E000
#define BK_840A 0x1081400
#define BK_8426 0x1084C00
#define BK_8429 0x1085200
#define BK_8432 0x1086400
#define BK_8433 0x1086600
#define BK_843A 0x1087400
#define BK_843F 0x1087E00
#define BK_8440 0x1088000
#define BK_8442 0x1088400
#define BK_8446 0x1088C00
#define BK_8450 0x108A000
#define BK_8452 0x108A400
#define BK_8453 0x108A600
#define BK_8461 0x108C200
#define BK_84B4 0x1096800
#define BK_84B7 0x1096E00
#define BK_84D4 0x109A800
#define BK_84F4 0x109E800
#define BK_8502 0x10A0400
#define BK_8503 0x10A0600
#define BK_8504 0x10A0800
#define BK_8517 0x10A2E00
#define BK_8518 0x10A3000
#define BK_8519 0x10A3200
#define BK_851A 0x10A3400
#define BK_851B 0x10A3600
#define BK_851C 0x10A3800
#define BK_851D 0x10A3A00
#define BK_851E 0x10A3C00
#define BK_851F 0x10A3E00
#define BK_8532 0x10A6400
#define BK_8533 0x10A6600

#define BK_8619 0x10C3200
#define BK_861B 0x10C3600
#define BK_861C 0x10C3800
#define BK_861F 0x10C3E00
#define BK_8620 0x10C4000
#define BK_8621 0x10C4200
#define BK_8622 0x10C4400
#define BK_8623 0x10C4600
#define BK_8624 0x10C4800
#define BK_8625 0x10C4A00
#define BK_8626 0x10C4C00
#define BK_8627 0x10C4E00
#define BK_8628 0x10C5000
#define BK_8629 0x10C5200
#define BK_862A 0x10C5400
#define BK_862B 0x10C5600
#define BK_862C 0x10C5800
#define BK_862D 0x10C5A00
#define BK_862E 0x10C5C00
#define BK_862F 0x10C5E00

#define BK_8630 0x10C6000
#define BK_8636 0x10C6C00
#define BK_8637 0x10C6E00
#define BK_8638 0x10C7000
#define BK_863A 0x10C7400
#define BK_8650 0x10CA000
#define BK_8670 0x10CE000
#define BK_8671 0x10CE200
#define BK_8672 0x10CE400
#define BK_8673 0x10CE600
#define BK_8674 0x10CE800

#define BK_A36B 0x146D600
#define BK_A36C 0x146D800
#define BK_A38D 0x1471A00
#define BK_A391 0x1472200
#define BK_A3A0 0x1474000
#define BK_A3A1 0x1474200
#define BK_A4F0 0x149E000

#define BK_102A 0x205400
#define BK_103A 0x207400
#define BK_103C 0x207A00
#define BK_103E 0x207C00

//FRC IP Version 2
#define BK_8416 0x1082C00
#define BK_8417 0x1082E00
#define BK_8418 0x1083000
#define BK_842C 0x1085800
#define BK_8456 0x108AC00
#define BK_8457 0x108AE00
#define BK_8548 0x10A9000
#define BK_854C 0x10A9800
#define BK_8550 0x10AA000
#define BK_8554 0x10AA800
#define BK_8558 0x10AB000
#define BK_8560 0x10AC000
#define BK_8568 0x10AD000

//FRC IP Version 5
#define BK_100B 0x201600


void _mtk_video_frc_init_reg_bank_node_init(struct mtk_tv_kms_context *pctx)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	uint8_t u8frc_ipversion = pctx_frc->frc_ip_version;

	_mtk_video_frc_set_RiuAdr(BK_8001, REG_BK_SIZE);//8001
	_mtk_video_frc_set_RiuAdr(BK_840A, REG_BK_SIZE);//840A
	_mtk_video_frc_set_RiuAdr(BK_8426, REG_BK_SIZE);//8426
	_mtk_video_frc_set_RiuAdr(BK_8429, REG_BK_SIZE);//8429
	_mtk_video_frc_set_RiuAdr(BK_842C, REG_BK_SIZE);//842C
	_mtk_video_frc_set_RiuAdr(BK_8432, REG_BK_SIZE);//8432
	_mtk_video_frc_set_RiuAdr(BK_8433, REG_BK_SIZE);//8433
	_mtk_video_frc_set_RiuAdr(BK_843A, REG_BK_SIZE);//843A
	_mtk_video_frc_set_RiuAdr(BK_8440, REG_BK_SIZE);//8440
	_mtk_video_frc_set_RiuAdr(BK_8442, REG_BK_SIZE);//8442
	_mtk_video_frc_set_RiuAdr(BK_8446, REG_BK_SIZE);//8446
	_mtk_video_frc_set_RiuAdr(BK_8450, REG_BK_SIZE);//8450
	_mtk_video_frc_set_RiuAdr(BK_8452, REG_BK_SIZE);//8452
	_mtk_video_frc_set_RiuAdr(BK_8453, REG_BK_SIZE);//8453
	_mtk_video_frc_set_RiuAdr(BK_8461, REG_BK_SIZE);//8461
	_mtk_video_frc_set_RiuAdr(BK_84B4, REG_BK_SIZE);//84B4
	_mtk_video_frc_set_RiuAdr(BK_84D4, REG_BK_SIZE);//84D4
	_mtk_video_frc_set_RiuAdr(BK_84F4, REG_BK_SIZE);//84F4
	_mtk_video_frc_set_RiuAdr(BK_8502, REG_BK_SIZE);//8502
	_mtk_video_frc_set_RiuAdr(BK_8503, REG_BK_SIZE);//8503
	_mtk_video_frc_set_RiuAdr(BK_8504, REG_BK_SIZE);//8504
	_mtk_video_frc_set_RiuAdr(BK_8517, REG_BK_SIZE);//8517
	_mtk_video_frc_set_RiuAdr(BK_851B, REG_BK_SIZE);//851B
	_mtk_video_frc_set_RiuAdr(BK_851E, REG_BK_SIZE);//851E
	_mtk_video_frc_set_RiuAdr(BK_8532, REG_BK_SIZE);//8532
	_mtk_video_frc_set_RiuAdr(BK_8533, REG_BK_SIZE);//8533

	_mtk_video_frc_set_RiuAdr(BK_8619, REG_BK_SIZE);//8619
	_mtk_video_frc_set_RiuAdr(BK_861B, REG_BK_SIZE);//861B
	_mtk_video_frc_set_RiuAdr(BK_861C, REG_BK_SIZE);//861C
	_mtk_video_frc_set_RiuAdr(BK_861F, REG_BK_SIZE);//861F
	_mtk_video_frc_set_RiuAdr(BK_8620, REG_BK_SIZE);//8620
	_mtk_video_frc_set_RiuAdr(BK_8621, REG_BK_SIZE);//8621
	_mtk_video_frc_set_RiuAdr(BK_8622, REG_BK_SIZE);//8622
	_mtk_video_frc_set_RiuAdr(BK_8623, REG_BK_SIZE);//8623
	_mtk_video_frc_set_RiuAdr(BK_8624, REG_BK_SIZE);//8624
	_mtk_video_frc_set_RiuAdr(BK_8625, REG_BK_SIZE);//8625
	_mtk_video_frc_set_RiuAdr(BK_8626, REG_BK_SIZE);//8626
	_mtk_video_frc_set_RiuAdr(BK_8627, REG_BK_SIZE);//8627
	_mtk_video_frc_set_RiuAdr(BK_8628, REG_BK_SIZE);//8628
	_mtk_video_frc_set_RiuAdr(BK_8629, REG_BK_SIZE);//8629
	_mtk_video_frc_set_RiuAdr(BK_862A, REG_BK_SIZE);//862A
	_mtk_video_frc_set_RiuAdr(BK_862B, REG_BK_SIZE);//862B
	_mtk_video_frc_set_RiuAdr(BK_862C, REG_BK_SIZE);//862C
	_mtk_video_frc_set_RiuAdr(BK_862D, REG_BK_SIZE);//862D
	_mtk_video_frc_set_RiuAdr(BK_862E, REG_BK_SIZE);//862E
	_mtk_video_frc_set_RiuAdr(BK_8630, REG_BK_SIZE);//8630
	_mtk_video_frc_set_RiuAdr(BK_8636, REG_BK_SIZE);//8636
	_mtk_video_frc_set_RiuAdr(BK_8637, REG_BK_SIZE);//8637
	_mtk_video_frc_set_RiuAdr(BK_8638, REG_BK_SIZE);//8638
	_mtk_video_frc_set_RiuAdr(BK_863A, REG_BK_SIZE);//863A
	_mtk_video_frc_set_RiuAdr(BK_8650, REG_BK_SIZE);//8650
	if ((u8frc_ipversion == FRC_IP_VERSION_2) ||
		(u8frc_ipversion == FRC_IP_VERSION_4) ||
		(u8frc_ipversion == FRC_IP_VERSION_5) ||
		(u8frc_ipversion == FRC_IP_VERSION_6)) {
		_mtk_video_frc_set_RiuAdr(BK_8416, REG_BK_SIZE);//8416
		_mtk_video_frc_set_RiuAdr(BK_8417, REG_BK_SIZE);//8417
		_mtk_video_frc_set_RiuAdr(BK_8418, REG_BK_SIZE);//8418
		_mtk_video_frc_set_RiuAdr(BK_842C, REG_BK_SIZE);//842C
		_mtk_video_frc_set_RiuAdr(BK_8456, REG_BK_SIZE);//8456
		_mtk_video_frc_set_RiuAdr(BK_8457, REG_BK_SIZE);//8457
		_mtk_video_frc_set_RiuAdr(BK_851C, REG_BK_SIZE);//851C
		_mtk_video_frc_set_RiuAdr(BK_8548, REG_BK_SIZE);//8548
		_mtk_video_frc_set_RiuAdr(BK_854C, REG_BK_SIZE);//854C
		_mtk_video_frc_set_RiuAdr(BK_8550, REG_BK_SIZE);//8550
		_mtk_video_frc_set_RiuAdr(BK_8554, REG_BK_SIZE);//8554
		_mtk_video_frc_set_RiuAdr(BK_8558, REG_BK_SIZE);//8558
		_mtk_video_frc_set_RiuAdr(BK_8560, REG_BK_SIZE);//8560
		_mtk_video_frc_set_RiuAdr(BK_8568, REG_BK_SIZE);//8568
		_mtk_video_frc_set_RiuAdr(BK_A3A0, REG_BK_SIZE);//A3A0
		_mtk_video_frc_set_RiuAdr(BK_A3A1, REG_BK_SIZE);//A3A1
	}
	if (u8frc_ipversion == FRC_IP_VERSION_1_E3) {
		_mtk_video_frc_set_RiuAdr(BK_102A, REG_BK_SIZE);//102A
		_mtk_video_frc_set_RiuAdr(BK_1030, REG_BK_SIZE);//1030
		_mtk_video_frc_set_RiuAdr(BK_103A, REG_BK_SIZE);//103A
		_mtk_video_frc_set_RiuAdr(BK_103C, REG_BK_SIZE);//103C
		_mtk_video_frc_set_RiuAdr(BK_854C, REG_BK_SIZE);//854C
		_mtk_video_frc_set_RiuAdr(BK_84B7, REG_BK_SIZE);//84B7
		_mtk_video_frc_set_RiuAdr(BK_8518, REG_BK_SIZE);//8518
		_mtk_video_frc_set_RiuAdr(BK_8519, REG_BK_SIZE);//8519
		_mtk_video_frc_set_RiuAdr(BK_851A, REG_BK_SIZE);//851A
		_mtk_video_frc_set_RiuAdr(BK_851C, REG_BK_SIZE);//851C
		_mtk_video_frc_set_RiuAdr(BK_851D, REG_BK_SIZE);//851D
		_mtk_video_frc_set_RiuAdr(BK_851F, REG_BK_SIZE);//851F
		_mtk_video_frc_set_RiuAdr(BK_8550, REG_BK_SIZE);//8550
		_mtk_video_frc_set_RiuAdr(BK_8554, REG_BK_SIZE);//8554
		_mtk_video_frc_set_RiuAdr(BK_8558, REG_BK_SIZE);//8558
		_mtk_video_frc_set_RiuAdr(BK_8568, REG_BK_SIZE);//8568
		_mtk_video_frc_set_RiuAdr(BK_862F, REG_BK_SIZE);//862F
		_mtk_video_frc_set_RiuAdr(BK_8670, REG_BK_SIZE);//8670
		_mtk_video_frc_set_RiuAdr(BK_8671, REG_BK_SIZE);//8671
		_mtk_video_frc_set_RiuAdr(BK_8672, REG_BK_SIZE);//8672
		_mtk_video_frc_set_RiuAdr(BK_8673, REG_BK_SIZE);//8673
		_mtk_video_frc_set_RiuAdr(BK_8674, REG_BK_SIZE);//8674
		_mtk_video_frc_set_RiuAdr(BK_A3A1, REG_BK_SIZE);//A3A1
	}

	if (u8frc_ipversion == FRC_IP_VERSION_5)
		_mtk_video_frc_set_RiuAdr(BK_100B, REG_BK_SIZE);//100B

	if (pctx_frc->frc_as_hse)
		_mtk_video_frc_set_RiuAdr(BK_843F, REG_BK_SIZE);//843F
}

void _mtk_video_frc_init_reg_bank_node_loop(void)
{
	_mtk_video_frc_set_RiuAdr(BK_A4F0, REG_BK_SIZE);//A4F0

	_mtk_video_frc_set_RiuAdr(BK_A36B, REG_BK_SIZE);//A36B
	_mtk_video_frc_set_RiuAdr(BK_A36C, REG_BK_SIZE);//A36C
	_mtk_video_frc_set_RiuAdr(BK_A38D, REG_BK_SIZE);//A38D
	_mtk_video_frc_set_RiuAdr(BK_A391, REG_BK_SIZE);//A391
	_mtk_video_frc_set_RiuAdr(BK_A4F0, REG_BK_SIZE);//A4F0

}

#define MTK_DRM_TV_FRC_COMPATIBLE_STR "mediatek,mediatek-frc"

static int get_memory_info(u32 *addr)
{
	struct device_node *target_memory_np = NULL;
	uint32_t len = 0;
	__be32 *p = NULL;

	// find memory_info node in dts
	target_memory_np = of_find_node_by_name(NULL, "memory_info");
	if (!target_memory_np)
		return -ENODEV;

	// query cpu_emi0_base value from memory_info node
	p = (__be32 *)of_get_property(target_memory_np, "cpu_emi0_base", &len);
	if (p != NULL) {
		*addr = be32_to_cpup(p);
		of_node_put(target_memory_np);
		p = NULL;
	} else {
		pr_err("can not find cpu_emi0_base info\n");
		of_node_put(target_memory_np);
		return -EINVAL;
	}
	return 0;
}

static int _mtk_video_display_frc_read_dts_shared_memory_info(
	struct mtk_frc_context *pctx_frc,
	struct device_node *frc_node)
{
	int ret = 0;
	struct of_mmap_info_data of_mmap_info = {0};
	uint32_t CPU_base_adr = 0;
	uint32_t u32Tmp = 0x0;
	const char *name = NULL;
	uint8_t u8Shm_buffer_index = 0;

	if ((pctx_frc == NULL) || (frc_node == NULL)) {
		DRM_ERROR("[%s, %d]pointer is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	name = "frc_shm_buffer_index";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]read DTS failed, name = %s\n",
			__func__, __LINE__, name);
		return ret;
	}
	u8Shm_buffer_index = (uint8_t)u32Tmp;
	DRM_INFO("[FRC][%s, %d]frc shared memory buffer index=%d\n",
		__func__, __LINE__, u8Shm_buffer_index);

	ret = of_mtk_get_reserved_memory_info_by_idx(frc_node, u8Shm_buffer_index,
		&of_mmap_info);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]of_mtk_get_reserved_memory_info_by_idx failed\n",
			__func__, __LINE__);
		return ret;
	}

	ret = get_memory_info(&CPU_base_adr);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]get_memory_info failed\n",
			__func__, __LINE__);
		return ret;
	}

	pctx_frc->frc_shm_adr = (uint64_t)of_mmap_info.start_bus_address -
								(uint64_t)CPU_base_adr;
	pctx_frc->frc_shm_size = (uint32_t)of_mmap_info.buffer_size;

	DRM_INFO("[FRC][%s, %d]frc_shm_adr = 0x%llx, frc_shm_size = %d.\n",
		__func__, __LINE__, pctx_frc->frc_shm_adr, pctx_frc->frc_shm_size);

	//Only support non-cache.
	_vaddr_frc = ioremap_wc(of_mmap_info.start_bus_address,
		of_mmap_info.buffer_size);
	if (!_vaddr_frc) {
		DRM_ERROR("[FRC][%s, %d]mmap failed to frc shm vaddr.\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	pctx_frc->frc_shm_virAdr = (size_t)_vaddr_frc;
	DRM_INFO("[FRC][%s, %d]frc_shm_virAdr = 0x%lx.\n",
		__func__, __LINE__, pctx_frc->frc_shm_virAdr);
	memset((unsigned char *)pctx_frc->frc_shm_virAdr, 0x0,
		sizeof(uint8_t) * pctx_frc->frc_shm_size);

	return ret;
}


static int _mtk_video_display_frc_read_dts_photo_info(
	struct mtk_frc_context *pctx_frc,
	struct device_node *frc_node)
{
	int ret = 0;
	uint32_t u32Tmp = 0x0;
	const char *name = NULL;

	if ((pctx_frc == NULL) || (frc_node == NULL)) {
		DRM_ERROR("[%s, %d]pointer is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	name = "frc_photo_enable";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]read DTS failed, name = %s\n",
			__func__, __LINE__, name);
		return ret;
	}
	pctx_frc->frc_photo_enable = (uint8_t)u32Tmp;
	DRM_INFO("[FRC][%s, %d]FRC_PHOTO_ENABLE=%d\n",
		__func__, __LINE__, pctx_frc->frc_photo_enable);

	if (pctx_frc->frc_photo_enable == true) {
		name = "frc_photo_rw_diff";
		ret = of_property_read_u32(frc_node, name, &u32Tmp);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d]read DTS failed, name = %s\n",
				__func__, __LINE__, name);
			return ret;
		}
		pctx_frc->frc_photo_rw_diff = (uint8_t)u32Tmp;
		DRM_INFO("[FRC][%s, %d]FRC_PHOTO_RW_DIFF=%d\n",
			__func__, __LINE__, pctx_frc->frc_photo_rw_diff);
	}

	return ret;

}

static int _mtk_video_display_frc_read_dts_common_info(
	struct mtk_frc_context *pctx_frc,
	struct device_node *frc_node)
{
	int ret = 0;
	const char *name = NULL;
	uint32_t u32Tmp = 0x0;

	if ((pctx_frc == NULL) || (frc_node == NULL)) {
		DRM_ERROR("[%s, %d]pointer is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	// read frc ip version
	name = "FRC_IP_VERSION";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d] Get DTS failed, name = %s \r\n",
			__func__, __LINE__, name);
		return ret;
	}
	pctx_frc->frc_ip_version = u32Tmp;
	DRM_INFO("[FRC][%s, %d]IP Version = %d.\n", __func__, __LINE__,
		pctx_frc->frc_ip_version);

	// read frc frame cnt
	name = "frc_frame_cnt";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d] Get DTS failed, name = %s \r\n",
			__func__, __LINE__, name);
		return ret;
	}
	pctx_frc->frc_frame_cnt = (uint8_t)u32Tmp;
	DRM_INFO("[FRC][%s, %d]frc_frame_cnt=%d.\n", __func__, __LINE__,
		pctx_frc->frc_frame_cnt);

	// read frc_max_frame_cnt for A2DP
	name = "frc_max_frame_cnt";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d] Get DTS failed, name = %s \r\n",
			__func__, __LINE__, name);
		return ret;
	}
	pctx_frc->frc_max_frame_cnt = (uint8_t)u32Tmp;
	DRM_INFO("[FRC][%s, %d]frc_max_frame_cnt=%d.\n", __func__, __LINE__,
		pctx_frc->frc_max_frame_cnt);

	// read frc hse
	name = "frc_hse";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0) {
		DRM_ERROR("[%s, %d] Get DTS failed, property = %s\n",
			__func__, __LINE__, name);
		return ret;
	}
	pctx_frc->frc_as_hse = (uint8_t)u32Tmp;

	// read memc algo engine
	name = "frc_alg_run_on";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d] Get DTS failed, name = %s \r\n",
			__func__, __LINE__, name);
	} else {
		pctx_frc->frc_alg_run_on = (uint8_t)u32Tmp;
		frc_alg_run_on = pctx_frc->frc_alg_run_on;
	}

	// read frc rw diff
	name = "frc_rw_diff";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d] Get rw diff failed, name = %s \r\n",
			__func__, __LINE__, name);
		return ret;
	}
	pctx_frc->frc_dts_rwdiff = (uint16_t)u32Tmp;
	DRM_INFO("Func:%s, Line:%d, frc rwdiff :%d\n", __func__,
		__LINE__, pctx_frc->frc_dts_rwdiff);

	return ret;
}

static int _mtk_video_display_frc_read_dts_vg_sync_info(
	struct mtk_frc_context *pctx_frc,
	struct device_node *frc_node)
{
	int ret = 0;
	const char *name = NULL;
	uint32_t u32Tmp = 0x0;
	uint8_t u8frc_vgsync_buf_index = 0;
	struct of_mmap_info_data of_mmap_info = {0};
	uint32_t CPU_base_adr = 0;

	if ((pctx_frc == NULL) || (frc_node == NULL)) {
		DRM_ERROR("[%s, %d]pointer is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	// read frc vg sync memory buffer index
	name = "frc_vgsync_buf_index";
	ret = of_property_read_u32(frc_node, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d] Get VG sync buffer index failed, name = %s \r\n",
			__func__, __LINE__, name);
		return ret;
	}
	u8frc_vgsync_buf_index = (uint8_t)u32Tmp;

	if (u8frc_vgsync_buf_index != INVALID_BUF_INDEX) {
		ret = get_memory_info(&CPU_base_adr);
		if (ret)
			ret = -EINVAL;

		ret = of_mtk_get_reserved_memory_info_by_idx(
			frc_node, u8frc_vgsync_buf_index, &of_mmap_info);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d]Get VG sync SHM failed, ret = %d.\r\n",
				__func__, __LINE__, ret);
			return ret;
		}

		_u64VgsyncBufAddr =
		pctx_frc->frc_vgsync_shm_adr =
			(uint64_t)of_mmap_info.start_bus_address - (uint64_t)CPU_base_adr;

		_u32VgsyncBufSize =
		pctx_frc->frc_vgsync_shm_size =
			(uint32_t)of_mmap_info.buffer_size;

		//Only support non-cache.
		_vaddr_vgsync = ioremap_wc(of_mmap_info.start_bus_address,
			of_mmap_info.buffer_size);
		if (!_vaddr_vgsync) {
			DRM_ERROR("[FRC][%s, %d]MMAP failed or not support vgsync",
				__func__, __LINE__);

			pctx_frc->frc_vgsync_shm_virAdr = 0;
			goto NO_VGSYNC_MMAP;
		}

		pctx_frc->frc_vgsync_shm_virAdr = (size_t)_vaddr_vgsync;
		DRM_INFO("[FRC][%s, %d]frc_vgsync_shm_virAdr = 0x%lx.\n",
			__func__, __LINE__, pctx_frc->frc_vgsync_shm_virAdr);
		memset((unsigned char *)pctx_frc->frc_vgsync_shm_virAdr, 0x0,
		sizeof(uint8_t) * pctx_frc->frc_vgsync_shm_size);

		DRM_INFO("[FRC][%s, %d]Get VG sync SHM addr = 0x%llx, size = %d.\n",
			__func__, __LINE__,
			_u64VgsyncBufAddr, _u32VgsyncBufSize);
	}

NO_VGSYNC_MMAP:
	return ret;
}


static int readDTB2FRCPrivate(
	struct mtk_video_context *pctx_video,
	struct mtk_frc_context *pctx_frc)
{
	int ret = 0;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, MTK_DRM_TV_FRC_COMPATIBLE_STR);

	if (np != NULL) {
		// read frc common info
		ret = _mtk_video_display_frc_read_dts_common_info(pctx_frc, np);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d]Read dts frc common info failed, ret = %d.\r\n",
				__func__, __LINE__, ret);
			return ret;
		}

		// read shared memory
		ret = _mtk_video_display_frc_read_dts_shared_memory_info(pctx_frc, np);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d]Read dts shared memory info failed, ret = %d.\r\n",
				__func__, __LINE__, ret);
			return ret;
		}

		// read frc photo info
		ret = _mtk_video_display_frc_read_dts_photo_info(pctx_frc, np);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d]Read dts photo info failed, ret = %d.\r\n",
				__func__, __LINE__, ret);
			return ret;
		}

		// read VG sync SHM addr and size
		ret = _mtk_video_display_frc_read_dts_vg_sync_info(pctx_frc, np);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d]Read dts vg sync info failed, ret = %d.\r\n",
				__func__, __LINE__, ret);
			return ret;
		}
	}

	return ret;
}


static int _mtk_video_frc_get_dts_arrray_to_u8_data(struct device_node *np,
	const char *name,
	__u8 *ret_array,
	int ret_array_length)
{
	int ret = 0;
	int i = 0;
	__u32 *u32TmpArray = NULL;

	if ((np == NULL) || (name == NULL) || (ret_array == NULL)) {
		DRM_ERROR("[FRC][%s, %d] null pointer fail !\n",
		__func__, __LINE__);
		return 0;
	}

	u32TmpArray = kmalloc_array(ret_array_length, sizeof(__u32), GFP_KERNEL);
	if (u32TmpArray == NULL) {
		DRM_ERROR("[FRC][%s, %d] malloc failed !\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret = of_property_read_u32_array(np, name, u32TmpArray, ret_array_length);
	if (ret != 0x0) {
		DRM_ERROR("[FRC][%s, %d] of_property_read_u32_array failed: %s, ret =%d\n",
		__func__, __LINE__, name, ret);
		kfree(u32TmpArray);
		return ret;
	}

	for (i = 0; i < ret_array_length; i++)
		*(ret_array + i) = (__u8)u32TmpArray[i];

	kfree(u32TmpArray);

	return 0;
}

static int readDTB2FRCSmiPrivate(
	struct mtk_frc_context *pctx_frc)
{
	int ret = 0;
	int i = 0;
	struct device_node *np = NULL;
	__u32 u32Tmp = 0x0;
	const char *name = NULL;

	if (pctx_frc == NULL) {
		DRM_ERROR("[%s, %d] Null pointer\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	// reset to default
	for (i = 0; i < MAX_FRC_SMI_PORT_NUM; i++) {
		pctx_frc->frc_smi_miu2gmc[i] = 0;
		pctx_frc->frc_smi_port[i] = 0;
	}
	np = of_find_compatible_node(NULL, NULL, MTK_DRM_TV_FRC_SMI_COMPATIBLE);

	if (np != NULL) {
		// read frc smi number
		name = "frc_smi_number";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d] Get DTS failed, name = %s \r\n",
				__func__, __LINE__, name);
			return ret;
		}
		pctx_frc->frc_smi_number = (uint8_t)u32Tmp;
		DRM_INFO("[FRC][%s, %d]smi_number = %d.\n", __func__, __LINE__,
			pctx_frc->frc_smi_number);

		// read frc smi miu2gmc id
		if (pctx_frc->frc_smi_number > 0) {
			name = "frc_smi_miu2gmc";
			ret = _mtk_video_frc_get_dts_arrray_to_u8_data(np, name,
				pctx_frc->frc_smi_miu2gmc, pctx_frc->frc_smi_number);
			if (ret != 0x0) {
				DRM_ERROR("[%s, %d] Get DTS failed, name = %s \r\n",
				__func__, __LINE__, name);
				return ret;
			}
		}

		// read frc smi port id
		if (pctx_frc->frc_smi_number > 0) {
			name = "frc_smi_port";
			ret = _mtk_video_frc_get_dts_arrray_to_u8_data(np, name,
				pctx_frc->frc_smi_port, pctx_frc->frc_smi_number);
			if (ret != 0x0) {
				DRM_ERROR("[%s, %d] Get DTS failed, name = %s \r\n",
				__func__, __LINE__, name);
				return ret;
			}
		}
	}

	return ret;
}


static int _mtk_video_display_frc_set_mux_clock(struct device *dev,
			int clk_index, const char *dev_clk_name, bool bExistParent)
{
	int ret = 0;
	struct clk *dev_clk;
	struct clk_hw *clk_hw;
	struct clk_hw *parent_clk_hw;

	dev_clk = devm_clk_get(dev, dev_clk_name);

	if (IS_ERR_OR_NULL(dev_clk)) {
		DRM_ERROR("[FRC][%s, %d]: failed to get %s\n", __func__, __LINE__, dev_clk_name);
		return -ENODEV;
	}
	DRM_INFO("[FRC][%s, %d]clk_name = %s\n", __func__, __LINE__, __clk_get_name(dev_clk));

	if (bExistParent == true) {
		clk_hw = __clk_get_hw(dev_clk);
		parent_clk_hw = clk_hw_get_parent_by_index(clk_hw, clk_index);
		if (IS_ERR_OR_NULL(parent_clk_hw)) {
			DRM_ERROR("[FRC][%s, %d]failed to get parent_clk_hw\n", __func__, __LINE__);
			devm_clk_put(dev, dev_clk);
			return -ENODEV;
		}

		//set_parent clk
		ret = clk_set_parent(dev_clk, parent_clk_hw->clk);
		if (ret) {
			DRM_ERROR("[FRC][%s, %d]failed to change clk_index [%d]\n",
				__func__, __LINE__, clk_index);
			devm_clk_put(dev, dev_clk);
			return -ENODEV;
		}
	}

	//prepare and enable clk
	ret = clk_prepare_enable(dev_clk);

	if (ret) {
		DRM_ERROR("[FRC][%s, %d]failed to enable clk_name = %s\n",
				__func__, __LINE__,  __clk_get_name(dev_clk));
		devm_clk_put(dev, dev_clk);
		return -ENODEV;
	}

	devm_clk_put(dev, dev_clk);
	return ret;
}

static int _mtk_video_display_frc_set_dis_clock(struct device *dev,
			const char *dev_clk_name)
{
	int ret = 0;
	struct clk *dev_clk;

	dev_clk = devm_clk_get(dev, dev_clk_name);

	if (IS_ERR_OR_NULL(dev_clk)) {
		DRM_ERROR("[FRC][%s, %d]: failed to get %s\n", __func__, __LINE__, dev_clk_name);
		return -ENODEV;
	}

	DRM_INFO("[FRC][%s, %d]clk_name = %s\n", __func__, __LINE__, __clk_get_name(dev_clk));

	//disable clk
	clk_disable_unprepare(dev_clk);

	devm_clk_put(dev, dev_clk);

	return ret;
}

int _mtk_video_display_set_frc_smi_mask(struct mtk_frc_context *pctx_frc,
	bool enable)
{
	int ret = 0;
	int i = 0;

	if (pctx_frc == NULL) {
		DRM_ERROR("[%s, %d] pointer is null.\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (pctx_frc->frc_smi_number > 0) {
		for (i = 0; i < pctx_frc->frc_smi_number; i++) {
			ret |= mtk_miu2gmc_mask(pctx_frc->frc_smi_miu2gmc[i],
			pctx_frc->frc_smi_port[i], enable);
		}
		if (ret < 0) {
			DRM_ERROR("[%s, %d] mtk_miu2gmc_mask failed.\n", __func__, __LINE__);
			return ret;
		}
	}

	return ret;
}

#define CK_NUM_MAX     (40)

#define CL_NUM_E3_MAX  (12)
#define CL_NUM_V2_MAX  (11)
#define CL_NUM_V4_MAX  (12)
#define CL_NUM_V6_MAX  (5)

#define DIS_CK_NUM_MAX (40)

#define MUX_CK_NUM_MAX (4)

static const char *ck_name[CK_NUM_MAX] = {
	"MEMC_EN_FRC_FCLK_2X2DEFLICKER",
	"MEMC_EN_FRC_FCLK_2X2FDCLK_2X",
	"MEMC_EN_FRC_FCLK_2X2FRCIOPM",
	"MEMC_EN_FRC_FCLK_2X2FRCIOPMRV55",
	"MEMC_EN_FRC_FCLK_2X2MI_PH3",
	"MEMC_EN_FRC_FCLK_2X2MI_PH4",
	"MEMC_EN_FRC_FCLK_2X2OUT_STAGE",
	"MEMC_EN_FRC_FCLK_2X2SNR",
	"MEMC_EN_FRC_FCLK_2XPLUS2FRCIOPMRV55",
	"MEMC_EN_FRC_FCLK_2XPLUS2SCIP",
	"MEMC_EN_FRC_FCLK_2XPLUS2SCMW",
	"MEMC_EN_FRC_FCLK2FDCLK",
	"MEMC_EN_FRC_FCLK2FRCIOPM",
	"MEMC_EN_FRC_FCLK2FRCIOPMRV55",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_ALL",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_MLB_SRAM",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_0",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_1",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_2",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_3",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_4",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_5",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_6",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_7",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_GMV",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_ALL",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_MLB_MI_L",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_MLB_MI_R",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_MLB_MI_SRAM_L",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_MLB_MI_SRAM_R",
	"MEMC_EN_FRC_FCLK2ME",
	"MEMC_EN_FRC_FCLK2ME_4X4",
	"MEMC_EN_FRC_FCLK2ME_PH1",
	"MEMC_EN_FRC_FCLK2ME_PH2",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_8",
	"MEMC_EN_XTAL_24M2FRCIOPM",
	"MEMC_EN_XTAL_24M2FRCIOPMRV55",
	"MEMC_EN_XTAL_24M2FRCMEHALO",
	"MEMC_EN_XTAL_24M2FRCMEMLB",
	"MEMC_EN_XTAL_24M2FRCMI"
};

static const char *ck_name_E3[CL_NUM_E3_MAX] = {
	"MEMC2_EN_FRC_FCLK_2X2FRCIOPMRV55",
	"MEMC2_EN_FRC_FCLK_2XPLUS2FRCIOPMRV55",
	"MEMC2_EN_FRC_FCLK2FRCIOPMRV55",
	"MEMC2_EN_XTAL_24M2FRCIOPMRV55",
	"MEMC2_EN_FRC_FCLK_2X2FRCMEHALO",
	"MEMC2_EN_FRC_FCLK2FRCMEHALO",
	"MEMC2_EN_RIU_FRC2RIU_DFT_FRC_FRCMEHALO",
	"MEMC2_EN_RIU_NONPM2RIU_DFT_NONPM_FRCMEHALO",
	"MEMC2_EN_XTAL_24M2FRCMEHALO",
	"MEMC2_EN_FRC_FCLK2FRCMI_CLK_ALL",
	"MEMC2_EN_XTAL_24M2FRCMI",
	"CLK_MEMC2_EN_NR"
};

static const char *ck_name_v2[CL_NUM_V2_MAX] = {
	"MEMC97_EN_FRC_FCLK_2X2FRCCORE1",
	"MEMC97_EN_FRC_FCLK_2XPLUS2FRCCORE1",
	"MEMC97_EN_SMI2FRCCORE1",
	"MEMC97_EN_XTAL_24M2FRCCORE1",
	"MEMC97_EN_FRC_FCLK2FRCCORE2",
	"MEMC97_EN_SMI2FRCCORE2",
	"MEMC97_EN_XTAL_24M2FRCCORE2",
	"MEMC97_EN_FRC_FCLK_2X2FRCCORE3",
	"MEMC97_EN_SMI2FRCCORE3",
	"MEMC97_EN_XTAL_24M2FRCCORE3",
	"MEMC97_EN_SMI2FRCMISC",
};

static const char *ck_name_v4[CL_NUM_V4_MAX] = {
	"MEMC2_EN_FRC_FCLK_2X2FRCCORE1",
	"MEMC2_EN_FRC_FCLK_2XPLUS2FRCCORE1",
	"MEMC2_EN_SMI2FRCCORE1",
	"MEMC2_EN_SMI2FRC_EMI",
	"MEMC2_EN_XTAL_24M2FRCCORE1",
	"MEMC2_EN_FRC_FCLK2FRCCORE2",
	"MEMC2_EN_SMI2FRCCORE2",
	"MEMC2_EN_XTAL_24M2FRCCORE2",
	"MEMC2_EN_FRC_FCLK_2X2FRCCORE3",
	"MEMC2_EN_SMI2FRCCORE3",
	"MEMC2_EN_XTAL_24M2FRCCORE3",
	"MEMC2_EN_SMI2FRCMISC"
};

static const char *ck_name_v6[CL_NUM_V6_MAX] = {
	"MEMC2_EN_FRC_FCLK_2X2FRCCORE1",
	"MEMC2_EN_FRC_FCLK_2XPLUS2FRCCORE1",
	"MEMC2_EN_FRC_FCLK2FRCCORE1",
	"MEMC2_EN_XTAL_24M2FRCCORE1",
	"MEMC2_EN_XTAL_12M2FRCMISC"
};

static const char *mux_ck_name[MUX_CK_NUM_MAX] = {
	"MEMC_FRC_FCLK_2X_INT_CK",
	"MEMC_FRC_FCLK_INT_CK",
	"TOPGEN_FRC_FCLK_2X_CK",
	"TOPGEN_FRC_FCLK_2XPLUS_CK"
};

static const char *dis_ck_name[DIS_CK_NUM_MAX] = {
	"MEMC_EN_FRC_FCLK_2X2DEFLICKER",
	"MEMC_EN_FRC_FCLK_2X2FDCLK_2X",
	"MEMC_EN_FRC_FCLK_2X2FRCIOPM",
	"MEMC_EN_FRC_FCLK_2X2FRCIOPMRV55",
	"MEMC_EN_FRC_FCLK_2X2MI_PH3",
	"MEMC_EN_FRC_FCLK_2X2MI_PH4",
	"MEMC_EN_FRC_FCLK_2X2OUT_STAGE",
	"MEMC_EN_FRC_FCLK_2X2SNR",
	"MEMC_EN_FRC_FCLK_2XPLUS2FRCIOPMRV55",
	"MEMC_EN_FRC_FCLK_2XPLUS2SCIP",
	"MEMC_EN_FRC_FCLK_2XPLUS2SCMW",
	"MEMC_EN_FRC_FCLK2FDCLK",
	"MEMC_EN_FRC_FCLK2FRCIOPM",
	"MEMC_EN_FRC_FCLK2FRCIOPMRV55",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_ALL",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_MLB_SRAM",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_0",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_1",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_2",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_3",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_4",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_5",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_6",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_7",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_GMV",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_ALL",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_MLB_MI_L",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_MLB_MI_R",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_MLB_MI_SRAM_L",
	"MEMC_EN_FRC_FCLK2FRCMI_CLK_MLB_MI_SRAM_R",
	"MEMC_EN_FRC_FCLK2ME",
	"MEMC_EN_FRC_FCLK2ME_4X4",
	"MEMC_EN_FRC_FCLK2ME_PH1",
	"MEMC_EN_FRC_FCLK2ME_PH2",
	"MEMC_EN_FRC_FCLK2FRCMEMLB_CLK_SAD_8",
	"MEMC_EN_XTAL_24M2FRCIOPM",
	"MEMC_EN_XTAL_24M2FRCIOPMRV55",
	"MEMC_EN_XTAL_24M2FRCMEHALO",
	"MEMC_EN_XTAL_24M2FRCMEMLB",
	"MEMC_EN_XTAL_24M2FRCMI"
};

//REG_01F0_CKGEN01_REG_CKG_FRC_FCLK_2X_FRCIOPMRV55
//0:frc_fclk_2x_ck, 1:frc_fclk_ck,
//REG_01E8_CKGEN01_REG_CKG_FRC_FCLK_FRCMI
//0:frc_fclk_ck, 1:frc_fclk_2x_ck,
//REG_01F0_CKGEN01_REG_CKG_FRC_FCLK_2X
//0:xcsrpll_vcod1_630m_ck, 1:sys2pll_vcod4_360m_ck, 2:mpll_vcod3_288m_ck, 3:mpll_vcod4_216m_ck
//REG_01F8_CKGEN01_REG_CKG_FRC_FCLK_2XPLUS
//0:xcpll_vcod1_720m_ck, 1:DUMMY_MIMI, 2:sys2pll_vcod4_360m_ck, 3:mpll_vcod3_288m_ck


static int mux_ck_val[MUX_CK_NUM_MAX] = {
	1,
	1,
	1,
	0
};

static int mtk_video_display_frc_init_clock(struct device *dev)
{
	uint8_t ck_num = 0;
	uint8_t mux_ck_num = 0;
	for (ck_num = 0; ck_num < CK_NUM_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_mux_clock(dev, 0, ck_name[ck_num], FALSE))
			return -ENODEV;
	}

	//FRC mux clock setting
	for (mux_ck_num = 0; mux_ck_num < MUX_CK_NUM_MAX; mux_ck_num++) {
		if (_mtk_video_display_frc_set_mux_clock(dev,
			mux_ck_val[mux_ck_num], mux_ck_name[mux_ck_num], TRUE))
			return -ENODEV;
	}

	return 0;
}


#define REG_CKG_FRC_FCLK_FRCMI 0x21e8
#define REG_CKG_FRC_FCLK_FRCMI_S 4
#define REG_CKG_FRC_FCLK_FRCMI_E 6
#define REG_CKG_FRC_FCLK_FRCMI_VAL 4
static int mtk_video_display_frc_init_clock_E3(struct device *dev)
{
	uint8_t ck_num = 0;
	int ret;

	//FRC mux clock setting
	ret = clock_write(CLK_NONPM, REG_CKG_FRC_FCLK_FRCMI, REG_CKG_FRC_FCLK_FRCMI_VAL,
				REG_CKG_FRC_FCLK_FRCMI_S, REG_CKG_FRC_FCLK_FRCMI_E);

	for (ck_num = 0; ck_num < CL_NUM_E3_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_mux_clock(dev, 0, ck_name_E3[ck_num], FALSE))
			return -ENODEV;
	}

	return 0;
}


static int mtk_video_display_frc_init_clock_v2(struct device *dev)
{
	uint8_t ck_num = 0;

	for (ck_num = 0; ck_num < CL_NUM_V2_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_mux_clock(dev, 0, ck_name_v2[ck_num], FALSE))
			return -ENODEV;
	}

	return 0;
}

static int mtk_video_display_frc_init_clock_v4(struct device *dev)
{
	uint8_t ck_num = 0;

	for (ck_num = 0; ck_num < CL_NUM_V4_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_mux_clock(dev, 0, ck_name_v4[ck_num], FALSE))
			return -ENODEV;
	}

	return 0;
}

static int mtk_video_display_frc_init_clock_v6(struct device *dev)
{
	uint8_t ck_num = 0;

	for (ck_num = 0; ck_num < CL_NUM_V6_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_mux_clock(dev, 0, ck_name_v6[ck_num], FALSE))
			return -ENODEV;
	}

	return 0;
}

static int mtk_video_display_frc_disable_clock(struct device *dev)
{
	uint8_t ck_num = 0;

	for (ck_num = 0; ck_num < DIS_CK_NUM_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_dis_clock(dev, dis_ck_name[ck_num]))
			return -ENODEV;
	}

	return 0;
}

static int mtk_video_display_frc_disable_clock_E3(struct device *dev)
{
	uint8_t ck_num = 0;

	for (ck_num = 0; ck_num < CL_NUM_E3_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_dis_clock(dev, ck_name_E3[ck_num]))
			return -ENODEV;
	}

	return 0;
}


static int mtk_video_display_frc_disable_clock_v2(struct device *dev)
{
	uint8_t ck_num = 0;

	for (ck_num = 0; ck_num < CL_NUM_V2_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_dis_clock(dev, ck_name_v2[ck_num]))
			return -ENODEV;
	}

	return 0;
}

static int mtk_video_display_frc_disable_clock_v4(struct device *dev)
{
	uint8_t ck_num = 0;

	for (ck_num = 0; ck_num < CL_NUM_V4_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_dis_clock(dev, ck_name_v4[ck_num]))
			return -ENODEV;
	}

	return 0;
}

static int mtk_video_display_frc_disable_clock_v6(struct device *dev)
{
	uint8_t ck_num = 0;

	for (ck_num = 0; ck_num < CL_NUM_V6_MAX; ck_num++) {
		if (_mtk_video_display_frc_set_dis_clock(dev, ck_name_v6[ck_num]))
			return -ENODEV;
	}

	return 0;
}

static void _mtk_video_display_frc_init(
	struct device *dev,
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	int ret;
	int i = 0;

	DRM_INFO("[PROFILE] %s+\n", __func__);

	ret = readDTB2FRCPrivate(pctx_video, pctx_frc);

	if (ret != 0) {
		DRM_ERROR("[%s, %d] Read DTB failed.\n",
			__func__, __LINE__);
	}
	DRM_INFO("\nfrc_ip_version=%x\n", pctx_frc->frc_ip_version);

	ret = drv_hwreg_render_frc_ipversion(pctx_frc->frc_ip_version);

	if (ret != 0) {
		DRM_ERROR("[%s, %d] Set frc ipversion failed.\n",
			__func__, __LINE__);
	}

	ret = readDTB2FRCSmiPrivate(pctx_frc);
	if (ret != 0) {
		DRM_ERROR("[%s, %d] Read DTB smi failed.\n",
			__func__, __LINE__);
	}

	//init RV55;
	#if (0)
		_mtk_video_display_set_init_value_for_rv55();
	#else
		memc_init_value_for_rv55 = INIT_VALUE_PASS;
	#endif

	if (bIsnode_1stInit == TRUE) {
		_mtk_video_frc_init_reg_bank_node_init(pctx);
		bIsnode_1stInit = FALSE;
	}
	//hal_hwreg would !(true) = 0 to set Rst, patch for Machili
	//8429_01[0] = 0 is Rst, 8429_01[0] = 1 is not Rst
	_mtk_video_frc_sw_reset(pctx, true);

	_mtk_video_frc_unused_MaskEn(pctx);

	//init clock
	if ((pctx_frc->frc_ip_version == FRC_IP_VERSION_4) ||
		(pctx_frc->frc_ip_version == FRC_IP_VERSION_5)) {
		ret = mtk_video_display_frc_init_clock_v4(dev);
	} else if (pctx_frc->frc_ip_version == FRC_IP_VERSION_6) {
		ret = mtk_video_display_frc_init_clock_v6(dev);
	} else if (pctx_frc->frc_ip_version == FRC_IP_VERSION_2) {
		ret = mtk_video_display_frc_init_clock_v2(dev);
	} else if (pctx_frc->frc_ip_version == FRC_IP_VERSION_1_E3) {
		ret = mtk_video_display_frc_init_clock_E3(dev);
	} else {
		ret = mtk_video_display_frc_init_clock(dev);
	}

	if (ret != 0) {
		DRM_ERROR("[%s, %d] Frc_init_clock failed.\n",
			__func__, __LINE__);
	}

	_mtk_video_display_set_frc_clock(pctx_video, true);

	ret = _mtk_video_display_set_frc_smi_mask(pctx_frc, false);
	if (ret != 0) {
		DRM_ERROR("[%s, %d] set_frc_smi_masks failed.\n",
			__func__, __LINE__);
	}

	pctx_frc->bIsmemc_1stInit = true;
	for (i = 0; i < MTK_VPLANE_TYPE_MAX; i++) {
		pctx_frc->bIspre_memc_en_Status[i] = false;
		pctx_video->video_plane_type[i] = MTK_VPLANE_TYPE_NONE;
	}

	pctx_frc->memc_InitState = MEMC_INIT_CHECKRV55;
	pctx_frc->frc_latency = 0;
	pctx_frc->frc_rwdiff = pctx_frc->frc_dts_rwdiff;
	pctx_frc->frc_user_rwdiff = 0;
	pctx_frc->frc_pattern_sel = MEMC_PAT_OFF;
	pctx_frc->frc_pattern_trig_cnt = 0;
	pctx_frc->frc_pattern_enable = false;
	pctx_frc->u64frcpts = 0;
	pctx_frc->frc_frame_count = 0;
	pctx_frc->video_memc_mode = DEFAULT_VALUE;
	pctx_frc->video_frc_mode = DEFAULT_VALUE;
	pctx_frc->frcopmMaskEnable = true;
	pctx_frc->bSkipGameMemc = 0;
	Gaming_MJC_Lvl = 0;

	_mtk_video_frc_init_reg_1(pctx);
	_mtk_video_frc_init_reg_2(pctx);
	_mtk_video_frc_init_reg(pctx);

	DRM_INFO("[PROFILE] %s-\n", __func__);
}

void mtk_video_display_frc_suspend(struct device *dev, struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	int ret = 0;

	DRM_INFO("[PROFILE] %s+\n", __func__);

	ret = _mtk_video_display_set_frc_smi_mask(pctx_frc, true);
	if (ret != 0) {
		DRM_ERROR("[%s, %d] set_frc_smi_masks failed.\n",
			__func__, __LINE__);
	}

	if ((pctx_frc->frc_ip_version == FRC_IP_VERSION_4) ||
		(pctx_frc->frc_ip_version == FRC_IP_VERSION_5))
		ret = mtk_video_display_frc_disable_clock_v4(dev);
	else if (pctx_frc->frc_ip_version == FRC_IP_VERSION_6)
		ret = mtk_video_display_frc_disable_clock_v6(dev);
	else if (pctx_frc->frc_ip_version == FRC_IP_VERSION_2)
		ret = mtk_video_display_frc_disable_clock_v2(dev);
	else if (pctx_frc->frc_ip_version == FRC_IP_VERSION_1_E3)
		ret = mtk_video_display_frc_disable_clock_E3(dev);
	else
		ret = mtk_video_display_frc_disable_clock(dev);

	if (ret != 0) {
		DRM_ERROR("[%s, %d] frc disable clock failed.\n",
			__func__, __LINE__);
	}


	_mtk_video_display_set_frc_clock(pctx_video, false);

	if (_vaddr_vgsync != NULL)
		iounmap(_vaddr_vgsync);

	if (_vaddr_frc != NULL)
		iounmap(_vaddr_frc);

	DRM_INFO("[PROFILE] %s-\n", __func__);
}

void mtk_video_display_frc_resume(
	struct device *dev,
	struct mtk_tv_kms_context *pctx)
{
	DRM_INFO("[PROFILE] %s+\n", __func__);

	_mtk_video_display_frc_init(dev, pctx);

	DRM_INFO("[PROFILE] %s-\n", __func__);
}

void mtk_video_display_frc_init(
	struct device *dev,
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_tv_kms_vsync_callback_param vsync_callback_param;

	DRM_INFO("[PROFILE] %s+\n", __func__);

	_mtk_video_display_frc_init(dev, pctx);

	memset(&vsync_callback_param, 0, sizeof(struct mtk_tv_kms_vsync_callback_param));
	vsync_callback_param.thread_name		= "frc_vsync_thread";
	vsync_callback_param.mi_rt_domain_name	= NULL;
	vsync_callback_param.mi_rt_class_name	= NULL;
	vsync_callback_param.priv_data			= pctx;
	vsync_callback_param.callback_func		= mtk_video_display_frc_vsync_callback;
	mtk_tv_kms_register_vsync_callback(&vsync_callback_param);

	DRM_INFO("[PROFILE] %s-\n", __func__);
}

//set MEMC_OPM path
void mtk_video_display_set_frc_opm_path(struct mtk_plane_state *state,
						 bool enable)
{
	struct drm_plane *plane = state->base.plane;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	unsigned int plane_inx = mplane->video_index;
	//struct mtk_video_plane_ctx *plane_ctx = (pctx_video->plane_ctx + plane_inx);
	//struct drm_framebuffer *fb = state->base.fb;
	static unsigned int drm_vcnt;

	drm_vcnt++;

	if (connect_frc_rv55_is_ok()) {
		if (pctx_frc->memc_InitState == MEMC_INIT_CHECKRV55) {
			pctx_frc->memc_InitState = MEMC_INIT_SENDDONE;
			_mtk_video_display_set_init_value_for_rv55(pctx);
			DRM_INFO("[%s][%d] memc_InitState = MEMC_INIT_SENDDONE vcnt:%d\n",
				__func__, __LINE__, drm_vcnt);
		}
	}

	//set info for rv55
	if (enable && (pctx_frc->memc_InitState != MEMC_INIT_CHECKRV55)) {
		//set info for rv55
		//Parsing_json_file
		_mtk_video_display_parsing_json_file(plane_inx, pctx);

		//set memc input size
		_mtk_video_display_set_input_frame_size(plane_inx, pctx, state);

		//set memc output size
		_mtk_video_display_set_output_frame_size(plane_inx, pctx, state);

		//set timing
		_mtk_video_display_set_timing(plane_inx, pctx);
		#if (0) //Remove to PQ DD
		// set pts
		_mtk_video_display_frc_set_rv55_pts(pctx);
		#endif
		//set lpll lock done
		_mtk_video_display_set_fpll_lock_done(pctx);

		//Qos
		_mtk_video_display_update_Qos(pctx);

		//set memc mode (FRC mode)
		_mtk_video_display_set_frc_mode(plane_inx, pctx, state);

		//set memc level
		_mtk_video_display_set_mfc_mode_cmd(plane_inx, pctx);

		//set demo mode
		_mtk_video_display_set_demo_mode(pctx, DEMOMode);

		_mtk_video_display_get_mfc_alg_status(pctx);

		_mtk_video_display_frc_vs_trig_ctl(pctx);

		_mtk_video_display_set_game_mode(plane_inx, pctx);

		// set photo mode
		_mtk_video_display_set_photo_mode(pctx);

		_mtk_video_display_set_bypass_mfc_mode(pctx);

		_mtk_video_display_set_real_cinema_mode(pctx, RealCinemaMode);

		_mtk_video_display_set_cadence(pctx, Cadence_cmd);
	}

	_mtk_video_frc_get_latency(plane_inx, pctx, enable);

	//the following setting is only 1 time for MEMC status change
	if ((pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) && (enable == TRUE)) {
		//If STR Case MEMC_INIT_SENDDONE is slow, souldn't return
	} else {
		if (pctx_frc->bIspre_memc_en_Status[plane_inx] == enable)
			return;
	}


	DRM_INFO("[%s][%d] plane_inx(window):%d enable:%u vcnt:%d\n",
		__func__, __LINE__, plane_inx, enable, drm_vcnt);

	if (enable) {

		if (pctx_frc->bIsmemc_1stInit) {
			pctx_frc->bIsmemc_1stInit = false;
			_mtk_video_frc_init_reg_bank_node_loop();
		}

		_mtk_video_frc_preultra_ultra(pctx, enable);
		if (pctx_frc->memc_InitState == MEMC_INIT_SENDDONE) {
			_mtk_video_display_set_init_done_for_rv55();
			pctx_frc->memc_InitState = MEMC_INIt_FINISH;
			DRM_INFO("[%s][%d] memc_InitState(1st Time) = MEMC_INIt_FINISH vcnt:%d\n",
				__func__, __LINE__, drm_vcnt);
		} else {
			if (pctx_frc->memc_InitState == MEMC_INIt_FINISH) {
				_mtk_video_display_set_init_done_for_rv55();
				DRM_INFO("[%s][%d] memc_InitState = MEMC_INIt_FINISH vcnt:%d\n",
					__func__, __LINE__, drm_vcnt);
			}
		}
	} else {
	}

	pctx_frc->bIspre_memc_en_Status[plane_inx] = enable;
}

void mtk_video_display_set_frc_property(
	struct video_plane_prop *plane_props,
	enum ext_video_plane_prop prop)
{
	switch (prop) {
	case EXT_VPLANE_PROP_MEMC_LEVEL:
		break;
	case EXT_VPLANE_PROP_MEMC_GAME_MODE:
		break;
	case EXT_VPLANE_PROP_MEMC_PATTERN:
		break;
	case EXT_VPLANE_PROP_MEMC_MISC_TYPE:
		break;
	case EXT_VPLANE_PROP_MEMC_DECODE_IDX_DIFF:
		break;
	default:
		break;
	}

	DRM_INFO("[%s][%d] memc ext_prop:[%d]val= %llx old_val= %llx\n", __func__, __LINE__,
		prop, plane_props->prop_val[prop], plane_props->old_prop_val[prop]);

}

void mtk_video_display_get_frc_property(
	struct video_plane_prop *plane_props,
	enum ext_video_plane_prop prop)
{
	switch (prop) {
	case EXT_VPLANE_PROP_MEMC_STATUS:
		break;
	case EXT_VPLANE_PROP_MEMC_TRIG_GEN:
		plane_props->prop_val[prop] = drv_hwreg_render_frc_get_trig_gen();
		break;
	case EXT_VPLANE_PROP_MEMC_RV55_INFO:
		plane_props->prop_val[prop] = memc_rv55_info;
		memc_rv55_info = (memc_rv55_info&
			(~(MEMC_STATUS|DEMO_STATUS|MEMC_LV_STATUS|CMD_STATUS
			|GAMING_LV_STATUS|DEJUDDER_STATUS|DEBLUR_STATUS|MEMC_USER_STATUS
			|MEMC_BYPASS_STATUS|REALCINEMA_STATUS|CADENCE_STATUS
			|CADENCE_D1_STATUS|CADENCE_D2_STATUS|CADENCE_D3_STATUS)));
		break;
	case EXT_VPLANE_PROP_MEMC_INIT_VALUE_FOR_RV55:
		plane_props->prop_val[prop] = memc_init_value_for_rv55;
		break;
	default:
		break;
	}

	DRM_INFO("[%s][%d] memc ext_prop:[%d]val= %llu old_val= %llu\n", __func__, __LINE__,
		prop, plane_props->prop_val[prop], plane_props->old_prop_val[prop]);

}

#define _PAT_TRIG_CNT 10
void mtk_video_display_frc_set_pattern(
		struct mtk_tv_kms_context *pctx,
		enum mtk_video_frc_pattern sel, bool enable)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	pctx_frc->frc_pattern_sel = sel;
	pctx_frc->frc_pattern_enable = enable;

	switch (pctx_frc->frc_pattern_sel) {

	case MEMC_PAT_OPM:
		_mtk_video_display_frc_set_opm_pattern(pctx, enable, FALSE);
		break;
	case MEMC_PAT_IPM:
		_mtk_video_display_frc_set_ipm_pattern(pctx, enable, FALSE);
		break;
	case MEMC_PAT_IPM_DYNAMIC: // Force Timing Onle for Debug
		_mtk_video_display_frc_set_ipm_pattern(pctx, enable, TRUE);
		break;
	case MEMC_PAT_IPM_STATIC: // Force Timing Onle for Debug
		_mtk_video_display_frc_set_ipm_pattern(pctx, enable, TRUE);
		pctx_frc->frc_pattern_trig_cnt = _PAT_TRIG_CNT;
		break;
	case MEMC_PAT_IPM_FORCE_TIMING:
		_mtk_video_display_frc_set_ipm_pattern(pctx, enable, TRUE);
		break;
	default:
		break;
	}
}

void mtk_video_display_set_frc_freeze(
		struct mtk_tv_kms_context *pctx,
		unsigned int plane_inx,
		bool enable)
{
	struct hwregFrcFreezeIn paramFrcFreezeIn;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	struct hwregOut paramOut;

	uint16_t reg_num = pctx_video->reg_num;
	struct mtk_video_plane_ctx *plane_ctx = (pctx_video->plane_ctx + plane_inx);

	DRM_INFO("[PROFILE] %s+\n", __func__);
	DRM_INFO("[FRC][%s][%d]freeze enable: %d\n", __func__, __LINE__, enable);

	plane_ctx->memc_freeze = enable;
	if (!pctx_frc->frc_as_hse) {
		memset(&paramFrcFreezeIn, 0, sizeof(struct hwregFrcFreezeIn));
		memset(&paramOut, 0, sizeof(struct hwregOut));

		paramOut.reg = malloc(sizeof(struct reg_info) * reg_num);
		if (paramOut.reg)
			memset(paramOut.reg, 0, sizeof(struct reg_info)*reg_num);
		else {
			DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
			return;
		}

		paramFrcFreezeIn.RIU = 1;
		paramFrcFreezeIn.bEnable = enable;

		drv_hwreg_render_frc_set_freeze(
			paramFrcFreezeIn, &paramOut);

		if (paramOut.reg)
			free(paramOut.reg);
	} else {
		struct msg_render_frc_hse_freeze msg_info;

		memset(&msg_info, 0, sizeof(msg_info));
		msg_info.enable = (uint32_t)enable;
		pqu_msg_send(PQU_MSG_SEND_RENDER_FRC_HSE_FREEZE, &msg_info);
	}

	DRM_INFO("[PROFILE] %s-\n", __func__);
}

bool mtk_video_display_check_frc_freeze(
		bool memc_enable,
		bool memc_freeze)
{
	// The MEMC should be unfrozen if there is no MEMC plane exist currently.
	DRM_INFO("[FRC][%s][%d]MEMC_enable: %d, MEMC_freeze: %d.\n",
		__func__, __LINE__, memc_enable, memc_freeze);
	return ((!memc_enable) && memc_freeze) ? false : true;
}

E_DRV_MFC_UNMASK mtk_video_frc_get_unmask(struct mtk_video_context *pctx_video)
{
	struct hwregOut paramOut;
	uint16_t reg_num = pctx_video->reg_num;
	struct hwregFrcUnmaskIn FrcPamaIn;

	memset(&FrcPamaIn, 0, sizeof(struct hwregFrcUnmaskIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = malloc(sizeof(struct reg_info) * reg_num);
	if (paramOut.reg)
		memset(paramOut.reg, 0, sizeof(struct reg_info)*reg_num);
	else {
		DRM_ERROR("\n[%s,%d]malloc fail", __func__, __LINE__);
		return E_MFC_UNMASK_OFF;
	}

	FrcPamaIn.RIU = 1;

	drv_hwreg_render_frc_get_unmask(&FrcPamaIn,  &paramOut);

	if (paramOut.reg)
		free(paramOut.reg);

	return FrcPamaIn.unmask;
}

char *___strtok;

char *strtok_frc(char *s, const char *ct)
{
	char *sbegin, *send;

	sbegin  = s ? s : ___strtok;
	if (!sbegin)
		return NULL;

	sbegin += strspn(sbegin, ct);
	if (*sbegin == '\0') {
		___strtok = NULL;
		return NULL;
	}
	send = strpbrk(sbegin, ct);
	if (send && *send != '\0')
		*send++ = '\0';
	___strtok = send;
	return sbegin;
}


int mtk_frc_get_all_value_from_string(char *buf, char *delim,
	unsigned int len, uint16_t *value, uint16_t *TokenCnt)
{
	char *token;
	char *endptr;
	uint16_t Cnt;

	if ((buf == NULL) || (delim == NULL) || (value == NULL)) {
		DRM_ERROR("[%s, %d]Pointer is NULL!\n", __func__, __LINE__);
		return -1;
	}

	token = strtok_frc(buf, (const char *)delim);
	Cnt = 0;
	while (token != NULL) {
		//printf( "\n%s", token );
		*value = strtoul(token, &endptr, 0);
		//printf("\nval=%d",*value);
		value++;
		Cnt++;

		token = strtok_frc(0, (const char *)delim);
	}
	*TokenCnt = Cnt;
	return 0;
}

static void _mtk_video_display_frc_set_rv55_extra_delay_num(
	struct mtk_tv_kms_context *pctx, uint8_t real_extra_latency_cnt)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	if (pctx_frc) {
		struct mtk_frc_shm_info *frc_shm_info =
			(struct mtk_frc_shm_info *) pctx_frc->frc_shm_virAdr;

		if (frc_shm_info != NULL) {
			frc_shm_info->extra_delay_num = real_extra_latency_cnt;
			DRM_INFO("[%s,%d]extra_delay_num = %d.\n",
				__func__, __LINE__, frc_shm_info->extra_delay_num);
		}
	}
}

static uint8_t mtk_video_display_frc_get_FramedelayCnt(uint32_t input_freq,
	uint32_t output_freq, uint8_t extra_latency_diff)
{
	struct mtk_frc_timinginfo_tbl *pinfotable = NULL;
	uint8_t u8latency = 0;
	uint8_t u8FrameDelayCntx10 = 0;

	pinfotable = _mtk_video_frc_find_in_timing_mappinginfo(output_freq,
				(input_freq*VFRQRATIO_1000));

	if (pinfotable)
		u8latency = pinfotable->frc_memc_latency;

	u8latency = _HEX2DEC(u8latency);
	u8FrameDelayCntx10 = (extra_latency_diff*LATENCYRATIO_10)+u8latency;
	return u8FrameDelayCntx10;
}

static inline struct mtk_drm_plane *_mtk_video_display_frc_get_mtk_drm_plane(
	struct mtk_tv_kms_context *pctx)
{
	struct ext_crtc_prop_info *crtc_props = NULL;
	uint64_t plane_id = 0;
	struct drm_mode_object *obj = NULL;
	struct drm_plane *plane = NULL;

	if (pctx == NULL)
		return NULL;

	crtc_props = pctx->ext_crtc_properties;
	if (crtc_props == NULL)
		return NULL;

	plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];
	obj = drm_mode_object_find(pctx->drm_dev, NULL, plane_id, DRM_MODE_OBJECT_ANY);
	if (obj == NULL)
		return NULL;

	plane = obj_to_plane(obj);
	if (plane == NULL)
		return NULL;

	return to_mtk_plane(plane);
}

static inline struct mtk_video_context *_mtk_video_display_frc_get_video_context(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_drm_plane *mplane = NULL;
	struct mtk_tv_kms_context *pctx_plane = NULL;

	if (pctx == NULL)
		return NULL;

	mplane = _mtk_video_display_frc_get_mtk_drm_plane(pctx);
	if (mplane == NULL)
		return NULL;

	pctx_plane = (struct mtk_tv_kms_context *)mplane->crtc_private;
	if (pctx_plane == NULL)
		return NULL;

	return &pctx_plane->video_priv;

}

int mtk_video_display_frc_set_extra_latency(
	struct mtk_tv_kms_context *pctx,
	struct drm_mtk_tv_video_extra_latency *latency)
{
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_drm_plane *mplane = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_frc_context *pctx_frc = NULL;
	struct video_plane_prop *plane_props = NULL;
	unsigned int plane_inx = 0;
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	uint8_t roundUpFrame = 0;
	uint8_t roundDownFrame = 0;
	uint32_t deltaTimeDiffUp = 0;
	uint32_t deltaTimeDiffDown = 0;
	uint32_t durationTime_msx1000 = 0;
	uint32_t input_freq = 0;
	uint64_t input_vfreq = 0;
	uint32_t output_freq = 0;
	uint32_t latency_msx1000 = 0;
	uint8_t extra_latency_diff = 0;
	uint8_t FramedelayCntx10 = 0;
	uint8_t FramedelayMaxCntX10 = 0;
	uint8_t real_extra_latency_cnt = 0;
	uint32_t real_latencyx1000 = 0;

	mplane = _mtk_video_display_frc_get_mtk_drm_plane(pctx);
	pctx_video = _mtk_video_display_frc_get_video_context(pctx);

	if ((pctx == NULL) || (latency == NULL) || (mplane == NULL)
		|| (pctx_video == NULL)) {
		DRM_ERROR("[%s, %d]Pointer is NULL!\n", __func__, __LINE__);
		return -ENODEV;
	}

	pctx_pnl = &pctx->panel_priv;
	plane_props = pctx_video->video_plane_properties;
	plane_inx = mplane->video_index;
	plane_ctx = pctx_video->plane_ctx + plane_inx;
	pctx_frc = &pctx->frc_priv;

	if ((pctx_pnl == NULL) || (pctx_frc == NULL) || (plane_ctx == NULL)
		|| (plane_props == NULL)) {
		DRM_ERROR("[%s, %d]Pointer is NULL!\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (pctx->stub)
		plane_ctx->framesyncInfo.input_source = META_PQ_INPUTSRC_HDMI_DIS;

	// only non vdec source, memc path, max count> frame count can support video extra latency
	if ((IS_INPUT_B2R(plane_ctx->framesyncInfo.input_source)) ||
		(latency->planetype != MTK_VPLANE_TYPE_MEMC) ||
		(pctx_frc->frc_max_frame_cnt == pctx_frc->frc_frame_cnt)) {
		latency->real_video_latency_ms = 0;
		return 0;
	}

	//transfer to frame number
	input_vfreq = plane_props->prop_val[EXT_VPLANE_PROP_INPUT_VFREQ];
	input_freq  = (uint32_t) DIV64_U64_ROUND_CLOSEST(input_vfreq, VFRQRATIO_1000);
	output_freq = DIV_ROUND_CLOSEST(pctx_pnl->outputTiming_info.u32OutputVfreq, VFRQRATIO_1000);

	if (input_freq == 0) {
		latency->real_video_latency_ms = 0;
		return 0;
	}

	durationTime_msx1000 = DIV_ROUND_CLOSEST(_1_SECOND, input_freq);
	latency_msx1000 = latency->video_latency_ms*LATENCYRATIO_1000;

	roundUpFrame = DIV_ROUND_UP(latency_msx1000, durationTime_msx1000);
	roundDownFrame = (latency_msx1000/durationTime_msx1000);

	deltaTimeDiffUp = abs(latency_msx1000 - roundUpFrame*durationTime_msx1000);
	deltaTimeDiffDown = abs(latency_msx1000 - roundDownFrame*durationTime_msx1000);

	if (pctx_video)
		extra_latency_diff = pctx_frc->frc_max_frame_cnt - pctx_frc->frc_frame_cnt;

	// get HW max support frame
	FramedelayCntx10 = mtk_video_display_frc_get_FramedelayCnt(input_freq, output_freq, 0);
	FramedelayMaxCntX10 = mtk_video_display_frc_get_FramedelayCnt(input_freq,
							output_freq, extra_latency_diff);

	if (deltaTimeDiffUp < deltaTimeDiffDown) {
		if (((roundUpFrame*LATENCYRATIO_10)+FramedelayCntx10) > FramedelayMaxCntX10) {
			real_latencyx1000 = extra_latency_diff*durationTime_msx1000;
			real_extra_latency_cnt = extra_latency_diff;
		} else {
			real_latencyx1000 = roundUpFrame*durationTime_msx1000;
			real_extra_latency_cnt = roundUpFrame;
		}
	} else {
		if (((roundDownFrame*LATENCYRATIO_10)+FramedelayCntx10) > FramedelayMaxCntX10) {
			real_latencyx1000 = extra_latency_diff*durationTime_msx1000;
			real_extra_latency_cnt = extra_latency_diff;
		} else {
			real_latencyx1000 = roundDownFrame*durationTime_msx1000;
			real_extra_latency_cnt = roundDownFrame;
		}
	}

	latency->real_video_latency_ms = DIV_ROUND_CLOSEST(real_latencyx1000, LATENCYRATIO_1000);

	//write extra delay number to share memory
	_mtk_video_display_frc_set_rv55_extra_delay_num(pctx, real_extra_latency_cnt);

	DRM_INFO("[%s][%d]extra latency:%d, real extra latency:%d, input_freq:%d, output_freq:%d\n",
		__func__, __LINE__, latency->video_latency_ms, latency->real_video_latency_ms,
		input_freq, output_freq);

	return 0;
}

static int _mtk_video_display_frc_check_ipm_hv_size(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	uint16_t IpmSrcHSize = 0;
	uint16_t IpmSrcVSize = 0;
	int ret = 0;

	if ((pctx_frc->frc_pattern_sel == MEMC_PAT_IPM_FORCE_TIMING) &&
		(pctx_frc->frc_pattern_enable == TRUE)) {
		ret = _mtk_video_display_frc_get_ipm_hv_size(&IpmSrcHSize, &IpmSrcVSize);
		if (ret != 0) {
			DRM_ERROR("[%s, %d]frc get IPM H/V size failed!\n", __func__, __LINE__);
		} else {
			if ((IpmSrcHSize != pctx_frc->frc_ipm_hsize) ||
				(IpmSrcVSize != pctx_frc->frc_ipm_vsize)) {
				_mtk_video_display_frc_set_ipm_pattern(pctx,
					pctx_frc->frc_pattern_enable, TRUE);
				pctx_frc->frc_ipm_hsize = IpmSrcHSize;
				pctx_frc->frc_ipm_vsize = IpmSrcVSize;
			}
		}
	}

	return 0;
}

void _mtk_video_display_frc_vgsync_debug_log(struct mtk_tv_kms_context *pctx)
{
	struct hwregFrcVgsyncDebug paramIn;
	struct mtk_frc_vgsync_buf_info *pstVgsyncBufInfo;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;

	if (_vaddr_vgsync == NULL)
		return;

	if ((pctx_frc->frc_ip_version == FRC_IP_VERSION_2) ||
		(pctx_frc->frc_ip_version == FRC_IP_VERSION_1_E3)) {
		pstVgsyncBufInfo = (struct mtk_frc_vgsync_buf_info *)_vaddr_vgsync;

		memset(&paramIn, 0, sizeof(struct hwregFrcVgsyncDebug));
		drv_hwreg_render_frc_get_vgsync_debug_info(&paramIn);

		if (paramIn.VgsyncStatus != _VG_SYNC_OFF) {
			DRM_INFO("[%s]%s:%d,FI:%x,FO:%x,Op2:%x,Mg:%x,IP:%3x,Opf:%3x,Opb:%3x\n",
				paramIn.VgsyncStatus != _VG_SYNC_ON ?
				(_VG_SYNC_BYPASS_BY_DS & paramIn.VgsyncStatus ?
				_VG_SYNC_OFF_LATE :
				_VG_SYNC_OFF_EARLY) :
				"VG sync on.",
				_VG_SYNC_FRAME_INDEX_FROM_SHM,
				pstVgsyncBufInfo->u8CurBufIdx,
				paramIn.FrcIpmIdx,
				paramIn.FrcOpmIdx,
				paramIn.DsOp2Idx,
				paramIn.DsMg01Idx,
				paramIn.DsIpIdx,
				paramIn.DsOpfIdx,
				paramIn.DsOpbIdx);
		}
	}
}

static int mtk_video_display_frc_vsync_callback(void *priv_data)
{
	uint8_t IpmWriteIdx = 0;
	uint8_t OpmF2ReadIdx = 0;
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)priv_data;

	_mtk_video_display_frc_vgsync_debug_log(pctx);
	_mtk_video_display_frc_check_ipm_hv_size(pctx);
	_mtk_video_display_frc_get_wr_idx(&IpmWriteIdx, &OpmF2ReadIdx);
	mtk_drm_atrace_int("VBLANK:frc_write_idx", IpmWriteIdx);
	mtk_drm_atrace_int("VBLANK:frc_read_F2_idx", OpmF2ReadIdx);

	return 0;
}

int mtk_video_display_frc_get_algo_version(uint32_t *pVer)
{
	int ret = 0;
	struct hwregFrcDebugInfo stFrcDebugInfo = {0};

	if (pVer == NULL) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	memset(&stFrcDebugInfo, 0, sizeof(struct hwregFrcDebugInfo));

	drv_hwreg_render_frc_get_debug_info(&stFrcDebugInfo);

	*pVer = stFrcDebugInfo.Rv55Ver;

	return ret;
}

int mtk_video_display_frc_get_algo_info(struct device *dev, char *buf)
{
	#define MFC_LEVL_MASK(X) ((X) & (0x000F))
	#define MFC_DEBLUR_MASK(X) (((X) & (0x0F00))>>8)
	#define MFC_DEJUDDER_MASK(X) (((X) & (0xF000))>>12)
	#define MFC_GAME_MASK(X) (((X) & (0x0800))>>11)
	#define MFC_GAME_MEMC_MASK(X) (((X) & (0x4000))>>14)

	int ret = 0;
	struct hwregFrcDebugInfo stFrcDebugInfo = {0};
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_frc_context *pctx_frc = NULL;

	if (buf == NULL) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	memset(&stFrcDebugInfo, 0, sizeof(struct hwregFrcDebugInfo));

	pctx = dev_get_drvdata(dev);
	pctx_frc = &pctx->frc_priv;

	drv_hwreg_render_frc_get_debug_info(&stFrcDebugInfo);

	ret = snprintf(buf, PAGE_SIZE, "		FRC algo status:\n\n"
				"		****  frc_rv55/r2_status ****\n"
				"		- RV55/R2	Ver = %X\n"
				"		- MFC		Level = %d\n"
				"		- MFC		Deblur = %d\n"
				"		- MFC		Dejudder = %d\n"
				"		- MFC		SW Scene = %s (%d)\n"
				"		- MFC		Game = %d\n"
				"		- MFC		GameMemc = %d\n"
				"		- MFC		In/Out Timing = %X\n"
				"		- MFC		In/Out Panel Timing = %X\n",
				stFrcDebugInfo.Rv55Ver,
				MFC_LEVL_MASK(stFrcDebugInfo.MfcLevel),
				MFC_DEBLUR_MASK(stFrcDebugInfo.MfcUserLevel),
				MFC_DEJUDDER_MASK(stFrcDebugInfo.MfcUserLevel),
				SenceToString(pctx_frc->frc_scene), pctx_frc->frc_scene,
				MFC_GAME_MASK(stFrcDebugInfo.MfcBWAndGameMode),
				MFC_GAME_MEMC_MASK(stFrcDebugInfo.MfcBWAndGameMode),
				stFrcDebugInfo.MfcIOTiming,
				stFrcDebugInfo.MfcIOPanelTiming
	);

	#undef MFC_LEVL_MASK
	#undef MFC_DEJUDDER_MASK
	#undef MFC_DEBLUR_MASK
	#undef MFC_GAME_MASK
	#undef MFC_GAME_MEMC_MASK

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

void mtk_video_display_frc_set_reset_aid(
	struct mtk_tv_kms_context *pctx)
{
	enum drm_mtk_video_plane_type video_plane_type = 0;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_FRC_AID];
	struct hwregFrcResetAid paramIn;
	struct hwregOut paramOut;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(paramIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;
	paramIn.RIU = 0;

	pctx = pctx_video_to_kms(pctx_video);
	video_plane_type = MTK_VPLANE_TYPE_MEMC;

	if (pctx_video->plane_num[video_plane_type] == 0) {
		drv_hwreg_render_frc_set_reset_aid(
			paramIn, &paramOut);
	}

	if (!paramIn.RIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

