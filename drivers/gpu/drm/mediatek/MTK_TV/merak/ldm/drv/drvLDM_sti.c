// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

//-------------------------------------------------------------------------------------------------
//  Include Files
//-------------------------------------------------------------------------------------------------
#ifdef MSOS_TYPE_LINUX_KERNEL
#include <linux/string.h>
#include <linux/namei.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/firmware.h>
#else
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#endif
//#include "MsCommon.h"
//#include "MsOS.h"
#include "drvLDM_sti.h"
#include <linux/vmalloc.h>
#include "drv_scriptmgt.h"
#include "pqu_msg.h"
#include "ext_command_render_if.h"
#include "mtk_tv_drm_log.h"
#include "pqu_render.h"
#include "ext_command_r2_if.h"
#include "ext_command_client_api.h"
//#include "ULog.h"
//#include "mdrv_ldm_algorithm.h"



//#include "regLDM.h"
//#include "halLDM.h"
//#include "drvMMIO.h"
//#include "utopia.h"
//#include "sti_msos.h"
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);


//#if defined(MSOS_TYPE_LINUX)
//#include <sys/ioctl.h>
//#endif
//FIXME
#define MTK_DRM_MODEL MTK_DRM_MODEL_LD

#define LDMMAPLENGTH                0x800000    //mmap length

#define BDMA_CONFIG_REG_NUM 7

#define MAX_BDMA_REG_NUM 8

#define MASK_START_BIT0	0
#define MASK_LENGTH_4BYTE 32

#define SleepTime 200
#define RETRYTIMES 2

#define CHECK_SPI_TIMES 0x20

#define DEFAULT_VALUE           (0x00)

#define LD_COMPENSATION_LENGTH      0x200       //Compensation.bin length
#define MEMALIGN_SIZE                    0xFF           //align size
#define MEMALIGN                    8           //align bits
#define LD_BIN_OFFSET			0x20        //bin header length
#define LD_10K_BIN_OFFSET			0x30        //bin header length
#define LDM_BIN_CHECK_POSITION	4    //according bin file
#define GAMMA_TABLE_LEN     256

#define HIGH_BYTE_16BIT_BASE	0x100
#define DTS_INFO_DRAM_SIZE		0x100

#define REF_WIDTH_OFFSET	0x20
#define REF_HEIGHT_OFFSET	0x22
#define LD10K_INIX_OFFSET	0x24

#define BACKLIGHTGAMMA_VER_OFFSET 0x1C

#define BIN_NAME 0x0C
#define BIN_VER1 0x1A
#define BIN_VER2 0x1B

#define MAPPING_TABLE_DRAM_SIZE		(0x300UL)
#define KNOWN_LED_DEVICE_DRAM_SIZE		(3UL)
#define BOOSTING_DRAM_SIZE		(3UL)
#define LDM_BACKLIGHT_DRAM_SIZE		(2UL)

#define COMPENSATION_BIN    "Compensation.bin"
#define EDGE2D_BIN          "Edge2D.bin"
#define BACKLIGHTGAMMA_BIN  "BacklightGamma.bin"
#define EDGE2D_OLED_BIN     "Edge2D_Oled.bin"
#define EDGE2D_GD_BIN     "Edge2D_GD.bin"

#define LD_AHB3_WA0_BIN     "ld_ahb3_wa0.bin"
#define LD_AHB4_WA0_BIN     "ld_ahb4_wa0.bin"
#define LD_AHB5_WA0_BIN     "ld_ahb5_wa0.bin"
#define LD_AHB3_WA1_BIN     "ld_ahb3_wa1_bdma.bin"
#define LD_AHB4_WA1_BIN     "ld_ahb4_wa1_bdma.bin"
#define LD_AHB5_WA1_BIN     "ld_ahb5_wa1_bdma.bin"

#define LD_TGEN_V_540 0x21C
#define LD_TGEN_V_1080 0x438
#define LD_TGEN_V_2160 0x870
#define LD_TGEN_V_4320 0x10E0

static uint16_t u16_current_tgen_v;
static uint16_t u16_pre_tgen_v;
static uint16_t global_strength;

//-------------------------------------------------------------------------------------------------
//  Driver Compiler Options
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------
#define WAIT_CNT	200
//-------------------------------------------------------------------------------------------------
//  Local Structurs
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
//  Global Variables
//-------------------------------------------------------------------------------------------------
//ST_DRV_LD_CUS_PATH stCusPath;
struct LD_CUS_PATH stCusPath;

struct pqu_render_ldm_init_param ldm_init_param;
struct pqu_render_ldm_init_reply_param ldm_init_reply_param;

struct pqu_render_ldm_suspend_param ldm_suspend_param;
struct pqu_render_ldm_suspend_reply_param ldm_suspend_reply_param;

struct LDM_BIN_HEADER ldm_bin_header[E_LDM_BIN_TYPE_MAX];

struct file *filp;
char *bufferAHB0;
char *buffer;
void __iomem *u64Vaddr;
u64 ld_addr_base;
void __iomem *u64Ld_Backlight_VA;

u64 u64Compensation_Addr;
u64 u64Compensation_mem_size;
u64 u64Compensation_offset;

u64 u64Edge2D_Addr;
u64 u64Edge2D_mem_size;
u64 u64Edge2D_bin_size;
u64 u64Edge2D_data_size;
u64 u64Edge2D_offset;

u64 u64Backlight_Gamma_Addr;
u64 u64Backlight_Gamma_mem_size;
u64 u64Backlight_Gamma_bin_size;
u64 u64Backlight_Gamma_data_size;

u64 u64Backlight_offset;

/* dts parameter addr*/
u64 u64Dts_ld_panel_Addr;
u64 u64Dts_ld_panel_mem_size = DTS_INFO_DRAM_SIZE;
u64 u64Dts_ld_panel_offset;

u64 u64Dts_ld_backlight_Addr;
u64 u64Dts_ld_backlight_mem_size = DTS_INFO_DRAM_SIZE * LDM_BACKLIGHT_DRAM_SIZE;
u64 u64Dts_ld_backlight_offset;

u64 u64Dts_ld_misc_Addr;
u64 u64Dts_ld_misc_mem_size = DTS_INFO_DRAM_SIZE;
u64 u64Dts_ld_misc_offset;

u64 u64Dts_ld_mspi_Addr;
u64 u64Dts_ld_mspi_mem_size = DTS_INFO_DRAM_SIZE;
u64 u64Dts_ld_mspi_offset;

u64 u64Dts_ld_dma_Addr;
u64 u64Dts_ld_dma_mem_size = DTS_INFO_DRAM_SIZE;
u64 u64Dts_ld_dma_offset;

u64 u64Dts_ld_boosting_Addr;
u64 u64Dts_ld_boosting_mem_size = DTS_INFO_DRAM_SIZE * BOOSTING_DRAM_SIZE;
u64 u64Dts_ld_boosting_offset;

u64 u64Dts_ld_led_device_Addr;
u64 u64Dts_ld_led_device_mem_size = DTS_INFO_DRAM_SIZE * MAPPING_TABLE_DRAM_SIZE;
u64 u64Dts_ld_led_device_offset;

u64 u64Dts_ld_led_device_as3824_Addr;
u64 u64Dts_ld_led_device_as3824_mem_size = DTS_INFO_DRAM_SIZE * KNOWN_LED_DEVICE_DRAM_SIZE;
u64 u64Dts_ld_led_device_as3824_offset;

u64 u64Dts_ld_led_device_nt50585_Addr;
u64 u64Dts_ld_led_device_nt50585_mem_size = DTS_INFO_DRAM_SIZE * KNOWN_LED_DEVICE_DRAM_SIZE;
u64 u64Dts_ld_led_device_nt50585_offset;

u64 u64Dts_ld_led_device_iw7039_Addr;
u64 u64Dts_ld_led_device_iw7039_mem_size = DTS_INFO_DRAM_SIZE * KNOWN_LED_DEVICE_DRAM_SIZE;
u64 u64Dts_ld_led_device_iw7039_offset;

u64 u64Dts_ld_led_device_mbi6353_Addr;
u64 u64Dts_ld_led_device_mbi6353_mem_size = DTS_INFO_DRAM_SIZE * KNOWN_LED_DEVICE_DRAM_SIZE;
u64 u64Dts_ld_led_device_mbi6353_offset;

u64 u64LDF_Addr;


const __u8 *pu8CompTable;
const __u8 *pu8CompTable1;
const __u8 *pu8CompTable2;
const __u8 *pu8Edge2DTable;
__u8 *pu8GammaTable;

__u8 *pTbl_LD_Gamma[16] = {
NULL, // NULL indicates linear
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL
};

__u8 *pTbl_LD_Remap[16] = {
NULL, // NULL indicates linear
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL
};

//local dimming miu read / write  use va:u64Vaddr, not pa:addr_base_L
#define MDrv_LD_MIUReadByte(addr_base_L, offset)  (*((unsigned char *)\
(u64Vaddr + (addr_base_L) + (offset))))
#define MDrv_LD_MIURead2Bytes(addr_base_L, offset) (*((__u16 *)\
(u64Vaddr + (addr_base_L) + (offset))))
#define MDrv_LD_MIUWriteByte(addr_base_L, offset, value) ((*((unsigned char *)\
(u64Vaddr + (addr_base_L) + (offset)))) = ((__u8)(value)))
#define MDrv_LD_MIUWrite2Bytes(addr_base_L, offset, val) ((*((__u16 *)\
(u64Vaddr + (addr_base_L) + (offset)))) = (__u16)(val))

//-------------------------------------------------------------------------------------------------
//  Local Variables
//-------------------------------------------------------------------------------------------------
static enum EN_LDM_DEBUG_LEVEL gLDMDbgLevel = E_LDM_DEBUG_LEVEL_ERROR;
//static bool gbLDMInitialized = FALSE;
//static u8 gu8LDMStatus;
//static u8 gLDMTemp;

//static bool bLdm_Inited = FALSE;
//static MS_S32 _gs32LDM_Mutex;
//LDM mutex wait time
//#define LDM_MUTEX_WAIT_TIME    3000
static DEFINE_MUTEX(Semutex_LD_BDMA);


//-------------------------------------------------------------------------------------------------
//  Debug Functions
//-------------------------------------------------------------------------------------------------
#define LDM_DBG_FUNC()	\
do {				\
	if (gLDMDbgLevel >= E_LDM_DEBUG_LEVEL_ALL)\
		printf("[LDM]: %s: %d:\n", __func__, __LINE__);\
} while (0)

#define LDM_DBG_INFO(msg...)	\
do {\
	if (gLDMDbgLevel >= E_LDM_DEBUG_LEVEL_ERROR/*E_LDM_DEBUG_LEVEL_INFO */)\
		printf("[LDM INFO]: %s: %d: ", __func__, __LINE__);\
		printf(msg);\
} while (0)

#define LDM_DBG_ERR(msg...)	\
do {\
	if (gLDMDbgLevel >= E_LDM_DEBUG_LEVEL_ERROR)\
		printf("[LDM ERROR]: %s: %d: ", __func__, __LINE__);\
		printf(msg);\
} while (0)

#define LDM_DBG_WARN(msg...)	\
do {\
	if (gLDMDbgLevel >= E_LDM_DEBUG_LEVEL_WARNING) \
		printf("[LDM WARNING]: %s: %d: ", __func__, __LINE__);\
		printf(msg); \
} while (0)


static char *ld_config_paths[] = {
	"/vendor/firmware/",
	"/lib/firmware/",
	"/mnt/vendor/config/ldm/",
	"/vendor/tvconfig/config/ldm/",
	"/tvconfig/config/ldm/",
	"/config/ldm/",
};

static char *ld_config_paths_root[] = {
	"../../vendor/firmware/",
	"../../lib/firmware/",
	"../../mnt/vendor/config/ldm/",
	"../../vendor/tvconfig/config/ldm/",
	"../../tvconfig/config/ldm/",
	"../../config/ldm/",
};

static struct ConfigFile_t stLdConfig[] = {
{
.size = 0,
.p    = NULL,
.name = COMPENSATION_BIN,
},
{
.size = 0,
.p    = NULL,
.name = EDGE2D_BIN,
},
{
.size = 0,
.p    = NULL,
.name = BACKLIGHTGAMMA_BIN,
},
{
.size = 0,
.p    = NULL,
.name = EDGE2D_OLED_BIN,
},
{
.size = 0,
.p    = NULL,
.name = EDGE2D_GD_BIN,
},
};

static void pqu_ready_ldm_init(void)
{
	int ret = 0;


	DRM_INFO("[LDM][%s,%5d] pqu notify callback\n", __func__, __LINE__);

	ret = pqu_render_ldm_init(&ldm_init_param, NULL);

	if (ret != 0)
		DRM_ERROR("[LDM]cpuif ready buf API fail");
}



/* firmware upload */
static int firmware_load(struct mtk_tv_kms_context *pctx, char *filename,
			 char **fw, size_t *fw_len)
{
	const struct firmware *fw_entry;
	int ret = 0;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}
	if (filename == NULL) {
		DRM_ERROR("[LDM][%s,%5d] filename is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	DRM_INFO("[LDM]%s: LDM Bin device inserted  %s\n", __func__, filename);

	do {
		ret = request_firmware(&fw_entry, filename, pctx->dev);
	} while (ret == -EAGAIN);

	if (ret) {
		DRM_ERROR("[LDM]%s: Firmware not available,error:%d\n", __func__, ret);
		release_firmware(fw_entry);
		return ret;
	}

	DRM_INFO("[LDM]%s: Firmware: Data:%p, size:%zx\n",
	__func__, fw_entry->data, fw_entry->size);

	*fw_len = fw_entry->size;

	*fw = vmalloc(fw_entry->size);

	if (*fw == NULL) {
		DRM_ERROR("[LDM]vmalloc error\n");
		release_firmware(fw_entry);
		ret = -ENOMEM;
		return ret;
	}

	memcpy(*fw, fw_entry->data, fw_entry->size);

	DRM_INFO("[LDM]%s: Firmware: memcpy  new_va:%p, data:%p\n",
	__func__, *fw, fw_entry->data);

	release_firmware(fw_entry);

	return ret;
}

bool is_path_exist(const char *path)
{
	struct path stPath;
	int s32Error = 0;

	memset(&stPath, 0, sizeof(struct path));

	if (!path || !*path)
		return false;

	s32Error = kern_path(path, LOOKUP_FOLLOW, &stPath);
	if (s32Error)
		return false;

	path_put(&stPath);
	return true;
}

bool MDrv_LD_PrepareConfigFile(struct mtk_tv_kms_context *pctx, struct LD_CUS_PATH *pstCusPath)
{
	int ret = 0, i;
	char configPath[MAX_PATH_LEN];
	char path[MAX_PATH_LEN] = {0};
	int n;
	char path_root[MAX_PATH_LEN] = "../..";

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	if (pstCusPath == NULL) {
		DRM_ERROR("[LDM][%s,%5d] Config path error:null\n", __func__, __LINE__);
		return false;
	}

	memset(configPath, 0, MAX_PATH_LEN);



	if (strlen(pstCusPath->aCusPath)) {
		if (is_path_exist((const char *)pstCusPath->aCusPath)) {
			//strncpy(configPath,pstCusPath->aCusPath,MAX_PATH_LEN-1);
			strncpy(configPath, path_root, MAX_PATH_LEN-1);
			strncat(configPath, pstCusPath->aCusPath, MAX_PATH_LEN-1);
			configPath[sizeof(configPath) - 1] = '\0';
		} else {
			DRM_ERROR(
			"[LDM]Config path error aCusPath:%s not exists\n",
			pstCusPath->aCusPathU);
			return false;
		}
	} else if (strlen(pstCusPath->aCusPathU)) {
		if (is_path_exist((const char *)pstCusPath->aCusPathU)) {
			strncpy(configPath, pstCusPath->aCusPathU, MAX_PATH_LEN-1);
			configPath[sizeof(configPath) - 1] = '\0';
		} else {
			DRM_ERROR(
			"[LDM]Config path error aCusPathU:%s not exists\n",
			pstCusPath->aCusPathU);
			return false;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(ld_config_paths); i++) {
			if (is_path_exist(ld_config_paths[i])) {
				DRM_INFO("[LDM]trace! find configPath path:%s\n",
				ld_config_paths[i]);
				strncpy(configPath, ld_config_paths_root[i], MAX_PATH_LEN-1);
				configPath[sizeof(configPath) - 1] = '\0';
				break;
			}
			DRM_INFO("[LDM]trace! skip configPath path:%s\n",
			ld_config_paths[i]);
		}
	}

	if (strlen(configPath) == 0) {
		DRM_INFO("[LDM]Config path error:null\n");
		return E_LDM_RET_FAIL;
	}

	DRM_INFO("[LDM]trace! using configPath path:%s\n", configPath);

	for (i = 0; i < ARRAY_SIZE(stLdConfig); i++) {
		n = snprintf(path, MAX_PATH_LEN-1, "%s%s", configPath, stLdConfig[i].name);
		if (n < 0 || n >= sizeof(path)) {
			/* Handle snprintf() error */
			DRM_INFO("[LDM]configPath unknown error");
			return E_LDM_RET_FAIL;
		}
		ret = firmware_load(
			pctx, path,
			&stLdConfig[i].p, &stLdConfig[i].size);
		if (ret) {
			DRM_ERROR("[LDM]Load Bin error file name:%s not exists\n",
			stLdConfig[i].name);
			DRM_ERROR("[LDM][%s,%5d] fail\n", __func__, __LINE__);
			if (stLdConfig[i].p != NULL)
				vfree((void *)stLdConfig[i].p);
			return E_LDM_RET_FAIL;
		}
		DRM_INFO("[LDM]data:%p  size:%zx\n", stLdConfig[i].p, stLdConfig[i].size);
	}
	return E_LDM_RET_SUCCESS;
}

struct ConfigFile_t *MDrv_LD_GetConfigFile(const char *configName)
{
	int i;

	if (configName == NULL) {
		DRM_ERROR("[LDM]configName is null\n");
		return NULL;
	}
	for (i = 0; i < ARRAY_SIZE(stLdConfig); i++) {
		if (strcmp(configName, stLdConfig[i].name) == 0)
			return &stLdConfig[i];
	}
	DRM_ERROR("[LDM]config %s not found\n", configName);
	return NULL;
}

bool MDrv_LD_CheckData(char *buff, int buff_len)
{
	__u16 u16Checked = 0;
	__u32 u32Counter = 0;
	__u64 u64Sum = 0;

	if (buff == NULL) {
		DRM_ERROR("[LDM]error! %s:%d, parametre error Pointer is null\n",
		__func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	//before checked data
	for (; u32Counter < LDM_BIN_CHECK_POSITION; u32Counter++)
		u64Sum += *(buff+u32Counter);

	u16Checked = *(buff + LDM_BIN_CHECK_POSITION) + ((*(buff+LDM_BIN_CHECK_POSITION+1))<<8);

	//after checked to 0xbuff_len
	for (u32Counter = LDM_BIN_CHECK_POSITION + 2; u32Counter < buff_len; u32Counter++)
		u64Sum += *(buff + u32Counter);

	DRM_INFO("[LDM]buff_len:%d, u16Checked:0x%x, u64Sum:0x%llx\n",
	buff_len, u16Checked, u64Sum);
	if (u16Checked != (u64Sum&0xFFFF))
		return E_LDM_RET_FAIL;

	return E_LDM_RET_SUCCESS;
}


void MDrv_LD_ADL_ALL_CompensationTable(__u32 addr, const void *pCompTable, const void *pCompTable2)
{
	int i = 0;

	const __u16 *pu16CompTable = pCompTable;
	const __u16 *pu16CompTable2 = pCompTable2;
	__u8 u8Byte0, u8Byte1, u8Byte2;

	if (pu16CompTable2 && pu16CompTable) {
		for (i = 0; i < LD_COMPENSATION_LENGTH / 2; i++) {
			u8Byte0 = (pu16CompTable[i]) & 0xFF;
			u8Byte1 = ((pu16CompTable[i] >> 8) & 0xF)
			| (((pu16CompTable2[i]) & 0xF) << 4);
			u8Byte2 =  (pu16CompTable2[i] >> 4) & 0xFF;
			MDrv_LD_MIUWriteByte(addr - ld_addr_base, (32 * i) + 0, u8Byte0);
			MDrv_LD_MIUWriteByte(addr - ld_addr_base, (32 * i) + 1, u8Byte1);
			MDrv_LD_MIUWriteByte(addr - ld_addr_base, (32 * i) + 2, u8Byte2);
		}
	} else if (pu16CompTable) {
		for (i = 0; i < LD_COMPENSATION_LENGTH / 2; i++) {
			u8Byte0 = (pu16CompTable[i]) & 0xFF;
			u8Byte1 = ((pu16CompTable[i] >> 8) & 0xF);
			MDrv_LD_MIUWriteByte(addr - ld_addr_base, (32 * i) + 0, u8Byte0);
			MDrv_LD_MIUWriteByte(addr - ld_addr_base, (32 * i) + 1, u8Byte1);
		}
	} else {
		DRM_ERROR("[LDM]LD Compensation Table is NULL!\n");
		//return addr /= psDrvLdInfo->u16PackLength;
	}
	//MDrv_LD_FlushDRAMData((unsigned long)u64Vaddr + addr - ld_addr_base, addr, 8*1024);

	DRM_INFO("[LDM]LD ADL CompensationTable done:%x\n", addr);

}

__u8 _MDrv_LDM_get_share_memory_addr(struct mtk_tv_kms_context *pctx)
{
	struct mtk_tv_drm_metabuf metabuf;
	struct mtk_ld_context *pctx_ld = &pctx->ld_priv;
	int ret = E_LDM_RET_SUCCESS;
	u64 u64Paddr = 0;
	u32 u64Size = 0;

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf,
			MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_BACKLIGHT)) {
		DRM_ERROR("get metabuf MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_BACKLIGHT fail");
		return E_LDM_RET_FAIL;
	}

	u64Paddr = metabuf.mmap_info.phy_addr;
	u64Size = metabuf.size;
	u64Ld_Backlight_VA = memremap((size_t)(u64Paddr + pctx_ld->u32Cpu_emi0_base),
					u64Size,
					MEMREMAP_WB);

	mtk_tv_drm_metabuf_free(&metabuf);

	return ret;
}

__u8 MDrv_LDM_DRAM_Layout(struct mtk_tv_kms_context *pctx)
{
	int ret = E_LDM_RET_SUCCESS;

	struct mtk_ld_context *pctx_ld = NULL;
	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;
	struct drm_st_ld_led_device_as3824_info *pld_led_device_as3824_info = NULL;
	struct drm_st_ld_led_device_nt50585_info *pld_led_device_nt50585_info = NULL;
	struct drm_st_ld_led_device_iw7039_info *pld_led_device_iw7039_info = NULL;
	struct drm_st_ld_led_device_mbi6353_info *pld_led_device_mbi6353_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pctx_ld = &pctx->ld_priv;
	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);
	pld_led_device_as3824_info = &(pctx->ld_priv.ld_led_device_as3824_info);
	pld_led_device_nt50585_info = &(pctx->ld_priv.ld_led_device_nt50585_info);
	pld_led_device_iw7039_info = &(pctx->ld_priv.ld_led_device_iw7039_info);
	pld_led_device_mbi6353_info = &(pctx->ld_priv.ld_led_device_mbi6353_info);

	/*------------------------Layout-----------------------*/
		/*Compensation Bin*/
		/*Edge2d Bin*/
		/*Backlight_Gamma Bin*/
		/*DTS Panel info*/
		/*DTS Backlight info*/
		/*DTS MISC info*/
		/*DTS MSPI info*/
		/*DTS DMA info*/
		/*DTS Boosting info*/
		/*DTS LED info*/
		/*DTS Leddevice info*/
		/*LDF*/
		/*LDB*/
		/*-----------------------------------------------------*/


		/*BIN MEN ADDR*/
		u64Compensation_Addr = pctx_ld->u64LDM_phyAddress;
		u64Compensation_offset = u64Compensation_Addr - ld_addr_base;

		u64Edge2D_Addr = u64Compensation_Addr + u64Compensation_mem_size;
		u64Edge2D_offset = u64Edge2D_Addr - ld_addr_base;

		u64Backlight_Gamma_Addr = u64Edge2D_Addr + u64Edge2D_mem_size;
		u64Backlight_offset = u64Backlight_Gamma_Addr - ld_addr_base;

		/*DTS MEN ADDR*/
		u64Dts_ld_panel_Addr = u64Backlight_Gamma_Addr + u64Backlight_Gamma_mem_size;
		u64Dts_ld_panel_offset = u64Dts_ld_panel_Addr - ld_addr_base;

		u64Dts_ld_backlight_Addr = u64Dts_ld_panel_Addr + u64Dts_ld_panel_mem_size;
		u64Dts_ld_backlight_offset = u64Dts_ld_backlight_Addr - ld_addr_base;

		u64Dts_ld_misc_Addr = u64Dts_ld_backlight_Addr + u64Dts_ld_backlight_mem_size;
		u64Dts_ld_misc_offset = u64Dts_ld_misc_Addr - ld_addr_base;

		u64Dts_ld_mspi_Addr = u64Dts_ld_misc_Addr + u64Dts_ld_misc_mem_size;
		u64Dts_ld_mspi_offset = u64Dts_ld_mspi_Addr - ld_addr_base;

		u64Dts_ld_dma_Addr = u64Dts_ld_mspi_Addr + u64Dts_ld_mspi_mem_size;
		u64Dts_ld_dma_offset = u64Dts_ld_dma_Addr - ld_addr_base;

		u64Dts_ld_boosting_Addr = u64Dts_ld_dma_Addr + u64Dts_ld_dma_mem_size;
		u64Dts_ld_boosting_offset = u64Dts_ld_boosting_Addr - ld_addr_base;

		u64Dts_ld_led_device_Addr = u64Dts_ld_boosting_Addr + u64Dts_ld_boosting_mem_size;
		u64Dts_ld_led_device_offset = u64Dts_ld_led_device_Addr - ld_addr_base;

		switch (pld_led_device_info->eLEDDeviceType) {
		case E_LD_DEVICE_UNSUPPORT:
			DRM_INFO("[LDM]E_LD_DEVICE_UNSUPPORT\n");
			u64LDF_Addr = u64Dts_ld_led_device_Addr + u64Dts_ld_led_device_mem_size;
			break;
		case E_LD_DEVICE_AS3824:
			DRM_INFO("[LDM]E_LD_DEVICE_AS3824\n");
			u64Dts_ld_led_device_as3824_Addr
			= u64Dts_ld_led_device_Addr + u64Dts_ld_led_device_mem_size;
			u64LDF_Addr
			= u64Dts_ld_led_device_as3824_Addr + u64Dts_ld_led_device_as3824_mem_size;
			u64Dts_ld_led_device_as3824_offset
			= u64Dts_ld_led_device_as3824_Addr - ld_addr_base;
			break;
		case E_LD_DEVICE_NT50585:
			DRM_INFO("[LDM]E_LD_DEVICE_NT50585\n");
			u64Dts_ld_led_device_nt50585_Addr
			= u64Dts_ld_led_device_Addr + u64Dts_ld_led_device_mem_size;
			u64LDF_Addr
			= u64Dts_ld_led_device_nt50585_Addr + u64Dts_ld_led_device_nt50585_mem_size;
			u64Dts_ld_led_device_nt50585_offset
			= u64Dts_ld_led_device_nt50585_Addr - ld_addr_base;
			break;
		case E_LD_DEVICE_IW7039:
			DRM_INFO("[LDM]E_LD_DEVICE_IW7039\n");
			u64Dts_ld_led_device_iw7039_Addr
			= u64Dts_ld_led_device_Addr + u64Dts_ld_led_device_mem_size;
			u64LDF_Addr
			= u64Dts_ld_led_device_iw7039_Addr + u64Dts_ld_led_device_iw7039_mem_size;
			u64Dts_ld_led_device_iw7039_offset
			= u64Dts_ld_led_device_iw7039_Addr - ld_addr_base;
			break;
		case E_LD_DEVICE_MCU:
			DRM_INFO("[LDM]E_LD_DEVICE_MCU\n");
			u64LDF_Addr = u64Dts_ld_led_device_Addr + u64Dts_ld_led_device_mem_size;
			break;
		case E_LD_DEVICE_CUS:
			DRM_INFO("E_LD_DEVICE_CUS\n");
			u64LDF_Addr = u64Dts_ld_led_device_Addr + u64Dts_ld_led_device_mem_size;
			break;
		case E_LD_DEVICE_MBI6353:
			DRM_INFO("[LDM]E_LD_DEVICE_MBI6353\n");
			u64Dts_ld_led_device_mbi6353_Addr
			= u64Dts_ld_led_device_Addr + u64Dts_ld_led_device_mem_size;
			u64LDF_Addr
			= u64Dts_ld_led_device_mbi6353_Addr + u64Dts_ld_led_device_mbi6353_mem_size;
			u64Dts_ld_led_device_mbi6353_offset
			= u64Dts_ld_led_device_mbi6353_Addr - ld_addr_base;
			break;
		default:
			DRM_INFO("[LDM]E_LD_DEVICE_UNSUPPORT.\n");
			u64LDF_Addr = u64Dts_ld_led_device_Addr + u64Dts_ld_led_device_mem_size;
			break;
		}


		memset(u64Vaddr + u64Compensation_offset, 0, u64Compensation_mem_size);
		MDrv_LD_ADL_ALL_CompensationTable(
		u64Compensation_Addr, pu8CompTable1, pu8CompTable2);

		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Compensation_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Compensation_mem_size,
		1);
		DRM_INFO("[LDM]u64Compensation_Addr:0x%llx", u64Compensation_Addr);

		memset(u64Vaddr + u64Edge2D_offset, 0, u64Edge2D_data_size);
		memcpy(u64Vaddr + u64Edge2D_offset, pu8Edge2DTable, u64Edge2D_data_size);
		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Edge2D_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Edge2D_data_size,
		1);
		DRM_INFO("[LDM]u64Edge2D_Addr:0x%llx", u64Edge2D_Addr);

		memset(u64Vaddr + u64Backlight_offset, 0, u64Backlight_Gamma_data_size);
		memcpy(u64Vaddr + u64Backlight_offset, pu8GammaTable, u64Backlight_Gamma_data_size);
		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Backlight_Gamma_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Backlight_Gamma_data_size,
		1);
		DRM_INFO("[LDM]u64Backlight_Gamma_Addr:0x%llx", u64Backlight_Gamma_Addr);


		memset(u64Vaddr + u64Dts_ld_panel_offset, 0, u64Dts_ld_panel_mem_size);
		memcpy(u64Vaddr + u64Dts_ld_panel_offset,
		pld_panel_info, sizeof(struct drm_st_ld_panel_info));
		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Dts_ld_panel_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Dts_ld_panel_mem_size,
		1);
		DRM_INFO("[LDM]u64Dts_ld_panel_Addr:0x%llx", u64Dts_ld_panel_Addr);

		memset(u64Vaddr + u64Dts_ld_backlight_offset, 0, u64Dts_ld_backlight_mem_size);
		memcpy(u64Vaddr + u64Dts_ld_backlight_offset,
		u64Ld_Backlight_VA, sizeof(struct pqu_render_backlight));
		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Dts_ld_backlight_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Dts_ld_backlight_mem_size,
		1);
		DRM_INFO("u64Dts_ld_backlight_Addr:0x%llx", u64Dts_ld_backlight_Addr);

		memset(u64Vaddr + u64Dts_ld_misc_offset, 0, u64Dts_ld_misc_mem_size);
		memcpy(u64Vaddr + u64Dts_ld_misc_offset,
		pld_misc_info, sizeof(struct drm_st_ld_misc_info));
		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Dts_ld_misc_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Dts_ld_misc_mem_size,
		1);
		DRM_INFO("[LDM]u64Dts_ld_misc_Addr:0x%llx", u64Dts_ld_misc_Addr);

		memset(u64Vaddr + u64Dts_ld_mspi_offset, 0, u64Dts_ld_mspi_mem_size);
		memcpy(u64Vaddr + u64Dts_ld_mspi_offset,
		pld_mspi_info, sizeof(struct drm_st_ld_mspi_info));
		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Dts_ld_mspi_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Dts_ld_mspi_mem_size,
		1);
		DRM_INFO("[LDM]u64Dts_ld_mspi_Addr:0x%llx", u64Dts_ld_mspi_Addr);

		memset(u64Vaddr + u64Dts_ld_dma_offset, 0, u64Dts_ld_dma_mem_size);
		memcpy(u64Vaddr + u64Dts_ld_dma_offset,
		pld_dma_info, sizeof(struct drm_st_ld_dma_info));
		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Dts_ld_dma_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Dts_ld_dma_mem_size,
		1);
		DRM_INFO("[LDM]u64Dts_ld_dma_Addr:0x%llx", u64Dts_ld_dma_Addr);

		memset(u64Vaddr + u64Dts_ld_boosting_offset, 0, u64Dts_ld_boosting_mem_size);
		memcpy(
		u64Vaddr + u64Dts_ld_boosting_offset,
		pld_boosting_info,
		sizeof(struct drm_st_ld_boosting_info));
		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Dts_ld_boosting_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Dts_ld_boosting_mem_size,
		1);
		DRM_INFO("[LDM]u64Dts_ld_boosting_Addr:0x%llx", u64Dts_ld_boosting_Addr);

		memset(u64Vaddr + u64Dts_ld_led_device_offset, 0, u64Dts_ld_led_device_mem_size);
		memcpy(
		u64Vaddr + u64Dts_ld_led_device_offset,
		pld_led_device_info,
		sizeof(struct drm_st_ld_led_device_info));

		dma_direct_sync_single_for_device(
		pctx->dev,
		u64Dts_ld_led_device_Addr + pctx_ld->u32Cpu_emi0_base,
		u64Dts_ld_led_device_mem_size,
		1);
		DRM_INFO("[LDM]u64Dts_led_device_Addr:0x%llx", u64Dts_ld_led_device_Addr);


		switch (pld_led_device_info->eLEDDeviceType) {
		case E_LD_DEVICE_UNSUPPORT:
			break;
		case E_LD_DEVICE_AS3824:
			memset(
			u64Vaddr + u64Dts_ld_led_device_as3824_offset,
			0, u64Dts_ld_led_device_as3824_mem_size);
			memcpy(
			u64Vaddr + u64Dts_ld_led_device_as3824_offset,
			pld_led_device_as3824_info,
			sizeof(struct drm_st_ld_led_device_as3824_info));

			dma_direct_sync_single_for_device(
			pctx->dev,
			u64Dts_ld_led_device_as3824_Addr + pctx_ld->u32Cpu_emi0_base,
			u64Dts_ld_led_device_as3824_mem_size,
			1);
			DRM_INFO("[LDM]u64Dts_ld_led_device_as3824_Addr:0x%llx",
			u64Dts_ld_led_device_as3824_Addr);
			break;
		case E_LD_DEVICE_NT50585:
			memset(
			u64Vaddr + u64Dts_ld_led_device_nt50585_offset,
			0, u64Dts_ld_led_device_nt50585_mem_size);
			memcpy(
			u64Vaddr + u64Dts_ld_led_device_nt50585_offset,
			pld_led_device_nt50585_info,
			sizeof(struct drm_st_ld_led_device_nt50585_info));

			dma_direct_sync_single_for_device(
			pctx->dev,
			u64Dts_ld_led_device_nt50585_Addr + pctx_ld->u32Cpu_emi0_base,
			u64Dts_ld_led_device_nt50585_mem_size,
			1);
			DRM_INFO("[LDM]u64Dts_ld_led_device_nt50585_Addr:0x%llx",
			u64Dts_ld_led_device_nt50585_Addr);
			break;
		case E_LD_DEVICE_IW7039:
			memset(
			u64Vaddr + u64Dts_ld_led_device_iw7039_offset,
			0, u64Dts_ld_led_device_iw7039_mem_size);
			memcpy(
			u64Vaddr + u64Dts_ld_led_device_iw7039_offset,
			pld_led_device_iw7039_info,
			sizeof(struct drm_st_ld_led_device_iw7039_info));

			dma_direct_sync_single_for_device(
			pctx->dev,
			u64Dts_ld_led_device_iw7039_Addr + pctx_ld->u32Cpu_emi0_base,
			u64Dts_ld_led_device_iw7039_mem_size,
			1);
			DRM_INFO("[LDM]u64Dts_ld_led_device_iw7039_Addr:0x%llx",
			u64Dts_ld_led_device_iw7039_Addr);
			break;
		case E_LD_DEVICE_MCU:
			break;
		case E_LD_DEVICE_CUS:
			break;
		case E_LD_DEVICE_MBI6353:
			memset(
			u64Vaddr + u64Dts_ld_led_device_mbi6353_offset,
			0, u64Dts_ld_led_device_mbi6353_mem_size);
			memcpy(
			u64Vaddr + u64Dts_ld_led_device_mbi6353_offset,
			pld_led_device_mbi6353_info,
			sizeof(struct drm_st_ld_led_device_mbi6353_info));

			dma_direct_sync_single_for_device(
			pctx->dev,
			u64Dts_ld_led_device_mbi6353_Addr + pctx_ld->u32Cpu_emi0_base,
			u64Dts_ld_led_device_mbi6353_mem_size,
			1);
			DRM_INFO("[LDM]u64Dts_ld_led_device_mbi6353_Addr:0x%llx",
			u64Dts_ld_led_device_mbi6353_Addr);
			break;

		default:
			break;
		}

		DRM_INFO("[LDM]u64LDF_Addr:0x%llx", u64LDF_Addr);

	return ret;
}


//----------------------------------------------------------------
// MDrv_LDM_Init_SetLDC - Set LD path
/// @ingroup G_LDM_CONTROL
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
struct ConfigFile_t *MDrv_LDM_Load_Edge2DBin(struct mtk_tv_kms_context *pctx)
{
	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct ConfigFile_t *Edge2DBin = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return NULL;
	}

	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);

	if (pld_misc_info->bOLEDEn)
		Edge2DBin = MDrv_LD_GetConfigFile(EDGE2D_OLED_BIN);
	else if (pld_panel_info->u16LEDWidth == 1 && pld_panel_info->u16LEDHeight == 1)
		Edge2DBin = MDrv_LD_GetConfigFile(EDGE2D_GD_BIN);
	else
		Edge2DBin = MDrv_LD_GetConfigFile(EDGE2D_BIN);
	return Edge2DBin;
}

char *MDrv_LDM_Get_BIN_Name(enum EN_LDM_BIN_TYPE eType)
{
	return ldm_bin_header[eType].abin_info;
}

__u8 MDrv_LDM_Get_BIN_Version1(enum EN_LDM_BIN_TYPE eType)
{
	__u8 u8Version = 0;

	u8Version = ldm_bin_header[eType].u8version1;

	return u8Version;
}

__u8 MDrv_LDM_Get_BIN_Version2(enum EN_LDM_BIN_TYPE eType)
{
	__u8 u8Version = 0;

	u8Version = ldm_bin_header[eType].u8version2;

	return u8Version;
}

void MDrv_LDM_Set_BIN_Info(enum EN_LDM_BIN_TYPE eType, struct ConfigFile_t *Bin)
{
	int i;

	switch (eType) {
	case E_LDM_BIN_TYPE_COMPENSATION:
		for (i = 0; i < MAX_BIN_INFO_NAME_LEN; i++)
			ldm_bin_header[E_LDM_BIN_TYPE_COMPENSATION].abin_info[i] =
			Bin->p[BIN_NAME + i];

		ldm_bin_header[E_LDM_BIN_TYPE_COMPENSATION].abin_info[i] = '\0';
		ldm_bin_header[E_LDM_BIN_TYPE_COMPENSATION].u8version1 = Bin->p[BIN_VER1];
		ldm_bin_header[E_LDM_BIN_TYPE_COMPENSATION].u8version2 = Bin->p[BIN_VER2];
	break;
	case E_LDM_BIN_TYPE_EDGE2D:
		for (i = 0; i < MAX_BIN_INFO_NAME_LEN; i++)
			ldm_bin_header[E_LDM_BIN_TYPE_EDGE2D].abin_info[i] = Bin->p[BIN_NAME + i];

		ldm_bin_header[E_LDM_BIN_TYPE_EDGE2D].abin_info[i] = '\0';
		ldm_bin_header[E_LDM_BIN_TYPE_EDGE2D].u8version1 = Bin->p[BIN_VER1];
		ldm_bin_header[E_LDM_BIN_TYPE_EDGE2D].u8version2 = Bin->p[BIN_VER2];
	break;
	case E_LDM_BIN_TYPE_BACKLIGHTGAMMA:
		for (i = 0; i < MAX_BIN_INFO_NAME_LEN; i++)
			ldm_bin_header[E_LDM_BIN_TYPE_BACKLIGHTGAMMA].abin_info[i] =
			Bin->p[BIN_NAME + i];

		ldm_bin_header[E_LDM_BIN_TYPE_BACKLIGHTGAMMA].abin_info[i] = '\0';
		ldm_bin_header[E_LDM_BIN_TYPE_BACKLIGHTGAMMA].u8version1 = Bin->p[BIN_VER1];
		ldm_bin_header[E_LDM_BIN_TYPE_BACKLIGHTGAMMA].u8version2 = Bin->p[BIN_VER2];
	break;
	default:
		DRM_ERROR("[LDM][%s,%5d] not support\n", __func__, __LINE__);
	break;
	}
}

__u8 MDrv_LDM_Load_Bin_File(struct mtk_tv_kms_context *pctx)
{
	int ret = E_LDM_RET_SUCCESS;
	//load Compensation.bin and write them to regs
	int i;

	struct ConfigFile_t *CompensationBin = MDrv_LD_GetConfigFile(COMPENSATION_BIN);
	struct ConfigFile_t *Edge2DBin = NULL;
	struct ConfigFile_t *BacklightGammaBin = MDrv_LD_GetConfigFile(BACKLIGHTGAMMA_BIN);

	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;
	struct drm_st_ld_led_device_as3824_info *pld_led_device_as3824_info = NULL;
	struct drm_st_ld_led_device_nt50585_info *pld_led_device_nt50585_info = NULL;
	struct drm_st_ld_led_device_iw7039_info *pld_led_device_iw7039_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		goto ERROR;
	}

	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);
	pld_led_device_as3824_info = &(pctx->ld_priv.ld_led_device_as3824_info);
	pld_led_device_nt50585_info = &(pctx->ld_priv.ld_led_device_nt50585_info);
	pld_led_device_iw7039_info = &(pctx->ld_priv.ld_led_device_iw7039_info);

	if (CompensationBin == NULL || CompensationBin->p == NULL) {
		DRM_ERROR("[LDM]load Compensation bin fail\n");
		goto ERROR;
	}
	if (MDrv_LD_CheckData(CompensationBin->p, CompensationBin->size) == E_LDM_RET_FAIL) {
		DRM_ERROR("Compensation bin format error\n");
		goto ERROR;
	}
	u64Compensation_mem_size = LD_COMPENSATION_LENGTH * 0x20;
	u64Compensation_mem_size = (
	u64Compensation_mem_size + MEMALIGN_SIZE) >> MEMALIGN << MEMALIGN; // align at 0x100

	pu8CompTable = CompensationBin->p + LD_BIN_OFFSET;
	DRM_INFO("[LDM]trace! Compensation bin addr buf:0x%p\n", CompensationBin->p);
	DRM_INFO("[LDM]addr comp:0x%p\n", pu8CompTable);
	DRM_INFO("[LDM]size: 0x%zx , size: 0x%llx\n",
	CompensationBin->size, u64Compensation_mem_size);

	MDrv_LDM_Set_BIN_Info(E_LDM_BIN_TYPE_COMPENSATION, CompensationBin);

	pu8CompTable1 = pu8CompTable + LD_COMPENSATION_LENGTH;
	pu8CompTable2 = pu8CompTable1 + LD_COMPENSATION_LENGTH;

	//load Edge2D.bin and write them to address of edge2D

	Edge2DBin = MDrv_LDM_Load_Edge2DBin(pctx);

	if (Edge2DBin == NULL || Edge2DBin->p == NULL) {
		DRM_ERROR("[LDM]load Edge2D bin fail\n");
		goto ERROR;
	}
	if (MDrv_LD_CheckData(Edge2DBin->p, Edge2DBin->size) == E_LDM_RET_FAIL) {
		DRM_ERROR("[LDM]Edge2D bin format error\n");
		goto ERROR;
	}

	u64Edge2D_bin_size = Edge2DBin->size;
	u64Edge2D_mem_size = Edge2DBin->size;
	u64Edge2D_mem_size = (
	u64Edge2D_mem_size + MEMALIGN_SIZE) >> MEMALIGN << MEMALIGN; // align at 0x100

	if (pld_panel_info->eLEDType == E_LD_10K_TYPE) {
		pu8Edge2DTable = Edge2DBin->p + LD_10K_BIN_OFFSET;
		u64Edge2D_data_size = u64Edge2D_bin_size - LD_10K_BIN_OFFSET;

		pld_panel_info->u16RefWidth
		= Edge2DBin->p[REF_WIDTH_OFFSET]
		+ (HIGH_BYTE_16BIT_BASE * Edge2DBin->p[REF_WIDTH_OFFSET + 1]);
		pld_panel_info->u16RefHeight
		= Edge2DBin->p[REF_HEIGHT_OFFSET]
		+ (HIGH_BYTE_16BIT_BASE * Edge2DBin->p[REF_HEIGHT_OFFSET + 1]);
		DRM_INFO("[LDM]pld_panel_info->u16RefWidth: %d\n", pld_panel_info->u16RefWidth);
		DRM_INFO("[LDM]pld_panel_info->u16RefHeight:%d\n", pld_panel_info->u16RefHeight);
		for (i = 0; i < LD10K_INIX_NUM; i++) {
			pld_panel_info->s8XsIni[i] = Edge2DBin->p[LD10K_INIX_OFFSET + i];
			DRM_INFO("[LDM]pld_panel_info->s8XsIni[%d] : %d\n",
			i, pld_panel_info->s8XsIni[i]);
		}
	} else {
		pu8Edge2DTable = Edge2DBin->p + LD_BIN_OFFSET;
		u64Edge2D_data_size = u64Edge2D_bin_size - LD_BIN_OFFSET;
	}

	MDrv_LDM_Set_BIN_Info(E_LDM_BIN_TYPE_EDGE2D, Edge2DBin);

	DRM_INFO("[LDM]trace! Edge2D bin  addr buf:0x%p\n", Edge2DBin->p);
	DRM_INFO("[LDM]addr edge:0x%p\n", pu8Edge2DTable);
	DRM_INFO("[LDM]u64Edge2D_mem_size:0x%llx\n", u64Edge2D_mem_size);
	DRM_INFO("[LDM]u64Edge2D_bin_size:0x%llx\n", u64Edge2D_bin_size);

	/*load BacklightGamma.bin and write them to dram ,the address is pGamma_blocks*/

	if (BacklightGammaBin == NULL || BacklightGammaBin->p == NULL) {
		DRM_ERROR("[LDM]load BacklightGamma bin fail\n");
		goto ERROR;
	}
	if (MDrv_LD_CheckData(BacklightGammaBin->p, BacklightGammaBin->size) == E_LDM_RET_FAIL) {
		DRM_ERROR("[LDM]BacklightGamma bin format error\n");
		goto ERROR;
	}
	u64Backlight_Gamma_bin_size = BacklightGammaBin->size;
	u64Backlight_Gamma_data_size = u64Backlight_Gamma_bin_size - LD_BIN_OFFSET;
	u64Backlight_Gamma_mem_size = BacklightGammaBin->size;
	u64Backlight_Gamma_mem_size = (
	u64Backlight_Gamma_mem_size + MEMALIGN_SIZE) >> MEMALIGN << MEMALIGN; // align at 0x100

	pld_panel_info->u8BacklightVer
	= BacklightGammaBin->p[BACKLIGHTGAMMA_VER_OFFSET];
	DRM_INFO("[LDM]pld_panel_info->u8BacklightVer: %d\n", pld_panel_info->u8BacklightVer);

	pu8GammaTable = BacklightGammaBin->p + LD_BIN_OFFSET;
	DRM_INFO("[LDM]trace! BacklightGamma bin  addr buf:0x%p\n", BacklightGammaBin->p);
	DRM_INFO("[LDM]addr edge:0x%p\n", pu8GammaTable);
	DRM_INFO("[LDM]size:0x%llx\n", u64Backlight_Gamma_mem_size);

	MDrv_LDM_Set_BIN_Info(E_LDM_BIN_TYPE_BACKLIGHTGAMMA, BacklightGammaBin);

	/*MDrv_LD_SetupGammaTable(BacklightGammaBin->p + LD_BIN_OFFSET);*/
	for (i = 0; i < 16; i++) {
		pTbl_LD_Gamma[i] = pu8GammaTable + i * GAMMA_TABLE_LEN;
		pTbl_LD_Remap[i] = pu8GammaTable + (16 + i) * GAMMA_TABLE_LEN;
	}

	ret = MDrv_LDM_DRAM_Layout(pctx);
	if (ret != E_LDM_RET_SUCCESS) {
		DRM_ERROR("[LDM]MDrv_LDM_DRAM_Layout fail:%d\n ", ret);
		goto ERROR;
	}

	for (i = 0; i < ARRAY_SIZE(stLdConfig); i++) {
		if (stLdConfig[i].p != NULL)
			vfree((void *)stLdConfig[i].p);
	}

	return ret;

ERROR:
	for (i = 0; i < ARRAY_SIZE(stLdConfig); i++) {
		if (stLdConfig[i].p != NULL)
			vfree((void *)stLdConfig[i].p);
	}

	return E_LDM_RET_PARAMETER_ERROR;
}

////remove-start trunk ld need refactor
//----------------------------------------------------------------
// MDrv_LDM_GetData - get local dimming status
/// @ingroup G_LDM_CONTROL
// @param: void
// @return: EN_LDM_STATUS
//----------------------------------------------------------------
//u8 MDrv_LDM_GetStatus(void)
//{
//	LDM_DBG_FUNC();
//	DRM_INFO("status: %d\n", gu8LDMStatus);
//
//	return gu8LDMStatus;
//}
////remove-end trunk ld need refactor


////remove-start trunk ld need refactor
//----------------------------------------------------------------
// MDrv_LDM_Enable - start local dimming algorithm
/// @ingroup G_LDM_CONTROL
// @param: na
// @return: E_LDM_RET_SUCCESS is enable
//----------------------------------------------------------------
//u8 MDrv_LDM_Enable(void)
//{
//	u8 iResult = 0;
//
//	iResult = MDrv_LD_Enable(TRUE, 0);
//
//	if (iResult == E_LDM_RET_FAIL) {
//		DRM_INFO("LDM Enable Fail\n");
//
//		return E_LDM_RET_FAIL;
//	}
//	gu8LDMStatus = E_LDM_STATUS_ENABLE;
//
//	DRM_INFO("OK\n");
//
//	return E_LDM_RET_SUCCESS;
//}
////remove-end trunk ld need refactor


////remove-start trunk ld need refactor
//----------------------------------------------------------------
// MDrv_LDM_Disable - stop local dimming algorithm, send constant luminance  to led
/// @ingroup G_LDM_CONTROL
// @param: u8Lumma: constant luminance range from 00 to 255
// @return: E_LDM_RET_SUCCESS is disable
//----------------------------------------------------------------
//u8 MDrv_LDM_Disable(u8 u8Lumma)
//{
//	u8 iResult = 0;
//
//	iResult = MDrv_LD_Enable(FALSE, u8Lumma);
//
//	if (iResult == E_LDM_RET_FAIL) {
//		DRM_INFO("LDM Disable Fail\n");
//		return E_LDM_RET_FAIL;
//	}
//	gu8LDMStatus = E_LDM_STATUS_DISNABLE;
//
//	DRM_INFO("OK\n");
//
//	return E_LDM_RET_SUCCESS;
//
//}
////remove-end trunk ld need refactor

//----------------------------------------------------------------
// MDrv_LDM_Suspend - Set  suspend reg
/// @ingroup G_LDM_CONTROL
// @param: phyAddr: local dimming mmap address in mmap.ini
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
__u8 MDrv_LDM_Suspend(__u8 u8LDM_Version, struct mtk_tv_kms_context *pctx)
{
	int ret_rtos = 0;
	struct mtk_ld_context *pctx_ld = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	global_strength = drv_hwreg_render_ldm_get_GD_strength(u8LDM_Version);

	drv_hwreg_render_ldm_set_spi_ready_flag(u8LDM_Version, FALSE);

	pctx_ld = &pctx->ld_priv;
	if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2) {
		drv_hwreg_render_ldm_adl_set_comp_stop(u8LDM_Version, FALSE);
		drv_hwreg_render_ldm_adl_set_mcg_stop(u8LDM_Version, FALSE);
	}

	drv_hwreg_render_ldm_set_local_dimming_en(u8LDM_Version, FALSE);
	drv_hwreg_render_ldm_set_spi_self_trig_offset(u8LDM_Version, FALSE);

	ldm_suspend_param.u8ld_suspend = TRUE;
	if (bPquEnable) {
		ret_rtos = pqu_render_ldm_suspend(&ldm_suspend_param, NULL);
		DRM_INFO(
		"[DRM][LDM][%s][%d] pqu_render_ldm_suspend reply  %x, %x !!\n",
		__func__,
		__LINE__,
		ret_rtos,
		ldm_suspend_reply_param.ret);
	} else {
		pqu_msg_send(PQU_MSG_SEND_LDM_SUSPEND, &ldm_suspend_param);
	}

	DRM_INFO("[LDM][%s,%5d] OK\n", __func__, __LINE__);
	return E_LDM_RET_SUCCESS;
}


//----------------------------------------------------------------
// MDrv_LDM_Resume - Set Reusme Flow
/// @ingroup G_LDM_CONTROL
// @param: phyAddr: local dimming mmap address in mmap.ini
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
__u8 MDrv_LDM_Resume(struct mtk_tv_kms_context *pctx)
{
	int ret = 0;
	struct mtk_ld_context *pctx_ld = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[%s,%5d][LDM] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pctx_ld = &pctx->ld_priv;

	drv_hwreg_render_ldm_set_GD_strength(pctx_ld->u8LDM_Version,global_strength);
	ret = ldm_set_ld_pwm_vsync_setting(pctx);
	if (ret != E_LDM_RET_SUCCESS) {
		DRM_ERROR("[LDM]ldm_set_ld_pwm_vsync_setting fail:%d\n ", ret);
		return E_LDM_RET_FAIL;
	}
	ret = ldm_leddevice_init(pctx);
	if (ret != E_LDM_RET_SUCCESS) {
		DRM_ERROR("[LDM]ldm_leddevice_init fail:%d\n ", ret);
		return E_LDM_RET_FAIL;
	}

	DRM_INFO("[LDM][%s,%5d] OK\n", __func__, __LINE__);
	return E_LDM_RET_SUCCESS;
}

//----------------------------------------------------------------
// MDrv_LDM_Sysfs_Check_Reg - Check  Local dimming reg for sysfs
/// @ingroup G_LDM_CONTROL
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
__u8 MDrv_LDM_Check_Reg_Sysfs(__u8 u8LDM_Version)
{
	u16 u16temp = 0;

	switch (u8LDM_Version) {
	case E_LDM_HW_VERSION_1:
	case E_LDM_HW_VERSION_2:
	case E_LDM_HW_VERSION_3:
	{
		u16temp = drv_hwreg_render_ldm_get_ldc_path_sel(u8LDM_Version);
		DRM_INFO("[LDM]Local_dimming_path = %x\n", u16temp);

		u16temp = drv_hwreg_render_ldm_get_ldc_bypass(u8LDM_Version);
		DRM_INFO("[LDM]Cus Local_dimming_bypass = %x\n", u16temp);

		u16temp = drv_hwreg_render_ldm_get_empty_en(u8LDM_Version);
		DRM_INFO("[LDM]Trunk Local_dimming_bypass = %x\n", u16temp);

		u16temp = drv_hwreg_render_ldm_get_local_dimming_en(u8LDM_Version);
		DRM_INFO("[LDM]Trunk Local_dimming_en = %x\n", u16temp);

		break;
	}
	default:
		DRM_INFO("[LDM]This chip is not support Local_dimming_path\n");
		DRM_INFO("[LDM]This chip is not support Cus Local_dimming_bypass\n");

		u16temp = drv_hwreg_render_ldm_get_empty_en(u8LDM_Version);
		DRM_INFO("[LDM]Trunk Local_dimming_bypass = %x\n", u16temp);

		u16temp = drv_hwreg_render_ldm_get_local_dimming_en(u8LDM_Version);
		DRM_INFO("[LDM]Trunk Local_dimming_en = %x\n", u16temp);

		break;
	}

	return 0;

}

//----------------------------------------------------------------
// MDrv_LDM_Init_Check_SPI_Num - Check  Local dimming SPI Num reg
/// @ingroup G_LDM_CONTROL
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
__u8 MDrv_LDM_Init_Check_SPI_Num(__u8 u8LDM_Version)
{
	int i = 0;
	u16 u16temp = 0;
	u16 u16spi_err_cnt = 0;

	if (u8LDM_Version == E_LDM_HW_VERSION_1) {
		DRM_INFO("[LDM][%s,%5d] This chip do not support 10k ld, always pass\n",
		__func__, __LINE__);
		return E_LDM_RET_SUCCESS;
	}

	for (i = 0; i < CHECK_SPI_TIMES; i++) {
		u16temp = drv_hwreg_render_ldm_get_spi_num_count(u8LDM_Version);
		DRM_INFO("[DRM][LDM] spi_num_count = %x\n", u16temp);
		if (u16temp == 0x01)
			u16spi_err_cnt++;
		else if (u16temp == 0x00)
			u16spi_err_cnt++;
	}

	DRM_INFO("[DRM][LDM] spi_err_cnt = %x\n", u16spi_err_cnt);

	if (u16spi_err_cnt > 0x01) {
		DRM_INFO("[LDM][%s,%5d] LD DMA SPI Fail, Please Check it\n",
		__func__, __LINE__);
		return E_LDM_RET_PARAMETER_ERROR;
	}

	DRM_INFO("[LDM][%s,%5d] LD DMA SPI OK\n", __func__, __LINE__);
	return E_LDM_RET_SUCCESS;

}

//----------------------------------------------------------------
// MDrv_LDM_Init_Check_En - Check  Local dimming enable reg
/// @ingroup G_LDM_CONTROL
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
__u8 MDrv_LDM_Init_Check_En(__u8 u8LDM_Version)
{
//	int i = 0;
	u16 u16temp = 0;

	if (u8LDM_Version == E_LDM_HW_VERSION_1) {
		DRM_INFO("[LDM][%s,%5d] This chip do not support 10k ld, always pass\n",
		__func__, __LINE__);
		return E_LDM_RET_SUCCESS;
	}

	u16temp = drv_hwreg_render_ldm_get_local_dimming_en(u8LDM_Version);
	DRM_INFO("[DRM][LDM] local_dimming_en  %x\n", u16temp);

	if (u16temp == TRUE) {
		DRM_INFO("[LDM][%s,%5d] LD Init EN OK\n", __func__, __LINE__);
		return E_LDM_RET_SUCCESS;
	}

	DRM_INFO("[LDM][%s,%5d] LD Init Fail, Please Check it\n", __func__, __LINE__);
	return E_LDM_RET_PARAMETER_ERROR;

}

bool MDrv_LDM_Get_En_Status(uint8_t u8ldm_version)
{
	bool ret = FALSE;
	//u16 u16temp = 0;

	ret = (bool)drv_hwreg_render_ldm_get_local_dimming_en(u8ldm_version);
	DRM_INFO("[DRM][LDM] local_dimming_en  %d\n", ret);

	return ret;

}

bool MDrv_LDM_Check_Tgen_Changed(uint8_t u8ldm_version)
{
	bool ret = FALSE;

	u16_pre_tgen_v = u16_current_tgen_v;
	u16_current_tgen_v = drv_hwreg_render_ldm_get_tgen_v(u8ldm_version);

	if ((u16_pre_tgen_v == LD_TGEN_V_2160 && u16_current_tgen_v == LD_TGEN_V_1080)
		|| (u16_pre_tgen_v == LD_TGEN_V_1080 && u16_current_tgen_v == LD_TGEN_V_2160)
		|| (u16_pre_tgen_v == LD_TGEN_V_4320 && u16_current_tgen_v == LD_TGEN_V_2160)
		|| (u16_pre_tgen_v == LD_TGEN_V_2160 && u16_current_tgen_v == LD_TGEN_V_4320)
		|| (u16_pre_tgen_v == LD_TGEN_V_1080 && u16_current_tgen_v == LD_TGEN_V_540)
		|| (u16_pre_tgen_v == LD_TGEN_V_540 && u16_current_tgen_v == LD_TGEN_V_1080)
		) {
		ret = TRUE;	
	} else {
		ret = FALSE;
	}

	DRM_INFO("[DRM][LDM] local_Tgen_Changed  %d\n", ret);

	return ret;

}

void Mdrv_ldm_set_spi_ready_flag(uint8_t u8ldm_version, uint8_t u8value)
{
	drv_hwreg_render_ldm_set_spi_ready_flag(u8ldm_version, u8value);
}

bool Mdrv_ldm_get_spi_ready_flag(uint8_t u8ldm_version)
{
	bool ret = FALSE;

	ret = (bool)drv_hwreg_render_ldm_get_spi_ready_flag(u8ldm_version);

	return ret;
}


void MDrv_LDM_set_spi_self_trig_off(__u8 u8ldm_version)
{
	drv_hwreg_render_ldm_set_spi_self_trig_offset(u8ldm_version, FALSE);
}

void Mdrv_ldm_set_GD_strength(uint8_t u8ldm_version,uint8_t u8value)
{
    drv_hwreg_render_ldm_set_GD_strength(u8ldm_version,u8value);
}

//----------------------------------------------------------------
// MDrv_LDM_Init_to_CPUIF - Set to CPU IF
/// @ingroup G_LDM_CONTROL
// @param: phyAddr: local dimming mmap address in mmap.ini
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
__u8 MDrv_LDM_Init_to_CPUIF(void)
{

	int ret_rtos = 0;

	ldm_init_param.u64Compensation_Addr = u64Compensation_Addr;
	ldm_init_param.u32Compensation_mem_size = u64Compensation_mem_size;
	ldm_init_param.u32Edge2D_mem_size = u64Edge2D_mem_size;
	ldm_init_param.u32Edge2D_bin_size = u64Edge2D_bin_size;
	ldm_init_param.u32Backlight_Gamma_mem_size = u64Backlight_Gamma_mem_size;
	ldm_init_param.u16DTS_Mem_Size = u64Dts_ld_panel_mem_size;

	DRM_INFO("[LDM]rpmsg mmap address: 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
	u64Compensation_Addr,
	u64Edge2D_Addr,
	u64Backlight_Gamma_Addr,
	u64Dts_ld_panel_Addr,
	u64Dts_ld_panel_mem_size,
	u64LDF_Addr);

	if (bPquEnable) {
		ret_rtos = pqu_render_ldm_init(&ldm_init_param, NULL);
		DRM_INFO(
		"[DRM][LDM][%s][%d] LD init reply  %x, %x !!\n",
		__func__,
		__LINE__,
		ret_rtos,
		ldm_init_reply_param.ret);

		if (ret_rtos) {
			DRM_ERROR("[LDM]CPUIF not ready, wait it ready and retry");
			fw_ext_command_register_notify_ready_callback(0, pqu_ready_ldm_init);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_LDM_INIT, &ldm_init_param);
	}



	DRM_INFO("[DRM][LDM][%s][%d]  OK\n",
	__func__,
	__LINE__);

	return E_LDM_RET_SUCCESS;
}

//----------------------------------------------------------------
// MDrv_LDM_Init_to_Mailbox - Set to Mailbox
/// @ingroup G_LDM_CONTROL
// @param:
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
__u8 MDrv_LDM_Init_to_Mailbox(void)
{
	struct pqu_frc_send_cmd_r2_param send_r2 = {DEFAULT_VALUE};
	struct callback_info callback_info = {DEFAULT_VALUE};

	ldm_init_param.u64Compensation_Addr = u64Compensation_Addr;
	ldm_init_param.u32Compensation_mem_size = u64Compensation_mem_size;
	ldm_init_param.u32Edge2D_mem_size = u64Edge2D_mem_size;
	ldm_init_param.u32Edge2D_bin_size = u64Edge2D_bin_size;
	ldm_init_param.u32Backlight_Gamma_mem_size = u64Backlight_Gamma_mem_size;
	ldm_init_param.u16DTS_Mem_Size = u64Dts_ld_panel_mem_size;

	DRM_INFO("[LDM]mailbox mmap address/size: 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
	u64Compensation_Addr,
	u64Compensation_mem_size,
	u64Edge2D_mem_size,
	u64Edge2D_bin_size,
	u64Backlight_Gamma_mem_size,
	u64Dts_ld_panel_mem_size);

	#define FRCR2_CLIENT_MEMC 0
	#define FRCR2_CLIENT_VR   1
	#define FRCR2_CLIENT_LD   2

	//typedef enum{
	//E_LD_SET_INIT = 0,//Mixed mode
	//E_LD_SET_GAMEMODE_EN,
	//E_LD_SET_LOG_LEVEL,
	//E_LD_SET_UI_LEVEL,
	//E_LD_GET_UI_LEVEL,
	//E_LD_SET_UI_INFORMATION,
	//E_LD_GET_UI_INFORMATION,
	//E_LD_SET_STI_INIT_1,//STI mode
	//E_LD_SET_STI_INIT_2,//STI mode
	//E_LD_SET_DLGMODE_EN,
	//E_LD_CMD_NUM,
	//} EN_API_LD_CMD_IDX;

	#define E_LD_SET_STI_INIT_1 0x7
	#define E_LD_SET_STI_INIT_2 0x8
	#define FRCR2_DUMMY_MAGIC1 0x5876
	#define FRCR2_DUMMY_MAGIC2 0x5879

	send_r2.ClientID = FRCR2_CLIENT_LD;
	send_r2.CmdID    = E_LD_SET_STI_INIT_1;
	send_r2.D1       = (__u32)ldm_init_param.u64Compensation_Addr;
	send_r2.D3       = ldm_init_param.u32Compensation_mem_size;
	send_r2.D4       = ldm_init_param.u32Edge2D_mem_size;
	send_r2.Dummy1   = FRCR2_DUMMY_MAGIC1;
	send_r2.Dummy2   = FRCR2_DUMMY_MAGIC2;
	pqu_send_cmd_r2(&send_r2, &callback_info);

	send_r2.ClientID = FRCR2_CLIENT_LD;
	send_r2.CmdID    = E_LD_SET_STI_INIT_2;
	send_r2.D1       = ldm_init_param.u32Edge2D_bin_size;
	send_r2.D2       = ldm_init_param.u32Backlight_Gamma_mem_size;
	send_r2.D3       = ldm_init_param.u16DTS_Mem_Size;
	send_r2.Dummy1   = FRCR2_DUMMY_MAGIC1;
	send_r2.Dummy2   = FRCR2_DUMMY_MAGIC2;
	pqu_send_cmd_r2(&send_r2, &callback_info);

	DRM_INFO("[DRM][LDM][%s][%d]  OK\n",
	__func__,
	__LINE__);

	return E_LDM_RET_SUCCESS;
}

//----------------------------------------------------------------
// MDrv_LDM_Init - Set  mmap address to register base
/// @ingroup G_LDM_CONTROL
// @param: phyAddr: local dimming mmap address in mmap.ini
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
__u8 MDrv_LDM_Init(struct mtk_tv_kms_context *pctx, struct LD_CUS_PATH *Ldm_CusPath)
{
	int ret = 0;
	struct mtk_ld_context *pctx_ld = NULL;
	struct LD_CUS_PATH stCusPath;

	if (pctx == NULL) {
		DRM_ERROR("[%s,%5d][LDM] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	if (Ldm_CusPath == NULL) {
		DRM_ERROR("[%s,%5d][LDM] Ldm_CusPath is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pctx_ld = &pctx->ld_priv;

	memset(&stCusPath, 0, sizeof(struct LD_CUS_PATH));

	DRM_INFO("[LDM]path:%s, pathU:%s\n", Ldm_CusPath->aCusPath, Ldm_CusPath->aCusPathU);

	if ((strlen(Ldm_CusPath->aCusPath) > 0) || (strlen(Ldm_CusPath->aCusPathU) > 0)) {
		strncpy(stCusPath.aCusPath, Ldm_CusPath->aCusPath,
		strlen(Ldm_CusPath->aCusPath));
		stCusPath.aCusPath[sizeof(stCusPath.aCusPath) - 1] = '\0';
		strncpy(stCusPath.aCusPathU, Ldm_CusPath->aCusPathU,
		strlen(Ldm_CusPath->aCusPathU));
		stCusPath.aCusPathU[sizeof(stCusPath.aCusPathU) - 1] = '\0';

		DRM_INFO("[LDM]path:%s, pathU:%s\n", stCusPath.aCusPath, stCusPath.aCusPathU);
	} else {
		DRM_ERROR("[LDM]Copy LD_CUS_PATH path error !!!!!!using default path\n");
	}

	DRM_INFO("[LDM]init mmap address: 0x%llx\n", pctx_ld->u64LDM_phyAddress);

	ld_addr_base = pctx_ld->u64LDM_phyAddress;

	if (u64Vaddr == NULL) {
		DRM_INFO(
			"[LDM]profile u32Cpu_emi0_base %tx: %tx\n",
			(size_t)pctx_ld->u32Cpu_emi0_base,
			(size_t)(ld_addr_base + pctx_ld->u32Cpu_emi0_base));
		u64Vaddr = memremap((size_t)(ld_addr_base + pctx_ld->u32Cpu_emi0_base),
					pctx_ld->u32LDM_mmapsize,
					MEMREMAP_WB);
	}
	if ((u64Vaddr == NULL) || (u64Vaddr == (void *)((size_t)-1))) {
		LDM_DBG_ERR(
			"[LDM]error! ioremap_cached paddr:0x%tx, length:%d\n",
			(size_t)(ld_addr_base + pctx_ld->u32Cpu_emi0_base),
			pctx_ld->u32LDM_mmapsize);
		return E_LDM_RET_FAIL;
	}

	DRM_INFO(
		"[LDM]profile LD_VA_BASE = 0x:%p, length = 0x:%x\n",
		u64Vaddr, pctx_ld->u32LDM_mmapsize);
	_MDrv_LDM_get_share_memory_addr(pctx);
	ret = MDrv_LD_PrepareConfigFile(pctx, (&stCusPath));
	if (ret != E_LDM_RET_SUCCESS) {
		DRM_ERROR("[LDM]MDrv_LD_PrepareConfigFile fail:%d\n ", ret);
		return E_LDM_RET_FAIL;
	}

	ret = MDrv_LDM_Load_Bin_File(pctx);
	if (ret != E_LDM_RET_SUCCESS) {
		DRM_ERROR("[LDM]MDrv_LDM_Load_Bin_File fail:%d\n ", ret);
		return E_LDM_RET_FAIL;
	}

	if (pctx_ld->bLDM_uboot == 0) {
		ret = ldm_set_ld_pwm_vsync_setting(pctx);
		if (ret != E_LDM_RET_SUCCESS) {
			DRM_ERROR("[LDM]ldm_set_ld_pwm_vsync_setting fail:%d\n ", ret);
			return E_LDM_RET_FAIL;
		}
		ret = ldm_leddevice_init(pctx);
		if (ret != E_LDM_RET_SUCCESS) {
			DRM_ERROR("[LDM]ldm_leddevice_init fail:%d\n ", ret);
			return E_LDM_RET_FAIL;
		}
	}
	u16_current_tgen_v = drv_hwreg_render_ldm_get_tgen_v(pctx_ld->u8LDM_Version);

	DRM_INFO("[DRM][LDM][%s][%d]  OK\n",
	__func__,
	__LINE__);

	return ret;
}

//----------------------------------------------------------------
// MDrv_LDM_Init_SetLDC - Set LD path
/// @ingroup G_LDM_CONTROL
// @return: E_LDM_RET_SUCCESS is initial
//----------------------------------------------------------------
u8 MDrv_LDM_Init_SetLDC(u32 u32LdmType)
{
	int ret = 0;
	struct reg_info reg[5];
	struct hwregLDCIn paramIn;
	struct hwregOut paramOut;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregLDCIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	paramIn.RIU = 1;
	paramIn.enLDMSupportType = u32LdmType;

	ret = drv_hwreg_render_ldm_set_LDC(paramIn, &paramOut);

	DRM_INFO("[LDM]SetLDC done\n");
	return E_LDM_RET_SUCCESS;
}

//----------------------------------------------------------------
// MDrv_LDM_Init_SetSwSetCtrl - SW set Ctrl
/// @ingroup G_LDM_CONTROL
// @return: E_LDM_RET_SUCCESS
//----------------------------------------------------------------
u8 MDrv_LDM_Init_SetSwSetCtrl(
	struct hwregSWSetCtrlIn *stSwSetCtrl)
{
	int ret = 0;
	struct reg_info reg[2];
	struct hwregSWSetCtrlIn paramIn;
	struct hwregOut paramOut;

	if (stSwSetCtrl == NULL) {
		DRM_ERROR("[LDM][%s,%5d] stSwSetCtrl is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregSWSetCtrlIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	paramIn.RIU = 1;
	paramIn.enLDMSwSetCtrlType = stSwSetCtrl->enLDMSwSetCtrlType;
	paramIn.u8Value = stSwSetCtrl->u8Value;

	ret = drv_hwreg_render_ldm_set_swsetctrl(paramIn, &paramOut);

	DRM_INFO("[LDM]SetSwSetCtrl %d %d done\n", paramIn.enLDMSwSetCtrlType, paramIn.u8Value);
	return E_LDM_RET_SUCCESS;
}

u8 MDrv_LDM_Init_updateprofileAHB0(
	struct mtk_tv_kms_context *pctx)
{

	int ret = 0;
	size_t pfilesize;
	int i;
	u32 AHB0_reg;
	u32 AHB0_value;

	struct reg_info reg[1];
	struct hwregAHBIn paramIn;
	struct hwregOut paramOut;

	char *filename;
	char configPath[MAX_PATH_LEN];
	char path[MAX_PATH_LEN] = {0};
	int n;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	memset(configPath, 0, MAX_PATH_LEN);

	if (pctx->panel_priv.v_cfg.timing == E_4K2K_60HZ) {
		filename = LD_AHB3_WA0_BIN;
	} else if (pctx->panel_priv.v_cfg.timing == E_4K2K_120HZ) {
		filename = LD_AHB4_WA0_BIN;
	} else if (pctx->panel_priv.v_cfg.timing == E_8K4K_60HZ) {
		filename = LD_AHB5_WA0_BIN;
	} else {
		filename = "";
		LDM_DBG_ERR("[LDM]Config path error:null\n");
		return E_LDM_RET_PARAMETER_ERROR;
	}

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregAHBIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	for (i = 0; i < ARRAY_SIZE(ld_config_paths); i++) {
		if (is_path_exist(ld_config_paths[i])) {
			DRM_INFO("[LDM]trace! find configPath path:%s\n", ld_config_paths[i]);
			strncpy(configPath, ld_config_paths_root[i], MAX_PATH_LEN-1);
			configPath[sizeof(configPath) - 1] = '\0';
			break;
		}
		DRM_INFO("[LDM]trace! skip configPath path:%s\n", ld_config_paths[i]);
	}

	if (strlen(configPath) == 0) {
		DRM_INFO("[LDM]Config path error:null\n");
		return false;
	}

	DRM_INFO("[LDM]trace! using configPath path:%s\n", configPath);

	n = snprintf(path, MAX_PATH_LEN-1, "%s%s", configPath, filename);
	if (n < 0 || n >= sizeof(path)) {
		/* Handle snprintf() error */
		DRM_INFO("[LDM]configPath unknown error");
		return false;
	}

	ret = firmware_load(pctx, path, &bufferAHB0, &pfilesize);
	if (ret) {
		DRM_ERROR("[LDM]Load Bin error file name:%s not exists\n", filename);
		DRM_ERROR("[LDM][%s,%5d]  fail\n", __func__, __LINE__);
		if (bufferAHB0 != NULL)
			vfree((void *)bufferAHB0);
		return false;
	}

	for (i = 0; i < (pfilesize/8); i++) {
		AHB0_value = ((*(bufferAHB0+(i*8+3)))<<24)+
			((*(bufferAHB0+(i*8+2)))<<16)+
			((*(bufferAHB0+(i*8+1)))<<8)+
			(*(bufferAHB0+(i*8)));
		AHB0_reg = ((*(bufferAHB0+(i*8+7)))<<24)+
			((*(bufferAHB0+(i*8+6)))<<16)+
			((*(bufferAHB0+(i*8+5)))<<8)+
			(*(bufferAHB0+(i*8+4)));

		paramOut.reg = reg;

		paramIn.RIU = 1;
		paramIn.u32AHBReg = AHB0_reg;
		paramIn.u32AHBData = AHB0_value;
		paramIn.u8BitStart = MASK_START_BIT0;
		paramIn.u8BitLength = MASK_LENGTH_4BYTE;
		drv_hwreg_render_ldm_write_ahb0(paramIn, &paramOut);
	}

	if (bufferAHB0 != NULL)
		vfree((void *)bufferAHB0);

	return E_LDM_RET_SUCCESS;

}

u8 MDrv_LDM_Init_updateprofileAHB1(
	u64 phyAddr,
	struct mtk_tv_kms_context *pctx)
{
	struct hwregWriteMethodInfo writeInfo;
	struct ldmDatebaseIn dbIn;
	int ret = 0;
	size_t pfilesize;
	int i;

	char *filename;
	struct reg_info reg[2];
	struct hwregSWSetCtrlIn paramIn;
	struct hwregOut paramOut;
	char configPath[MAX_PATH_LEN];
	char path[MAX_PATH_LEN] = {0};
	int n;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	memset(configPath, 0, MAX_PATH_LEN);

	if (pctx->panel_priv.v_cfg.timing == E_4K2K_60HZ) {
		filename = LD_AHB3_WA1_BIN;
	} else if (pctx->panel_priv.v_cfg.timing == E_4K2K_120HZ) {
		filename = LD_AHB4_WA1_BIN;
	} else if (pctx->panel_priv.v_cfg.timing == E_8K4K_60HZ) {
		filename = LD_AHB5_WA1_BIN;
	} else {
		filename = "";
		LDM_DBG_ERR("[LDM]Config path error:null\n");
		return E_LDM_RET_PARAMETER_ERROR;
	}

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregSWSetCtrlIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	memset(&writeInfo, 0, sizeof(struct hwregWriteMethodInfo));
	memset(&dbIn, 0, sizeof(struct ldmDatebaseIn));

	paramOut.reg = reg;

	paramIn.RIU = 1;
	paramIn.enLDMSwSetCtrlType = E_LDM_SW_LDC_XIU2AHB_SEL1;
	paramIn.u8Value = 1;


	ret = drv_hwreg_render_ldm_set_swsetctrl(paramIn, &paramOut);


	ld_addr_base = phyAddr;


	if (u64Vaddr == NULL) {
		DRM_INFO(
			"[LDM]profile u32Cpu_emi0_base %tx: %tx\n",
			(size_t)pctx->ld_priv.u32Cpu_emi0_base,
			(size_t)(ld_addr_base + pctx->ld_priv.u32Cpu_emi0_base));

	u64Vaddr = memremap((ld_addr_base + pctx->ld_priv.u32Cpu_emi0_base),
		pctx->ld_priv.u32LDM_mmapsize, MEMREMAP_WB);

	}
	if ((u64Vaddr == NULL) || (u64Vaddr == (void *)((size_t)-1))) {
		LDM_DBG_ERR(
			"[LDM]error! ioremap_cached paddr:0x%tx, length:%d\n",
			(size_t)(ld_addr_base + pctx->ld_priv.u32Cpu_emi0_base),
			pctx->ld_priv.u32LDM_mmapsize);
		return E_LDM_RET_FAIL;
	}

	DRM_INFO(
		"[LDM]profile LD_VA_BASE = 0x:%p, length = 0x:%x\n",
		u64Vaddr, pctx->ld_priv.u32LDM_mmapsize);

	for (i = 0; i < ARRAY_SIZE(ld_config_paths); i++) {
		if (is_path_exist(ld_config_paths[i])) {
			DRM_INFO("[LDM]trace! find configPath path:%s\n", ld_config_paths[i]);
			strncpy(configPath, ld_config_paths_root[i], MAX_PATH_LEN-1);
			configPath[sizeof(configPath) - 1] = '\0';
			break;
		}
		DRM_INFO("[LDM]trace! skip configPath path:%s\n", ld_config_paths[i]);
	}

	if (strlen(configPath) == 0) {
		DRM_INFO("[LDM]Config path error:null\n");
		return false;
	}

	DRM_INFO("[LDM]trace! using configPath path:%s\n", configPath);

	n = snprintf(path, MAX_PATH_LEN-1, "%s%s", configPath, filename);
	if (n < 0 || n >= sizeof(path)) {
		/* Handle snprintf() error */
		DRM_INFO("[LDM]configPath unknown error");
		return false;
	}

	ret = firmware_load(pctx, path, &buffer, &pfilesize);
	if (ret) {
		DRM_ERROR("[LDM]Load Bin error file name:%s not exists\n", filename);
		DRM_ERROR("[LDM][%s,%5d]  fail\n", __func__, __LINE__);
		if (buffer != NULL)
			vfree((void *)buffer);
		return false;
	}
	memcpy(u64Vaddr, buffer, pfilesize);

	DRM_INFO("[LDM]trace! Start dma_direct_sync\n");

	dma_direct_sync_single_for_device(
		pctx->dev,
		ld_addr_base+pctx->ld_priv.u32Cpu_emi0_base,
		pfilesize,
		1);

	writeInfo.eMethod = HWREG_WRITE_BY_RIU;
	writeInfo.hwOut.reg = vmalloc(sizeof(struct reg_info)*BDMA_CONFIG_REG_NUM);
	memset(writeInfo.hwOut.reg, 0, sizeof(struct reg_info)*BDMA_CONFIG_REG_NUM);
	writeInfo.maxRegNum = MAX_BDMA_REG_NUM;
	dbIn.srcAddr = phyAddr;
	dbIn.dataSize = pfilesize;//size
	dbIn.dstAddr = 0x0;//start address, no offset

	DRM_INFO("[LDM]bdma debug filesize %zx\n", pfilesize);

	drv_hwreg_render_ldm_update_database(dbIn, &writeInfo);

	msleep(SleepTime);
	paramIn.u8Value = 0;
	ret = drv_hwreg_render_ldm_set_swsetctrl(paramIn, &paramOut);



	paramIn.enLDMSwSetCtrlType = E_LDM_SW_LDC_PATH_SEL;
	paramIn.u8Value = 1;
	ret = drv_hwreg_render_ldm_set_swsetctrl(paramIn, &paramOut);

	if (buffer != NULL)
		vfree((void *)buffer);

	vfree(writeInfo.hwOut.reg);

	return E_LDM_RET_SUCCESS;


}


u8 MDrv_LDM_Init_updateprofile(u64 pa, struct mtk_tv_kms_context *pctx)
{
	int ret = 0;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	ret = MDrv_LDM_Init_updateprofileAHB0(pctx);
	if (ret != 0)
		return ret;

	ret = MDrv_LDM_Init_updateprofileAHB1(pa, pctx);
	if (ret != 0)
		return ret;

	return E_LDM_RET_SUCCESS;

}


////remove-start trunk ld need refactor
//----------------------------------------------------------------
// MDrv_LDM_SetStrength - Set back light percent
/// @ingroup G_LDM_CONTROL
// @param: u8Percent: the percent ranged from 0 to 100
// @return: E_LDM_RET_SUCCESS is setted
//----------------------------------------------------------------
//u8 MDrv_LDM_SetStrength(u8 u8Percent)
//{
//	u8 iResult = 0;
//
//	iResult = MDrv_LD_SetGlobalStrength(u8Percent);
//
//	if (iResult == E_LDM_RET_FAIL) {
//		DRM_INFO("LDM setStrength Fail\n");
//		return E_LDM_RET_FAIL;
//	}
//
//	DRM_INFO("OK\n");
//	return E_LDM_RET_SUCCESS;
//
//}
////remove-end trunk ld need refactor



////remove-start trunk ld need refactor
//----------------------------------------------------------------
// MDrv_LDM_DemoPattern - demo pattern from customer
/// @ingroup G_LDM_CONTROL
// @param: stPattern: demo type: turn on led, left-right half show
// @return: E_LDM_RET_SUCCESS is demonstrative
//----------------------------------------------------------------
//u8 MDrv_LDM_DemoPattern(ST_LDM_DEMO_PATTERN stPattern)
//{
//
//	MDrv_LD_SetDemoPattern(
//		stPattern.enDemoPattern,
//		stPattern.bOn,
//		stPattern.u16LEDNum
//		);
//
//	DRM_INFO("Demo OK\n");
//
//	return E_LDM_RET_SUCCESS;
//
//}
////remove-end trunk ld need refactor



////remove-start trunk ld need refactor
//----------------------------------------------------------------
// MDrv_LDM_GetData - get LDF/LDB/SPI data pre frame in buffer
/// @ingroup G_LDM_CONTROL
// @param: stData:  data type and mmap address filled with the requied type
// @return: E_LDM_RET_SUCCESS is getted
//----------------------------------------------------------------
//u8 MDrv_LDM_GetData(ST_LDM_GET_DATA *stData)
//{
//	stData->phyAddr = MDrv_LD_GetDataAddr(stData->enDataType);
//	DRM_INFO("OK\n");
//	return E_LDM_RET_SUCCESS;
//
//}
////remove-end trunk ld need refactor



