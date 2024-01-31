/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_EARC_H
#define MTK_EARC_H
#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <linux/mtk-v4l2-earc.h>
#include "drvXC_HDMI_if.h"
#include "../mtk_earc_sysfs.h"

#define MTK_EARC_NAME "mtk_earc"
#define EARC_DRIVER_NAME "mtk-earc"
#define EARC_LABEL "[EARC] "

#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif


/* ============================================================================================== */
/* ------------------------------------------- Macros ------------------------------------------- */
/* ============================================================================================== */

#define EARC_EVENTQ_SIZE (8)
#define EARC_NSEC_PER_USEC (1000)
#define EARC_CAP_EFFECTIVE_BLOCK_ID_MAX	(3)

#define IOREMAP_OFFSET_SIZE			2
#define IOREMAP_REG_INFO_OFFSET		2
#define IOREMAP_REG_PA_SIZE_IDX		3

#define EARC_UNSPECIFIED_NODE_NUM	0xFF

#define EARC_MSG_POINTER_CHECK() \
		do { \
			pr_err(EARC_LABEL"[%s,%d] pointer is NULL\n", __func__, __LINE__); \
			dump_stack(); \
		} while (0)

#define EARC_MSG_RETURN_CHECK(result) \
			pr_err(EARC_LABEL"[%s,%d] return is %d\n", __func__, __LINE__, (result))

#define EARC_GET_TIMESTAMP(tv) \
		do { \
			struct timespec ts; \
			ktime_get_ts(&ts); \
			((struct timeval *)tv)->tv_sec = ts.tv_sec; \
			((struct timeval *)tv)->tv_usec = ts.tv_nsec / EARC_NSEC_PER_USEC; \
		} while (0)

enum earc_hdmi_input_source_ui_port {
	INPUT_SOURCE_UI_PORT_HDMI1 = 1,
	INPUT_SOURCE_UI_PORT_HDMI2,
	INPUT_SOURCE_UI_PORT_HDMI3,
	INPUT_SOURCE_UI_PORT_HDMI4,
	INPUT_SOURCE_UI_PORT_NONE = 0xff
};

enum earc_hdmi_input_source_hw_port {
	INPUT_SOURCE_HW_PORT_HDMI0 = 0,
	INPUT_SOURCE_HW_PORT_HDMI1,
	INPUT_SOURCE_HW_PORT_HDMI2,
	INPUT_SOURCE_HW_PORT_HDMI3,
	INPUT_SOURCE_HW_PORT_NONE = 0xff
};

struct earc_clock {
	struct clk *earc_audio2earc_dmacro;
	struct clk *earc_audio2earc;        //EN
	struct clk *earc_cm2earc;           //EN
	struct clk *earc_debounce2earc;     //EN
	struct clk *earc_dm_prbs2earc;      //EN
	struct clk *earc_atop_txpll_ck;     //FIX
	struct clk *earc_atop_audio_ck;     //FIX
	struct clk *hdmirx_earc_debounce_int_ck;   //MUX
	struct clk *hdmirx_earc_dm_prbs_int_ck;    //MUX
	struct clk *hdmirx_earc_cm_int_ck;         //MUX
	struct clk *hdmirx_earc_audio_int_ck;      //MUX
	struct clk *earc_earc_audio_int_ck;      //MUX
};

struct mtk_earc_hdmi_port_map {
	enum earc_hdmi_input_source_ui_port ui_port; //ui port
	enum earc_hdmi_input_source_hw_port hw_port; //phy port
};

struct mtk_earc_dev {
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct v4l2_m2m_dev *mdev;
	struct video_device *vdev;
	u16 dev_id;
	struct kobject *mtkdbg_kobj;
	struct kobject *rddbg_kobj;
	struct earc_clock st_earc_clk;
	struct v4l2_ctrl_handler earc_ctrl_handler;
	//struct earc_info earc_info;

	struct mutex mutex;
	struct clk *clk;
	spinlock_t lock;
	void __iomem	**reg_base;
	u64 *pIoremap;
	int earc_port_sel;
	int earc_bank_num;
	int earc_test;
	int earc_fixed_dd_index;
	int earc_support_mode;
	int earc_hw_ver_1;
	int earc_hw_ver_2;
	int hpd_en_ivt;
	u8 reg_info_offset;
	u8 reg_pa_idx;
	u8 reg_pa_size_idx;
	struct task_struct *earc_event_task;
	struct mtk_earc_info earc_info;
	struct mtk_earc_hdmi_port_map map[HDMI_PORT_MAX_NUM];
	//int irq_earc; //CY TODO
};

struct mtk_earc_ctx {
	struct v4l2_fh fh;
	struct mtk_earc_dev *earc_dev;
};

/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */

int mtk_earc_sysfs_init(struct mtk_earc_dev *earc_dev);
int mtk_earc_sysfs_deinit(struct mtk_earc_dev *earc_dev);

#endif  //#ifndef MTK_EARC_H

