// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek tcon common data tye and function
 *
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/time.h>

#include "mtk_tcon_common.h"
#include "hwreg_render_video_tcon.h"
#include "hwreg_render_video_tcon_od.h"
#include "hwreg_render_video_tcon_dga.h"
#include "mtk_tv_drm_video_panel.h"

//log level setting
EN_TCON_LOG_LEVEL g_enTconLogLevel = DEFAULT_TCON_LOG_LEVEL;

#define TCON_LOG_ERR                "ERROR"
#define TCON_LOG_INFO               "INFO"
#define TCON_LOG_DEBUG              "DEBUG"

/* krenel default serch path: /vendor/firmware */
#define FILE_FIXED_FOLDER           ""
#define TCON_FILE_PATH              "4k60/TCON20.bin"
#define TCON_HFR_FILE_PATH          "4k120/TCON20_HFR.bin"
#define TCON_HPC_FILE_PATH          "4k120/TCON20_HPC.bin"
#define TCON_GAME_FILE_PATH         "4k120/TCON20_GAME.bin"
#define TCON_POWER_ON_FILE_PATH     "4k120/TCON20_POST.bin"
#define TCON_POWER_OFF_FILE_PATH    "4k120/TCON20_PRE.bin"
#define PANELGAMMA_FILE_PATH        "4k60/gamma.bin"
#define FILE_PATH_LENGTH            (128)

//#define for TCON.BIN
#define TCON20_VERSION              (2)
#define SKIP_TCON_SUBNUM            (34)
#define SKIP_TCON_MAIMHEADER        (32)
#define SKIP_TCON_SUBHEADER         (8)
#define SKIP_TCON20_SUBHEADER       (40)
#define SKIP_TCON20_PANETYPE        (2)

#define TCON_REGISTERTYPE_1_BYTES   1
#define TCON_REGISTERTYPE_6_BYTES   6
#define TCON_REGISTERTYPE_4_BYTES   4
#define TCON_REGISTERTYPE_POWERSEQENCE_9_BYTES   9
#define TCON_REGISTERTYPE_POWERSEQENCE_7_BYTES   7
#define TCON_REGISTERTYPE_POWERSEQENCE_WITHSUB_15_BYTES   15
#define TCON_REGISTERTYPE_POWERSEQENCE_WITHSUB_11_BYTES   11
#define TCON_REGISTERTYPE_POWERSEQENCE_WITHSUB_14_BYTES   14

//pointer to the current to tcon bin data
loff_t g_tcon_data_size;
unsigned char *g_tcon_data_buf;

//pointer to the current to tcon bin data
loff_t g_gamma_data_size;
unsigned char *g_gamma_data_buf;

/* 0: store default tcon bin data
 * 1: store higt frame rate tcon bin data
 * 2: store higt pixel clock tcon bin data
 * 3: store game tcon bin data
 */
static unsigned char *g_tcon_mode_buf[EN_TCON_MODE_MAX];
static loff_t g_tcon_mode_buf_size[EN_TCON_MODE_MAX];

/* store default gamma data */
static loff_t g_pnlgamma_data_size[EN_TCON_MODE_MAX];
static unsigned char *g_pnlgamma_data_buf[EN_TCON_MODE_MAX];

/* pointer to the tcon power seq. on/off bin data */
loff_t g_tcon_power_on_data_size;
unsigned char *g_tcon_power_on_data_buf;
loff_t g_tcon_power_off_data_size;
unsigned char *g_tcon_power_off_data_buf;

EN_TCON_MODE g_current_mode = EN_TCON_MODE_DEFAULT;

loff_t g_vrr_tcon_data_size[VRR_OD_TABLE_NUM_MAX];
unsigned char *g_vrr_tcon_data_buf[VRR_OD_TABLE_NUM_MAX];
loff_t g_vrr_pnlgamma_data_size[VRR_OD_TABLE_NUM_MAX];
unsigned char *g_vrr_pnlgamma_data_buf[VRR_OD_TABLE_NUM_MAX];

enum en_adl_client_type {
	E_ADL_CLIENT_PANEL_GAMMA_0,
	E_ADL_CLIENT_PCID,
	E_ADL_CLIENT_OD,
	E_ADL_CLIENT_VAC,
	E_ADL_CLIENT_MAX
};

struct st_adl_check_info {
	bool bCheckStatus;
	int fd;
	__u8 u8MemIndex;
};

struct st_adl_check_info g_adl_info[E_ADL_CLIENT_MAX] = {0};
bool g_bAdlChecking = FALSE;
static struct task_struct *g_wait_worker;

// TCon register type
typedef enum {
	// 32-bit address, 4+1+1, address+mask+value
	EN_TCON20_REGISTERTYPE_6_BYTES = 0,
	// 16-bit address, 2+1+1, address+mask+value
	EN_TCON20_REGISTERTYPE_4_BYTES,
	// 32-bit address, 4+1+1+1+1+1, address+mask+value+delayReady+delayTime+signalType
	EN_TCON20_REGISTERTYPE_POWERSEQENCE_9_BYTES,
	// 16-bit address, 2+1+1+1+1+1, address+mask+value+delayReady+delayTime+signalType
	EN_TCON20_REGISTERTYPE_POWERSEQENCE_7_BYTES,
	// 32-bit address, 4+1+1+4+1+1+1+1+1,
	// subAddress+subMask+subValue+address+mask+value+delayReady+delayTime+signalType
	EN_TCON20_REGISTERTYPE_POWERSEQENCE_WITHSUB_15_BYTES,
	// 16-bit address, 2+1+1+2+1+1+1+1+1,
	// subAddress+subMask+subValue+address+mask+value+delayReady+delayTime+signalType
	EN_TCON20_REGISTERTYPE_POWERSEQENCE_WITHSUB_11_BYTES,
	// Register type 6: P-Gamma (Power Gamma)
	// 4-byte register format (16-bit address size and 16-bit value size)
	EN_TCON20_REGISTERTYPE_POWERGAMMA_4_BYTES,
	// Register type 7: TCON 3-byte register format (12-bit address size)
	EN_TCON20_REGISTERTYPE_3_BYTES,
	// Register type 8: 1-byte only data.
	EN_TCON20_REGISTERTYPE_PANEL_1_BYTES,
	//Register type 9: 2+4, panelinfo ID+value
	EN_TCON20_REGISTERTYPE_PANEL_6_BYTES,
	// Register type A : 3+1+1+3+2+2+1+1
	// subbankaddress + mask+subbank+address+mask+value+delaytime+prioriry
	EN_TCON20_REGISTERTYPE_POWERSEQUENCE_14BYTES,
	//Register type 9: 2+4, panelinfo ID+value, for new SSC calculation
	EN_TCON20_REGISTERTYPE_PANEL_6_BYTES_NEWSSC,
	EN_TCON20_REGISTERTYPE_END,
} E_TCON20_REGISTERTYPE;

uint16_t _regTypeToSize(uint8_t u8Registertype)
{
	E_TCON20_REGISTERTYPE en = (E_TCON20_REGISTERTYPE)u8Registertype;

	switch (en) {
	case EN_TCON20_REGISTERTYPE_6_BYTES:
		return (uint16_t)TCON_REGISTERTYPE_6_BYTES;
	case EN_TCON20_REGISTERTYPE_4_BYTES:
		return (uint16_t)TCON_REGISTERTYPE_4_BYTES;
	case EN_TCON20_REGISTERTYPE_POWERSEQENCE_9_BYTES:
		return (uint16_t)TCON_REGISTERTYPE_POWERSEQENCE_9_BYTES;
	case EN_TCON20_REGISTERTYPE_POWERSEQENCE_7_BYTES:
		return (uint16_t)TCON_REGISTERTYPE_POWERSEQENCE_7_BYTES;
	case EN_TCON20_REGISTERTYPE_POWERSEQENCE_WITHSUB_15_BYTES:
		return (uint16_t)TCON_REGISTERTYPE_POWERSEQENCE_WITHSUB_15_BYTES;
	case EN_TCON20_REGISTERTYPE_POWERSEQENCE_WITHSUB_11_BYTES:
		return (uint16_t)TCON_REGISTERTYPE_POWERSEQENCE_WITHSUB_11_BYTES;
	case EN_TCON20_REGISTERTYPE_PANEL_1_BYTES:
		return(uint16_t) TCON_REGISTERTYPE_1_BYTES;
	case EN_TCON20_REGISTERTYPE_PANEL_6_BYTES:
	case EN_TCON20_REGISTERTYPE_PANEL_6_BYTES_NEWSSC:
		return (uint16_t)TCON_REGISTERTYPE_6_BYTES;
	case EN_TCON20_REGISTERTYPE_POWERSEQUENCE_14BYTES:
		return (uint16_t)TCON_REGISTERTYPE_POWERSEQENCE_WITHSUB_14_BYTES;
	default:
		TCON_ERROR("register type out of range: %d", en);
		return 0;
	}
}

unsigned char *_read_storage_file_to_memory(
			const char *path,
			loff_t *size, struct device *dev)
{
	const struct firmware *fw;
	unsigned char *pDataBuf = NULL;
	int32_t s32BufLen = 0;
	int32_t s32Rst = 0;

	if (!dev) {
		TCON_ERROR("dev is null\n");
		return NULL;
	}

	if (!size) {
		TCON_ERROR("size is null\n");
		return NULL;
	}

	*size = 0;
	TCON_DEBUG("file path='%s' length=%u\n", path, (unsigned int)strlen(path));

	//open the file.
	//The search path is defined in the firmware_class.path parameter of bootarg
	s32Rst = request_firmware_direct(&fw, path, dev);
	if (s32Rst) {
		TCON_ERROR("Fail to get firmware. Error code=%d\n", s32Rst);
		return NULL;
	}

	TCON_DEBUG("request_firmware_direct ...end'\n");

	if (fw->size <= 0) {
		release_firmware(fw);
		TCON_ERROR("file length is 0\n");
		return NULL;
	}

	//calculate file length
	s32BufLen = fw->size + 1;

	//read file data to memory
	pDataBuf = vmalloc(s32BufLen);
	if (!pDataBuf) {
		release_firmware(fw);
		TCON_ERROR("malloc file len(%d) fail\n", s32BufLen);
		return NULL;
	}

	memcpy(pDataBuf, fw->data, fw->size);

	*size = (loff_t)(fw->size);
	release_firmware(fw);

	return pDataBuf;
}

char *_basename(char *path)
{
	char *basename;

	basename = strrchr(path, '/');
	return basename ? (basename + 1) : path;
}

loff_t _get_file_data(struct device *dev,
						const char *default_path, const char *path,
						unsigned char **data_buf)
{
	char chFilePath[FILE_PATH_LENGTH];
	const char *pchPath;
	loff_t data_buf_size = 0;
	size_t len;

	if (!dev) {
		TCON_ERROR("input parameter dev is NULL\n");
		return 0;
	}

	if (!default_path) {
		TCON_ERROR("input parameter default_path is NULL\n");
		return 0;
	}

	if (!path) {
		TCON_ERROR("input parameter path is NULL\n");
		return 0;
	}

	if (!data_buf) {
		TCON_ERROR("input parameter data_buf is NULL\n");
		return 0;
	}

	memset(chFilePath, '\0', sizeof(chFilePath));

	/* load bin */
	pchPath = (path[0] != '\0') ? path : default_path;

	len = strlen(FILE_FIXED_FOLDER) + strlen(pchPath) + 1;
	if (len >= sizeof(chFilePath)) {
		TCON_ERROR(
			"Warning: The length of the path is %d dytes\n"
			"more than the maximum file length supported\n"
			"by the system is %d bytes.\n",
			(int)len, (int)sizeof(chFilePath));
		return 0;
	}

	len = sizeof(chFilePath) - 1;
	strncat(chFilePath, FILE_FIXED_FOLDER, len);
	len = len - strlen(FILE_FIXED_FOLDER);
	strncat(chFilePath, pchPath, len);
	TCON_INFO("Use %s path '%s'\n",
					(path[0] != '\0') ? "customized" : "default",
					chFilePath);

	/* krenel default serch path: /vendor/firmware */
	*data_buf = _read_storage_file_to_memory(
						chFilePath,
						&data_buf_size, dev);

	if (!(*data_buf))
		TCON_ERROR("Read '%s' failure\n", chFilePath);

	return data_buf_size;
}

bool init_log_level(const uint32_t u32Level)
{
	g_enTconLogLevel = (u32Level) ? u32Level : EN_TCON_LOG_LEVEL_ERROR;
	TCON_DEBUG("TconLogLevel=0x%X\n", g_enTconLogLevel);

	return TRUE;
}

int get_tcon_common_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize)
{
	int ret = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}

	if (!pInfo || u32InfoSize == 0) {
		TCON_ERROR("pInfo is NULL or info size is zero\n");
		return -EINVAL;
	}

	ret = snprintf(pInfo, u32InfoSize,
		"Tcon bin file info:\n"
		"	- [%s] Default bin = %s\n"
		"	- [%s] Higt frame rate bin = %s\n"
		"	- [%s] Higt pixel clock bin = %s\n"
		"	- [%s] Game mode bin = %s\n",
		(g_tcon_mode_buf[EN_TCON_MODE_DEFAULT]) ? "Exist" : "-----",
		pCon->cus.tcon_path,
		(g_tcon_mode_buf[EN_TCON_MODE_HFR]) ? "Exist" : "-----",
		pCon->tconless_model_info.higt_frame_rate_bin_path,
		(g_tcon_mode_buf[EN_TCON_MODE_HPC]) ? "Exist" : "-----",
		pCon->tconless_model_info.higt_pixel_clock_bin_path,
		(g_tcon_mode_buf[EN_TCON_MODE_GAME]) ? "Exist" : "-----",
		pCon->tconless_model_info.game_mode_bin_path);

	return ((ret < 0) || (ret > u32InfoSize)) ? -EINVAL : ret;
}

bool set_tcon_mode(EN_TCON_MODE enMode)
{
	bool bModeChange = FALSE;

	if (g_current_mode != enMode &&
		g_tcon_mode_buf[enMode] && g_tcon_mode_buf_size[enMode] > 0) {
		g_tcon_data_buf = g_tcon_mode_buf[enMode];
		g_tcon_data_size = g_tcon_mode_buf_size[enMode];

		//change the new gamma data of the tcon mode to the current data pointer
		if (g_pnlgamma_data_buf[enMode] && g_pnlgamma_data_size[enMode] > 0) {
			g_gamma_data_buf = g_pnlgamma_data_buf[enMode];
			g_gamma_data_size = g_pnlgamma_data_size[enMode];
		} else
			TCON_DEBUG("The new mode gamma data is null\n");

		TCON_DEBUG("The current mode=%d -> %d\n", g_current_mode, enMode);
		g_current_mode = enMode;
		bModeChange = TRUE;
	} else
		TCON_DEBUG("The current mode(=%d->%d) has not changed.\n", g_current_mode, enMode);

	return bModeChange;
}

EN_TCON_MODE get_tcon_mode(void)
{
	return g_current_mode;
}

bool load_tcon_files(struct mtk_panel_context *pCon)
{
	const char *default_bin_path[EN_TCON_MODE_MAX];
	const char *bin_path[EN_TCON_MODE_MAX];
	uint8_t i;
	int64_t tCur;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	/* get current time */
	tCur = ktime_get();

	/* store default tcon bin data */
	default_bin_path[EN_TCON_MODE_DEFAULT] = TCON_FILE_PATH;
	bin_path[EN_TCON_MODE_DEFAULT] = pCon->cus.tcon_path;

	/* store higt frame rate tcon bin data */
	default_bin_path[EN_TCON_MODE_HFR] = TCON_HFR_FILE_PATH;
	bin_path[EN_TCON_MODE_HFR] = pCon->tconless_model_info.higt_frame_rate_bin_path;

	/* store higt pixel clock tcon bin data */
	default_bin_path[EN_TCON_MODE_HPC] = TCON_HPC_FILE_PATH;
	bin_path[EN_TCON_MODE_HPC] = pCon->tconless_model_info.higt_pixel_clock_bin_path;

	/* store game tcon bin data */
	default_bin_path[EN_TCON_MODE_GAME] = TCON_GAME_FILE_PATH;
	bin_path[EN_TCON_MODE_GAME] = pCon->tconless_model_info.game_mode_bin_path;

	for (i = 0; i < EN_TCON_MODE_MAX; i++) {
		if (!g_tcon_mode_buf[i]) {
			g_tcon_mode_buf_size[i] = _get_file_data(
								pCon->dev,
								default_bin_path[i], bin_path[i],
								&g_tcon_mode_buf[i]);

			if (g_tcon_mode_buf[i] && g_tcon_mode_buf_size[i] > 1)
				TCON_DEBUG("tcon buffer[0]=0x%X [1]=0x%X, size=%lld\n",
						g_tcon_mode_buf[i][0], g_tcon_mode_buf[i][1],
						g_tcon_mode_buf_size[i]);
		} else
			TCON_DEBUG("tcon mode(=%d) data already exists\n", i);
	}

	/* set the current tcon data */
	g_tcon_data_buf = g_tcon_mode_buf[g_current_mode];
	g_tcon_data_size = g_tcon_mode_buf_size[g_current_mode];

	TCON_DEBUG("The current mode=%d, total load time=%lld ms\n",
				g_current_mode, ktime_ms_delta(ktime_get(), tCur));

	/* load power squence on/off bin */
	tCur = ktime_get();

	if (!g_tcon_power_on_data_buf) {
		g_tcon_power_on_data_size = _get_file_data(
					pCon->dev,
					TCON_POWER_ON_FILE_PATH,
					pCon->tconless_model_info.power_seq_on_bin_path,
					&g_tcon_power_on_data_buf);
		TCON_DEBUG("power on data buffer size=%lld, total load time=%lld ms\n",
				g_tcon_power_on_data_size, ktime_ms_delta(ktime_get(), tCur));
	}

	tCur = ktime_get();

	if (!g_tcon_power_off_data_buf) {
		g_tcon_power_off_data_size = _get_file_data(
					pCon->dev,
					TCON_POWER_OFF_FILE_PATH,
					pCon->tconless_model_info.power_seq_off_bin_path,
					&g_tcon_power_off_data_buf);
		TCON_DEBUG("power off data buffer size=%lld, total load time=%lld ms\n",
				g_tcon_power_off_data_size, ktime_ms_delta(ktime_get(), tCur));
	}

	/* get current time */
	tCur = ktime_get();

	//load panel gamma bin
	if (!(pCon->cus.pgamma_en)) {
		TCON_DEBUG("Not support panel gamma function.\n");
		return TRUE;
	}

	/* store default tcon bin data */
	default_bin_path[EN_TCON_MODE_DEFAULT] = PANELGAMMA_FILE_PATH;
	bin_path[EN_TCON_MODE_DEFAULT] = pCon->cus.pgamma_path;

	/* store higt frame rate tcon bin data */
	default_bin_path[EN_TCON_MODE_HFR] = pCon->cus.pgamma_path;
	bin_path[EN_TCON_MODE_HFR] = pCon->tconless_model_info.higt_frame_rate_gamma_bin_path;

	/* store higt pixel clock tcon bin data */
	default_bin_path[EN_TCON_MODE_HPC] = pCon->cus.pgamma_path;
	bin_path[EN_TCON_MODE_HPC] = pCon->tconless_model_info.higt_pixel_clock_gamma_bin_path;

	/* store game tcon bin data */
	default_bin_path[EN_TCON_MODE_GAME] = pCon->cus.pgamma_path;
	bin_path[EN_TCON_MODE_GAME] = pCon->tconless_model_info.game_mode_gamma_bin_path;

	for (i = 0; i < EN_TCON_MODE_MAX; i++) {
		if (!g_pnlgamma_data_buf[i]) {
			g_pnlgamma_data_size[i] = _get_file_data(
						pCon->dev,
						default_bin_path[i], bin_path[i],
						&g_pnlgamma_data_buf[i]);

			if (g_pnlgamma_data_buf[i] && g_pnlgamma_data_size[i] > 1)
				TCON_DEBUG("tcon buffer[0]=0x%X [1]=0x%X, size=%lld\n",
					g_pnlgamma_data_buf[i][0], g_pnlgamma_data_buf[i][1],
					g_pnlgamma_data_size[i]);
		} else
			TCON_DEBUG("pnlgamma(=%d) data already exists\n", i);
	}

	/* set the current tcon data */
	g_gamma_data_buf = g_pnlgamma_data_buf[g_current_mode];
	g_gamma_data_size = g_pnlgamma_data_size[g_current_mode];

	TCON_DEBUG("The current mode=%d, pnlgamma total load time=%lld ms\n",
				g_current_mode, ktime_ms_delta(ktime_get(), tCur));

	return TRUE;
}

bool is_tcon_data_exist(unsigned char **ppdata, loff_t *plen)
{
	bool bExist = (g_tcon_data_buf && g_tcon_data_size > 0) ? TRUE : FALSE;

	if (ppdata)
		*ppdata = g_tcon_data_buf;

	if (plen)
		*plen =  g_tcon_data_size;

	return bExist;
}

bool is_tcon_power_seq_data_exist(unsigned char **ppdata, loff_t *plen, bool bOn)
{
	bool bExist;

	if (bOn)
		bExist = (g_tcon_power_on_data_buf && g_tcon_power_on_data_size > 0) ?
					TRUE : FALSE;
	else
		bExist = (g_tcon_power_off_data_buf && g_tcon_power_off_data_size > 0) ?
					TRUE : FALSE;

	if (ppdata)
		*ppdata = (bOn) ? g_tcon_power_on_data_buf : g_tcon_power_off_data_buf;

	if (plen)
		*plen = (bOn) ? g_tcon_power_on_data_size : g_tcon_power_off_data_size;

	return bExist;
}

bool is_pnlgamma_data_exist(unsigned char **ppdata, loff_t *plen)
{
	bool bExist = (g_gamma_data_buf && g_gamma_data_size > 0) ? TRUE : FALSE;

	if (ppdata)
		*ppdata = g_gamma_data_buf;

	if (plen)
		*plen =  g_gamma_data_size;

	return bExist;
}

bool load_vrr_tcon_files(struct mtk_panel_context *pCon)
{
	char chFilePath[FILE_PATH_LENGTH];
	uint16_t i = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	memset(chFilePath, '\0', sizeof(chFilePath));

	//load tcon bin
	if (!(pCon->vrr_od_En)) {
		TCON_DEBUG("Not support vrr tcon function.\n");
		return FALSE;
	}

	for (i = 0; i < pCon->VRR_OD_info->u8VRR_OD_table_Total; i++) {
		if (pCon->VRR_OD_info->stVRR_OD_Table[i].bEnable
			&& !g_vrr_tcon_data_buf[i]) {
			//load tcon bin
			if (pCon->VRR_OD_info->stVRR_OD_Table[i].s8VRR_OD_table_Name[0]
				!= '\0') {
				if (strlen(
				pCon->VRR_OD_info->stVRR_OD_Table[i].s8VRR_OD_table_Name
					) >= sizeof(chFilePath)) {
					TCON_DEBUG(
					"Warning: Length of the customized path %d dytes\n"
					"more than the maximum file length supported\n"
					"by the system is %d bytes.\n",
					(int)strlen(
				pCon->VRR_OD_info->stVRR_OD_Table[i].s8VRR_OD_table_Name
					), (int)sizeof(chFilePath));
				}
				strncpy(chFilePath,
				pCon->VRR_OD_info->stVRR_OD_Table[i].s8VRR_OD_table_Name,
					sizeof(chFilePath) - 1);
				TCON_DEBUG("Read vrr tcon bin file from customized path %s\n",
					chFilePath);
			} else {
				strncpy(chFilePath, TCON_FILE_PATH, sizeof(chFilePath) - 1);
				TCON_DEBUG("Read vrr tcon bin file from default path %s\n",
					chFilePath);
			}

			g_vrr_tcon_data_size[i] = _get_file_data(
				pCon->dev,
				TCON_FILE_PATH,
				pCon->VRR_OD_info->stVRR_OD_Table[i].s8VRR_OD_table_Name,
				&g_vrr_tcon_data_buf[i]);
		}
	}

	if (g_vrr_tcon_data_buf[0])
		TCON_DEBUG("vrr tcon buffer[0]=0x%X [1]=0x%X, size[0]=%lld [1]=%lld\n",
				g_vrr_tcon_data_buf[0][0], g_vrr_tcon_data_buf[1][0],
				g_vrr_tcon_data_size[0], g_vrr_tcon_data_size[1]);
	else
		TCON_ERROR("Error: Read vrr tcon data failure\n");

	return TRUE;
}

bool load_vrr_pga_files(struct mtk_panel_context *pCon)
{
	char chFilePath[FILE_PATH_LENGTH];
	uint16_t i = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	memset(chFilePath, '\0', sizeof(chFilePath));

	//load panel gamma bin
	if (!(pCon->vrr_pga_En)) {
		TCON_DEBUG("Not support vrr panel gamma function.\n");
		return TRUE;
	}

	for (i = 0; i < pCon->VRR_PGA_info->u8VRR_PGA_table_Total; i++) {
		if (pCon->VRR_PGA_info->stVRR_PGA_Table[i].bEnable
			&& !g_vrr_pnlgamma_data_buf[i]) {
			//load tcon bin
			if (pCon->VRR_PGA_info->stVRR_PGA_Table[i].s8VRR_PGA_table_Name[0]
				!= '\0') {
				if (strlen(
				pCon->VRR_PGA_info->stVRR_PGA_Table[i].s8VRR_PGA_table_Name
					) >= sizeof(chFilePath)) {
					TCON_DEBUG(
					"Warning: Length of the customized path %d dytes\n"
					"more than the maximum file length supported\n"
					"by the system is %d bytes.\n",
					(int)strlen(
				pCon->VRR_PGA_info->stVRR_PGA_Table[i].s8VRR_PGA_table_Name
					), (int)sizeof(chFilePath));
				}
				strncpy(chFilePath,
				pCon->VRR_PGA_info->stVRR_PGA_Table[i].s8VRR_PGA_table_Name,
					sizeof(chFilePath) - 1);
				TCON_DEBUG("Read vrr pga bin file from customized path %s\n",
					chFilePath);
			} else {
				strncpy(chFilePath, PANELGAMMA_FILE_PATH, sizeof(chFilePath) - 1);
				TCON_DEBUG("Read vrr panel gamma bin file from default path %s\n",
					chFilePath);
			}

			g_vrr_pnlgamma_data_size[i] = _get_file_data(
				pCon->dev,
				TCON_FILE_PATH,
				pCon->VRR_PGA_info->stVRR_PGA_Table[i].s8VRR_PGA_table_Name,
				&g_vrr_pnlgamma_data_buf[i]);
		}
	}

	if (g_vrr_pnlgamma_data_buf[0])
		TCON_DEBUG("pga buffer[0]=0x%X [1]=0x%X, size[0]=%lld [1]=%lld\n",
				g_vrr_pnlgamma_data_buf[0][0], g_vrr_pnlgamma_data_buf[1][0],
				g_vrr_pnlgamma_data_size[0], g_vrr_pnlgamma_data_size[1]);
	else
		TCON_ERROR("Error: Read vrr panel gamma data failure\n");

	return TRUE;
}

bool is_vrr_tcon_data_exist(unsigned char **ppdata, loff_t *plen, unsigned int index)
{
	bool bExist = (g_vrr_tcon_data_buf[index] && g_vrr_tcon_data_size[index] > 0) ?
		TRUE : FALSE;

	if (ppdata)
		*ppdata = g_vrr_tcon_data_buf[index];

	if (plen)
		*plen =  g_vrr_tcon_data_size[index];

	return bExist;
}

bool is_vrr_pnlgamma_data_exist(unsigned char **ppdata, loff_t *plen, unsigned int index)
{
	bool bExist = (g_vrr_pnlgamma_data_buf[index] && g_vrr_pnlgamma_data_size[index] > 0) ?
		TRUE : FALSE;

	if (ppdata)
		*ppdata = g_vrr_pnlgamma_data_buf[index];

	if (plen)
		*plen =  g_vrr_pnlgamma_data_size[index];

	return bExist;
}

const char *_tabidx_to_string(uint8_t u8Idx)
{
	uint8_t u8TableNum = ARRAY_SIZE(TCON_TAB_TYPE_STRING);

	TCON_INFO("Table Num=%d Idx=%d\n", u8TableNum, u8Idx);
	return (u8Idx < u8TableNum) ?
		TCON_TAB_TYPE_STRING[u8Idx] : TCON_TAB_TYPE_STRING[u8TableNum - 1];
}

bool get_tcon_version(uint8_t *pu8Table, uint8_t *pu8Version)
{
	if (!pu8Table) {
		TCON_ERROR("pu8Table is NULL parameter\n");
		return FALSE;
	}

	if (!pu8Version) {
		TCON_ERROR("pu8Version is NULL parameter\n");
		return FALSE;
	}
	*pu8Version = *((uint8_t *)(pu8Table));

	return TRUE;
}

bool get_tcon_dump_table(st_tcon_tab_info *pstInfo)
{
	uint8_t *pu8Table = NULL;
	uint8_t u8TableType = 0;
	uint16_t u16RegisterCount = 0;
	uint8_t  u8RegisterType = 0;
	uint32_t u32RegisterlistOffset = 0;
	uint32_t u32RegisterlistSize = 0;
	uint8_t u8PanelInterface = 0;
	uint8_t u8TconType = 0;
	uint8_t u8version = 0;
	bool bRet = TRUE;

	if (!pstInfo) {
		TCON_ERROR("pstInfo is NULL parameter\n");
		return FALSE;
	}

	pu8Table = pstInfo->pu8Table;
	u8TconType = pstInfo->u8TconType;

	if (!pu8Table) {
		TCON_ERROR("pu8Table is NULL parameter\n");
		return FALSE;
	}
	u8version = *((uint8_t *)(pu8Table));

	pu8Table += SKIP_TCON_MAIMHEADER;  // skip 32 bytes main header
	pu8Table += SKIP_TCON_SUBHEADER;   // skip 8 byte sub header

	if (u8version > TCON20_VERSION) {
		// skip 8byte TCON2.0 subheader
		pu8Table += SKIP_TCON20_SUBHEADER;

		//get panel Interface
		u8PanelInterface = *((uint8_t *)(pu8Table + SKIP_TCON20_PANETYPE));
	}
	// sn MAY NEED version , so there needs a way to pass this info.
	u8TableType           = *((uint8_t *)(pu8Table + (u8TconType * 8) + 0));
	u16RegisterCount      = ((*((uint8_t *)(pu8Table + (u8TconType * 8) + 2)))<<8) |
				*((uint8_t *)(pu8Table + (u8TconType * 8) + 1));
	u8RegisterType        = *((uint8_t *)(pu8Table + (u8TconType * 8) + 3));
	u32RegisterlistOffset = ((*((uint8_t *)(pu8Table + (u8TconType * 8) + 7)))<<24) |
				((*((uint8_t *)(pu8Table + (u8TconType * 8) + 6)))<<16) |
				((*((uint8_t *)(pu8Table + (u8TconType * 8) + 5)))<<8) |
				*((uint8_t *)(pu8Table + (u8TconType * 8) + 4));
	u32RegisterlistSize   = u16RegisterCount * _regTypeToSize(u8RegisterType);

	//error handling
	if ((u8TableType != u8TconType) || (u16RegisterCount == 0)) {
		pu8Table -= (SKIP_TCON_MAIMHEADER + SKIP_TCON_SUBHEADER);

		if (u8version > TCON20_VERSION)
			pu8Table -= SKIP_TCON20_SUBHEADER;

		bRet = FALSE;
	} else {
		if (u8version > TCON20_VERSION) {
			pu8Table += (u32RegisterlistOffset -
			(SKIP_TCON_MAIMHEADER + SKIP_TCON_SUBHEADER + SKIP_TCON20_SUBHEADER));
		} else {
			pu8Table += (u32RegisterlistOffset -
			(SKIP_TCON_MAIMHEADER + SKIP_TCON_SUBHEADER));
		}
	}

	pstInfo->pu8Table = pu8Table;
	pstInfo->u8Version = u8version;
	pstInfo->u16RegCount = u16RegisterCount;
	pstInfo->u8RegType = u8RegisterType;
	pstInfo->u32ReglistOffset = u32RegisterlistOffset;
	pstInfo->u32ReglistSize = u32RegisterlistSize;
	pstInfo->u8PanelInterface = u8PanelInterface;

	TCON_DEBUG("Find '%s' table [%s]\n",
		_tabidx_to_string(u8TconType), bRet ? "OK" : "Not Found");
	TCON_DEBUG("Current Type='%s' Version=%d PanelInterface=%d "
		"RegisterCount=%d RegisterType=%d RegisterlistOffset=%d RegisterlistSize=%d\n",
		_tabidx_to_string(u8TableType),
		pstInfo->u8Version, pstInfo->u8PanelInterface,
		pstInfo->u16RegCount, pstInfo->u8RegType, pstInfo->u32ReglistOffset,
		pstInfo->u32ReglistSize);

	return bRet;
}

bool release_tcon_resource(void)
{
	uint8_t i;

	for (i = 0; i < EN_TCON_MODE_MAX; i++) {
		if (g_tcon_mode_buf[i]) {
			vfree(g_tcon_mode_buf[i]);
			g_tcon_mode_buf[i] = NULL;
		}
	}

	if (g_tcon_power_on_data_buf) {
		vfree(g_tcon_power_on_data_buf);
		g_tcon_power_on_data_buf = NULL;
	}

	if (g_tcon_power_off_data_buf) {
		vfree(g_tcon_power_off_data_buf);
		g_tcon_power_off_data_buf = NULL;
	}

	for (i = 0; i < EN_TCON_MODE_MAX; i++) {
		if (g_pnlgamma_data_buf[i]) {
			vfree(g_pnlgamma_data_buf[i]);
			g_pnlgamma_data_buf[i] = NULL;
		}
	}
	return TRUE;
}

bool release_vrr_od_gamma_tcon_resource(void)
{
	unsigned int i = 0;

	for (i = 0; i < VRR_OD_TABLE_NUM_MAX; i++) {
		if (g_vrr_tcon_data_buf[i] != NULL) {
			vfree(g_vrr_tcon_data_buf[i]);
			g_vrr_tcon_data_buf[i] = NULL;
		}
	}
	for (i = 0; i < VRR_OD_TABLE_NUM_MAX; i++) {
		if (g_vrr_pnlgamma_data_buf[i] != NULL) {
			vfree(g_vrr_pnlgamma_data_buf[i]);
			g_vrr_pnlgamma_data_buf[i] = NULL;
		}
	}
	return TRUE;
}

bool write_byte_reg(uint32_t u32Addr, uint8_t u8Val, uint8_t u8Mask)
{
	bool bRet = TRUE;

	drv_hwreg_render_tcon_set_byte_reg(u32Addr, u8Val, u8Mask);
	return bRet;
}

bool write_2byte_reg(uint32_t u32Addr, uint16_t u16Val, uint16_t u16Mask)
{
	bool bRet = TRUE;

	drv_hwreg_render_tcon_set_2byte_reg(u32Addr, u16Val, u16Mask);
	return bRet;
}

bool is_adl_support(enum en_sm_adl_client enClient)
{
	struct sm_adl_caps_info caps_info;
	enum sm_return_type sm_ret;
	enum en_sm_adl_client client_type = enClient;

	memset(&caps_info, 0, sizeof(struct sm_adl_caps_info));

	sm_ret = sm_adl_get_caps(client_type, &caps_info);
	if (sm_ret != E_SM_RET_OK)
		TCON_ERROR("Fail to get ADL caps result = %d\n", sm_ret);

	TCON_DEBUG("[%s:%d] client=%d, client_support=%d ret=%d\n",
			__func__, __LINE__, client_type, caps_info.client_support, sm_ret);

	return caps_info.client_support;
}

static int _wait_tcon_setting_handler(void *data)
{
	int i = 0, fd = 0, checkDoneCnt = 0;
	__u16 u16Timeout = 0;
	enum sm_return_type enRet = E_SM_RET_FAIL;
	struct sm_adl_status stStatusinfo[E_ADL_CLIENT_MAX] = {0};
	int64_t tCur;
	struct completion *comp = (struct completion *)data;

	//get current time
	tCur = ktime_get();

	//total check client count
	for (i = 0; i < E_ADL_CLIENT_MAX; i++) {
		if (g_adl_info[i].fd != 0 && g_adl_info[i].bCheckStatus)
			checkDoneCnt++;
	}

	TCON_DEBUG("Total client check cnt=%d\n", checkDoneCnt);

	//check client adl done
	while (checkDoneCnt > 0 && u16Timeout < MAX_DELAY_TIME) {
		for (i = 0; i < E_ADL_CLIENT_MAX; i++) {
			fd = g_adl_info[i].fd;
			stStatusinfo[i].mem_index = g_adl_info[i].u8MemIndex;

			if (fd != 0  && g_adl_info[i].bCheckStatus &&
				stStatusinfo[i].checkdone == 0) {
				enRet = sm_adl_checkstatus(fd, &stStatusinfo[i]);

				if (enRet != E_SM_RET_OK) {
					TCON_ERROR("Check client[%d] status ret=%d\n",
							i, enRet);
				} else {
					TCON_DEBUG("Check client[%d] checkdone=%d cnt=%d\n",
							i, stStatusinfo[i].checkdone, checkDoneCnt);

					if (stStatusinfo[i].checkdone)
						checkDoneCnt--;
				}

			}
		}

		u16Timeout++;

		if (checkDoneCnt > 0 && u16Timeout < MAX_DELAY_TIME) {
			//delay 1 vsync
			mdelay(ONE_VSYNC_TIME_MS);
		}
	}

	if (comp) {
		complete(comp);
		TCON_DEBUG("Send complete event\n");
	} else
		TCON_ERROR("pComp is NULL\n");

	for (i = 0; i < E_ADL_CLIENT_MAX; i++) {
		fd = g_adl_info[i].fd;

		if (fd != 0 && g_adl_info[i].bCheckStatus) {
			TCON_DEBUG("Check client[%d] release\n", i);

			//release memory index
			enRet = sm_adl_release_mem_index(fd, g_adl_info[i].u8MemIndex);
			if (enRet != E_SM_RET_OK) {
				TCON_ERROR("Check client[%d] release mem index ret=%d\n",
						i, enRet);
			}

			//destroy instance
			enRet = sm_adl_destroy_resource(fd);
			if (enRet != E_SM_RET_OK) {
				TCON_ERROR("Check client[%d] destroy instance ret=%d\n",
						i, enRet);
			}
		}
	}

	TCON_DEBUG("Total time=%lld ms\n", ktime_ms_delta(ktime_get(), tCur));
	g_bAdlChecking = FALSE;
	return 0;
}

bool wait_tcon_setting_done(struct mtk_panel_context *pCon, struct completion *pComp)
{
	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	if (!g_bAdlChecking) {
		g_bAdlChecking = TRUE;
		//create mtk_panel_handler for panel sequence control
		g_wait_worker = kthread_run(_wait_tcon_setting_handler,
				(void *)pComp,
				"wait tcon_setting_thread");
		if (IS_ERR(g_wait_worker)) {
			TCON_ERROR("create thread failed!(err=%d)\n",
				(int)PTR_ERR(g_wait_worker));
			g_wait_worker = NULL;
			_wait_tcon_setting_handler((void *)pComp);
		} else
			TCON_DEBUG("thread was created.(PID=%d)\n",
				g_wait_worker->pid);
	} else
		TCON_ERROR("The latest action is still going on.\n");

	return TRUE;
}

bool _do_adl_finally_proc(
	enum en_sm_adl_client client_type, int fd, __u8 u8MemIndex, bool bCheckAdlStatus)
{
	bool bRet = TRUE;
	enum sm_return_type enRet = E_SM_RET_FAIL;
	enum en_adl_client_type enClientType = E_ADL_CLIENT_MAX;

	if (client_type == E_SM_TCON_ADL_PANEL_GAMMA_0)
		enClientType = E_ADL_CLIENT_PANEL_GAMMA_0;
	else if (client_type == E_SM_TCON_ADL_PCID)
		enClientType = E_ADL_CLIENT_PCID;
	else if (client_type == E_SM_TCON_ADL_OD)
		enClientType = E_ADL_CLIENT_OD;
	else if (client_type == E_SM_TCON_ADL_VAC)
		enClientType = E_ADL_CLIENT_VAC;
	else
		TCON_DEBUG("Unknown adl client=%d mapping.\n", client_type);

	if (enClientType < E_ADL_CLIENT_MAX) {
		g_adl_info[enClientType].bCheckStatus = bCheckAdlStatus;
		g_adl_info[enClientType].fd = fd;
		g_adl_info[enClientType].u8MemIndex = u8MemIndex;

		TCON_DEBUG("adl client=%d fd=%d MemIndex=%d\n", enClientType, fd, u8MemIndex);
	} else {
		TCON_DEBUG("adl client[%d] release\n", client_type);

		//release memory index
		enRet = sm_adl_release_mem_index(fd, u8MemIndex);
		if (enRet != E_SM_RET_OK) {
			TCON_ERROR("adl client[%d] release mem index ret=%d\n",
					client_type, enRet);
			bRet = FALSE;
		}

		//destroy instance
		enRet = sm_adl_destroy_resource(fd);
		if (enRet != E_SM_RET_OK) {
			TCON_ERROR("adl client[%d] destroy instance ret=%d\n",
					client_type, enRet);
			bRet = FALSE;
		}
	}

	return bRet;
}

bool set_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize,
			enum en_sm_adl_client enClient, enum en_sm_adl_trigger_mode enTrigMode)
{
	bool bRet = TRUE;
	bool bCheckAdlStatus = false;
	enum sm_return_type enRet = E_SM_RET_FAIL;
	int fd = 0;
	__u8 u8MemIndex = 0;
	struct sm_adl_res stRes;
	struct sm_adl_add_info stDatainfo;
	struct sm_adl_fire_info stFireinfo;
	struct sm_adl_info stIndexinfo;

	if (!pu8Tbl) {
		TCON_ERROR("pu8Tbl is NULL parameter\n");
		return FALSE;
	}

	memset(&stRes, 0, sizeof(struct sm_adl_res));
	memset(&stDatainfo, 0, sizeof(struct sm_adl_add_info));
	memset(&stFireinfo, 0, sizeof(struct sm_adl_fire_info));
	memset(&stIndexinfo, 0, sizeof(struct sm_adl_info));

	stRes.client_type = enClient;
	stRes.trig_mode = enTrigMode;

	/*if HW support*/
	//1.create instance
	enRet = sm_adl_create_resource(&fd, &stRes);
	if (enRet != E_SM_RET_OK) {
		if (enRet == E_SM_RET_FAIL_FUN_NOT_SUPPORT) {
			TCON_ERROR("adl client[%d] is not support. ret=%d\n",
					stRes.client_type, enRet);
			bRet = FALSE;
			goto finally;
		}

		TCON_ERROR("adl client[%d] failed to create instance. ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else
		TCON_DEBUG("adl client[%d] buffer_cnt=%d fd=%d\n",
				stRes.client_type, stRes.buffer_cnt, fd);

	//2.get memory index
	enRet = sm_adl_get_mem_index(fd, &u8MemIndex);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] get mem index fail. ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else
		TCON_DEBUG("adl memory index = %d\n", u8MemIndex);

	//3.get adl index info
	enRet = sm_adl_get_info(fd, u8MemIndex, &stIndexinfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] get index info ret=%d\n",
		stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else {
		TCON_DEBUG("memory size=%d, index=%d, phybuf_addr=0x%llx\n",
				stIndexinfo.buf_size,
				u8MemIndex,
				stIndexinfo.phybuf_addr);

		if ((u32TblSize > stIndexinfo.buf_size) || (u32TblSize <= 0) ||
			(stIndexinfo.phybuf_addr == 0)) {
			TCON_ERROR("Buf got from kernel is wrong. Table size=%d\n",
					u32TblSize);
			bRet = FALSE;
			goto finally;
		}
	}

	//4.write cmd
	stDatainfo.sub_client = E_SM_ADL_SUB_CLIENT_MAX;
	stDatainfo.mem_index = u8MemIndex;
	stDatainfo.data = pu8Tbl;
	stDatainfo.dataSize = u32TblSize;
	enRet = sm_adl_add_cmd(fd, &stDatainfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] write ret=%d\n", stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	}

	//5.fire
	stFireinfo.mem_index = u8MemIndex;
	enRet = sm_adl_fire(fd, &stFireinfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] fire ret=%d\n", stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	}

	bCheckAdlStatus = true;

finally:
	bRet = _do_adl_finally_proc(stRes.client_type, fd, u8MemIndex, bCheckAdlStatus);

	return bRet;
}

bool set_vrr_od_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize,
			enum en_sm_adl_client enClient, enum en_sm_adl_trigger_mode enTrigMode)
{
	bool bRet = TRUE;
	enum sm_return_type enRet = E_SM_RET_FAIL;
	int fd = 0;
	__u8 u8MemIndex = 0;
	__u16 u16Timeout = 0;
	struct sm_adl_res stRes;
	struct sm_adl_add_info stDatainfo;
	struct sm_adl_fire_info stFireinfo;
	struct sm_adl_info stIndexinfo;
	struct sm_adl_status stStatusinfo;

	if (!pu8Tbl) {
		TCON_ERROR("pu8Tbl is NULL parameter\n");
		return FALSE;
	}

	memset(&stRes, 0, sizeof(struct sm_adl_res));
	memset(&stDatainfo, 0, sizeof(struct sm_adl_add_info));
	memset(&stFireinfo, 0, sizeof(struct sm_adl_fire_info));
	memset(&stIndexinfo, 0, sizeof(struct sm_adl_info));
	memset(&stStatusinfo, 0, sizeof(struct sm_adl_status));

	stRes.client_type = enClient;
	stRes.trig_mode = enTrigMode;

	/*if HW support*/
	//1.create instance
	enRet = sm_adl_create_resource(&fd, &stRes);
	if (enRet != E_SM_RET_OK) {
		if (enRet == E_SM_RET_FAIL_FUN_NOT_SUPPORT) {
			TCON_ERROR("adl client[%d] is not support. ret=%d\n",
					stRes.client_type, enRet);
			bRet = FALSE;
			goto finally;
		}

		TCON_ERROR("adl client[%d] failed to create instance. ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else
		TCON_DEBUG("adl client[%d] buffer_cnt=%d fd=%d\n",
				stRes.client_type, stRes.buffer_cnt, fd);

	//2.get memory index
	enRet = sm_adl_get_mem_index(fd, &u8MemIndex);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] get mem index fail. ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else
		TCON_DEBUG("adl memory index = %d\n", u8MemIndex);

	//3.get adl index info
	enRet = sm_adl_get_info(fd, u8MemIndex, &stIndexinfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] get index info ret=%d\n",
		stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else {
		TCON_DEBUG("memory size=%d, index=%d, phybuf_addr=0x%llx\n",
				stIndexinfo.buf_size,
				u8MemIndex,
				stIndexinfo.phybuf_addr);

		if ((u32TblSize > stIndexinfo.buf_size) || (u32TblSize <= 0) ||
			(stIndexinfo.phybuf_addr == 0)) {
			TCON_ERROR("Buf got from kernel is wrong. Table size=%d\n",
					u32TblSize);
			bRet = FALSE;
			goto finally;
		}
	}

	//4.write cmd
	stDatainfo.sub_client = E_SM_ADL_SUB_CLIENT_MAX;
	stDatainfo.mem_index = u8MemIndex;
	stDatainfo.data = pu8Tbl;
	stDatainfo.dataSize = u32TblSize;
	enRet = sm_adl_add_cmd(fd, &stDatainfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] write ret=%d\n", stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	}

	//5.fire
	stFireinfo.mem_index = u8MemIndex;
	stFireinfo.cmdcnt = XC_AUTODOWNLOAD_OVERDRIVER_DEPTH * BYTE_PER_WORD;
	enRet = sm_adl_fire(fd, &stFireinfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] fire ret=%d\n", stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	}

	//6. check status
	while (stStatusinfo.checkdone == 0 && u16Timeout < MAX_DELAY_TIME) {
		//use ADL to set panel gamma table, use ML to set panel gamma enable
		//but ML trigger early ADL so that it shows garbage in AC on
		//so delay 1 vsync for ML
		mdelay(ONE_VSYNC_TIME_MS);

		stStatusinfo.mem_index = u8MemIndex;
		enRet = sm_adl_checkstatus(fd, &stStatusinfo);
		if (enRet != E_SM_RET_OK) {
			TCON_ERROR("adl client[%d] check status ret=%d\n",
					stRes.client_type, enRet);
			bRet = FALSE;
			goto finally;
		} else {
			TCON_DEBUG("adl client[%d] checkdone = %d\n",
					stRes.client_type, stStatusinfo.checkdone);
			bRet = stStatusinfo.checkdone ? TRUE : FALSE;
		}
		u16Timeout++;
	}

finally:
	//7. release memory index
	enRet = sm_adl_release_mem_index(fd, u8MemIndex);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] release mem index ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
	};

	//8. destroy instance
	enRet = sm_adl_destroy_resource(fd);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] destroy instance ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
	};

	return bRet;
}

bool set_vrr_pnlgamma_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize,
			enum en_sm_adl_client enClient, enum en_sm_adl_trigger_mode enTrigMode,
			uint32_t cmdcnt)
{
	bool bRet = TRUE;
	enum sm_return_type enRet = E_SM_RET_FAIL;
	int fd = 0;
	__u8 u8MemIndex = 0;
	__u16 u16Timeout = 0;
	struct sm_adl_res stRes;
	struct sm_adl_add_info stDatainfo;
	struct sm_adl_fire_info stFireinfo;
	struct sm_adl_info stIndexinfo;
	struct sm_adl_status stStatusinfo;

	if (!pu8Tbl) {
		TCON_ERROR("pu8Tbl is NULL parameter\n");
		return FALSE;
	}

	memset(&stRes, 0, sizeof(struct sm_adl_res));
	memset(&stDatainfo, 0, sizeof(struct sm_adl_add_info));
	memset(&stFireinfo, 0, sizeof(struct sm_adl_fire_info));
	memset(&stIndexinfo, 0, sizeof(struct sm_adl_info));
	memset(&stStatusinfo, 0, sizeof(struct sm_adl_status));

	stRes.client_type = enClient;
	stRes.trig_mode = enTrigMode;

	/*if HW support*/
	//1.create instance
	enRet = sm_adl_create_resource(&fd, &stRes);
	if (enRet != E_SM_RET_OK) {
		if (enRet == E_SM_RET_FAIL_FUN_NOT_SUPPORT) {
			TCON_ERROR("adl client[%d] is not support. ret=%d\n",
					stRes.client_type, enRet);
			bRet = FALSE;
			goto finally;
		}

		TCON_ERROR("adl client[%d] failed to create instance. ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else
		TCON_DEBUG("adl client[%d] buffer_cnt=%d fd=%d\n",
				stRes.client_type, stRes.buffer_cnt, fd);

	//2.get memory index
	enRet = sm_adl_get_mem_index(fd, &u8MemIndex);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] get mem index fail. ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else
		TCON_DEBUG("adl memory index = %d\n", u8MemIndex);

	//3.get adl index info
	enRet = sm_adl_get_info(fd, u8MemIndex, &stIndexinfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] get index info ret=%d\n",
		stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	} else {
		TCON_DEBUG("memory size=%d, index=%d, phybuf_addr=0x%llx\n",
				stIndexinfo.buf_size,
				u8MemIndex,
				stIndexinfo.phybuf_addr);

		if ((u32TblSize > stIndexinfo.buf_size) || (u32TblSize <= 0) ||
			(stIndexinfo.phybuf_addr == 0)) {
			TCON_ERROR("Buf got from kernel is wrong. Table size=%d\n",
					u32TblSize);
			bRet = FALSE;
			goto finally;
		}
	}

	//4.write cmd
	stDatainfo.sub_client = E_SM_ADL_SUB_CLIENT_MAX;
	stDatainfo.mem_index = u8MemIndex;
	stDatainfo.data = pu8Tbl;
	stDatainfo.dataSize = u32TblSize;
	enRet = sm_adl_add_cmd(fd, &stDatainfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] write ret=%d\n", stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	}

	//5.fire
	stFireinfo.mem_index = u8MemIndex;
	stFireinfo.cmdcnt = cmdcnt;
	enRet = sm_adl_fire(fd, &stFireinfo);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] fire ret=%d\n", stRes.client_type, enRet);
		bRet = FALSE;
		goto finally;
	}

	//6. check status
	while (stStatusinfo.checkdone == 0 && u16Timeout < MAX_DELAY_TIME) {
		//use ADL to set panel gamma table, use ML to set panel gamma enable
		//but ML trigger early ADL so that it shows garbage in AC on
		//so delay 1 vsync for ML
		mdelay(ONE_VSYNC_TIME_MS);

		stStatusinfo.mem_index = u8MemIndex;
		enRet = sm_adl_checkstatus(fd, &stStatusinfo);
		if (enRet != E_SM_RET_OK) {
			TCON_ERROR("adl client[%d] check status ret=%d\n",
					stRes.client_type, enRet);
			bRet = FALSE;
			goto finally;
		} else {
			TCON_DEBUG("adl client[%d] checkdone = %d\n",
					stRes.client_type, stStatusinfo.checkdone);
			bRet = stStatusinfo.checkdone ? TRUE : FALSE;
		}
		u16Timeout++;
	}

finally:
	//7. release memory index
	enRet = sm_adl_release_mem_index(fd, u8MemIndex);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] release mem index ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
	};

	//8. destroy instance
	enRet = sm_adl_destroy_resource(fd);
	if (enRet != E_SM_RET_OK) {
		TCON_ERROR("adl client[%d] destroy instance ret=%d\n",
				stRes.client_type, enRet);
		bRet = FALSE;
	};

	return bRet;
}
