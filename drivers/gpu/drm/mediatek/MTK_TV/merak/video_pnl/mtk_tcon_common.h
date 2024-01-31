/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_TCON_COMMON_H_
#define _MTK_TCON_COMMON_H_

#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_video_panel.h"
#include "drv_scriptmgt.h"
#include <linux/vmalloc.h>

#define VERSION4                    (0x004)
#define VERSION5                    (0x005)

//#define for TCON.BIN
#define TCON20_VERSION              (2)
#define SKIP_TCON_SUBNUM            (34)
#define SKIP_TCON_MAIMHEADER        (32)
#define SKIP_TCON_SUBHEADER         (8)
#define SKIP_TCON20_SUBHEADER       (40)
#define SKIP_TCON20_PANETYPE        (2)

#define TCON_BIN_VERSION_RGBW       (1)
#define TCON_BIN_HEADER_BYTE_NUBMER(byte_n) (byte_n)

#define MAX_DELAY_TIME              (10)
#define ONE_VSYNC_TIME_MS           (8) /* 120hz */
#define ALIGN_8(x)                  (((x) + 0x7) & (~0x7))

#define TCON_LOG_TAG                "TCON"

typedef enum {
	EN_TCON_LOG_LEVEL_DISABLE = 0,
	EN_TCON_LOG_LEVEL_ERROR = 0x01,
	EN_TCON_LOG_LEVEL_INFO = 0x02,
	EN_TCON_LOG_LEVEL_DEBUG = 0x04,
	EN_TCON_LOG_LEVEL_INVALID = 0x1000,
	EN_TCON_LOG_LEVEL_MAX = EN_TCON_LOG_LEVEL_INVALID
} EN_TCON_LOG_LEVEL;

typedef enum {
	/* input vfreq <= default max refresh rate */
	EN_TCON_MODE_DEFAULT = 0,
	/* high frame rate, ex: default max refresh rate < input vfreq <= HFR max refresh rate */
	EN_TCON_MODE_HFR,
	/* high pixel colck, ex: input freq = 144hz */
	EN_TCON_MODE_HPC,
	/* game, ex: direct 60hz */
	EN_TCON_MODE_GAME,
	EN_TCON_MODE_MAX
} EN_TCON_MODE;

//define a marco with the tcon type of tcon tabs
typedef enum {
	E_TCON_TAB_TYPE_GENERAL,
	E_TCON_TAB_TYPE_GPIO,
	E_TCON_TAB_TYPE_SCALER,
	E_TCON_TAB_TYPE_MOD,
	E_TCON_TAB_TYPE_GAMMA,
	E_TCON_TAB_TYPE_POWER_SEQUENCE_ON,
	E_TCON_TAB_TYPE_POWER_SEQUENCE_OFF,
	E_TCON_TAB_TYPE_PANEL_INFO,
	E_TCON_TAB_TYPE_OVERDRIVER,
	E_TCON_TAB_TYPE_PCID,
	E_TCON_TAB_TYPE_PATCH,
	E_TCON_TAB_TYPE_LINE_OD_TABLE, //11
	E_TCON_TAB_TYPE_LINE_OD_REG,
	E_TCON_TAB_TYPE_VAC_REG,
	E_TCON_TAB_TYPE_PMIC, //14
	E_TCON_TAB_TYPE_VAC_TABLE, //15
	E_TCON_TAB_TYPE_COUNT,
	E_TCON_TAB_TYPE_NULL,
} EN_TCON_TAB_TYPE;

enum EN_TCON_CMD_TYPE {
	E_TCON_CMD_OD_EN = 0,
	E_TCON_CMD_OD_BYPASS,
	E_TCON_CMD_LINEOD_EN,
	E_TCON_CMD_PCID_EN,
	E_TCON_CMD_PCID_BYPASS,
	E_TCON_CMD_VAC_EN,
	E_TCON_CMD_VAC_BYPASS,
	E_TCON_CMD_DGA_EN,
	E_TCON_CMD_DGA_BYPASS,
	E_TCON_CMD_DGA_RESET,
	E_TCON_CMD_LOG_LEVEL,
	E_TCON_CMD_ONOFF,
	E_TCON_CMD_PMIC_LSIC_RELOAD,
	E_TCON_CMD_MAX
};

static const char * const TCON_TAB_TYPE_STRING[] = {
	"General",
	"Gpio",
	"Scaler",
	"Mod",
	"Gamma",
	"Power Seq on",
	"Power Seq off",
	"Panel info",
	"Over Driver",
	"Pixel OD",
	"Patch",
	"Line od table",
	"Line od reg",
	"VAC reg",
	"PMIC",
	"VAC table",
	"Count",
	"Null",
};

typedef struct {
	uint8_t u8TconType;             //IN: tcon tab type
	uint8_t *pu8Table;              //IN & OUT: the pointer to tcon tab table
	uint8_t u8Version;              //OUT:
	uint16_t u16RegCount;           //OUT:
	uint8_t  u8RegType;             //OUT:
	uint32_t u32ReglistOffset;      //OUT:
	uint32_t u32ReglistSize;        //OUT:
	uint8_t u8PanelInterface;       //OUT:
} st_tcon_tab_info;

#define DEFAULT_TCON_LOG_LEVEL      (EN_TCON_LOG_LEVEL_ERROR|EN_TCON_LOG_LEVEL_DEBUG)

extern EN_TCON_LOG_LEVEL g_enTconLogLevel;

#define TCON_ERROR(msg, ...) \
	pr_err("[%s] %s:%d: "msg, TCON_LOG_TAG, __func__, __LINE__, ##__VA_ARGS__)

#define TCON_INFO(msg, ...) \
	do { \
		if (g_enTconLogLevel&EN_TCON_LOG_LEVEL_INFO) \
			pr_info("[%s] %s:%d: "msg, TCON_LOG_TAG, \
				__func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define TCON_DEBUG(msg, ...) \
	do { \
		if (g_enTconLogLevel&EN_TCON_LOG_LEVEL_DEBUG) \
			pr_info("[%s] %s:%d: "msg, TCON_LOG_TAG, \
				__func__, __LINE__, ##__VA_ARGS__); \
	} while (0)


#define TCON_FUNC_ENTER() TCON_DEBUG(" >>>\n")
#define TCON_FUNC_EXIT(ret) TCON_DEBUG(" <<< ....'%s'\n", ret ? "OK" : "Fail")
#define TCON_FUNC_EXIT_ERR(fmt, args...) TCON_ERROR(" <<< "fmt, ##args)

#define TCON_CHECK_EQUAL_AND_ASSIGN(a, b, c) \
	do { \
		if ((b) != (c)) { \
			TCON_DEBUG("%s: %u -> %u\n", a, (uint32_t)(b), (uint32_t)(c)); \
			(b) = (c); \
		} else { \
			TCON_INFO("%s: %u -> the same\n", a, (uint32_t)(b)); \
		} \
	} while (0)

#define PNL_MALLOC_MEM(pu8Addr, size, bRet) \
	do { \
		pu8Addr = vmalloc(size); \
		if (pu8Addr == NULL) { \
			TCON_ERROR("malloc fail.\n"); \
			bRet &= FALSE; \
		} else { \
			memset(pu8Addr, 0, size); \
			bRet &= TRUE; \
		} \
	} while (0)

#define PNL_FREE_MEM(pu8Addr) \
	do { \
		if (pu8Addr != NULL) { \
			vfree(pu8Addr); pu8Addr = NULL; \
		} \
	} while (0)

#define TCON_GET_RET(x) ((x == 0) ? TRUE : FALSE)

bool init_log_level(const uint32_t u32Level);
int get_tcon_common_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize);
bool set_tcon_mode(EN_TCON_MODE enMode);
EN_TCON_MODE get_tcon_mode(void);
bool load_tcon_files(struct mtk_panel_context *pCon);
bool is_tcon_data_exist(unsigned char **ppdata, loff_t *plen);
bool is_tcon_power_seq_data_exist(unsigned char **ppdata, loff_t *plen, bool bOn);
bool is_pnlgamma_data_exist(unsigned char **ppdata, loff_t *plen);
bool get_tcon_dump_table(st_tcon_tab_info *pstInfo);
bool get_tcon_version(uint8_t *pu8Table, uint8_t *pu8Version);
bool release_tcon_resource(void);
bool write_byte_reg(uint32_t u32Addr, uint8_t u8Val, uint8_t u8Mask);
bool write_2byte_reg(uint32_t u32Addr, uint16_t u16Val, uint16_t u16Mask);
bool is_adl_support(enum en_sm_adl_client enClient);
bool wait_tcon_setting_done(struct mtk_panel_context *pCon, struct completion *pComp);
bool set_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize,
			enum en_sm_adl_client enClient, enum en_sm_adl_trigger_mode enTrigMode);
bool set_vrr_od_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize,
			enum en_sm_adl_client enClient, enum en_sm_adl_trigger_mode enTrigMode);
bool set_vrr_pnlgamma_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize,
			enum en_sm_adl_client enClient, enum en_sm_adl_trigger_mode enTrigMode,
			uint32_t cmdcnt);
bool load_vrr_tcon_files(struct mtk_panel_context *pCon);
bool is_vrr_tcon_data_exist(unsigned char **ppdata, loff_t *plen, unsigned int index);
bool is_vrr_pnlgamma_data_exist(unsigned char **ppdata, loff_t *plen, unsigned int index);
bool load_vrr_pga_files(struct mtk_panel_context *pCon);

#define TCON_W1BYTEMSK(u32Addr, u8Val, u8Mask) write_byte_reg(u32Addr, u8Val, u8Mask)
#define TCON_W2BYTEMSK(u32Addr, u16Val, u16Mask) write_2byte_reg(u32Addr, u16Val, u16Mask)

#endif
