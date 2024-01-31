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
#include <linux/delay.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include "pqu_msg.h"

#include "mtk_pq.h"
#include "mtk_pq_hdr.h"
#include "mtk_pq_dv.h"
#include "mtk_pq_dv_version.h"
#include "mtk_pq_common_ca.h"
#include "utpa2_XC.h"

//-----------------------------------------------------------------------------
// Driver Capability
//-----------------------------------------------------------------------------

static int mtk_pq_dv_cap_hw_bond_status = 1;	// default not support
static int mtk_pq_dv_cap_sw_bond_status = -1;	// default not support


//-----------------------------------------------------------------------------
// Macros and Defines
//-----------------------------------------------------------------------------

#define DV_PQDD_HW_TAG_V1		(1)	//ref to DV_HW_TAG_V1
#define DV_PQDD_HW_TAG_V2		(2)	//ref to DV_HW_TAG_V2
#define DV_PQDD_HW_TAG_V3		(3)	//ref to DV_HW_TAG_V3
#define DV_PQDD_HW_TAG_V4		(4)	//ref to DV_HW_TAG_V4
#define DV_PQDD_HW_TAG_V5		(5)	//ref to DV_HW_TAG_V5


#define DV_DMA_ALIGN(val, align) (((val) + ((align) - 1)) & (~((align) - 1)))
#define DV_DMA_BUF_TAG (23)
#define DV_DMA_BUF_TAG_SHIFT (24)

#define DV_PYR_CHANNEL_SIZE      (10)
#define DV_PYR_FRAME_NUM         (3)
#define DV_PYR_RW_DIFF           (1)
#define DV_PYR_0_IDX             (0)
#define DV_PYR_0_WIDTH           (1024)
#define DV_PYR_0_HEIGHT          (576)
#define DV_PYR_1_IDX             (1)
#define DV_PYR_1_WIDTH           (512)
#define DV_PYR_1_HEIGHT          (288)
#define DV_PYR_2_IDX             (2)
#define DV_PYR_2_WIDTH           (256)
#define DV_PYR_2_HEIGHT          (144)
#define DV_PYR_3_IDX             (3)
#define DV_PYR_3_WIDTH           (128)
#define DV_PYR_3_HEIGHT          (72)
#define DV_PYR_4_IDX             (4)
#define DV_PYR_4_WIDTH           (64)
#define DV_PYR_4_HEIGHT          (36)
#define DV_PYR_5_IDX             (5)
#define DV_PYR_5_WIDTH           (32)
#define DV_PYR_5_HEIGHT          (18)
#define DV_PYR_6_IDX             (6)
#define DV_PYR_6_WIDTH           (16)
#define DV_PYR_6_HEIGHT          (9)
#define DV_BIT_PER_WORD          (256)
#define DV_BIT_PER_BYTE          (8)
#define DV_MAX_CMD_LENGTH        (0xFF)
#define DV_MAX_ARG_NUM           (64)
#define DV_CMD_REMOVE_LEN        (2)

#define Fld(wid, shft, ac)  (((uint32_t)wid<<16)|(shft<<8)|ac)
#define AC_FULLB0  (1)
#define AC_FULLB1  (2)
#define AC_FULLB2  (3)
#define AC_FULLB3  (4)
#define AC_FULLW10 (5)
#define AC_FULLW21 (6)
#define AC_FULLW32 (7)
#define AC_FULLDW  (8)
#define AC_MSKB0   (9)
#define AC_MSKB1   (10)
#define AC_MSKB2   (11)
#define AC_MSKB3   (12)
#define AC_MSKW10  (13)
#define AC_MSKW21  (14)
#define AC_MSKW32  (15)
#define AC_MSKDW   (16)

#define MASK_FULLDW  (0xFFFFFFFF)
#define MASK_FULLW32 (0xFFFF0000)
#define MASK_FULLW21 (0x00FFFF00)
#define MASK_FULLW10 (0x0000FFFF)
#define MASK_FULLB3  (0xFF000000)
#define MASK_FULLB2  (0x00FF0000)
#define MASK_FULLB1  (0x0000FF00)
#define MASK_FULLB0  (0x000000FF)

#define MASK_X32_BASE_BANK (0xFFF000)
#define MASK_X32_BANK      (0xF00)
#define MASK_X32_ADDR      (0xFF)
#define MASK_X16_ADDR      (0xFF)
#define MUL_X32_ADDR_32TO8 (4)

#define SYSFS_MAX_BUF_COUNT        (0x100)
#define IDK_ENABLE_POSITION        (14)


#define DEBUG_CMD_ARG_NUM_L1L4GEN	(4)


#define DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, _fmt, _args...) \
	do { \
		(ret) = snprintf((buf) + (count), PAGE_SIZE - (count), _fmt, ##_args); \
		if ((ret) >= 0) \
			(count) += (ret); \
	} while (0)


//-----------------------------------------------------------------------------
// Enums and Structures
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------
int g_mtk_pq_dv_debug_level;

//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------
static bool dv_get_idk_status(void)
{
#ifdef DOLBY_IDK_DUMP_ENABLE
	mm_segment_t cur_mm_seg;
	struct file *verfile = NULL;
	loff_t pos;
	char *pu8EnableFlag = NULL;
	int u32ReadCount = 0;

	// check if Dolby IDK is enabled
	PQ_MALLOC(pu8EnableFlag, sizeof(char) * SYSFS_MAX_BUF_COUNT);
	if (pu8EnableFlag == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "cannot malloc to read idk_dump node\n");
		return false;
	}

	cur_mm_seg = get_fs();
	set_fs(KERNEL_DS);
	verfile = filp_open("/sys/devices/platform/mtk-pq/mtk_dbg/mtk_pq_idkdump",
		O_RDONLY, 0);

	if (IS_ERR_OR_NULL(verfile)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "cannot open idk_dump node\n");
		set_fs(cur_mm_seg);
		PQ_FREE(pu8EnableFlag, sizeof(char) * SYSFS_MAX_BUF_COUNT);
		return false;
	}

	pos = vfs_llseek(verfile, 0, SEEK_SET);
	u32ReadCount = kernel_read(verfile, pu8EnableFlag, SYSFS_MAX_BUF_COUNT, &pos);
	set_fs(cur_mm_seg);

	if (u32ReadCount < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "open idk_dump node error %d\n", u32ReadCount);
		PQ_FREE(pu8EnableFlag, sizeof(char) * SYSFS_MAX_BUF_COUNT);
		filp_close(verfile, NULL);
		return false;
	}

	// check string "enable_status:%d\n", gu16IDKEnable);
	if (pu8EnableFlag[IDK_ENABLE_POSITION] == '1') {
		PQ_FREE(pu8EnableFlag, sizeof(char) * SYSFS_MAX_BUF_COUNT);
		filp_close(verfile, NULL);
		return true;
	}

	PQ_FREE(pu8EnableFlag, sizeof(char) * SYSFS_MAX_BUF_COUNT);
	filp_close(verfile, NULL);
	return false;
#else
	/* Dolby IDK not enabled */
	return false;
#endif
}

static unsigned int dv_get_bits_set(unsigned int v)
{
	// c accumulates the total bits set in v
	unsigned int c;

	for (c = 0; v; c++) {
		// clear the least significant bit set
		v &= v - 1;
	}

	return c;
}

static unsigned int dv_get_ac(uint32_t mask)
{
	bool MaskB0 = FALSE, MaskB1 = FALSE, MaskB2 = FALSE, MaskB3 = FALSE;

	switch (mask) {
	case MASK_FULLDW:
		return AC_FULLDW;
	case MASK_FULLW32:
		return AC_FULLW32;
	case MASK_FULLW21:
		return AC_FULLW21;
	case MASK_FULLW10:
		return AC_FULLW10;
	case MASK_FULLB3:
		return AC_FULLB3;
	case MASK_FULLB2:
		return AC_FULLB2;
	case MASK_FULLB1:
		return AC_FULLB1;
	case MASK_FULLB0:
		return AC_FULLB0;
	}

	MaskB0 = !!(mask & MASK_FULLB0);
	MaskB1 = !!(mask & MASK_FULLB1);
	MaskB2 = !!(mask & MASK_FULLB2);
	MaskB3 = !!(mask & MASK_FULLB3);

	if (MaskB3 && MaskB2 && MaskB1 && MaskB0)
		return AC_MSKDW;
	else if (MaskB3 && MaskB2)
		return AC_MSKW32;
	else if (MaskB2 && MaskB1)
		return AC_MSKW21;
	else if (MaskB1 && MaskB0)
		return AC_MSKW10;
	else if (MaskB3)
		return AC_MSKB3;
	else if (MaskB2)
		return AC_MSKB2;
	else if (MaskB1)
		return AC_MSKB1;
	else if (MaskB0)
		return AC_MSKB0;

	return 0;
}

static bool dv_is_X32_bank(u32 addr)
{
	u32 addr_masked = 0;

	addr_masked = addr & MASK_X32_BASE_BANK;

	switch (addr_masked) {
	case 0xAF0000: /* DV core 1  */
	case 0xAF1000: /* DV core 1b */
	case 0xAE0000: /* DV core 2  */
	case 0xAE8000: /* DV core 2b */
		return TRUE;
	default:
		return FALSE;
	}

	return FALSE;
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

static int dv_debug_print_help(struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	struct mtk_pq_dv_debug_force_disable_dolby *disable_dolby = NULL;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		return ret;
	}

	disable_dolby = &(dv_debug->force_ctrl.disable_dolby);

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"----------------dv information start----------------\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"DV_PQ_DV_SUPPORT: %d\n", mtk_pq_dv_get_dv_support());
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"DV_PQ_VERSION: %u\n", DV_PQ_VERSION);
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"----------------dv information end----------------\n\n");

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"----------------dv debug mode info start----------------\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"force_disable_dolby: %d\n", disable_dolby->en);
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"set_debug_mode: %d (0x%X 0x%X 0x%X)\n", dv_debug->debug_mode.mode,
		dv_debug->debug_mode.arg1, dv_debug->debug_mode.arg2, dv_debug->debug_mode.arg3);
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"----------------dv debug mode info end----------------\n\n");

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"----------------debug commands help start----------------\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		" set_debug_level=debug_level_pq,debug_level_pqu,debug_level_3pty\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		" force_reg=en,addr,val,mask,addr2,val2,mask2,addr3,val3,mask3\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		" force_viewmode=en,view_id\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"               =1,1\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, " force_darkdetail=en,on\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, "                 =1,1\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, " force_apo=en,on\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, "                 =1,1\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, " force_whitepoint=en,wp_val\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, "                 =1,10\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, " force_lightsense=en,mode,lux_val\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, "                 =1,5,200\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, " force_l1l4gen=en,mode,is_core1,distance\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, "                 =1,1,1,1\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, " dv_debug_force_turnoff_pr=ctrl\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO, "                 =0(disable); 1(turnoff)\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"force_ctrl_flow=en,debug_ctrl_pqu,debug_ctrl_3pty\n");
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
		"----------------debug commands help end------------------\n");

	return  ret;
}

static int dv_debug_set_debug_level(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 val;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 3)
		return -EINVAL;

	arg_idx = 0;
	ret = kstrtoint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	g_mtk_pq_dv_debug_level = val;

	ret = kstrtoint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->debug_level.debug_level_pqu = val;

	ret = kstrtoint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->debug_level.debug_level_3pty = val;

	dv_debug->debug_level.en = TRUE;

exit:
	return  ret;
}

static int dv_debug_set_debug_mode(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 mode;
	u32 argu;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 1)
		return -EINVAL;

	arg_idx = 0;

	/* arg 0: mode */
	ret = kstrtoint(args[arg_idx++], 0, &mode);
	if (ret < 0)
		goto exit;
	dv_debug->debug_mode.mode = mode;
	dv_debug->debug_mode.arg1 = 0;
	dv_debug->debug_mode.arg2 = 0;
	dv_debug->debug_mode.arg3 = 0;

	/* arg 1 */
	if (arg_num > arg_idx) {
		ret = kstrtoint(args[arg_idx++], 0, &argu);
		if (ret < 0)
			goto exit;
		dv_debug->debug_mode.arg1 = argu;
	}

	/* arg 2 */
	if (arg_num > arg_idx) {
		ret = kstrtoint(args[arg_idx++], 0, &argu);
		if (ret < 0)
			goto exit;
		dv_debug->debug_mode.arg2 = argu;
	}

	/* arg 3 */
	if (arg_num > arg_idx) {
		ret = kstrtoint(args[arg_idx++], 0, &argu);
		if (ret < 0)
			goto exit;
		dv_debug->debug_mode.arg3 = argu;
	}

exit:
	return  ret;
}

static int dv_debug_force_reg(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	int reg_idx;
	u32 addr;
	u32 val;
	u32 mask;
	u32 mask_width, LSB, AC;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 1)
		return -EINVAL;

	arg_idx = 0;
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_reg.en = !!val;

	dv_debug->force_reg.reg_num = 0;
	while (arg_idx + 2 < arg_num) {
		ret = kstrtouint(args[arg_idx++], 0, &addr);
		ret |= kstrtouint(args[arg_idx++], 0, &val);
		ret |= kstrtouint(args[arg_idx++], 0, &mask);
		if (ret < 0)
			continue;

		reg_idx = dv_debug->force_reg.reg_num;
		if ((reg_idx < 0) || (reg_idx >= MTK_PQ_DV_DBG_MAX_SET_REG_NUM))
			break;

		if (dv_is_X32_bank(addr)) {
			/* the input addr is XYZPQR, XYZ is basebank, like AF0, AF1, AE0, AE8. */
			/* X is bank number. YZ is address in 32bit format*/
			dv_debug->force_reg.regs[reg_idx].addr = ((addr&MASK_X32_BASE_BANK)<<1)
							+ (addr&MASK_X32_BANK)
							+ (addr&MASK_X32_ADDR)*MUL_X32_ADDR_32TO8;
			dv_debug->force_reg.regs[reg_idx].val = val;
			dv_debug->force_reg.regs[reg_idx].mask = MASK_FULLDW;
		} else {
			// get mask width [15:8] gives 8. mask must be continuous
			mask_width = dv_get_bits_set(mask);

			// get LSB
			LSB = ffs(mask) - 1;

			// get AC
			AC = dv_get_ac(mask);

			if (AC == 0)
				continue;

			dv_debug->force_reg.regs[reg_idx].addr =
				((addr + (addr & MASK_X16_ADDR)) << 1);
			dv_debug->force_reg.regs[reg_idx].val = (val >> LSB);
			dv_debug->force_reg.regs[reg_idx].mask = Fld(mask_width, LSB, AC);
		}
		dv_debug->force_reg.reg_num++;
	}

exit:
	return  ret;
}

static int dv_debug_force_viewmode(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 en;
	u32 val;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 2)
		return -EINVAL;

	arg_idx = 0;

	/* arg 0: en */
	ret = kstrtouint(args[arg_idx++], 0, &en);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.view_mode.en = !!en;

	/* arg 1: view_id */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.view_mode.view_id = (uint8_t)val;

exit:
	return  ret;
}

static int dv_debug_force_idk_ctrl_flow(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 val;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 3)
		return -EINVAL;

	arg_idx = 0;
	ret = kstrtoint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;

	ret = kstrtoint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_idk_ctrl.force_idk_pqu_ctrl = val;

	ret = kstrtoint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_idk_ctrl.force_idk_3pty_ctrl = val;

	dv_debug->force_idk_ctrl.enable_idk_ctrl = TRUE;

exit:
	return	ret;
}

static int dv_debug_force_pr(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx = 0;
	int extra_frame_num_valid = 0;
	int extra_frame_num = 0;
	int pr_en_valid = 0;
	int pr_en = 0;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 1)
		return -EINVAL;

	arg_idx = 0;
	ret = kstrtoint(args[arg_idx++], 0, &extra_frame_num_valid);
	if (ret < 0)
		goto exit;

	ret = kstrtoint(args[arg_idx++], 0, &extra_frame_num);
	if (ret < 0)
		goto exit;

	ret = kstrtoint(args[arg_idx++], 0, &pr_en_valid);
	if (ret < 0)
		goto exit;

	ret = kstrtoint(args[arg_idx++], 0, &pr_en);
	if (ret < 0)
		goto exit;

	dv_debug->force_pr.extra_frame_num_valid = !!extra_frame_num_valid;
	dv_debug->force_pr.extra_frame_num = extra_frame_num;
	dv_debug->force_pr.pr_en_valid = !!pr_en_valid;
	dv_debug->force_pr.pr_en = pr_en;

exit:
	return  ret;
}

static int dv_debug_force_disable_dolby(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug,
	struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	int arg_idx = 0;
	int valid = 0;
	struct mtk_pq_remap_buf *dv_config = NULL;

	if (dv_debug == NULL || pqdev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}
	dv_config = &pqdev->dv_config;

	if (arg_num < 1)
		return -EINVAL;

	arg_idx = 0;
	ret = kstrtoint(args[arg_idx++], 0, &valid);
	if (ret < 0)
		goto exit;


	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_DEBUG, "force_disable_dolby=%d\n", valid);
	dv_debug->force_ctrl.disable_dolby.en = (valid == 1);

	/* refresh SW bond */
	mtk_pq_dv_refresh_dolby_support(NULL, dv_debug);

	/* update to PQU */
	if (dv_config->va != 0) {
		struct st_hdr_info hdr_info = {0};
		bool ctrl = FALSE;
		struct v4l2_PQ_dv_config_info_s *dv_config_info = NULL;

		/* disable bin_info and mode_info update */
		dv_config_info = (struct v4l2_PQ_dv_config_info_s *)dv_config->va;
		dv_config_info->bin_info.en = FALSE;
		dv_config_info->mode_info.en = FALSE;

		/* do not update cap_info */

		/* set control info */
		dv_config_info->ctrl_info.disable_dv = dv_debug->force_ctrl.disable_dolby.en;

		ctrl = TRUE;
		mtk_hdr_SetDVBinDone(&hdr_info, &ctrl, pqdev);

		ctrl = FALSE;
		mtk_hdr_SetDVBinDone(&hdr_info, &ctrl, pqdev);
	}

exit:
	return  ret;
}

static int dv_debug_force_darkdetail(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 en;
	u32 val;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 2)
		return -EINVAL;

	arg_idx = 0;

	/* arg 0: en */
	ret = kstrtouint(args[arg_idx++], 0, &en);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.dark_detail.en = !!en;

	/* arg 1: on */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.dark_detail.on = !!val;

exit:
	return  ret;
}


static int dv_debug_force_apo(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 en;
	u32 val;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 2)
		return -EINVAL;

	arg_idx = 0;

	/* arg 0: en */
	ret = kstrtouint(args[arg_idx++], 0, &en);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.iq_apo.en = !!en;

	/* arg 1: on */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.iq_apo.on = (uint8_t)val;

exit:
	return  ret;
}

static int dv_debug_force_whitepoint(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 en;
	u32 val;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 2)
		return -EINVAL;

	arg_idx = 0;

	/* arg 0: en */
	ret = kstrtouint(args[arg_idx++], 0, &en);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.white_point.en = !!en;

	/* arg 1: wp_val */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.white_point.wp_val = (uint8_t)val;

exit:
	return  ret;
}

static int dv_debug_force_lightsense(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 en;
	u32 val;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 3)
		return -EINVAL;

	arg_idx = 0;

	/* arg 0: en */
	ret = kstrtouint(args[arg_idx++], 0, &en);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.light_sense.en = !!en;

	/* arg 1: mode */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.light_sense.mode = (uint8_t)val;

	/* arg 1: lux_val */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.light_sense.lux_val = (uint32_t)val;

exit:
	return  ret;
}

static int dv_debug_force_l1l4gen(
	const char *args[],
	int arg_num,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;
	int arg_idx;
	u32 en;
	u32 val;

	if (dv_debug == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < DEBUG_CMD_ARG_NUM_L1L4GEN)
		return -EINVAL;

	arg_idx = 0;

	/* arg 0: en */
	ret = kstrtouint(args[arg_idx++], 0, &en);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.l1l4_gen.en = !!en;

	/* arg 1: mode */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.l1l4_gen.mode = (uint8_t)val;

	/* arg 2: is_core1 */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.l1l4_gen.is_core1 = (val == 1);

	/* arg 3: distance */
	ret = kstrtouint(args[arg_idx++], 0, &val);
	if (ret < 0)
		goto exit;
	dv_debug->force_ctrl.l1l4_gen.distance = (uint32_t)val;

exit:
	return  ret;
}

static int dv_debug_force_turnoff_pr(
	const char *args[],
	int arg_num,
	struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	int arg_idx = 0;
	int pr_ctrl = 0;
	uint8_t turnoff = 0;

	if (pqdev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (arg_num < 1)
		return -EINVAL;

	arg_idx = 0;
	ret = kstrtoint(args[arg_idx++], 0, &pr_ctrl);
	if (ret < 0)
		goto exit;

	turnoff = pr_ctrl;
	mtk_pq_dv_set_pr_ctrl(&turnoff, pqdev);

exit:
	return  ret;
}



/* dolby debug process for qbuf */
static int dv_debug_qbuf(
	struct mtk_pq_device *pq_dev,
	struct meta_pq_dv_info *meta_dv_pq,
	struct m_pq_dv_debug *meta_dv_debug)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_dv_debug *dv_debug = NULL;

	if ((pq_dev == NULL) ||
		(meta_dv_pq == NULL) ||
		(meta_dv_debug == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);
	dv_debug = &pqdev->dv_ctrl.debug;

	/* prepare dolby debug info into */
	if (dv_debug->debug_level.en) {
		meta_dv_debug->debug_level.valid = TRUE;
		meta_dv_debug->debug_level.debug_level_pqu =
			dv_debug->debug_level.debug_level_pqu;
		meta_dv_debug->debug_level.debug_level_3pty =
			dv_debug->debug_level.debug_level_3pty;
	}

	if (dv_debug->debug_mode.mode != 0) {
		meta_dv_debug->debug_mode.mode = dv_debug->debug_mode.mode;
		meta_dv_debug->debug_mode.arg1 = dv_debug->debug_mode.arg1;
		meta_dv_debug->debug_mode.arg2 = dv_debug->debug_mode.arg2;
		meta_dv_debug->debug_mode.arg3 = dv_debug->debug_mode.arg3;
	}

	if (dv_debug->force_reg.en) {
		meta_dv_debug->set_reg.valid = TRUE;
		meta_dv_debug->set_reg.reg_num = dv_debug->force_reg.reg_num;
		memcpy(meta_dv_debug->set_reg.regs,
			dv_debug->force_reg.regs,
			sizeof(meta_dv_debug->set_reg.regs));
	}

	if (dv_debug->force_ctrl.view_mode.en) {
		meta_dv_debug->force_ctrl.view_mode.en = TRUE;
		meta_dv_debug->force_ctrl.view_mode.view_id =
			dv_debug->force_ctrl.view_mode.view_id;
	}

	if (dv_debug->force_pr.extra_frame_num_valid)
		pq_dev->dv_win_info.common.extra_frame_num =
			dv_debug->force_pr.extra_frame_num;

	if (dv_debug->force_pr.pr_en_valid)
		meta_dv_pq->pr_ctrl.en = dv_debug->force_pr.pr_en;

	if (dv_debug->force_ctrl.dark_detail.en) {
		meta_dv_debug->force_ctrl.dark_detail.en = TRUE;
		meta_dv_debug->force_ctrl.dark_detail.on =
			dv_debug->force_ctrl.dark_detail.on;
	}

	if (dv_debug->force_ctrl.iq_apo.en) {
		meta_dv_debug->force_ctrl.iq_apo.en = TRUE;
		meta_dv_debug->force_ctrl.iq_apo.on = dv_debug->force_ctrl.iq_apo.on;
	}

	if (dv_debug->force_ctrl.white_point.en) {
		meta_dv_debug->force_ctrl.white_point.en = TRUE;
		meta_dv_debug->force_ctrl.white_point.wp_val =
			dv_debug->force_ctrl.white_point.wp_val;
	}

	if (dv_debug->force_ctrl.light_sense.en) {
		meta_dv_debug->force_ctrl.light_sense.en = TRUE;
		meta_dv_debug->force_ctrl.light_sense.mode =
			dv_debug->force_ctrl.light_sense.mode;
		meta_dv_debug->force_ctrl.light_sense.lux_val =
			dv_debug->force_ctrl.light_sense.lux_val;
	}

	if (dv_debug->force_ctrl.l1l4_gen.en) {
		meta_dv_debug->force_ctrl.l1l4_gen.en = TRUE;
		meta_dv_debug->force_ctrl.l1l4_gen.mode =
			dv_debug->force_ctrl.l1l4_gen.mode;
		meta_dv_debug->force_ctrl.l1l4_gen.is_core1 =
			dv_debug->force_ctrl.l1l4_gen.is_core1;
		meta_dv_debug->force_ctrl.l1l4_gen.distance =
			dv_debug->force_ctrl.l1l4_gen.distance;
	}

	if (dv_get_idk_status() == TRUE)
		meta_dv_debug->force_ctrl.idkenable = TRUE;

	if (dv_debug->force_idk_ctrl.enable_idk_ctrl) {
		meta_dv_debug->force_idk_ctrl.enable_idk_ctrl = TRUE;
		meta_dv_debug->force_idk_ctrl.force_idk_pqu_ctrl =
			dv_debug->force_idk_ctrl.force_idk_pqu_ctrl;
		meta_dv_debug->force_idk_ctrl.force_idk_3pty_ctrl =
			dv_debug->force_idk_ctrl.force_idk_3pty_ctrl;
	}

exit:
	return ret;
}

static int dv_allocate_pyr_buf(struct mtk_pq_device *pq_dev)
{
	int ret = 0, i = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	u64 offset = 0;

	if (pq_dev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (pq_dev->dv_win_info.pyr_buf.valid) {
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
			"pyr buffer exist\n");
		goto exit;
	}

	if (pq_dev->dv_win_info.pyr_buf.size == 0) {
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
			"pyr buffer size = 0\n");
		goto exit;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
		"mmap addr: %llx, mmap size: %x, required size: %x\n",
		pqdev->dv_ctrl.pyr_ctrl.mmap_addr,
		pqdev->dv_ctrl.pyr_ctrl.mmap_size,
		pq_dev->dv_win_info.pyr_buf.size);

	/* NOT an error if buffer allocation fail */
	if (pq_dev->dv_win_info.pyr_buf.size >
		pqdev->dv_ctrl.pyr_ctrl.mmap_size) {
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
			"mmap size not enough\n");
		goto exit;
	}

	/* NOT an error if buffer allocation fail */
	if (pqdev->dv_ctrl.pyr_ctrl.mmap_addr == 0) {
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
			"mmap addr = 0\n");
		goto exit;
	}

	/* allocate buffer */
	pq_dev->dv_win_info.pyr_buf.addr =
		(pqdev->dv_ctrl.pyr_ctrl.mmap_addr -
		 BUSADDRESS_TO_IPADDRESS_OFFSET);

	offset = 0;
	for (i = 0; i < MTK_PQ_DV_PYR_NUM; i++) {
		/* unit: n bit align */
		pq_dev->dv_win_info.pyr_buf.pyr_addr[i] =
			(pq_dev->dv_win_info.pyr_buf.addr + offset) *
			DV_BIT_PER_BYTE / DV_BIT_PER_WORD;

		/* unit: byte */
		offset += pq_dev->dv_win_info.pyr_buf.pyr_size[i];

		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
			"pyramid: %d, addr: %llx, pitch: %x, size: %x\n",
			i,
			pq_dev->dv_win_info.pyr_buf.pyr_addr[i],
			pq_dev->dv_win_info.pyr_buf.frame_pitch[i],
			pq_dev->dv_win_info.pyr_buf.pyr_size[i]);
	}

	/* set dolby window info */
	pq_dev->dv_win_info.pyr_buf.valid = TRUE;

	/* set dolby global control */
	pqdev->dv_ctrl.pyr_ctrl.available = FALSE;

	if (pq_dev->dv_win_info.pyr_buf.valid &&
		pq_dev->dv_win_info.secure.svp_id_valid) {
		/* lock dma buffer */
		if (pq_dev->dv_win_info.secure.svp_buf_authed) {
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
				"buffer already locked\n");
			goto already_auth;
		}
		ret = mtkd_iommu_buffer_authorize(
				(DV_DMA_BUF_TAG << DV_DMA_BUF_TAG_SHIFT),
				pq_dev->dv_win_info.pyr_buf.addr,
				pq_dev->dv_win_info.pyr_buf.size,
				pq_dev->dv_win_info.secure.svp_id);
		if (ret) {
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
				"lock buffer fail, return: %d\n", ret);
		} else {
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_SVP,
				"lock buffer with id: %u\n",
				pq_dev->dv_win_info.secure.svp_id);
			pq_dev->dv_win_info.secure.svp_buf_authed = TRUE;
		}
	}

already_auth:
exit:
	return ret;
}

static int dv_free_pyr_buf(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	if (pq_dev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);

	if (pq_dev->dv_win_info.pyr_buf.valid &&
		pq_dev->dv_win_info.secure.svp_id_valid) {
		/* unlock dma buffer */
		if (!pq_dev->dv_win_info.secure.svp_buf_authed) {
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
				"buffer already unlocked\n");
			goto already_unauth;
		}
		ret = mtkd_iommu_buffer_unauthorize(pq_dev->dv_win_info.pyr_buf.addr);
		if (ret) {
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
				"unlock buffer fail, return: %d\n", ret);
		} else {
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_SVP,
				"unlock buffer with id: %u\n",
				pq_dev->dv_win_info.secure.svp_id);
			pq_dev->dv_win_info.secure.svp_buf_authed = FALSE;
		}
	}

	if (pq_dev->dv_win_info.pyr_buf.valid) {
		/* set dolby window info */
		pq_dev->dv_win_info.pyr_buf.valid = FALSE;

		/* set dolby global control */
		pqdev->dv_ctrl.pyr_ctrl.available = TRUE;
	}

already_unauth:
exit:
	return ret;
}

static int dv_try_allocate_pyr_buf(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	bool pr_support = FALSE;

	if (pq_dev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);
	if (pqdev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	mtk_pq_dv_get_pr_cap(pqdev, &pr_support);

	if (!pr_support || pqdev->dv_ctrl.pyr_ctrl.turnoff) {
		/* PR not available */
		if (pq_dev->dv_win_info.pyr_buf.valid)
			ret = dv_free_pyr_buf(pq_dev);
	}

	if (!pr_support)
		pqdev->dv_ctrl.pyr_ctrl.available = FALSE;

	if (pqdev->config_info.u8DV_PQ_Version == DV_PQDD_HW_TAG_V5) {
		if ((pqdev->dv_ctrl.pyr_ctrl.available) &&
			(pqdev->dv_ctrl.pyr_ctrl.turnoff == FALSE) &&
			(!pq_dev->dv_win_info.pyr_buf.valid) &&
			(pq_dev->dv_win_info.pr_ctrl.is_dolby))
			ret = dv_allocate_pyr_buf(pq_dev);
	} else {
		if ((pqdev->dv_ctrl.pyr_ctrl.available) &&
			(pqdev->dv_ctrl.pyr_ctrl.turnoff == FALSE) &&
			(!pq_dev->dv_win_info.pyr_buf.valid) &&
			(pq_dev->dv_win_info.pr_ctrl.is_dolby) &&
			(pq_dev->dv_win_info.pr_ctrl.width > 0) &&
			(pq_dev->dv_win_info.pr_ctrl.height > 0))
			ret = dv_allocate_pyr_buf(pq_dev);
	}

exit:
	return ret;
}

static int dv_set_pr(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	if (pq_dev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);

	pq_dev->dv_win_info.pr_ctrl.en = FALSE;

	if (pqdev->config_info.u8DV_PQ_Version == DV_PQDD_HW_TAG_V5) {
		if ((pq_dev->dv_win_info.pyr_buf.valid) &&
			(pqdev->dv_ctrl.pyr_ctrl.turnoff == FALSE) &&
			(pq_dev->dv_win_info.pr_ctrl.is_dolby) &&
			(pq_dev->dv_win_info.pr_ctrl.is_dolby ==
			 pq_dev->dv_win_info.pr_ctrl.pre_is_dolby))
			// pr_ctrl width/height are assigned in pqu_alg3pty_dovi_calc
			pq_dev->dv_win_info.pr_ctrl.en = TRUE;
	} else {
		if ((pq_dev->dv_win_info.pyr_buf.valid) &&
			(pqdev->dv_ctrl.pyr_ctrl.turnoff == FALSE) &&
			(pq_dev->dv_win_info.pr_ctrl.is_dolby) &&
			(pq_dev->dv_win_info.pr_ctrl.is_dolby ==
			 pq_dev->dv_win_info.pr_ctrl.pre_is_dolby) &&
			 pq_dev->dv_win_info.pr_ctrl.width > 0 &&
			 pq_dev->dv_win_info.pr_ctrl.height > 0)
			pq_dev->dv_win_info.pr_ctrl.en = TRUE;
	}

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_DEBUG,
		"[%s]%d pr_ctrl=%d\n", __func__, __LINE__, pq_dev->dv_win_info.pr_ctrl.en);

exit:
	return ret;
}

static int dv_set_extra_frame_num(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct mtk_pq_platform_device *pq_platform_dev = NULL;

	if (pq_dev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}
	pq_platform_dev = dev_get_drvdata(pq_dev->dev);

	if ((pq_dev->dv_win_info.pr_ctrl.en) ||
		(pq_dev->dv_win_info.common.extra_frame_num == 1))
		pq_dev->dv_win_info.common.extra_frame_num = 1;

	if (pq_platform_dev->config_info.u8DV_PQ_Version == DV_PQDD_HW_TAG_V5) {
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_DEBUG,
			"for HW ver %d, always set extra to 0\n",
			pq_platform_dev->config_info.u8DV_PQ_Version);
		pq_dev->dv_win_info.common.extra_frame_num = 0;
	}

exit:
	return ret;
}

static int dv_svp_create_pipeline(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	u32 error = 0;
	XC_STI_OPTEE_HANDLER optee_handler;
	TEEC_Session *session = NULL;
	TEEC_Operation op;

	memset(&optee_handler, 0, sizeof(XC_STI_OPTEE_HANDLER));
	memset(&op, 0, sizeof(TEEC_Operation));

	if (pq_dev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	session = pq_dev->display_info.secure_info.pstSession;

	optee_handler.u16Version = XC_STI_OPTEE_HANDLER_VERSION;
	optee_handler.u16Length = sizeof(XC_STI_OPTEE_HANDLER);
	optee_handler.enAID = (EN_XC_STI_AID)pq_dev->display_info.secure_info.aid;

	op.params[PQ_CA_SMC_PARAM_IDX_0].tmpref.buffer = (void *)&optee_handler;
	op.params[PQ_CA_SMC_PARAM_IDX_0].tmpref.size = sizeof(XC_STI_OPTEE_HANDLER);
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_NONE, TEEC_NONE, TEEC_NONE);

	ret = mtk_pq_common_ca_teec_invoke_cmd(pq_dev, session,
			E_XC_OPTEE_CREATE_DV_PIPELINE, &op, &error);
	if (ret) {
		pq_dev->dv_win_info.secure.svp_id_valid = FALSE;
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
			"PQ_TEE_ACT_CREATE_DV_PIPELINE fail, error: %d\n", error);
	} else {
		pq_dev->dv_win_info.secure.svp_id_valid = TRUE;
		pq_dev->dv_win_info.secure.svp_id = optee_handler.u32DVPipelineID;
	}

exit:
	return ret;
}


static int dv_svp_destroy_pipeline(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	u32 error = 0;
	XC_STI_OPTEE_HANDLER optee_handler;
	TEEC_Session *session = NULL;
	TEEC_Operation op;

	memset(&optee_handler, 0, sizeof(XC_STI_OPTEE_HANDLER));
	memset(&op, 0, sizeof(TEEC_Operation));

	if (pq_dev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	session = pq_dev->display_info.secure_info.pstSession;

	optee_handler.u16Version = XC_STI_OPTEE_HANDLER_VERSION;
	optee_handler.u16Length = sizeof(XC_STI_OPTEE_HANDLER);
	optee_handler.enAID = (EN_XC_STI_AID)pq_dev->display_info.secure_info.aid;
	optee_handler.u32DVPipelineID = pq_dev->dv_win_info.secure.svp_id;

	op.params[PQ_CA_SMC_PARAM_IDX_0].tmpref.buffer = (void *)&optee_handler;
	op.params[PQ_CA_SMC_PARAM_IDX_0].tmpref.size = sizeof(XC_STI_OPTEE_HANDLER);
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_NONE, TEEC_NONE, TEEC_NONE);

	ret = mtk_pq_common_ca_teec_invoke_cmd(pq_dev, session,
			E_XC_OPTEE_DESTROY_DV_PIPELINE, &op, &error);
	if (ret)
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
			"PQ_TEE_ACT_DESTROY_DV_PIPELINE fail, error: %d\n", error);

	pq_dev->dv_win_info.secure.svp_id_valid = FALSE;
	pq_dev->dv_win_info.secure.svp_id = 0;

exit:
	return ret;
}

static int dv_svp_try_create_pipeline(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	pqdev = dev_get_drvdata(pq_dev->dev);
	if (pqdev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		return ret;
	}

	/* ott case create pipeline */
	if ((IS_INPUT_B2R(pq_dev->common_info.input_source) ||
		(pqdev->config_info.u8DV_PQ_Version == DV_PQDD_HW_TAG_V5)) &&
		pq_dev->dv_win_info.secure.svp_en &&
		pq_dev->dv_win_info.secure.svp_id_valid == FALSE) {
		ret = dv_svp_create_pipeline(pq_dev);
		if (ret)
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
				"create pipeline fail, return: %d\n", ret);
	}

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_SVP,
		"svp en: %d, valid: %d, id: %u\n",
		pq_dev->dv_win_info.secure.svp_en,
		pq_dev->dv_win_info.secure.svp_id_valid,
		pq_dev->dv_win_info.secure.svp_id);

	return ret;
}

static int dv_get_pr_info(struct mtk_pq_device *pq_dev, struct meta_buffer *meta_buf)
{
	int ret = 0;
	int meta_ret = 0;
	struct meta_srccap_dv_info *meta_dv_hdmi_info = NULL;
	struct vdec_dd_dolby_desc *meta_dv_vdec_info = NULL;
	struct vdec_dd_dolby_desc_parsing *meta_dv_vdec_info_parsing = NULL;
	struct mtk_pq_frame_info *meta_frame_info = NULL;
	enum vdec_dolby_type dolby_type = VDEC_DOLBY_NONE;

	if ((pq_dev == NULL) || (meta_buf == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	/* get dolby info in hdmi case */
	meta_ret = mtk_pq_common_read_metadata_addr_ptr(meta_buf,
			EN_PQ_METATAG_SRCCAP_DV_HDMI_INFO, (void **)&meta_dv_hdmi_info);
	if (meta_ret >= 0) {
		/* enum srccap_dv_descrb_interface */
		if ((meta_dv_hdmi_info->descrb.interface > M_DV_INTERFACE_NONE) &&
			(meta_dv_hdmi_info->descrb.interface < M_DV_INTERFACE_MAX))
			pq_dev->dv_win_info.pr_ctrl.is_dolby = TRUE;

		if (meta_dv_hdmi_info->dma.status == M_DV_DMA_STATUS_ENABLE_FB) {
			pq_dev->dv_win_info.pr_ctrl.width = meta_dv_hdmi_info->dma.width;
			pq_dev->dv_win_info.pr_ctrl.height = meta_dv_hdmi_info->dma.height;
		}

		if (meta_dv_hdmi_info->dma.svp_id_valid) {
			pq_dev->dv_win_info.secure.svp_id_valid = TRUE;
			pq_dev->dv_win_info.secure.svp_id = meta_dv_hdmi_info->dma.svp_id;
		}
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
			"dv hdmi: interface=%d, is_dolby=%d, size=%dx%d, dma=%d, svp=%d,%d\n",
			meta_dv_hdmi_info->descrb.interface, pq_dev->dv_win_info.pr_ctrl.is_dolby,
			pq_dev->dv_win_info.pr_ctrl.width, pq_dev->dv_win_info.pr_ctrl.height,
			meta_dv_hdmi_info->dma.status,
			pq_dev->dv_win_info.secure.svp_id_valid,
			pq_dev->dv_win_info.secure.svp_id);
	}

	/* get dolby info in ott case */
	meta_ret = mtk_pq_common_read_metadata_addr_ptr(meta_buf,
			EN_PQ_METATAG_VDEC_DV_PARSING_TAG, (void **)&meta_dv_vdec_info_parsing);
	if (meta_ret < 0) {
		meta_ret = mtk_pq_common_read_metadata_addr_ptr(meta_buf,
			EN_PQ_METATAG_VDEC_DV_DESCRB_INFO, (void **)&meta_dv_vdec_info);
		if (meta_ret >= 0)
			dolby_type = meta_dv_vdec_info->dolby_type;

	} else
		dolby_type = meta_dv_vdec_info_parsing->dolby_type;

	meta_ret |= mtk_pq_common_read_metadata_addr_ptr(meta_buf,
			EN_PQ_METATAG_SH_FRM_INFO, (void **)&meta_frame_info);
	if (meta_ret >= 0) {
		/* enum vdec_dolby_type */
		if (dolby_type != VDEC_DOLBY_NONE)
			pq_dev->dv_win_info.pr_ctrl.is_dolby = TRUE;

		if (meta_frame_info->submodifier.vsd_mode != 0) {
			pq_dev->dv_win_info.pr_ctrl.width = meta_frame_info->stSubFrame.u32Width;
			pq_dev->dv_win_info.pr_ctrl.height = meta_frame_info->stSubFrame.u32Height;
		} else if (meta_frame_info->modifier.compress == 0) {
			pq_dev->dv_win_info.pr_ctrl.width = meta_frame_info->stFrames[0].u32Width;
			pq_dev->dv_win_info.pr_ctrl.height = meta_frame_info->stFrames[0].u32Height;
		} else {
			pq_dev->dv_win_info.pr_ctrl.width = 0;
			pq_dev->dv_win_info.pr_ctrl.height = 0;
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
				"vsd mode of sub1 = none and compress of main = 1\n");
		}
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_INFO,
			"dv ott: dolby_type=%d, is_dolby=%d, size=%dx%d, vsd_mode=%d, comp=%d\n",
			dolby_type, pq_dev->dv_win_info.pr_ctrl.is_dolby,
			pq_dev->dv_win_info.pr_ctrl.width, pq_dev->dv_win_info.pr_ctrl.height,
			meta_frame_info->submodifier.vsd_mode,
			meta_frame_info->modifier.compress);
	}

exit:

	return ret;
}

//-----------------------------------------------------------------------------
// Global Functions
//-----------------------------------------------------------------------------
int mtk_pq_dv_ctrl_init(
	struct mtk_pq_dv_ctrl_init_in *in,
	struct mtk_pq_dv_ctrl_init_out *out)
{
	int ret = 0;
	bool hw_cap = FALSE;

	if ((in == NULL) || (out == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (in->pqdev->config_info.u8DV_PQ_Version == DV_PQDD_HW_TAG_V4)
		hw_cap = FALSE;
	else
		hw_cap = TRUE;

	/* set dolby global control */
	in->pqdev->dv_ctrl.pyr_ctrl.mmap_size = in->mmap_size;
	in->pqdev->dv_ctrl.pyr_ctrl.mmap_addr = in->mmap_addr;
	in->pqdev->dv_ctrl.pyr_ctrl.hw_cap = hw_cap;
	in->pqdev->dv_ctrl.pyr_ctrl.sw_cap = hw_cap;
	in->pqdev->dv_ctrl.pyr_ctrl.available = hw_cap;
	in->pqdev->dv_ctrl.pyr_ctrl.turnoff = FALSE;

	memset(&in->pqdev->dv_ctrl.debug, 0, sizeof(struct mtk_pq_dv_debug));

	g_mtk_pq_dv_debug_level = MTK_PQ_DV_DBG_LEVEL_ERR;

exit:
	return ret;
}

int mtk_pq_dv_win_init(
	struct mtk_pq_dv_win_init_in *in,
	struct mtk_pq_dv_win_init_out *out)
{
	int ret = 0;

	if ((in == NULL) || (out == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	/* set dolby window info */
	in->dev->dv_win_info.pr_ctrl.is_dolby = FALSE;
	in->dev->dv_win_info.pr_ctrl.pre_is_dolby = FALSE;
	in->dev->dv_win_info.pr_ctrl.width = 0;
	in->dev->dv_win_info.pr_ctrl.height = 0;
	in->dev->dv_win_info.pyr_buf.valid = FALSE;
	in->dev->dv_win_info.secure.svp_en = FALSE;
	in->dev->dv_win_info.secure.svp_id_valid = FALSE;
	in->dev->dv_win_info.secure.svp_buf_authed = FALSE;

exit:
	return ret;
}

int mtk_pq_dv_streamon(
	struct mtk_pq_dv_streamon_in *in,
	struct mtk_pq_dv_streamon_out *out)
{
	int ret = 0;
	int i = 0;
	struct mtk_pq_dv_pyr_buf *pyr_buf = NULL;

	/* check dolby support */
	if (mtk_pq_dv_get_dv_support() != TRUE)
		return 0;

	if ((in == NULL) || (out == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	pyr_buf = &(in->dev->dv_win_info.pyr_buf);

	/* set dolby window info */
	in->dev->dv_win_info.common.extra_frame_num = 0;
	in->dev->dv_win_info.pr_ctrl.is_dolby = FALSE;
	in->dev->dv_win_info.pr_ctrl.pre_is_dolby = FALSE;
	in->dev->dv_win_info.pr_ctrl.width = 0;
	in->dev->dv_win_info.pr_ctrl.height = 0;
	pyr_buf->valid = FALSE;

	in->dev->dv_win_info.secure.svp_en = in->svp_en;
	in->dev->dv_win_info.secure.svp_id_valid = FALSE;
	in->dev->dv_win_info.secure.svp_id = 0;


	/* calculate size, unit: n bit align */
	pyr_buf->frame_num = DV_PYR_FRAME_NUM;
	pyr_buf->rw_diff = DV_PYR_RW_DIFF;
	pyr_buf->frame_pitch[DV_PYR_0_IDX] =
		DV_PYR_0_HEIGHT
		* DV_DMA_ALIGN((DV_PYR_0_WIDTH * DV_PYR_CHANNEL_SIZE), DV_BIT_PER_WORD)
		/ DV_BIT_PER_WORD;
	pyr_buf->pyr_width[DV_PYR_0_IDX] = DV_PYR_0_WIDTH;
	pyr_buf->pyr_height[DV_PYR_0_IDX] = DV_PYR_0_HEIGHT;
	pyr_buf->frame_pitch[DV_PYR_1_IDX] =
		DV_PYR_1_HEIGHT
		* DV_DMA_ALIGN((DV_PYR_1_WIDTH * DV_PYR_CHANNEL_SIZE), DV_BIT_PER_WORD)
		/ DV_BIT_PER_WORD;
	pyr_buf->pyr_width[DV_PYR_1_IDX] = DV_PYR_1_WIDTH;
	pyr_buf->pyr_height[DV_PYR_1_IDX] = DV_PYR_1_HEIGHT;
	pyr_buf->frame_pitch[DV_PYR_2_IDX] =
		DV_PYR_2_HEIGHT
		* DV_DMA_ALIGN((DV_PYR_2_WIDTH * DV_PYR_CHANNEL_SIZE), DV_BIT_PER_WORD)
		/ DV_BIT_PER_WORD;
	pyr_buf->pyr_width[DV_PYR_2_IDX] = DV_PYR_2_WIDTH;
	pyr_buf->pyr_height[DV_PYR_2_IDX] = DV_PYR_2_HEIGHT;
	pyr_buf->frame_pitch[DV_PYR_3_IDX] =
		DV_PYR_3_HEIGHT
		* DV_DMA_ALIGN((DV_PYR_3_WIDTH * DV_PYR_CHANNEL_SIZE), DV_BIT_PER_WORD)
		/ DV_BIT_PER_WORD;
	pyr_buf->pyr_width[DV_PYR_3_IDX] = DV_PYR_3_WIDTH;
	pyr_buf->pyr_height[DV_PYR_3_IDX] = DV_PYR_3_HEIGHT;
	pyr_buf->frame_pitch[DV_PYR_4_IDX] =
		DV_PYR_4_HEIGHT
		* DV_DMA_ALIGN((DV_PYR_4_WIDTH * DV_PYR_CHANNEL_SIZE), DV_BIT_PER_WORD)
		/ DV_BIT_PER_WORD;
	pyr_buf->pyr_width[DV_PYR_4_IDX] = DV_PYR_4_WIDTH;
	pyr_buf->pyr_height[DV_PYR_4_IDX] = DV_PYR_4_HEIGHT;
	pyr_buf->frame_pitch[DV_PYR_5_IDX] =
		DV_PYR_5_HEIGHT
		* DV_DMA_ALIGN((DV_PYR_5_WIDTH * DV_PYR_CHANNEL_SIZE), DV_BIT_PER_WORD)
		/ DV_BIT_PER_WORD;
	pyr_buf->pyr_width[DV_PYR_5_IDX] = DV_PYR_5_WIDTH;
	pyr_buf->pyr_height[DV_PYR_5_IDX] = DV_PYR_5_HEIGHT;
	pyr_buf->frame_pitch[DV_PYR_6_IDX] =
		DV_PYR_6_HEIGHT
		* DV_DMA_ALIGN((DV_PYR_6_WIDTH * DV_PYR_CHANNEL_SIZE), DV_BIT_PER_WORD)
		/ DV_BIT_PER_WORD;
	pyr_buf->pyr_width[DV_PYR_6_IDX] = DV_PYR_6_WIDTH;
	pyr_buf->pyr_height[DV_PYR_6_IDX] = DV_PYR_6_HEIGHT;
	pyr_buf->size = 0;
	for (i = 0; i < MTK_PQ_DV_PYR_NUM; i++) {
		/* unit: byte */
		pyr_buf->pyr_size[i] =
			(pyr_buf->frame_pitch[i] * DV_BIT_PER_WORD / DV_BIT_PER_BYTE) *
			pyr_buf->frame_num;
		pyr_buf->pyr_size[i] =
			DV_DMA_ALIGN(pyr_buf->pyr_size[i], (DV_BIT_PER_WORD << 1));	// 2-word
		pyr_buf->size += pyr_buf->pyr_size[i];
	}

exit:
	return ret;
}

int mtk_pq_dv_streamoff(
	struct mtk_pq_dv_streamoff_in *in,
	struct mtk_pq_dv_streamoff_out *out)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	pqdev = dev_get_drvdata(in->dev->dev);
	if (pqdev == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	/* check dolby support */
	if (mtk_pq_dv_get_dv_support() != TRUE)
		return 0;

	if ((in == NULL) || (out == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	ret = dv_free_pyr_buf(in->dev);
	if (ret < 0) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* ott case release pipeline */
	if ((IS_INPUT_B2R(in->dev->common_info.input_source) ||
		(pqdev->config_info.u8DV_PQ_Version == DV_PQDD_HW_TAG_V5)) &&
		in->dev->dv_win_info.secure.svp_id_valid) {
		ret = dv_svp_destroy_pipeline(in->dev);
		if (ret)
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_SVP,
				"destroy pipeline fail, return: %d\n", ret);
		/* auto pipeline destroy return fail that cause stream off abnormally */
		ret = 0;
	}

exit:
	return ret;
}

int mtk_pq_dv_set_ambient(void *ctrl, struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	v4l2_PQ_dv_config_info_t *dv_config_info = NULL;
	struct meta_pq_dv_ambient *ctrl_meta = NULL;

	/* check dolby support */
	if (mtk_pq_dv_get_dv_support() != TRUE)
		return 0;

	if ((pq_dev == NULL) || (ctrl == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	memcpy(&(pq_dev->dv_win_info.ambient), ctrl, sizeof(struct meta_pq_dv_ambient));

	// also copy to share bin struct //
	ctrl_meta = &(pq_dev->dv_win_info.ambient);
	dv_config_info = (v4l2_PQ_dv_config_info_t *)pqdev->dv_config.va;
	if (dv_config_info != NULL && ctrl_meta != NULL) {
		dv_config_info->in_frame_info.ambientDebugEn = FALSE;
		dv_config_info->in_frame_info.u32AmbientMode =
			(ctrl_meta->bIsModeValid)?(ctrl_meta->u32Mode):0;
		dv_config_info->in_frame_info.s64FrontLux =
			(ctrl_meta->bIsFrontLuxValid)?(ctrl_meta->s64FrontLux):0;
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_DEBUG, "set ambient mode:%d front:%lld\n",
			dv_config_info->in_frame_info.u32AmbientMode,
			dv_config_info->in_frame_info.s64FrontLux);
	}

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_DEBUG,
		"front sensor lux = %lld, mode = %d\n",
		pq_dev->dv_win_info.ambient.s64FrontLux, pq_dev->dv_win_info.ambient.u32Mode);

exit:
	return ret;
}

int mtk_pq_dv_update_pr_cap(
	v4l2_PQ_dv_config_info_t *dv_config_info,
	struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	bool sw_cap = FALSE;

	if (pqdev == NULL || dv_config_info == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	sw_cap = pqdev->dv_ctrl.pyr_ctrl.sw_cap;

	if (dv_config_info) {
		if (dv_config_info->cap_info.capability & V4L2_PQ_DV_CAPABILITY_DOLBY_PR_SUPPORT)
			sw_cap = TRUE;
		else
			sw_cap = FALSE;
	}

	pqdev->dv_ctrl.pyr_ctrl.sw_cap = sw_cap;
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR, "update pr sw_cap=%d\n", sw_cap);

exit:
	return ret;
}

int mtk_pq_dv_get_pr_cap(
	struct mtk_pq_platform_device *pqdev,
	bool *pr_support)
{
	bool supported = FALSE;

	if ((!pqdev) || (!pr_support))
		return -EFAULT;

	if (pqdev->dv_ctrl.pyr_ctrl.hw_cap && pqdev->dv_ctrl.pyr_ctrl.sw_cap)
		supported = TRUE;
	else
		supported = FALSE;

	*pr_support = supported;

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
		"get pr support=%d (version: %d)\n",
		supported,
		pqdev->config_info.u8DV_PQ_Version);

	return 0;
}

int mtk_pq_dv_set_pr_ctrl(
	void *ctrl,
	struct mtk_pq_platform_device *pqdev)
{
	uint8_t pr_ctrl = 0;
	bool turnoff = FALSE;

	if ((!ctrl) || (!pqdev))
		return -EFAULT;

	pr_ctrl = *(uint8_t *)ctrl;

	if (pr_ctrl == 0)
		turnoff = TRUE;
	else
		turnoff = FALSE;

	pqdev->dv_ctrl.pyr_ctrl.turnoff = turnoff;

	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
		"set pr ctrl = %d, turn-off = %d\n",
		pr_ctrl, turnoff);

	return 0;
}

int mtk_pq_dv_qbuf(
	struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_buf)
{
	int ret = 0, fd = 0, i = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_dv_debug *dv_debug = NULL;
	struct mtk_pq_dv_pyr_buf *dv_pyr_buf = NULL;
	struct meta_pq_dv_info meta_dv_pq;
	static struct m_pq_dv_debug meta_dv_debug;
	struct meta_buffer meta_buf;

	/* check dolby support */
	if (mtk_pq_dv_get_dv_support() != TRUE)
		return 0;

	if ((pq_dev == NULL) || (pq_buf == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	memset(&meta_dv_pq, 0, sizeof(struct meta_pq_dv_info));
	memset(&meta_dv_debug, 0, sizeof(struct m_pq_dv_debug));
	memset(&meta_buf, 0, sizeof(struct meta_buffer));

	pqdev = dev_get_drvdata(pq_dev->dev);
	dv_debug = &pqdev->dv_ctrl.debug;

	fd = pq_buf->vb.planes[pq_buf->vb.vb2_buf.num_planes - 1].m.fd;
	dv_pyr_buf = &(pq_dev->dv_win_info.pyr_buf);

	pq_dev->dv_win_info.pr_ctrl.en = FALSE;
	pq_dev->dv_win_info.pr_ctrl.pre_is_dolby =
		pq_dev->dv_win_info.pr_ctrl.is_dolby;
	pq_dev->dv_win_info.pr_ctrl.is_dolby = FALSE;
	pq_dev->dv_win_info.pr_ctrl.width = 0;
	pq_dev->dv_win_info.pr_ctrl.height = 0;

	meta_buf.paddr = pq_buf->meta_buf.va;
	meta_buf.size = pq_buf->meta_buf.size;

	/* ott case create pipeline */
	dv_svp_try_create_pipeline(pq_dev);

	/* get pr info */
	ret = dv_get_pr_info(pq_dev, &meta_buf);
	if (ret) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* try to allocate pyramid buffer */
	ret = dv_try_allocate_pyr_buf(pq_dev);
	if (ret < 0) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* determine pr status */
	ret = dv_set_pr(pq_dev);
	if (ret < 0) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set extra frame number */
	ret = dv_set_extra_frame_num(pq_dev);
	if (ret < 0) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* prepare dolby pq info */
	meta_dv_pq.pyr.valid = dv_pyr_buf->valid;
	meta_dv_pq.pyr.frame_num = dv_pyr_buf->frame_num;
	meta_dv_pq.pyr.rw_diff = dv_pyr_buf->rw_diff;
	for (i = 0; i < MTK_PQ_DV_PYR_NUM; i++) {
		meta_dv_pq.pyr.frame_pitch[i] = dv_pyr_buf->frame_pitch[i];
		meta_dv_pq.pyr.addr[i]        = dv_pyr_buf->pyr_addr[i];
		meta_dv_pq.pyr.width[i]       = dv_pyr_buf->pyr_width[i];
		meta_dv_pq.pyr.height[i]      = dv_pyr_buf->pyr_height[i];
	}
	meta_dv_pq.pr_ctrl.en = pq_dev->dv_win_info.pr_ctrl.en;
	meta_dv_pq.pr_ctrl.fe_in_width = pq_dev->dv_win_info.pr_ctrl.width;
	meta_dv_pq.pr_ctrl.fe_in_height = pq_dev->dv_win_info.pr_ctrl.height;

	/* prepare ambient info of dolby Adv. SDK light sense */
	memcpy(&(meta_dv_pq.ambient), &(pq_dev->dv_win_info.ambient),
		sizeof(struct meta_pq_dv_ambient));
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_DEBUG,
		"front sensor lux = %lld, mode = %d\n",
		meta_dv_pq.ambient.s64FrontLux, meta_dv_pq.ambient.u32Mode);

	/* dolby debug process */
	ret = dv_debug_qbuf(pq_dev, &meta_dv_pq, &meta_dv_debug);
	if (ret < 0) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* write dolby debug info into metadata */
	ret = mtk_pq_common_write_metadata_addr(&meta_buf,
			EN_PQ_METATAG_DV_DEBUG, &meta_dv_debug);
	if (ret < 0) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* write dolby pq info into metadata */
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_PR,
		"%s%d, %s%d, %s%u, %s%u, %s%d\n",
		"pyramid valid: ", pq_dev->dv_win_info.pyr_buf.valid,
		"pr enable: ", pq_dev->dv_win_info.pr_ctrl.en,
		"width: ", pq_dev->dv_win_info.pr_ctrl.width,
		"height: ", pq_dev->dv_win_info.pr_ctrl.height,
		"extra frame number: ", pq_dev->dv_win_info.common.extra_frame_num);

	ret = mtk_pq_common_write_metadata_addr(&meta_buf, EN_PQ_METATAG_DV_INFO, &meta_dv_pq);
	if (ret < 0) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

exit:
	return ret;
}

int mtk_pq_dv_show(struct device *dev, const char *buf)
{
	int ssize = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_dv_debug *dv_debug = NULL;

	if ((dev == NULL) || (buf == NULL))
		return 0;

	pqdev = dev_get_drvdata(dev);
	dv_debug = &pqdev->dv_ctrl.debug;

	dv_debug_print_help(dv_debug);

	return ssize;
}

ssize_t dv_debug_show_dovi_pqu_status_report(
	char *buf,
	ssize_t count,
	v4l2_PQ_dv_config_info_t *dv_config_info,
	int hw_ver)
{
	int ret = 0;
	struct m_pq_dv_pqu_status_report *dovi_pqu_report = NULL;
	int i = 0;

	if ((buf == NULL) || (dv_config_info == NULL))
		return count;

	dovi_pqu_report = (struct m_pq_dv_pqu_status_report *)dv_config_info->dovi_pqu_report;

	if (!dovi_pqu_report->report_enable)
		return count;	// report not enabled

	/* start dovi pqu report */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "============ PQU ============\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- mem -------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "m_config: %lu/%d\n",
		sizeof(v4l2_PQ_dv_config_info_t), DV_CONFIG_BIN_OFFSET);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "m_sysfs: %lu/%d\n",
		sizeof(struct m_pq_dv_sysfs_control), V4L2_DV_MAX_DBG_SYSFS_CTRL_SIZE);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "m_pqu: %lu/%d\n",
		sizeof(struct m_pq_dv_pqu_status_report), V4L2_DV_MAX_PQU_REPORT_SIZE);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "m_core: %lu/%d\n",
		sizeof(struct m_pq_dv_core_status_report), V4L2_DV_MAX_CORE_REPORT_SIZE);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "m_mipq: %lu/%d\n",
		sizeof(struct v4l2_PQ_dv_mi_pq_report), V4L2_DV_MAX_MIPQ_REPORT_SIZE);

	/* system */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- sys -------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "p_cal: %d\n", dovi_pqu_report->count_calc);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "p_que: %d\n", dovi_pqu_report->count_queue);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "p_irq: %d\n", dovi_pqu_report->count_irq);

	/* control info */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- cal -------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "rw_diff:%d,%d,%d,%d (%d,%d) %d\n",
		dovi_pqu_report->ip_diff,
		dovi_pqu_report->opf_diff, dovi_pqu_report->opb_diff, dovi_pqu_report->op2_diff,
		dovi_pqu_report->scmi_enable, dovi_pqu_report->ucm_enable,
		dovi_pqu_report->scmi_format);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "fmt_mem:%d,%d (en:%d,%d,%d)\n",
		dovi_pqu_report->scmi_fmt, dovi_pqu_report->ucm_fmt,
		dovi_pqu_report->aisr_en, dovi_pqu_report->vip1_en, dovi_pqu_report->spf_en);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "scene:  %d\n", dovi_pqu_report->scene);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "sc_pc:  %d\n", dovi_pqu_report->pc);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "dv_osd: %s\n", dovi_pqu_report->pqparam_str);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "dv_type:%d\n", dovi_pqu_report->hdr_type);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "dv_mode:%d\n", dovi_pqu_report->view_id);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "dv_ip:  %d\n", dovi_pqu_report->is_ip);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "dv_2p:  %d\n", dovi_pqu_report->mode_2p);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "dv_422: %d\n", dovi_pqu_report->op_422_en);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "pr_en:  %d,%d\n",
		dovi_pqu_report->pr_en_curr, dovi_pqu_report->pr_en_next);

	/* size info */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- data ------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "s_fps: %d\n", dovi_pqu_report->frame_rate);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "s_sourc: %dx%d\n",
		dovi_pqu_report->src_width, dovi_pqu_report->src_height);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "s_input: %dx%d\n",
		dovi_pqu_report->in_width, dovi_pqu_report->in_height);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "s_align: %dx%d\n",
		dovi_pqu_report->in_w_align, dovi_pqu_report->in_h_align);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- gd --------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "gd_en:   %d\n", dovi_pqu_report->gd_valid);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "gd_val:  %d\n", dovi_pqu_report->gd_value);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "gd_delay:%d\n", dovi_pqu_report->gd_delay);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "gd_gray: %d\n", dovi_pqu_report->gd_gray_out);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- L11 --------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "L11:     %d (%02X-%02X-%02X-%02X-%02X)\n",
		dovi_pqu_report->l11_valid,
		dovi_pqu_report->l11_ct, dovi_pqu_report->l11_wp_valid,
		dovi_pqu_report->l11_wp_value,
		dovi_pqu_report->l11_b2, dovi_pqu_report->l11_b3);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- LS --------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ls_front:%d (0x%X)\n",
		dovi_pqu_report->ls_front_lux,
		dovi_pqu_report->ls_front_scale);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ls_rear: %d (0x%X)\n",
		dovi_pqu_report->ls_rear_lum,
		dovi_pqu_report->ls_rear_scale);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- Ambient ---\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "am_mode: %d\n", dovi_pqu_report->am_mode);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "am_lux:  %lld, %lld\n",
		dovi_pqu_report->am_front_lux, dovi_pqu_report->am_rear_lum);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "am_Wxy:  %d, %d\n",
		dovi_pqu_report->am_wx, dovi_pqu_report->am_wy);

	/* reg count */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- set -------\n");

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "reg_be: %d,%d,%d,%d,%d\n",
		dovi_pqu_report->reg_X16_prio, dovi_pqu_report->reg_X16,
		dovi_pqu_report->reg_PquCtrl,
		dovi_pqu_report->reg_X32_prio, dovi_pqu_report->reg_X32);
	if (hw_ver != DV_PQDD_HW_TAG_V4) {
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "reg_fe: %d,%d,%d,%d\n",
			dovi_pqu_report->reg_fe_X16_prio,
			dovi_pqu_report->reg_fe_X16,
			dovi_pqu_report->reg_fe_X32_prio,
			dovi_pqu_report->reg_fe_X32);
	}
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "adl:    %d,%d,%d\n",
		dovi_pqu_report->adl_c2,
		dovi_pqu_report->adl_c1, dovi_pqu_report->adl_c2t5);

	/* dv ml */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- dv_ml -----\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ml_o_idx:");
	for (i = 1; i <= PQ_DV_DBG_ML_BUF_DEPTH; i++) {
		int ptr = ((dovi_pqu_report->ml_ptr_op + PQ_DV_DBG_ML_BUF_DEPTH - i)
				% PQ_DV_DBG_ML_BUF_DEPTH);

		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, " %d",
			dovi_pqu_report->ml_idx_op[ptr]);
	}
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "\nml_o_dep: %d\n",
		dovi_pqu_report->ml_depth_op);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ml_i_idx:");
	for (i = 1; i <= PQ_DV_DBG_ML_BUF_DEPTH; i++) {
		int ptr = ((dovi_pqu_report->ml_ptr_ip + PQ_DV_DBG_ML_BUF_DEPTH - i)
				% PQ_DV_DBG_ML_BUF_DEPTH);

		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, " %d",
			dovi_pqu_report->ml_idx_ip[ptr]);
	}
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "\nml_i_dep: %d\n",
		dovi_pqu_report->ml_depth_ip);

	return count;
}

ssize_t dv_debug_show_dv_core_status_report(
	char *buf,
	ssize_t count,
	v4l2_PQ_dv_config_info_t *dv_config_info,
	int hw_ver)
{
	int ret = 0;
	struct m_pq_dv_core_status_report *dv_core_report = NULL;

	if ((buf == NULL) || (dv_config_info == NULL))
		return count;

	dv_core_report = (struct m_pq_dv_core_status_report *)dv_config_info->dv_core_report;

	if (!dv_core_report->report_enable)
		return count;	// report not enabled

	/* start dv core report */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "============ CORE ===========\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "bin_size: %d\n", dv_core_report->lut_bin_size);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "mode_num: %d\n", dv_core_report->view_num);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "mode_id:  %d\n", dv_core_report->view_id);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- ui  -------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_pr:   %d\n", dv_core_report->ui_pr);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_dd:   %d\n", dv_core_report->ui_dd);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_ls:   %d\n", dv_core_report->ui_ls);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_apo:  %d\n", dv_core_report->ui_apo);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_gd:   %d\n", dv_core_report->ui_gd);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_l1l4: %d\n", dv_core_report->ui_l1l4);

	if (hw_ver != DV_PQDD_HW_TAG_V4) {
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- pr  -------\n");
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "pr_cap:  %d\n",
			dv_core_report->pr_cap);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "pr_en:   %d,%d\n",
			dv_core_report->pr_en_curr, dv_core_report->pr_en_next);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "pr_diff: %d\n",
			dv_core_report->pr_off_diff);
	}

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- reg -------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "reg_Path: 0x%X\n", dv_core_report->dv_path);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "reg_Tran: 0x%X\n", dv_core_report->dv_tran);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "reg_Ctrl: 0x%X\n", dv_core_report->dv_ctrl);

	/* size info */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "------- size ------\n");
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "BE_source:  %dx%d\n",
		dv_core_report->be_src_width, dv_core_report->be_src_height);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "BE_hdr_in:  %dx%d\n",
		dv_core_report->be_in_width, dv_core_report->be_in_height);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "BE_crop_1:  %dx%d\n",
		dv_core_report->be_pre_crop_width, dv_core_report->be_pre_crop_height);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "BE_crop_2:  %dx%d\n",
	dv_core_report->be_post_crop_width, dv_core_report->be_post_crop_height);

	if (dv_core_report->fe_in_width > 0 && dv_core_report->fe_in_height > 0) {
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "FE_hdr_in: %dx%d\n",
			dv_core_report->fe_in_width, dv_core_report->fe_in_height);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "FE_crop_1: %dx%d\n",
			dv_core_report->fe_crop_width, dv_core_report->fe_crop_height);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "FE_scaler: %dx%d\n",
			dv_core_report->fe_sd_width, dv_core_report->fe_sd_height);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "FE_crop_2: %dx%d\n",
			dv_core_report->fe_crop2_width, dv_core_report->fe_crop2_height);
	}

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_be_in:  %dx%d\n",
		dv_core_report->be_in_h, dv_core_report->be_in_v);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_c2_in:  %dx%d\n",
		dv_core_report->c2_in_h, dv_core_report->c2_in_v);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_c2_out: %dx%d\n",
		dv_core_report->c2_out_h, dv_core_report->c2_out_v);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_be_out: %dx%d\n",
		dv_core_report->be_out_h, dv_core_report->be_out_v);

	if (dv_core_report->fe_in_width > 0 && dv_core_report->fe_in_height > 0) {
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_c1_in:  %dx%d\n",
			dv_core_report->c2tv5_in_h, dv_core_report->c2tv5_in_v);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_c1_out: %dx%d\n",
			dv_core_report->c2tv5_out_h, dv_core_report->c2tv5_out_v);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_c1b_in: %dx%d\n",
			dv_core_report->c1b_in_h, dv_core_report->c1b_in_v);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_pyr_w:  %dx%d\n",
			dv_core_report->pyr_w_in_h, dv_core_report->pyr_w_in_v);
	}

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_aul_c1: 0x%X,0x%X\n",
		dv_core_report->aul_c1_h, dv_core_report->aul_c1_v);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "RPT_adl_c2: 0x%X,0x%X,0x%X,0x%X\n",
		dv_core_report->adl_c2_csc_h, dv_core_report->adl_c2_csc_v,
		dv_core_report->adl_c2_cvm_h, dv_core_report->adl_c2_cvm_v);
	if (hw_ver <= DV_PQDD_HW_TAG_V3) {
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret,
			"RPT_adl_c1: 0x%X,0x%X\n",
			dv_core_report->adl_c1_h, dv_core_report->adl_c1_v);
	}
	if (hw_ver == DV_PQDD_HW_TAG_V5) {
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret,
			"RPT_adl_tv5:0x%X,0x%X,0x%X,0x%X\n",
			dv_core_report->adl_c2tv5_csc_h, dv_core_report->adl_c2tv5_csc_v,
			dv_core_report->adl_c2tv5_cvm_h, dv_core_report->adl_c2tv5_cvm_v);
	}

	return count;
}

ssize_t dv_debug_show_mipq_status_report(
	char *buf,
	ssize_t count,
	v4l2_PQ_dv_config_info_t *dv_config_info,
	int hw_ver)
{
	int ret = 0;
	struct v4l2_PQ_dv_mi_pq_report *dv_mipq_report = NULL;

	if ((buf == NULL) || (dv_config_info == NULL))
		return count;

	dv_mipq_report = (struct v4l2_PQ_dv_mi_pq_report *)dv_config_info->dv_mipq_report;

	if (!dv_mipq_report->report_enable)
		return count;	// report not enabled

	/* start dv mipq report */
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "============ MIPQ ===========\n");

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cfg_dolby: %s\n",
		dv_mipq_report->path_dolby_cfg);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cfg_user:  %s\n",
		dv_mipq_report->path_user_cfg);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cfg_cust:  %s\n",
		dv_mipq_report->path_cust_cfg);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cap_dolby: %d\n", dv_mipq_report->cap_dv);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cap_iq:    %d\n", dv_mipq_report->cap_iq);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cap_apo:   %d\n", dv_mipq_report->cap_apo);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cap_ls:    %d\n", dv_mipq_report->cap_ls);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cap_euc:   %d\n", dv_mipq_report->cap_euc);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "cap_pd:    %d\n", dv_mipq_report->cap_pd);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "idx_dark:  %d\n", dv_mipq_report->idx_dark);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "idx_iq:    %d\n", dv_mipq_report->idx_iq);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "idx_game:  %d\n", dv_mipq_report->idx_game);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "idx_fmmdv: %d\n", dv_mipq_report->idx_fmmdv);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "idx_fmmiq: %d\n", dv_mipq_report->idx_fmmiq);

	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_apo_on: %d\n", dv_mipq_report->ui_apo);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_ls_on:  %d\n", dv_mipq_report->ui_ls);
	DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "ui_rule:   %s\n", dv_mipq_report->ui_rule_str);

	return count;
}

ssize_t mtk_pq_dv_attr_show(
	struct device *dev,
	char *buf)
{
	int ret = 0;
	ssize_t count = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	v4l2_PQ_dv_config_info_t *dv_config_info = NULL;
	struct mtk_pq_dv_ctrl *dv_ctrl = NULL;
	bool pr_support = FALSE;

	if ((dev == NULL) || (buf == NULL))
		return 0;

	buf[0] = 0;

	pqdev = dev_get_drvdata(dev);

	do {
		dv_ctrl = &(pqdev->dv_ctrl);
		dv_config_info = (v4l2_PQ_dv_config_info_t *)pqdev->dv_config.va;
		if (dv_config_info == NULL)
			break;

		/* check DV support */
		if (!(dv_config_info->cap_info.capability &
			V4L2_PQ_DV_CAPABILITY_DOLBY_VISION_SUPPORT)) {
			DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "dolby_vision=0\n");
			break;
		}
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "dolby_vision=1\n");

		/* check IQ support */
		if (dv_config_info->cap_info.capability & V4L2_PQ_DV_CAPABILITY_DOLBY_IQ_SUPPORT)
			DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "IQ=1\n");

		/* check PR support */
		mtk_pq_dv_get_pr_cap(pqdev, &pr_support);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "precision_detail=%d\n", pr_support);

		if (dv_ctrl) {
			DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret,
				"dv debug mode: 0x%X (0x%X 0x%X 0x%X)\n",
				dv_ctrl->debug.debug_mode.mode,
				dv_ctrl->debug.debug_mode.arg1,
				dv_ctrl->debug.debug_mode.arg2,
				dv_ctrl->debug.debug_mode.arg3);
		}

		count = dv_debug_show_dovi_pqu_status_report(buf, count,
			dv_config_info, pqdev->config_info.u8DV_PQ_Version);
		count = dv_debug_show_dv_core_status_report(buf, count,
			dv_config_info, pqdev->config_info.u8DV_PQ_Version);
		count = dv_debug_show_mipq_status_report(buf, count,
			dv_config_info, pqdev->config_info.u8DV_PQ_Version);
		DV_PQDD_ADD_ATTR_SHOW_LOG(buf, count, ret, "-- count: %lu/%lu --\n",
			count, PAGE_SIZE);
	} while (0);

	return count;
}

struct m_pq_dv_sysfs_control *dv_get_sysfs_ctrl(v4l2_PQ_dv_config_info_t *dv_config_info)
{
	struct m_pq_dv_sysfs_control *dv_sysfs_ctrl = NULL;

	if (dv_config_info != NULL) {
		if (sizeof(struct m_pq_dv_sysfs_control) <= V4L2_DV_MAX_DBG_SYSFS_CTRL_SIZE) {
			// available
			dv_sysfs_ctrl = (struct m_pq_dv_sysfs_control *)dv_config_info->sysfs_ctrl;
		} else {
			MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
				"size not enough %ld > %d\n",
				sizeof(struct m_pq_dv_sysfs_control),
				V4L2_DV_MAX_DBG_SYSFS_CTRL_SIZE);
			dv_sysfs_ctrl = NULL;
		}
	} else {
		MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
			"dv_config_info NULL\n");
		dv_sysfs_ctrl = NULL;
	}

	return dv_sysfs_ctrl;
}

void dv_store_sysfs_ctrl(
	struct m_pq_dv_sysfs_control *dv_sysfs_ctrl,
	struct mtk_pq_dv_debug *dv_debug)
{
	if (!dv_sysfs_ctrl || !dv_debug)
		return;

	memcpy(&(dv_sysfs_ctrl->debug_level),
		&(dv_debug->debug_level),
		sizeof(struct m_pq_dv_debug_level));

	memcpy(&(dv_sysfs_ctrl->debug_mode),
		&(dv_debug->debug_mode),
		sizeof(struct m_pq_dv_debug_set_debug_mode));

	memcpy(&(dv_sysfs_ctrl->force_ctrl.view_mode),
		&(dv_debug->force_ctrl.view_mode),
		sizeof(struct m_pq_dv_debug_force_view_mode));

	memcpy(&(dv_sysfs_ctrl->force_ctrl.disable_dolby),
		&(dv_debug->force_ctrl.disable_dolby),
		sizeof(struct m_pq_dv_debug_force_disable_dolby));

	memcpy(&(dv_sysfs_ctrl->force_ctrl.dark_detail),
		&(dv_debug->force_ctrl.dark_detail),
		sizeof(struct m_pq_dv_debug_force_dark_detail));

	memcpy(&(dv_sysfs_ctrl->force_ctrl.iq_apo),
		&(dv_debug->force_ctrl.iq_apo),
		sizeof(struct m_pq_dv_debug_force_iq_apo));

	memcpy(&(dv_sysfs_ctrl->force_ctrl.white_point),
		&(dv_debug->force_ctrl.white_point),
		sizeof(struct m_pq_dv_debug_force_white_point));

	memcpy(&(dv_sysfs_ctrl->force_ctrl.light_sense),
		&(dv_debug->force_ctrl.light_sense),
		sizeof(struct m_pq_dv_debug_force_light_sense));

	memcpy(&(dv_sysfs_ctrl->force_ctrl.l1l4_gen),
		&(dv_debug->force_ctrl.l1l4_gen),
		sizeof(struct m_pq_dv_debug_force_l1l4_gen));
}

int mtk_pq_dv_store(struct device *dev, const char *buf)
{
	int ret = 0;
	char cmd[DV_MAX_CMD_LENGTH];
	char *args[DV_MAX_ARG_NUM] = {NULL};
	int arg_num;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_dv_debug *dv_debug = NULL;
	v4l2_PQ_dv_config_info_t *dv_config_info = NULL;
	struct m_pq_dv_sysfs_control *dv_sysfs_ctrl = NULL;

	if ((dev == NULL) || (buf == NULL)) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	pqdev = dev_get_drvdata(dev);
	dv_debug = &pqdev->dv_ctrl.debug;

	strncpy(cmd, buf, DV_MAX_CMD_LENGTH);
	cmd[DV_MAX_CMD_LENGTH - 1] = '\0';

	arg_num = dv_parse_cmd_helper(cmd, args, DV_MAX_ARG_NUM);
	MTK_PQ_DV_LOG_TRACE(MTK_PQ_DV_DBG_LEVEL_ERR,
		"cmd: %s, num: %d\n", cmd, arg_num);

	if (strncmp(cmd, "set_debug_level", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_set_debug_level((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "set_debug_mode", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_set_debug_mode((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "force_reg", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_reg((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "force_viewmode", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_viewmode((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "force_pr", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_pr((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "force_darkdetail", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_darkdetail((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "force_apo", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_apo((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "force_whitepoint", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_whitepoint((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "force_lightsense", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_lightsense((const char **)&args[1], arg_num, dv_debug);
		if ((ret == 0) && dv_debug->force_ctrl.light_sense.en) {
			dv_config_info = (v4l2_PQ_dv_config_info_t *)pqdev->dv_config.va;
			if (dv_config_info != NULL) {
				dv_config_info->in_frame_info.ambientDebugEn =
					dv_debug->force_ctrl.light_sense.en;
				dv_config_info->in_frame_info.u32AmbientMode =
					dv_debug->force_ctrl.light_sense.mode;
				dv_config_info->in_frame_info.s64FrontLux =
					(long long)dv_debug->force_ctrl.light_sense.lux_val;
			}
		}
	} else if (strncmp(cmd, "force_l1l4gen", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_l1l4gen((const char **)&args[1], arg_num, dv_debug);
	} else if (strncmp(cmd, "force_turnoff_pr", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_turnoff_pr((const char **)&args[1], arg_num, pqdev);
	} else if (strncmp(cmd, "force_disable_dolby", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_disable_dolby(
			(const char **)&args[1], arg_num, dv_debug, pqdev);
	} else if (strncmp(cmd, "force_idk_ctrl_flow", DV_MAX_CMD_LENGTH) == 0) {
		ret = dv_debug_force_idk_ctrl_flow((const char **)&args[1], arg_num, dv_debug);
	} else {
		ret = dv_debug_print_help(dv_debug);
		goto exit;
	}

	dv_config_info = (v4l2_PQ_dv_config_info_t *)pqdev->dv_config.va;
	dv_sysfs_ctrl = dv_get_sysfs_ctrl(dv_config_info);
	dv_store_sysfs_ctrl(dv_sysfs_ctrl, dv_debug);

	if (ret < 0) {
		MTK_PQ_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}


exit:
	return ret;
}

int mtk_pq_dv_refresh_dolby_support(
	v4l2_PQ_dv_config_info_t *dv_config_info,
	struct mtk_pq_dv_debug *dv_debug)
{
	int ret = 0;

	/* HW bond: update from efuse/hashkey */
	if (dv_config_info != NULL) {
		if (dv_config_info->cap_info.en == 1
			&& (dv_config_info->cap_info.capability &
				V4L2_PQ_DV_CAPABILITY_DOLBY_VISION_SUPPORT))
			mtk_pq_dv_cap_hw_bond_status = 0;	// support
		else
			mtk_pq_dv_cap_hw_bond_status = 1;	// not support
	}
	mtk_pq_dv_cap_hw_bond_status = 0;   // temp: do not block by hw bond now

	/* SW bond: default enable */
	if (mtk_pq_dv_cap_sw_bond_status == -1)
		mtk_pq_dv_cap_sw_bond_status = 0;		// enable for 1st time
	if (dv_debug != NULL) {
		if (dv_debug->force_ctrl.disable_dolby.en == TRUE)
			mtk_pq_dv_cap_sw_bond_status = 1;	// force disable mode
		else
			mtk_pq_dv_cap_sw_bond_status = 0;
	}

	return ret;
}

bool mtk_pq_dv_get_dv_support(void)
{
	bool ret = FALSE;

	ret = (!(mtk_pq_dv_cap_hw_bond_status | mtk_pq_dv_cap_sw_bond_status));

	return ret;
}

int mtk_pq_dv_get_status(struct mtk_pq_dv_status *pstatus)
{
	int ret = 0;

	if (pstatus == NULL) {
		MTK_PQ_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	//collect status
	pstatus->idk_enable = dv_get_idk_status();

exit:
	return ret;
}

//-----------------------------------------------------------------------------
// Debug Functions
//-----------------------------------------------------------------------------
