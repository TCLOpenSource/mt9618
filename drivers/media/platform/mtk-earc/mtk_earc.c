// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#include <asm/siginfo.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/videodev2.h>
#include <linux/errno.h>
#include <linux/compat.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-event.h>

#include <uapi/linux/mtk-v4l2-srccap.h>

#include "mtk_earc.h"
#include "drvEARC.h"
#include "hwreg_common_riu.h"


bool gEarcInitFlag = FALSE;
bool gEnableHPDInvert = TRUE;

static inline struct mtk_earc_ctx *mtk_earc_fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_earc_ctx, fh);
}

static int mtk_earc_set_stub(
	bool stub)
{
	int ret = 0;

	ret = mdrv_EARC_Set_Stub(stub);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

static int mtk_earc_get_stub(
	bool *stub)
{
	int ret = 0;

	if (stub == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = mdrv_EARC_Get_Stub(stub);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

static int mtk_earc_get_clk(struct device *dev, char *s, struct clk **clk)
{
	if (dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (s == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (clk == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	*clk = devm_clk_get(dev, s);
	if (IS_ERR(*clk)) {
		EARC_MSG_CRIT("unable to retrieve %s\n", s);
		return PTR_ERR(clk);
	}

	return 0;
}

#ifdef REMAIN_NOT_USED_FUNCTION
static void mtk_earc_put_clk(struct device *dev, struct clk *clk)
{
	if (dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return;
	}

	devm_clk_put(dev, clk);
}

static int mtk_earc_enable_parent(struct mtk_earc_dev *earc_dev, struct clk *clk, char *s)
{
	int ret = 0;
	struct clk *parent = NULL;

	ret = mtk_earc_get_clk(earc_dev->dev, s, &parent);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}
	ret = clk_set_parent(clk, parent);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}
	ret = clk_prepare_enable(clk);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}

	mtk_earc_put_clk(earc_dev->dev, parent);

	return 0;
}
#endif

static int mtk_earc_set_parent_clock(struct clk *clk, int clk_index)
{
	int ret = 0;
	struct clk *dev_clk;
	struct clk_hw *clk_hw;
	struct clk_hw *parent_clk_hw;

	dev_clk = clk;
	EARC_MSG_INFO("clk_name = %s\n", __clk_get_name(dev_clk));

	//the way 2 : get change_parent clk
	clk_hw = __clk_get_hw(dev_clk);
	parent_clk_hw = clk_hw_get_parent_by_index(clk_hw, clk_index);
	if (IS_ERR(parent_clk_hw)) {
		EARC_MSG_CRIT("failed to get parent_clk_hw\n");
		return -ENODEV;
	}

	//set_parent clk
	ret = clk_set_parent(dev_clk, parent_clk_hw->clk);
	if (ret) {
		EARC_MSG_CRIT("failed to change clk_index [%d]\n", clk_index);
		return -ENODEV;
	}

	//prepare and enable clk
	ret = clk_prepare_enable(dev_clk);
	if (ret) {
		EARC_MSG_CRIT("failed to enable clk_index [%d]\n", clk_index);
		return -ENODEV;
	}

	return ret;
}

static int earc_read_dts_u32(
	struct device_node *node,
	char *s,
	u32 *val)
{
	int ret = 0;

	if (node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (s == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (val == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	ret = of_property_read_u32(node, s, val);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	EARC_MSG_INFO("dts:%s = 0x%x(%u)\n", s, *val, *val);
	return ret;
}

static const char *mtk_earc_read_dts_string(
	struct device_node *node,
	char *prop_name)
{
	int ret = 0;
	const char *out_str = NULL;

	if (node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return (const char *)(-EINVAL);
	}

	if (prop_name == NULL) {
		EARC_MSG_POINTER_CHECK();
		return (const char *)(-EINVAL);
	}

	ret = of_property_read_string(node, prop_name, &out_str);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return (const char *)(-ENOENT);
	}

	EARC_MSG_INFO("%s = (%s)\n", prop_name, out_str);
	return out_str;
}

#define SRCCAP_MAX_INPUT_SOURCE_PORT_NUM (25)
static int mtk_earc_get_input_source_num(
	const char *input_src_name)
{
	int strlen = 0;
	enum earc_hdmi_input_source_ui_port src;

	if (input_src_name == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	strlen = strnlen(input_src_name, SRCCAP_MAX_INPUT_SOURCE_PORT_NUM);
	if (strlen >= SRCCAP_MAX_INPUT_SOURCE_PORT_NUM)
		return -ENOENT;

	if (strncmp("INPUT_SOURCE_HDMI", input_src_name, strlen) == 0)
		src = INPUT_SOURCE_UI_PORT_HDMI1;
	else if (strncmp("INPUT_SOURCE_HDMI2", input_src_name, strlen) == 0)
		src = INPUT_SOURCE_UI_PORT_HDMI2;
	else if (strncmp("INPUT_SOURCE_HDMI3", input_src_name, strlen) == 0)
		src = INPUT_SOURCE_UI_PORT_HDMI3;
	else if (strncmp("INPUT_SOURCE_HDMI4", input_src_name, strlen) == 0)
		src = INPUT_SOURCE_UI_PORT_HDMI4;
	else
		src = INPUT_SOURCE_UI_PORT_NONE;

	EARC_MSG_INFO("%s = (%d)\n", input_src_name, src);
	return (int)src;
}

static int mtk_earc_get_input_port_num(
	const char *input_port_name)
{
	int strlen = 0;
	enum earc_hdmi_input_source_hw_port src;

	if (input_port_name == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	strlen = strnlen(input_port_name, SRCCAP_MAX_INPUT_SOURCE_PORT_NUM);
	if (strlen >= SRCCAP_MAX_INPUT_SOURCE_PORT_NUM)
		return -ENOENT;

	if (strncmp("INPUT_PORT_HDMI0", input_port_name, strlen) == 0)
		src = INPUT_SOURCE_HW_PORT_HDMI0;
	else if (strncmp("INPUT_PORT_HDMI1", input_port_name, strlen) == 0)
		src = INPUT_SOURCE_HW_PORT_HDMI1;
	else if (strncmp("INPUT_PORT_HDMI2", input_port_name, strlen) == 0)
		src = INPUT_SOURCE_HW_PORT_HDMI2;
	else if (strncmp("INPUT_PORT_HDMI3", input_port_name, strlen) == 0)
		src = INPUT_SOURCE_HW_PORT_HDMI3;
	else
		src = INPUT_SOURCE_HW_PORT_NONE;

	EARC_MSG_INFO("%s = (%d)\n", input_port_name, src);
	return (int)src;
}

static int mtk_earc_parse_dts_earc_port_sel(
	struct mtk_earc_dev *earc_dev, struct device *property_dev)
{
	int ret = 0;
	struct device_node *node;

	node = property_dev->of_node;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENOENT;
	}

	ret |= earc_read_dts_u32(node, "earc_port_sel", &earc_dev->earc_port_sel);

	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return ret;
}

static int mtk_earc_parse_dts_earc_bank_num(
	struct mtk_earc_dev *earc_dev, struct device *property_dev)
{
	int ret = 0;
	struct device_node *node;

	node = property_dev->of_node;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENOENT;
	}

	ret |= earc_read_dts_u32(node, "earc_bank_num", &earc_dev->earc_bank_num);

	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return ret;
}

static int mtk_earc_parse_dts_test_earc(
	struct mtk_earc_dev *earc_dev, struct device *property_dev)
{
	int ret = 0;
	struct device_node *node;

	node = property_dev->of_node;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENOENT;
	}

	ret |= earc_read_dts_u32(node, "test_earc", &earc_dev->earc_test);

	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return ret;
}

static int mtk_earc_parse_dts_fixed_dd_index(
	struct mtk_earc_dev *earc_dev, struct device *property_dev)
{
	int ret = 0;
	struct device_node *node;

	node = property_dev->of_node;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENOENT;
	}

	ret |= earc_read_dts_u32(node, "fixed_dd_index", &earc_dev->earc_fixed_dd_index);

	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return ret;
}

static int mtk_earc_parse_dts_support_mode(
	struct mtk_earc_dev *earc_dev, struct device *property_dev)
{
	int ret = 0;
	struct device_node *node;

	node = property_dev->of_node;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENOENT;
	}

	ret |= earc_read_dts_u32(node, "support_mode", &earc_dev->earc_support_mode);

	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return ret;
}

static int mtk_earc_parse_dts_hpd_en_ivt(
	struct mtk_earc_dev *earc_dev, struct device *property_dev)
{
	int ret = 0;
	struct device_node *node;

	node = property_dev->of_node;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENOENT;
	}

	ret |= earc_read_dts_u32(node, "hpd_en_ivt", &earc_dev->hpd_en_ivt);
	gEnableHPDInvert = (earc_dev->hpd_en_ivt ? TRUE : FALSE);

	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return ret;
}

static int mtk_earc_parse_dts_clk(struct mtk_earc_dev *earc_dev)
{
	int ret = 0;
	struct clk *clk = NULL;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret |= mtk_earc_get_clk(earc_dev->dev, "EARC_EN_EARC_AUDIO2EARC_DMACRO", &clk);
	earc_dev->st_earc_clk.earc_audio2earc_dmacro = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "EARC_EN_EARC_AUDIO2EARC", &clk);
	earc_dev->st_earc_clk.earc_audio2earc = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "EARC_EN_EARC_CM2EARC", &clk);
	earc_dev->st_earc_clk.earc_cm2earc = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "EARC_EN_EARC_DEBOUNCE2EARC", &clk);
	earc_dev->st_earc_clk.earc_debounce2earc = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "EARC_EN_EARC_DM_PRBS2EARC", &clk);
	earc_dev->st_earc_clk.earc_dm_prbs2earc = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "EARC_EARC_ATOP_TXPLL_CK", &clk);
	earc_dev->st_earc_clk.earc_atop_txpll_ck = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "EARC_EARC_ATOP_AUDIO_CK", &clk);
	earc_dev->st_earc_clk.earc_atop_audio_ck = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "HDMIRX_EARC_DEBOUNCE_INT_CK", &clk);
	earc_dev->st_earc_clk.hdmirx_earc_debounce_int_ck = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "HDMIRX_EARC_DM_PRBS_INT_CK", &clk);
	earc_dev->st_earc_clk.hdmirx_earc_dm_prbs_int_ck = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "HDMIRX_EARC_CM_INT_CK", &clk);
	earc_dev->st_earc_clk.hdmirx_earc_cm_int_ck = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "HDMIRX_EARC_AUDIO_INT_CK", &clk);
	earc_dev->st_earc_clk.hdmirx_earc_audio_int_ck = clk;

	ret |= mtk_earc_get_clk(earc_dev->dev, "EARC_EARC_AUDIO_INT_CK", &clk);
	earc_dev->st_earc_clk.earc_earc_audio_int_ck = clk;

	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return ret;
}

static int mtk_earc_parse_dts_cap(struct mtk_earc_dev *earc_dev)
{
	int ret = 0;
	struct device_node *cap_node;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	cap_node = of_find_node_by_name(earc_dev->dev->of_node, "capability");
	if (cap_node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENOENT;
	}

	/* external use */

	/* internal use */
	ret |= earc_read_dts_u32(cap_node, "hw_version_1", &earc_dev->earc_hw_ver_1);
	ret |= earc_read_dts_u32(cap_node, "hw_version_2", &earc_dev->earc_hw_ver_2);

	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return ret;
}

#define EARC_STRING_SIZE_10 (10)
static int mtk_earc_parse_dts_srccap_port_map(struct mtk_earc_dev *earc_dev)
{
	int ret = 0;
	int i = 0, val = 0, cnt = 0;
	char input[EARC_STRING_SIZE_10] = {0};
	const char *out_str = NULL;
	u32 input_num = 0;
	u32 max_input_num = V4L2_SRCCAP_INPUT_SOURCE_NUM;
	u64 string_size = 0;
	struct device_node *map_node;
	struct device_node *input_node;

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	map_node = of_find_node_by_name(NULL, "port_map");
	if (map_node == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENOENT;
	}

	ret |= earc_read_dts_u32(map_node, "input_num", &input_num);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		of_node_put(map_node);
		return -ENOENT;
	}

	if (input_num > max_input_num) {
		of_node_put(map_node);
		EARC_MSG_INFO("input_num > max_input_num\n");
		return -E2BIG;
	}

	for (i = 0; i < input_num; i++) {
		memset(input, 0, sizeof(input));
		string_size += snprintf(input, sizeof(input), "input%d", i);

		of_node_get(map_node);

		input_node = of_find_node_by_name(map_node, input);
		if (input_node == NULL) {
			EARC_MSG_POINTER_CHECK();
			of_node_put(map_node);
			return -ENOENT;
		}

		out_str = mtk_earc_read_dts_string(input_node, "source");
		if (out_str == NULL) {
			EARC_MSG_POINTER_CHECK();
			of_node_put(input_node);
			continue;
		} else {
			val = mtk_earc_get_input_source_num(out_str);
			if (val == INPUT_SOURCE_UI_PORT_NONE) {
				EARC_MSG_RETURN_CHECK(val);
				of_node_put(input_node);
				continue;
			} else {
				if (cnt < HDMI_PORT_MAX_NUM) {
					earc_dev->map[cnt].ui_port =
						(enum earc_hdmi_input_source_ui_port)val;
				} else {
					EARC_MSG_INFO("array index cnt > HDMI_PORT_MAX_NUM\n");
					break;
				}
			}

			out_str = mtk_earc_read_dts_string(input_node, "data_port");
			if (out_str == NULL) {
				EARC_MSG_POINTER_CHECK();
				of_node_put(input_node);
				continue;
			} else {
				val = 0;
				val = mtk_earc_get_input_port_num(out_str);
				if (val == INPUT_SOURCE_HW_PORT_NONE) {
					EARC_MSG_RETURN_CHECK(val);
					of_node_put(input_node);
					continue;
				} else {
					if (cnt < HDMI_PORT_MAX_NUM) {
						earc_dev->map[cnt].hw_port =
							(enum earc_hdmi_input_source_hw_port)val;
						cnt++;
					} else {
						EARC_MSG_INFO
						("array index cnt > HDMI_PORT_MAX_NUM\n");
						break;
					}
				}
			}
		}
	}

	of_node_put(input_node);
	of_node_put(map_node);
	return ret;
}

int mtk_earc_enable_clock(struct mtk_earc_dev *earc_dev)
{
	struct clk *clk = NULL;
	int ret;
	int clk_earc_debounce_int_ck = 0;
	//"0:mpll_vcod4_216m_ck"
	int clk_earc_cm_int_ck = 1;
	//"0:xtal_12m_ck","1:mpll_vcod4_216m_ck";
#if (defined(SEL_MT5876) || defined(SEL_MT5879))
	int clk_earc_audio_int_ck = 1;
#endif
	//"0:xtal_12m_ck","1:earc_atop_audio_ck"

	/* enable register : reg_sw_en_earc_audio2earc_dmacro */
	clk = earc_dev->st_earc_clk.earc_audio2earc_dmacro;
	ret = clk_prepare_enable(clk);
	if (ret) {
		dev_err(earc_dev->dev,
			"[eARC]Failed to enable EARC_EN_EARC_AUDIO2EARC_DMACRO: %d\n", ret);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_earc_audio2earc */
	clk = earc_dev->st_earc_clk.earc_audio2earc;
	ret = clk_prepare_enable(clk);
	if (ret) {
		dev_err(earc_dev->dev,
			"[eARC]Failed to enable EARC_EN_EARC_AUDIO2EARC: %d\n", ret);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_earc_cm2earc */
	clk = earc_dev->st_earc_clk.earc_cm2earc;
	ret = clk_prepare_enable(clk);
	if (ret) {
		dev_err(earc_dev->dev,
			"[eARC]Failed to enable EARC_EN_EARC_CM2EARC: %d\n", ret);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_earc_debounce2earc */
	clk = earc_dev->st_earc_clk.earc_debounce2earc;
	ret = clk_prepare_enable(clk);
	if (ret) {
		dev_err(earc_dev->dev,
			"[eARC]Failed to enable EARC_EN_EARC_DEBOUNCE2EARC: %d\n", ret);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_earc_dm_prbs2earc */
	clk = earc_dev->st_earc_clk.earc_dm_prbs2earc;
	ret = clk_prepare_enable(clk);
	if (ret) {
		dev_err(earc_dev->dev,
			"[eARC]Failed to enable EARC_EN_EARC_DM_PRBS2EARC: %d\n", ret);
		return -EINVAL;
	}

	ret =
		mtk_earc_set_parent_clock(earc_dev->st_earc_clk.hdmirx_earc_debounce_int_ck,
			clk_earc_debounce_int_ck);

	if (ret) {
		dev_err(earc_dev->dev,
			"[eARC]Failed to enable HDMIRX_EARC_DEBOUNCE_INT_CK: %d\n", ret);
		return -EINVAL;
	}

	ret =
		mtk_earc_set_parent_clock(earc_dev->st_earc_clk.hdmirx_earc_cm_int_ck,
			clk_earc_cm_int_ck);

	if (ret) {
		dev_err(earc_dev->dev,
			"[eARC]Failed to enable HDMIRX_EARC_CM_INT_CK: %d\n", ret);
		return -EINVAL;
	}

#if (defined(SEL_MT5876) || defined(SEL_MT5879))
	ret =
		mtk_earc_set_parent_clock(earc_dev->st_earc_clk.earc_earc_audio_int_ck,
			clk_earc_audio_int_ck);

	if (ret) {
		dev_err(earc_dev->dev,
			"[eARC]Failed to enable EARC_EARC_AUDIO_INT_CK: %d\n", ret);
		return -EINVAL;
	}
#endif
	return 0;
}

int mtk_earc_SetClockSource(struct mtk_earc_dev *earc_dev)
{
	struct clk *clk = NULL;
	int ret = 0;

	clk = earc_dev->st_earc_clk.hdmirx_earc_dm_prbs_int_ck;
	ret = clk_set_parent(clk, earc_dev->st_earc_clk.earc_atop_txpll_ck);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}
	EARC_MSG_INFO("hdmirx_earc_dm_prbs_int_ck\n");

	clk = earc_dev->st_earc_clk.hdmirx_earc_audio_int_ck;
	ret = clk_set_parent(clk, earc_dev->st_earc_clk.earc_atop_txpll_ck);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}
	EARC_MSG_INFO("hdmirx_earc_cm_int_ck\n");

	clk = earc_dev->st_earc_clk.earc_earc_audio_int_ck;
	ret = clk_set_parent(clk, earc_dev->st_earc_clk.earc_atop_audio_ck);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}
	EARC_MSG_INFO("earc_earc_audio_int_ck\n");

	return 0;
}

void mtk_earc_disable_clock(struct mtk_earc_dev *earc_dev)
{
	struct clk *clk = NULL;

	/* disable register : reg_sw_en_earc_audio2earc_dmacro */
	clk = earc_dev->st_earc_clk.earc_audio2earc_dmacro;
	if (__clk_is_enabled(clk))
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_earc_audio2earc */
	clk = earc_dev->st_earc_clk.earc_audio2earc;
	if (__clk_is_enabled(clk))
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_earc_cm2earc */
	clk = earc_dev->st_earc_clk.earc_cm2earc;
	if (__clk_is_enabled(clk))
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_earc_debounce2earc */
	clk = earc_dev->st_earc_clk.earc_debounce2earc;
	if (__clk_is_enabled(clk))
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_earc_dm_prbs2earc */
	clk = earc_dev->st_earc_clk.earc_dm_prbs2earc;
	if (__clk_is_enabled(clk))
		clk_disable_unprepare(clk);
}

int earc_get_status(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_info earc_info = {0};
	unsigned long ret;

	earc_info.u8IsSupportEarc = TRUE;
	earc_info.u8EarcConnectState = mdrv_EARC_GetConnectState();
	earc_info.enSupportEarcPort = mdrv_EARC_GetEarcUIPortSel();
	earc_info.u8SupportMode = mdrv_EARC_Get_SupportMode();

	ret = copy_to_user((void *)ext_ctrl->ptr, &earc_info,
			sizeof(struct v4l2_ext_earc_info));

	if (ret != 0) {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_get_capibility(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_info earc_info = {0};
	int i;
	unsigned long ret;
	MS_U8 block_id_cnt = 0, skip = 0;
	MS_U8 payload_num = 0, payload_cnt = 0;

	for (i = 0; i < CAPIBILITY_NUM; i++) {
		earc_info.u8EarcCapibility[i] = mdrv_EARC_GetCapibility(i);

		if (i == 0 || skip == 1)
			continue;

		if (payload_cnt == 0 && skip == 0 &&
			earc_info.u8EarcCapibility[i] > 0 &&
			earc_info.u8EarcCapibility[i] <= EARC_CAP_EFFECTIVE_BLOCK_ID_MAX) {
			block_id_cnt++;
			payload_cnt = 2 + mdrv_EARC_GetCapibility(i + 1);
			payload_num += mdrv_EARC_GetCapibility(i + 1);
		} else if (payload_cnt == 0) {
			skip = 1;
		}

		if (payload_cnt > 0)
			payload_cnt--;
	}

	if (block_id_cnt > 0)
		earc_info.size = 1 + (block_id_cnt << 1) + payload_num;

	EARC_MSG_DEBUG("cap_size = %d, block_num = %d, payload_num = %d\n",
		earc_info.size, block_id_cnt, payload_num);

	ret = copy_to_user((void *)ext_ctrl->ptr, &earc_info,
			sizeof(struct v4l2_ext_earc_info));

	if (ret != 0) {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_get_latency(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_info earc_info = {0};
	unsigned long ret;

	earc_info.u8EarcLatency = mdrv_EARC_GetLatency();

	ret = copy_to_user((void *)ext_ctrl->ptr, &earc_info,
			sizeof(struct v4l2_ext_earc_info));

	if (ret != 0) {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_get_stub(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_info earc_info = {0};
	bool stub;
	int RetStub = 0;
	unsigned long ret;

	memset(&stub, 0, sizeof(bool));

	RetStub = mtk_earc_get_stub(&stub);
	EARC_MSG_DEBUG("eARC stub = %d\n", stub);

	earc_info.u8EarcStub = stub;

	ret = copy_to_user((void *)ext_ctrl->ptr, &earc_info,
			sizeof(struct v4l2_ext_earc_info));

	if (ret != 0) {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcSupportPort(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_config earc_config;
	MS_BOOL bEn5VDetectInvert;
	MS_BOOL bEnHPDInvert;
	unsigned long ret;

	ret = copy_from_user(&earc_config, (void *)ext_ctrl->ptr,
		sizeof(struct v4l2_ext_earc_config));

	if (ret == 0) {
		bEn5VDetectInvert = (earc_config.u8Enable5VDetectInvert ? TRUE : FALSE);
		bEnHPDInvert = (gEnableHPDInvert ? TRUE : FALSE);

		mdrv_EARC_SetEarcPort(earc_config.enSupportEarcPort, bEn5VDetectInvert,
			bEnHPDInvert);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcSupportMode(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_config earc_config;
	unsigned long ret;

	ret = copy_from_user(&earc_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_config));

	if (ret == 0) {
		mdrv_EARC_SetEarcSupportMode(earc_config.enEarcSupportMode);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_ArcPin(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_config earc_config;
	unsigned long ret;

	ret = copy_from_user(&earc_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_config));

	if (ret == 0) {
		mdrv_EARC_SetArcPin(earc_config.u8SetArcPinEnable);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcStub(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_config earc_config;
	unsigned long ret;

	ret = copy_from_user(&earc_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_config));

	if (ret == 0) {
		mtk_earc_set_stub((bool)earc_config.u8SetStubEnable);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcHeartbeatStatus(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_config earc_config;
	unsigned long ret;

	ret = copy_from_user(&earc_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_config));

	if (ret == 0) {
		mdrv_EARC_Set_HeartbeatStatus(earc_config.u8SetHeartBeatStatus,
			(earc_config.u8OverWriteEnable & 0x1),
			(earc_config.u8SetValue & 0x1));
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcLatencyInfo(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_config earc_config;
	unsigned long ret;

	ret = copy_from_user(&earc_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_config));

	if (ret == 0) {
		mdrv_EARC_SetLatencyInfo(earc_config.u8SetEarcLatency);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcDifferentialDriveStrength(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_hw_config earc_hw_config;
	unsigned long ret;

	ret = copy_from_user(&earc_hw_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_hw_config));

	if (ret == 0) {
		mdrv_EARC_Set_DifferentialDriveStrength(earc_hw_config.u8DifferentialDriveStrength);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcDifferentialSkew(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_hw_config earc_hw_config;
	unsigned long ret;

	ret = copy_from_user(&earc_hw_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_hw_config));

	if (ret == 0) {
		mdrv_EARC_Set_DifferentialSkew(earc_hw_config.u8DifferentialSkew);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcCommonDriveStrength(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_hw_config earc_hw_config;
	unsigned long ret;

	ret = copy_from_user(&earc_hw_config, (void *)ext_ctrl->ptr,
		sizeof(struct v4l2_ext_earc_hw_config));

	if (ret == 0) {
		mdrv_EARC_Set_CommonDriveStrength(earc_hw_config.u8CommonDriveStrength);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcDifferentialOnOff(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_hw_config earc_hw_config;
	unsigned long ret;

	ret = copy_from_user(&earc_hw_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_hw_config));
	if (ret == 0) {
		mdrv_EARC_Set_DifferentialOnOff(earc_hw_config.u8DifferentialOnOff);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

int earc_set_EarcCommonOnOff(struct v4l2_ext_control *ext_ctrl)
{
	struct v4l2_ext_earc_hw_config earc_hw_config;
	unsigned long ret;

	ret = copy_from_user(&earc_hw_config, (void *)ext_ctrl->ptr,
			sizeof(struct v4l2_ext_earc_hw_config));
	if (ret == 0) {
		mdrv_EARC_Set_CommonOnOff(earc_hw_config.u8CommonOnOff);
	} else {
		EARC_MSG_ERROR("copy_to_user failed\n");
		ret = -EINVAL;
	}

	return ret;
}

static int earc_create_sysfs(struct mtk_earc_dev *earc_dev)
{
	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	earc_dev->mtkdbg_kobj = kobject_create_and_add("mtk_dbg", &earc_dev->dev->kobj);
	//earc_dev->rddbg_kobj  = kobject_create_and_add("RD", &earc_dev->dev->kobj);

	mtk_earc_sysfs_init(earc_dev);

	return 0;
}

static int earc_remove_sysfs(struct mtk_earc_dev *earc_dev)
{
	dev_info(earc_dev->dev, "[eARC]Remove device attribute files");
	mtk_earc_sysfs_deinit(earc_dev);

	kobject_del(earc_dev->mtkdbg_kobj);
	kobject_del(earc_dev->rddbg_kobj);

	earc_dev->mtkdbg_kobj = NULL;
	earc_dev->rddbg_kobj = NULL;

	return 0;
}

int vidioc_earc_g_ext_ctrls(struct file *file, void *prv,
				   struct v4l2_ext_controls *ext_ctrls)
{
	struct v4l2_ext_control *ctrl;
	unsigned int idx;

	for (idx = 0; idx < ext_ctrls->count; idx++) {
		ctrl = ext_ctrls->controls + idx;

		switch (ctrl->id) {
		case V4L2_CID_EARC_STATUS:
			earc_get_status(ctrl);
			break;
		case V4L2_CID_EARC_CAPABILITY:
			earc_get_capibility(ctrl);
			break;
		case V4L2_CID_EARC_LATENCY_INFO:
			earc_get_latency(ctrl);
			break;
		case V4L2_CID_EARC_STUB:
			earc_get_stub(ctrl);
			break;
		}
	}
	return 0;
}

int vidioc_earc_s_ext_ctrls(struct file *file, void *prv,
				   struct v4l2_ext_controls *ext_ctrls)
{
	int ret = 0;
	unsigned int idx;
	struct v4l2_ext_control *ext_ctrl;

	for (idx = 0; idx < ext_ctrls->count; idx++) {
		ext_ctrl = &ext_ctrls->controls[idx];

		switch (ext_ctrl->id) {
		case V4L2_CID_EARC_PORT_SEL:
			ret = earc_set_EarcSupportPort(ext_ctrl);
			break;
		case V4L2_CID_EARC_SUPPORT_MODE:
			ret = earc_set_EarcSupportMode(ext_ctrl);
			break;
		case V4L2_CID_EARC_ARC_PIN:
			ret = earc_set_ArcPin(ext_ctrl);
			break;
		case V4L2_CID_EARC_HEARTBEAT_STATUS:
			ret = earc_set_EarcHeartbeatStatus(ext_ctrl);
			break;
		case V4L2_CID_EARC_LATENCY:
			ret = earc_set_EarcLatencyInfo(ext_ctrl);
			break;
		case V4L2_CID_EARC_STUB:
			ret = earc_set_EarcStub(ext_ctrl);
			break;
		case V4L2_CID_EARC_DIFF_DRIV:
			ret = earc_set_EarcDifferentialDriveStrength(ext_ctrl);
			break;
		case V4L2_CID_EARC_DIFF_SKEW:
			ret = earc_set_EarcDifferentialSkew(ext_ctrl);
			break;
		case V4L2_CID_EARC_COMM_DRIV:
			ret = earc_set_EarcCommonDriveStrength(ext_ctrl);
			break;
		case V4L2_CID_EARC_DIFF_ONOFF:
			ret = earc_set_EarcDifferentialOnOff(ext_ctrl);
			break;
		case V4L2_CID_EARC_COMM_ONOFF:
			ret = earc_set_EarcCommonOnOff(ext_ctrl);
			break;
		default:
			EARC_MSG_NOTICE("V4L2 Id is invalid!\n");
			ret = -1;
			break;
		}
	}
	return ret;
}

static int mtk_earc_ctrl_earc_status_monitor_task(void *data)
{
	struct mtk_earc_dev *earc_dev = (struct mtk_earc_dev *)data;
	struct v4l2_event event;

	static MS_U8 u8PreEarcStatus, u8CurEarcStatus;

	memset(&u8PreEarcStatus, 0, sizeof(MS_U8));
	memset(&u8CurEarcStatus, 0, sizeof(MS_U8));

	while (video_is_registered(earc_dev->vdev)) {
		memset(&event, 0, sizeof(struct v4l2_event));

		u8CurEarcStatus = mdrv_EARC_GetConnectState();
		if (u8PreEarcStatus != u8CurEarcStatus) {
			event.u.ctrl.changes |= V4L2_EVENT_CTRL_CH_EARC_CONNECT_STATUS;
			event.u.ctrl.value = u8CurEarcStatus;
			u8PreEarcStatus = u8CurEarcStatus;
		}

		if (mdrv_EARC_GetCapibilityChange() != 0) {
			mdrv_EARC_SetCapibilityChangeClear();
			event.u.ctrl.changes |= V4L2_EVENT_CTRL_CH_EARC_CAPIBILITY;
		}

		if (mdrv_EARC_GetLatencyChange() != 0) {
			mdrv_EARC_SetLatencyChangeClear();
			event.u.ctrl.changes |= V4L2_EVENT_CTRL_CH_EARC_LATENCY;
		}

		if (event.u.ctrl.changes != 0) {
			event.type = V4L2_EVENT_EARC_STATUS_CHANGE;
			event.id = V4L2_EARC_CTRL_EVENT_STATUS;
			//event.u.ctrl.changes = V4L2_EVENT_CTRL_CH_EARC_CONNECT_STATUS;
			event.u.ctrl.value = 0;
			v4l2_event_queue(earc_dev->vdev, &event);
		}

		usleep_range(10000, 11000);	// sleep 10ms

		if (kthread_should_stop()) {
			EARC_MSG_ERROR("The process has been stopped.\n");
			return 0;
		}
	}

	return 0;
}


static int mtk_earc_subscribe_event(struct v4l2_fh *fh,
				    const struct v4l2_event_subscription *event_sub)
{
	struct mtk_earc_dev *earc_dev = NULL;

	earc_dev = video_get_drvdata(fh->vdev);

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	EARC_MSG_INFO("Subscribe event, type:%d, id:%d\n",
		event_sub->type, event_sub->id);

	switch (event_sub->type) {
	case V4L2_EVENT_EARC_STATUS_CHANGE:
		if (earc_dev->earc_event_task == NULL) {
			earc_dev->earc_event_task =
			    kthread_create(mtk_earc_ctrl_earc_status_monitor_task, earc_dev,
					   "earc_event_task");
			if (earc_dev->earc_event_task == ERR_PTR(-ENOMEM))
				return -ENOMEM;
			wake_up_process(earc_dev->earc_event_task);
		}
		break;
	default:
		return -EINVAL;
	}

	return v4l2_event_subscribe(fh, event_sub, EARC_EVENTQ_SIZE, NULL);
}

static int mtk_earc_unsubscribe_event(struct v4l2_fh *fh,
				      const struct v4l2_event_subscription *event_sub)
{
	struct mtk_earc_dev *earc_dev = NULL;

	earc_dev = video_get_drvdata(fh->vdev);

	if (earc_dev == NULL) {
		EARC_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	EARC_MSG_INFO("Unsubscribe event, type:%d, id:%d\n",
		event_sub->type, event_sub->id);

	switch (event_sub->type) {
	case V4L2_EVENT_EARC_STATUS_CHANGE:
		if (earc_dev->earc_event_task != NULL) {
			kthread_stop(earc_dev->earc_event_task);
			earc_dev->earc_event_task = NULL;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return v4l2_event_unsubscribe(fh, event_sub);
}


static int mtk_earc_ioremap(struct mtk_earc_dev *earc_dev, struct device *pdev)
{
	int ret = 0, regSize = 0, i = 0, offset[IOREMAP_OFFSET_SIZE] = {0, 0};
	struct device *property_dev = pdev;
	u32 *reg = NULL;

	earc_dev->reg_info_offset = IOREMAP_REG_INFO_OFFSET;
	earc_dev->reg_pa_idx = 1;
	earc_dev->reg_pa_size_idx = IOREMAP_REG_PA_SIZE_IDX;

	ret = of_property_read_u32_array(property_dev->of_node,
		"earc_bank_num", &earc_dev->earc_bank_num, 1);
	if (ret) {
		EARC_MSG_ERROR("Failed to get earc_bank_num\r\n");
		return -EINVAL;
	}

	regSize = (earc_dev->earc_bank_num << earc_dev->reg_info_offset);
	reg = kcalloc(regSize, sizeof(u32), GFP_KERNEL);
	if (reg == NULL) {
		EARC_MSG_ERROR("Failed to kcalloc register\r\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(property_dev->of_node, "ioremap", reg, regSize);
	if (ret < 0) {
		EARC_MSG_ERROR("Failed to get register\r\n");
		kfree(reg);
		return -EINVAL;
	}

	earc_dev->pIoremap = kcalloc(earc_dev->earc_bank_num, sizeof(u64), GFP_KERNEL);
	if (earc_dev->pIoremap == NULL) {
		EARC_MSG_ERROR("Failed to kcalloc g_ioremap\r\n");
		kfree(reg);
		return -EINVAL;
	}

	for (i = 0; i < earc_dev->earc_bank_num; ++i) {
		offset[0] = (i << earc_dev->reg_info_offset) + earc_dev->reg_pa_idx;
		offset[1] = (i << earc_dev->reg_info_offset) + earc_dev->reg_pa_size_idx;

		earc_dev->pIoremap[i] = (u64)ioremap(reg[offset[0]], reg[offset[1]]);

		EARC_MSG_INFO("0x%llx = 0x%x(%x)\n", earc_dev->pIoremap[i],
			reg[offset[0]], reg[offset[1]]);

		mdrv_EARC_SetRIUAddr(reg[offset[0]] - 0x1C000000, reg[offset[1]],
			earc_dev->pIoremap[i]);
	}
	kfree(reg);
	kfree(earc_dev->pIoremap);
	return ret;
}

static int mtk_earc_open(struct file *file)
{
	struct mtk_earc_dev *earc = video_drvdata(file);
	struct video_device *vdev = video_devdata(file);
	struct mtk_earc_ctx *ctx = NULL;

	EARC_MSG_INFO("eARC_open start\r\n");
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		EARC_MSG_ERROR("Failed to get ctx!\r\n");
		return -ENOMEM;
	}

	if (mutex_lock_interruptible(&earc->mutex)) {
		kfree(ctx);
		return -ERESTARTSYS;
	}

	v4l2_fh_init(&ctx->fh, vdev);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	ctx->earc_dev = earc;

	mutex_unlock(&earc->mutex);

	EARC_MSG_INFO("eARC_open finish\n");
	return 0;
}

static int mtk_earc_release(struct file *file)
{
	struct mtk_earc_dev *earc = video_drvdata(file);
	struct mtk_earc_ctx *ctx = mtk_earc_fh_to_ctx(file->private_data);

	EARC_MSG_WARNING("earc_release\r\n");
	mutex_lock(&earc->mutex);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	mutex_unlock(&earc->mutex);
	return 0;
}

static const struct v4l2_file_operations mtk_earc_fops = {
	.owner = THIS_MODULE,
	.open = mtk_earc_open,
	.release = mtk_earc_release,
	.unlocked_ioctl = video_ioctl2,
};

static int mtk_earc_query_cap(struct file *file, void *fh,
	struct v4l2_capability *cap)
{
	//struct mtk_earc_dev *earc_dev = video_drvdata(file);

	strncpy(cap->driver, EARC_DRIVER_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, EARC_DRIVER_NAME, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->device_caps =  V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static const struct v4l2_ioctl_ops mtk_earc_ioctl_ops = {

	.vidioc_g_ext_ctrls			= vidioc_earc_g_ext_ctrls,
	.vidioc_s_ext_ctrls			= vidioc_earc_s_ext_ctrls,
	.vidioc_subscribe_event		= mtk_earc_subscribe_event,
	.vidioc_unsubscribe_event	= mtk_earc_unsubscribe_event,
	.vidioc_querycap			= mtk_earc_query_cap,
};

static int earc_parse_dts(
	struct mtk_earc_dev *earc_dev,
	struct platform_device *pdev)
{
	int ret = 0;
	struct device *property_dev = &pdev->dev;

	/* parse clock */
	ret |= mtk_earc_parse_dts_clk(earc_dev);

	/* parse ioremap */
	ret |= mtk_earc_ioremap(earc_dev, property_dev);

	/* parse earc_port_sel */
	ret |= mtk_earc_parse_dts_earc_port_sel(earc_dev, property_dev);

	/* parse earc_bank_num */
	ret |= mtk_earc_parse_dts_earc_bank_num(earc_dev, property_dev);

	/* parse test_earc */
	ret |= mtk_earc_parse_dts_test_earc(earc_dev, property_dev);

	/* parse fixed_dd_index */
	ret |= mtk_earc_parse_dts_fixed_dd_index(earc_dev, property_dev);

	/* parse support_mode */
	ret |= mtk_earc_parse_dts_support_mode(earc_dev, property_dev);

	/* parse hpd_en_ivt */
	ret |= mtk_earc_parse_dts_hpd_en_ivt(earc_dev, property_dev);

	/* parse capability */
	ret |= mtk_earc_parse_dts_cap(earc_dev);

	/* parse srccap port map */
	ret |= mtk_earc_parse_dts_srccap_port_map(earc_dev);

	return ret;
}


static const struct of_device_id mtk_earc_match[] = {
	{
		.compatible = "mediatek,earc",
	},
	{},
};

static int earc_probe(struct platform_device *pdev)
{
	struct mtk_earc_dev *earc_dev;
	struct v4l2_device *v4l2_dev = NULL;
	const struct of_device_id *of_id;
	int ret = 0, nr = 0;

	EARC_MSG_INFO("eARC Probe starts!\n");
	if (gEarcInitFlag)
		return 0;

	earc_dev = devm_kzalloc(&pdev->dev, sizeof(struct mtk_earc_dev), GFP_KERNEL);
	if (!earc_dev) {
		EARC_MSG_ERROR("eARC Probe FAIL!\n");
		return -ENOMEM;
	}

	spin_lock_init(&earc_dev->lock);
	mutex_init(&earc_dev->mutex);

	/* save device */
	earc_dev->dev = &pdev->dev;

	/* find device ID */
	if (pdev->dev.of_node != NULL) {
		of_id = of_match_node(mtk_earc_match, pdev->dev.of_node);
		if (!of_id) {
			ret = -ENODEV;
			EARC_MSG_ERROR("Find device ID fail!\r\n");
			goto err_earc_of_match_node;
		}
		//earc_dev->dev_id = *(u16 *)of_id->data;
	}

	/* parse earc device tree */
	ret = earc_parse_dts(earc_dev, pdev);
	if (ret) {
		dev_err(&pdev->dev, "[eARC]\033[1;31mfailed to parse earc dts\033[0m\n");
		goto EXIT;
	}

	/* register v4l2 device */
	v4l2_dev = &earc_dev->v4l2_dev;
	ret = v4l2_device_register(&pdev->dev, v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "[eARC]Failed to register v4l2 device\n");
		ret = -EINVAL;
		return ret;
	}

	/* allocate video device */
	earc_dev->vdev = video_device_alloc();
	if (!earc_dev->vdev) {
		ret = -ENOMEM;
		EARC_MSG_ERROR("video_device allocation fail!\n");
		goto err_earc_vdev_register;
	}

	mdrv_EARC_HWInit(earc_dev->earc_hw_ver_1, earc_dev->earc_hw_ver_2);

	earc_dev->vdev->fops		= &mtk_earc_fops;
	earc_dev->vdev->device_caps	= V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	earc_dev->vdev->v4l2_dev	= v4l2_dev;
	earc_dev->vdev->vfl_dir		= VFL_DIR_M2M;
	earc_dev->vdev->minor		= -1;
	earc_dev->vdev->tvnorms		= V4L2_STD_ALL;
	earc_dev->vdev->release		= video_device_release;
	earc_dev->vdev->ioctl_ops	= &mtk_earc_ioctl_ops;
	earc_dev->vdev->lock		= &earc_dev->mutex;

	/* register video device */
	if (earc_dev->earc_fixed_dd_index == EARC_UNSPECIFIED_NODE_NUM)
		nr = -1;
	else
		nr = earc_dev->earc_fixed_dd_index;

	ret = video_register_device(earc_dev->vdev, VFL_TYPE_GRABBER, nr);
	if (ret) {
		EARC_MSG_ERROR("Fail to register video device\n");
		v4l2_err(v4l2_dev, "[eARC]Failed to register video device\n");
		goto err_earc_vdev_register;
	}

	/* write sysfs */
	earc_create_sysfs(earc_dev);

	video_set_drvdata(earc_dev->vdev, earc_dev);
	platform_set_drvdata(pdev, earc_dev);

	v4l2_info(v4l2_dev, "[eARC]device registered as /dev/video%d\n", earc_dev->vdev->num);
	EARC_MSG_INFO("eARC device has been registered as /dev/video%d\n", earc_dev->vdev->num);

	pm_runtime_enable(&pdev->dev);
	of_id = of_match_node(mtk_earc_match, pdev->dev.of_node);
	if (!of_id) {
		ret = -ENODEV;
		goto err_earc_of_match_node;
	}

	snprintf(earc_dev->vdev->name, sizeof(earc_dev->vdev->name),
		"%s-vdev", MTK_EARC_NAME);

	/* earc initialize */
	mtk_earc_enable_clock(earc_dev);
	mtk_earc_SetClockSource(earc_dev);
	mdrv_EARC_Init((MS_U8)(earc_dev->earc_support_mode));
	mdrv_EARC_SetEarcPort(earc_dev->earc_port_sel, TRUE, earc_dev->hpd_en_ivt);
	mdrv_EARC_SetArcPin(FALSE);
	mdrv_EARC_Set_HDMI_Port_Map(earc_dev->map);


	gEarcInitFlag = TRUE;
	EARC_MSG_INFO("Disable ARC pin. eARC probe successful\n");

	return 0;

err_earc_of_match_node:
	video_unregister_device(earc_dev->vdev);
err_earc_vdev_register:
	video_device_release(earc_dev->vdev);
EXIT:
	return ret;
}

static int earc_remove(struct platform_device *pdev)
{
	struct mtk_earc_dev *dev = platform_get_drvdata(pdev);

	video_unregister_device(dev->vdev);
	v4l2_device_unregister(&dev->v4l2_dev);
	earc_remove_sysfs(dev);
	return 0;
}

static int earc_suspend(struct device *dev)
{
	struct mtk_earc_dev *earc = dev_get_drvdata(dev);

	mtk_earc_disable_clock(earc);
	EARC_MSG_INFO("eARC suspend done\n");
	return 0;
}

static int earc_resume(struct device *dev)
{
	struct mtk_earc_dev *earc = dev_get_drvdata(dev);

	mtk_earc_enable_clock(earc);
	mtk_earc_SetClockSource(earc);
	mdrv_EARC_Resume_Init();
	mdrv_EARC_SetArcPin(FALSE);
	EARC_MSG_INFO("Disable ARC pin. eARC resume done.\n");

	return 0;
}

static const struct dev_pm_ops earc_pm_ops = {
	.suspend = earc_suspend,
	.resume = earc_resume,
};

MODULE_DEVICE_TABLE(of, mtk_earc_match);
static struct platform_driver earc_pdrv = {
	.probe = earc_probe,
	.remove = earc_remove,
	.driver = {
		   .name = MTK_EARC_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mtk_earc_match,
		   .pm = &earc_pm_ops,
		   },
};

module_platform_driver(earc_pdrv);
MODULE_AUTHOR("MediaTek TV");
MODULE_DESCRIPTION("MTK EARC");
MODULE_LICENSE("GPL");
