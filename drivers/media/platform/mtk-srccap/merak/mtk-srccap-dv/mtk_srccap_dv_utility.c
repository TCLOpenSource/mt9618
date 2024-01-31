// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

//-----------------------------------------------------------------------------
// Include Files
//-----------------------------------------------------------------------------
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/platform_device.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>

#include "mtk_srccap.h"
#include "mtk_srccap_dv.h"

//-----------------------------------------------------------------------------
// Driver Capability
//-----------------------------------------------------------------------------

static int mtk_srccap_dv_cap_sw_bond_status = 1;	// default not support
static int mtk_srccap_dv_cap_hw_bond_status = 1;	// default not support


//-----------------------------------------------------------------------------
// Macros and Defines
//-----------------------------------------------------------------------------
#define DV_MAX_CMD_LENGTH (0xFF)
#define DV_MAX_ARG_NUM (64)
#define DV_CMD_REMOVE_LEN (2)

//-----------------------------------------------------------------------------
// Enums and Structures
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------
int g_dv_debug_level;
int g_dv_pr_level;

static bool mtk_srccap_dv_dbg_force_disable_dv = FALSE;


//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------

static void _debug_set_force_disable_dv(bool valid)
{
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "set force disable DV = %d\n", valid);
	mtk_srccap_dv_dbg_force_disable_dv = valid;
}


static int dv_parse_cmd_helper(char *buf, char *sep[], int max_cnt)
{
	char delim[] = " =,\n\r";
	char **b = &buf;
	char *token = NULL;
	int cnt = 0;

	while (cnt < max_cnt) {
		token = strsep(b, delim);
		if (token == NULL)
			break;
		sep[cnt++] = token;
	}

	// Exclude command and new line symbol
	return (cnt - DV_CMD_REMOVE_LEN);
}

static int dv_debug_print_help(void)
{
	int ret = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"----------------dv information start----------------\n");
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"SRCCAP_DV_SUPPORT: %u\n", mtk_dv_utility_get_dv_support());
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"SRCCAP_DV_VERSION: %u\n", SRCCAP_DV_VERSION);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"SRCCAP_DV_COMMON_VERSION: %u\n", SRCCAP_DV_COMMON_VERSION);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"SRCCAP_DV_DESCRB_VERSION: %u\n", SRCCAP_DV_DESCRB_VERSION);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"SRCCAP_DV_DMA_VERSION: %u\n", SRCCAP_DV_DMA_VERSION);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"SRCCAP_DV_META_VERSION: %u\n", SRCCAP_DV_META_VERSION);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"----------------dv information end----------------\n\n");

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"----------------dv debug mode info start----------------\n");
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"force_disable_dv=%d\n", mtk_srccap_dv_dbg_force_disable_dv);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"----------------dv debug mode info end----------------\n");

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"----------------dv debug commands help start----------------\n");
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"set_debug_level=debug_level\n");
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"dv_debug_set_pr_level=pr_level\n");

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"force_disable_dolby=valid\n");
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"    valid: 1, 0\n");

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"force_game_mode=mode\n");
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"    mode: %d(invalid), %d(force on), %d(force off)\n",
		SRCCAP_DV_DEBUG_FORCE_GAME_MODE_NONE,
		SRCCAP_DV_DEBUG_FORCE_GAME_MODE_ON,
		SRCCAP_DV_DEBUG_FORCE_GAME_MODE_OFF);

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
		"----------------dv debug commands help end----------------\n");

	return  ret;
}

static int dv_debug_set_debug_level(const char *args[], int arg_num)
{
	int ret = 0;
	int dv_debug_level = 0;

	if (arg_num < 1)
		goto exit;

	ret = kstrtoint(args[0], 0, &dv_debug_level);
	if (ret < 0)
		goto exit;

	g_dv_debug_level = dv_debug_level;

exit:
	return ret;
}

static int dv_debug_set_pr_level(const char *args[], int arg_num)
{
	int ret = 0;
	int pr_level = 0;

	if (arg_num < 1)
		goto exit;

	ret = kstrtoint(args[0], 0, &pr_level);
	if (ret < 0)
		goto exit;

	g_dv_pr_level = pr_level;

exit:
	return ret;
}

static int dv_debug_set_force_disable_dolby(const char *args[], int arg_num)
{
	int ret = 0;
	int valid = 0;
	bool en = FALSE;

	if (args == NULL)
		goto exit;

	if (arg_num < 1)
		goto exit;

	ret = kstrtoint(args[0], 0, &valid);
	if (ret < 0)
		goto exit;

	en = (valid > 0);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_DEBUG, "force_disable_dolby = %d\n", en);
	_debug_set_force_disable_dv(en);

	/* refresh SW bond */
	mtk_dv_utility_refresh_dv_support();

exit:
	return ret;
}

static int dv_debug_set_force_game_mode(
	const char *args[],
	int arg_num,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	int value = 0;
	enum srccap_dv_debug_force_game_mode game_mode = SRCCAP_DV_DEBUG_FORCE_GAME_MODE_NONE;

	if ((args == NULL) || (srccap_dev == NULL))
		goto exit;

	if (arg_num < 1)
		goto exit;

	ret = kstrtoint(args[0], 0, &value);
	if (ret < 0)
		goto exit;

	if (value == SRCCAP_DV_DEBUG_FORCE_GAME_MODE_ON)
		game_mode = SRCCAP_DV_DEBUG_FORCE_GAME_MODE_ON;
	else if (value == SRCCAP_DV_DEBUG_FORCE_GAME_MODE_OFF)
		game_mode = SRCCAP_DV_DEBUG_FORCE_GAME_MODE_OFF;
	else
		game_mode = SRCCAP_DV_DEBUG_FORCE_GAME_MODE_NONE;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_DEBUG, "force_game_mode = %d\n", game_mode);
	srccap_dev->dv_info.debug_mode.force_game_mode = game_mode;

exit:
	return ret;
}


//-----------------------------------------------------------------------------
// Global Functions
//-----------------------------------------------------------------------------

int mtk_dv_utility_refresh_dv_support(void)
{
	int ret = 0;

	/* SW bond: default enable */
	if (mtk_srccap_dv_dbg_force_disable_dv)
		mtk_srccap_dv_cap_sw_bond_status = 1;
	else
		mtk_srccap_dv_cap_sw_bond_status = 0;

	/* todo: HW bond: update from efuse/hashkey */
	mtk_srccap_dv_cap_hw_bond_status = 0;

	return ret;
}

bool mtk_dv_utility_get_dv_support(void)
{
	bool ret = FALSE;

	ret = (!(mtk_srccap_dv_cap_sw_bond_status | mtk_srccap_dv_cap_hw_bond_status));

	return ret;
}


//-----------------------------------------------------------------------------
// Debug Functions
//-----------------------------------------------------------------------------
int mtk_dv_debug_show(
	struct device *dev,
	const char *buf)
{
	int ssize = 0;

	if ((dev == NULL) || (buf == NULL))
		return 0;

	dv_debug_print_help();

	return ssize;
}

int mtk_dv_debug_store(
	struct device *dev,
	const char *buf)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	char cmd[DV_MAX_CMD_LENGTH];
	char *args[DV_MAX_ARG_NUM] = {NULL};
	int arg_num;

	if ((dev == NULL) || (buf == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	srccap_dev = dev_get_drvdata(dev);

	strncpy(cmd, buf, DV_MAX_CMD_LENGTH);
	cmd[DV_MAX_CMD_LENGTH - 1] = '\0';

	arg_num = dv_parse_cmd_helper(cmd, args, DV_MAX_ARG_NUM);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "cmd: %s, num: %d.\n", cmd, arg_num);

	if (strncmp(cmd, "set_debug_level", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_set_debug_level((const char **)&args[1], arg_num);
	} else if (strncmp(cmd, "set_pr_level", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_set_pr_level((const char **)&args[1], arg_num);
	} else if (strncmp(cmd, "force_disable_dolby", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_set_force_disable_dolby((const char **)&args[1], arg_num);
	} else if (strncmp(cmd, "force_game_mode", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_set_force_game_mode((const char **)&args[1], arg_num, srccap_dev);
	} else {
		dv_debug_print_help();
		goto exit;
	}

	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

exit:
	return ret;
}

int mtk_dv_debug_checksum(
	__u8 *data,
	__u32 size,
	__u32 *sum)
{
	int ret = 0;
	int i = 0, s = 0;

	if ((data == NULL) || (sum == NULL))
		return -EINVAL;

	for (i = 0; i < size; i++)
		s += *(data + i);

	*sum = s;

	return ret;
}

int mtk_dv_debug_reset_debug_mode(void)
{
	int ret = 0;

	mtk_srccap_dv_dbg_force_disable_dv = FALSE;

	return ret;
}

bool mtk_dv_is_descrb_control(void)
{
	bool ret = false;

	if (mtk_dv_utility_get_dv_support() != TRUE)
		goto exit;

	ret = !pqu_descrb_irq_status;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO,
		"pqu_descrb_irq_status = %d, ret = %d\n",
		pqu_descrb_irq_status, ret);
exit:
	return ret;
}

