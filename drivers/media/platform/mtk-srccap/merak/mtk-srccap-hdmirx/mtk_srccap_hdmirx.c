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
//#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <asm/siginfo.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/videodev2.h>
#include <linux/errno.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/mutex.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-subdev.h>

#include "linux/metadata_utility/m_srccap.h"
#include "mtk_srccap.h"
#include "mtk_srccap_hdmirx.h"
#include "mtk_srccap_hdmirx_phy.h"
#include "mtk_srccap_hdmirx_sysfs.h"
#include "hwreg_srccap_hdmirx_packetreceiver.h"
#include "hwreg_srccap_hdmirx_mux.h"
#include "hwreg_srccap_hdmirx_dsc.h"
#include "drv_HDMI.h"
#include "mtk-reserved-memory.h"

#include "hwreg_srccap_hdmirx.h"
#include "hwreg_srccap_hdmirx_hdcp.h"
#include "hwreg_srccap_hdmirx_phy.h"
#include "hwreg_srccap_hdmirx_mac.h"
#include "hwreg_srccap_hdmirx_irq.h"
#include "hwreg_srccap_hdmirx_edid.h"

#define DEF_READ_DONE_EVENT 1
#define DEF_TEST_USE 0
#if DEF_TEST_USE
#define DEF_TEST_CONST1 0x10
#define DEF_TEST_CONST2 0x11
#define DEF_TEST_CONST3 0x50
#define DEF_TEST_CONST4 0x2C
#define DEF_TEST_CONST5 0x15
#endif
static bool gHDMIRxInitFlag = FALSE;
static stHDMIRx_Bank stHDMIRxBank;
static struct srccap_port_map *pV4l2SrccapPortMap;
static DEFINE_MUTEX(hdmirx_mutex);
static DEFINE_MUTEX(hdmirx_subscribe_mutex);


static int priv_lock_id;
#define USE_STATIC_MEM  1

static struct srccap_hdmirx_metainfo _stPreHdmiMetaInfo[HDMI_PORT_MAX_NUM];

#define SUBSCRIBE_CTRL_EVT(dev, task, func, string, retval)	\
do {	\
	if ((task) == NULL) {	\
		task = kthread_create(func, dev, string);	\
		if (IS_ERR_OR_NULL(task))	\
			retval = -ENOMEM;	\
		else	\
			wake_up_process(task);	\
	}	\
} while (0)

#define UNSUBSCRIBE_CTRL_EVT(task) \
do { \
	if (!IS_ERR_OR_NULL(task)) { \
		kthread_stop(task); \
		task = NULL; \
	} \
} while (0)

#define VTEM_EVENT_BITMAP(event)\
(1<<(event - V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_VRR_EN_CHG))

#define IS_VTEM_EVENT_SUBSCRIBE(map, event)\
(map & VTEM_EVENT_BITMAP(event))

#define CHECK_META_VALUE_UPDATED(prefix, port, old, new, result)\
do { \
	if ((MS_U8)old != (MS_U8)new) { \
		HDMIRX_MSG_INFO("[META][%s]Port[%d] old{%d} -> new{%d}.", \
			prefix, port, (MS_U8)old, (MS_U8)new); \
		old = new; \
		result = TRUE; \
	} \
} while (0)

typedef  void (*funptr_mac_top_irq_handler) (void *);

static int _hdmirx_interrupt_register(
	struct mtk_srccap_dev *srccap_dev,
	struct platform_device *pdev);
static void _hdmirx_isr_handler_setup(struct srccap_hdmirx_interrupt *p_hdmi_int_s);
static int _hdmirx_enable(struct mtk_srccap_dev *srccap_dev);
static int _hdmirx_hw_init(struct mtk_srccap_dev *srccap_dev, int hw_version);
static int _hdmirx_hw_deinit(struct mtk_srccap_dev *srccap_dev);
static int _hdmirx_disable(struct mtk_srccap_dev *srccap_dev);
static void
_hdmirx_isr_handler_release(struct srccap_hdmirx_interrupt *p_hdmi_int_s);
static int _hdmirx_irq_suspend(struct mtk_srccap_dev *srccap_dev);
static int _hdmirx_irq_resume(struct mtk_srccap_dev *srccap_dev);


static void _irq_handler_mac_dec_misc_p0(void *);
static void _irq_handler_mac_dec_misc_p1(void *);
static void _irq_handler_mac_dec_misc_p2(void *);
static void _irq_handler_mac_dec_misc_p3(void *);
static void _irq_handler_mac_dep_misc_p0(void *);
static void _irq_handler_mac_dep_misc_p1(void *);
static void _irq_handler_mac_dep_misc_p2(void *);
static void _irq_handler_mac_dep_misc_p3(void *);
static void _irq_handler_mac_dtop_misc(void *);
static void _irq_handler_mac_inner_misc(void *);
static void _irq_handler_mac_dscd(void *);

static void _mtk_srccap_hdmirx_irq_save_restore(MS_U8 u8_save);

static funptr_mac_top_irq_handler _irq_handler_mac_top[E_MAC_TOP_ISR_EVT_N] = {
	_irq_handler_mac_dec_misc_p0,
	_irq_handler_mac_dec_misc_p1,
	_irq_handler_mac_dec_misc_p2,
	_irq_handler_mac_dec_misc_p3,
	_irq_handler_mac_dep_misc_p0,
	_irq_handler_mac_dep_misc_p1,
	_irq_handler_mac_dep_misc_p2,
	_irq_handler_mac_dep_misc_p3,
	_irq_handler_mac_dtop_misc,
	_irq_handler_mac_inner_misc,
	_irq_handler_mac_dscd
};


static bool isV4L2PortAsHDMI(enum v4l2_srccap_input_source src)
{
	bool isHDMI = false;

	if ((src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI &&
		 src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4) ||
		(src >= V4L2_SRCCAP_INPUT_SOURCE_DVI &&
		 src <= V4L2_SRCCAP_INPUT_SOURCE_DVI4)) {
		isHDMI = true;
	}
	return isHDMI;
}

static int get_memory_info(u64 *addr)
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
		HDMIRX_MSG_ERROR("can not find cpu_emi0_base info\n");
		of_node_put(target_memory_np);
		return -EINVAL;
	}
	return 0;
}

enum srccap_input_port
v4l2src_to_srccap_src(enum v4l2_srccap_input_source src)
{
	enum srccap_input_port eSrccapPort = SRCCAP_INPUT_PORT_NONE;

	if (isV4L2PortAsHDMI(src))
		eSrccapPort = pV4l2SrccapPortMap[src].data_port;
	return eSrccapPort;
}

E_MUX_INPUTPORT mtk_hdmirx_to_muxinputport(enum v4l2_srccap_input_source src)
{
	E_MUX_INPUTPORT ePort = INPUT_PORT_NONE_PORT;
	enum srccap_input_port eSrccapPort = SRCCAP_INPUT_PORT_NONE;

	if ((src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI) &&
		(src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4)) {
		eSrccapPort = v4l2src_to_srccap_src(src);

		switch (eSrccapPort) {
		case SRCCAP_INPUT_PORT_HDMI0:
			ePort = INPUT_PORT_DVI0;
			break;
		case SRCCAP_INPUT_PORT_HDMI1:
			ePort = INPUT_PORT_DVI1;
			break;
		case SRCCAP_INPUT_PORT_HDMI2:
			ePort = INPUT_PORT_DVI2;
			break;
		case SRCCAP_INPUT_PORT_HDMI3:
			ePort = INPUT_PORT_DVI3;
			break;
		default:
			ePort = INPUT_PORT_NONE_PORT;
			break;
		}
	} else if (
		(src >= V4L2_SRCCAP_INPUT_SOURCE_DVI) &&
		(src <= V4L2_SRCCAP_INPUT_SOURCE_DVI4)) {

		switch (src) {
		case V4L2_SRCCAP_INPUT_SOURCE_DVI:
			ePort = INPUT_PORT_DVI0;
			break;
		case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
			ePort = INPUT_PORT_DVI1;
			break;
		case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
			ePort = INPUT_PORT_DVI2;
			break;
		case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
			ePort = INPUT_PORT_DVI3;
			break;
		default:
			ePort = INPUT_PORT_NONE_PORT;
			break;
		}
	}
	return ePort;
}

static enum v4l2_srccap_input_source
srccap_src_to_v4l2src(enum srccap_input_port eSrccapPort)
{
	enum v4l2_srccap_input_source ePort = V4L2_SRCCAP_INPUT_SOURCE_NONE;
	MS_U8 eIdx = 0;

	if (gHDMIRxInitFlag) {
		for (eIdx = 0; (eIdx+V4L2_SRCCAP_INPUT_SOURCE_HDMI)
			<= V4L2_SRCCAP_INPUT_SOURCE_HDMI4; eIdx++) {
			if (pV4l2SrccapPortMap[eIdx+V4L2_SRCCAP_INPUT_SOURCE_HDMI].data_port
				== eSrccapPort) {
				ePort = eIdx + V4L2_SRCCAP_INPUT_SOURCE_HDMI;
				return ePort;
			}
		}
	} else
		HDMIRX_MSG_ERROR("mdbgerr_%s: need _hdmirx_hw_init and return source none\r\n",
						 __func__);

	return ePort;
}

enum v4l2_srccap_input_source mtk_hdmirx_muxinput_to_v4l2src(E_MUX_INPUTPORT src)
{
	enum srccap_input_port eSrccapPort;

	switch (src) {
	case INPUT_PORT_DVI0:
		eSrccapPort = SRCCAP_INPUT_PORT_HDMI0;
		break;
	case INPUT_PORT_DVI1:
		eSrccapPort = SRCCAP_INPUT_PORT_HDMI1;
		break;
	case INPUT_PORT_DVI2:
		eSrccapPort = SRCCAP_INPUT_PORT_HDMI2;
		break;
	case INPUT_PORT_DVI3:
		eSrccapPort = SRCCAP_INPUT_PORT_HDMI3;
		break;
	default:
		eSrccapPort = SRCCAP_INPUT_PORT_HDMI0;
		break;
	}

	return srccap_src_to_v4l2src(eSrccapPort);
}

static MS_HDMI_CONTENT_TYPE
mtk_hdmirx_Content_Type(enum v4l2_srccap_input_source src)
{
	MS_U8 u8ITCValue = 0;
	MS_U8 u8CN10Value = 0;
	enum v4l2_ext_hdmi_content_type enContentType = V4L2_EXT_HDMI_CONTENT_NoData;
	MS_U8 u8HDMIInfoSource = 0;

	if (isV4L2PortAsHDMI(src)) {
		E_MUX_INPUTPORT ePort = mtk_hdmirx_to_muxinputport(src);

		u8HDMIInfoSource = KDrv_SRCCAP_HDMIRx_mux_inport_2_src(ePort);

		if (u8HDMIInfoSource < HDMI_PORT_MAX_NUM) {
			if (MDrv_HDMI_Get_PacketStatus(ePort) &
				HDMI_STATUS_AVI_PACKET_RECEIVE_FLAG) {
				stHDMI_POLLING_INFO polling = MDrv_HDMI_Get_HDMIPollingInfo(ePort);

				u8ITCValue =
					KDrv_SRCCAP_HDMIRx_pkt_avi_infoframe_info(
						u8HDMIInfoSource, _BYTE_3, &polling) &
							 BIT(7);
				u8CN10Value =
					KDrv_SRCCAP_HDMIRx_pkt_avi_infoframe_info(
						u8HDMIInfoSource, _BYTE_5, &polling) &
							  BMASK(5 : 4);

				switch (u8CN10Value) {
				case 0x00:
					if (u8ITCValue > 0)
						enContentType = V4L2_EXT_HDMI_CONTENT_Graphics;
					else
						enContentType = V4L2_EXT_HDMI_CONTENT_NoData;
					break;

				case 0x10:
					enContentType = V4L2_EXT_HDMI_CONTENT_Photo;
					break;

				case 0x20:
					enContentType = V4L2_EXT_HDMI_CONTENT_Cinema;
					break;

				case 0x30:
					enContentType = V4L2_EXT_HDMI_CONTENT_Game;
					break;

				default:
					enContentType = V4L2_EXT_HDMI_CONTENT_NoData;
					break;
				};
			}
		}
	}

	return (MS_HDMI_CONTENT_TYPE)enContentType;
}

static MS_HDMI_COLOR_FORMAT
mtk_hdmirx_Get_ColorFormat(enum v4l2_srccap_input_source src)
{
	MS_U8 u8HDMIInfoSource = 0;
	MS_HDMI_COLOR_FORMAT enColorFormat = MS_HDMI_COLOR_UNKNOWN;

	if (isV4L2PortAsHDMI(src)) {
		E_MUX_INPUTPORT ePort = mtk_hdmirx_to_muxinputport(src);

		u8HDMIInfoSource = KDrv_SRCCAP_HDMIRx_mux_inport_2_src(ePort);

		if (u8HDMIInfoSource < HDMI_PORT_MAX_NUM) {
			stHDMI_POLLING_INFO polling = MDrv_HDMI_Get_HDMIPollingInfo(ePort);

			switch (KDrv_SRCCAP_HDMIRx_pkt_avi_infoframe_info(
						u8HDMIInfoSource, _BYTE_1, &polling) &
					0x60) {
			case 0x00:
				enColorFormat = MS_HDMI_COLOR_RGB;
				break;

			case 0x40:
				enColorFormat = MS_HDMI_COLOR_YUV_444;
				break;

			case 0x20:
				enColorFormat = MS_HDMI_COLOR_YUV_422;
				break;

			case 0x60:
				enColorFormat = MS_HDMI_COLOR_YUV_420;
				break;

			default:
				break;
			};
		}
	}

	return enColorFormat;
}

static void mtk_hdmirx_mode_change(
	struct mtk_srccap_dev *srccap_dev, __u8 u8Index,
	E_MUX_INPUTPORT enInputPortType)
{
	struct v4l2_event event;

	if (srccap_dev->hdmirx_info.bHDMIMode[u8Index] !=
		MDrv_HDMI_IsHDMI_Mode(enInputPortType)) {
		memset(&event, 0, sizeof(struct v4l2_event));
		srccap_dev->hdmirx_info.bHDMIMode[u8Index] =
			MDrv_HDMI_IsHDMI_Mode(enInputPortType);
		event.type = V4L2_EVENT_CTRL;
		event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_MODE;
		event.u.ctrl.changes =
			(srccap_dev->hdmirx_info.bHDMIMode[u8Index]
				? V4L2_EVENT_CTRL_CH_HDMI_RX_HDMI_MODE
				: V4L2_EVENT_CTRL_CH_HDMI_RX_DVI_MODE);
		event.u.ctrl.value = enInputPortType;
		v4l2_event_queue(srccap_dev->vdev, &event);
                HDMIRX_MSG_INFO("** HDMI HDMIMode Changed : %s, port %d!\r\n",
			(srccap_dev->hdmirx_info.bHDMIMode[u8Index]	? "HDMI" : "DVI"), u8Index);
	}
}

static void mtk_hdmirx_DEstatus_change(
	struct mtk_srccap_dev *srccap_dev, __u8 u8Index,
	E_MUX_INPUTPORT enInputPortType)
{
	struct v4l2_event event;

	if (srccap_dev->hdmirx_info.bDEStatus[u8Index] !=
		KDrv_SRCCAP_HDMIRx_mac_get_destable(
		enInputPortType)) {
		memset(&event, 0, sizeof(struct v4l2_event));
		srccap_dev->hdmirx_info.bDEStatus[u8Index] =
			KDrv_SRCCAP_HDMIRx_mac_get_destable(
			enInputPortType);
		event.type = V4L2_EVENT_CTRL;
		event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_DE_STATUS;
		event.u.ctrl.changes =
			(srccap_dev->hdmirx_info.bDEStatus[u8Index]
				? V4L2_EVENT_CTRL_CH_HDMI_RX_DE_STABLE
				: V4L2_EVENT_CTRL_CH_HDMI_RX_DE_UNSTABLE);
		event.u.ctrl.value = enInputPortType;
		v4l2_event_queue(srccap_dev->vdev, &event);
                HDMIRX_MSG_INFO("** HDMI DEStatus Changed : %s, port %d!\r\n",
			(srccap_dev->hdmirx_info.bDEStatus[u8Index]	? "Stable" : "Unstable"), u8Index);
	}
}

static void mtk_hdmirx_status_change(
	struct mtk_srccap_dev *srccap_dev, __u8 u8Index,
	E_MUX_INPUTPORT enInputPortType)
{
#define DEF_STABLE_CNT 10
#define DEF_TIMING_TOL 2

	bool bClkChg = 0;
	bool bStatusChg = 0;
	bool bClkStatus = 0;
	bool bHDMIStatus = 0;
	static u8 u8Cnt[HDMI_PORT_MAX_NUM] = {0};
	struct v4l2_event event;
	u32 u32HDE = 0;
	u32 u32VDE = 0;
	stHDMI_POLLING_INFO polling = MDrv_HDMI_Get_HDMIPollingInfo(enInputPortType);
	u8 u8HDMIInfoSource = KDrv_SRCCAP_HDMIRx_mux_inport_2_src(enInputPortType);
	enum v4l2_srccap_input_source src = mtk_hdmirx_muxinput_to_v4l2src(enInputPortType);

	u32HDE = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(E_HDMI_GET_HDE,
		u8HDMIInfoSource, &polling);
	u32VDE = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(E_HDMI_GET_VDE,
		u8HDMIInfoSource, &polling);

	if (src != V4L2_SRCCAP_INPUT_SOURCE_NONE) {
		bClkStatus = mtk_srccap_hdmirx_get_ClkStatus(src);
		if (srccap_dev->hdmirx_info.bClkStatus[u8Index] !=
			bClkStatus) {
			srccap_dev->hdmirx_info.bClkStatus[u8Index] =
				bClkStatus;
			//HDMIRX_MSG_INFO("P: %d, Clk change : %X !\r\n",
				//u8Index, srccap_dev->hdmirx_info.bClkStatus[u8Index]);
			bClkChg = 1;
		}

		bHDMIStatus = mtk_srccap_hdmirx_get_HDMIStatus(src);
		if (srccap_dev->hdmirx_info.bHDMIStatus[u8Index] !=
			bHDMIStatus) {
			srccap_dev->hdmirx_info.bHDMIStatus[u8Index] =
				bHDMIStatus;
			//HDMIRX_MSG_INFO("P: %d, HDMI Status change : %X !\r\n",
				//u8Index, srccap_dev->hdmirx_info.bHDMIStatus[u8Index]);
		}

		// status OK, need to check H/VDE
		if (srccap_dev->hdmirx_info.bHDMIStatus[u8Index]) {
			if ((ABS_MINUS(srccap_dev->hdmirx_info.u32HDE[u8Index],
				u32HDE) > DEF_TIMING_TOL) ||
				(ABS_MINUS(srccap_dev->hdmirx_info.u32VDE[u8Index],
				u32VDE) > DEF_TIMING_TOL)) {
				srccap_dev->hdmirx_info.u32HDE[u8Index] = u32HDE;
				srccap_dev->hdmirx_info.u32VDE[u8Index] = u32VDE;
				u8Cnt[u8Index] = 0;
				//HDMIRX_MSG_INFO("P: %d, HDE change : %X !\r\n",
					//u8Index, srccap_dev->hdmirx_info.u32HDE[u8Index]);
				//HDMIRX_MSG_INFO("P: %d, VDE change : %X !\r\n",
					//u8Index, srccap_dev->hdmirx_info.u32VDE[u8Index]);
			} else { // H/V DE not change for several times
				u8Cnt[u8Index]++;
				if (u8Cnt[u8Index] >= DEF_STABLE_CNT)
					u8Cnt[u8Index] = DEF_STABLE_CNT;
			}

			if (u8Cnt[u8Index] >= DEF_STABLE_CNT) {
				// unstable to stable
				if (srccap_dev->hdmirx_info.bStable[u8Index] == 0) {
					srccap_dev->hdmirx_info.bStable[u8Index] = 1;
					bStatusChg = 1; // trigger event
					//HDMIRX_MSG_INFO("P: %d, unstable to stable : %X!\r\n",
						//u8Index,
						//srccap_dev->hdmirx_info.bStable[u8Index]);
				}
			} else {
				srccap_dev->hdmirx_info.bStable[u8Index] = 0;
			}
		} else {
			srccap_dev->hdmirx_info.u32HDE[u8Index] = 0;
			srccap_dev->hdmirx_info.u32VDE[u8Index] = 0;
			u8Cnt[u8Index] = 0;
			if (srccap_dev->hdmirx_info.bStable[u8Index]) { // stable to unstable
				srccap_dev->hdmirx_info.bStable[u8Index] = 0;
				bStatusChg = 1; // trigger event
				//HDMIRX_MSG_INFO("P: %d, stable to unstable : %X!\r\n",
					//u8Index, srccap_dev->hdmirx_info.bStable[u8Index]);
			}
		}

		if (bClkChg) {
			memset(&event, 0, sizeof(struct v4l2_event));
			event.type = V4L2_EVENT_CTRL;
			event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_STATUS;
			event.u.ctrl.value = enInputPortType;
			if (!srccap_dev->hdmirx_info.bClkStatus[u8Index]) {
				event.u.ctrl.changes =
					V4L2_EVENT_CTRL_CH_HDMI_RX_HDMI_SIGNAL_LOST;
			//HDMIRX_MSG_INFO("P: %d, Clk Lost Event : %X !\r\n",
			//u8Index, srccap_dev->hdmirx_info.bClkStatus[u8Index]);
				v4l2_event_queue(srccap_dev->vdev, &event);
			} else {
				if (srccap_dev->hdmirx_info.bHDMIStatus[u8Index] &&
					srccap_dev->hdmirx_info.bStable[u8Index]) {
					event.u.ctrl.changes =
						V4L2_EVENT_CTRL_CH_HDMI_RX_HDMI_STABLE;
				//HDMIRX_MSG_INFO("P: %d, Clk & HDMI Stable Event : %X !\r\n",
				//u8Index, srccap_dev->hdmirx_info.bClkStatus[u8Index]);
					v4l2_event_queue(srccap_dev->vdev, &event);
				} else {
					event.u.ctrl.changes =
						V4L2_EVENT_CTRL_CH_HDMI_RX_HDMI_UNSTABLE;
				//HDMIRX_MSG_INFO("P: %d, Clk Stable but HDMI unStable : %X !\r\n",
				//u8Index, srccap_dev->hdmirx_info.bClkStatus[u8Index]);
					v4l2_event_queue(srccap_dev->vdev, &event);
				}
			}
		} else if (bStatusChg) {  // clk is 1 but status change
			memset(&event, 0, sizeof(struct v4l2_event));
			event.type = V4L2_EVENT_CTRL;
			event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_STATUS;
			event.u.ctrl.value = enInputPortType;
			if (srccap_dev->hdmirx_info.bHDMIStatus[u8Index] &&
				srccap_dev->hdmirx_info.bStable[u8Index]) {
				event.u.ctrl.changes =
					V4L2_EVENT_CTRL_CH_HDMI_RX_HDMI_STABLE;
				//HDMIRX_MSG_INFO("P: %d, HDMI Stable Event : %X !\r\n",
					//u8Index, srccap_dev->hdmirx_info.bHDMIStatus[u8Index]);
				v4l2_event_queue(srccap_dev->vdev, &event);
			} else {
				event.u.ctrl.changes =
					V4L2_EVENT_CTRL_CH_HDMI_RX_HDMI_UNSTABLE;
				//HDMIRX_MSG_INFO("P: %d, HDMI UnStable Event : %X !\r\n",
					//u8Index, srccap_dev->hdmirx_info.bHDMIStatus[u8Index]);
				v4l2_event_queue(srccap_dev->vdev, &event);
			}
		}
	}
}


static bool mtk_hdmirx_vrr_monitor(E_MUX_INPUTPORT enInputPortType,
	MS_HDMI_EXTEND_PACKET_RECEIVE_t *st_pk_status, struct v4l2_event *v4l2_event)
{
	static MS_U8 u8PreVrrEnStatus[HDMI_PORT_MAX_NUM] = {0};
	bool bReVal = FALSE;
	MS_U8 u8NowVrrEnStatus = 0;
	MS_U8 u8PortIndex = 0;
	stHDMI_POLLING_INFO *pstPollingInfo;

	pstPollingInfo = kmalloc(sizeof(stHDMI_POLLING_INFO), GFP_KERNEL);
	if (pstPollingInfo == NULL) {
		HDMIRX_MSG_ERROR("kmalloc Fail!\r\n");
		return FALSE;
	}

	*pstPollingInfo = MDrv_HDMI_Get_HDMIPollingInfo(enInputPortType);
	v4l2_event->u.ctrl.changes = 0;
	v4l2_event->type = V4L2_EVENT_CTRL;
	v4l2_event->u.ctrl.value = enInputPortType;
	v4l2_event->id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_VRR_EN_CHG;
	u8PortIndex = enInputPortType - INPUT_PORT_DVI0;

	if (st_pk_status->bPKT_EM_VTEM_RECEIVE)
		u8NowVrrEnStatus =
		((pstPollingInfo->u16EMPacketInfo) & HDMI_EM_PACKET_VRR_ENABLE) >> 1;
	else
		u8NowVrrEnStatus = 0;

	if (u8NowVrrEnStatus != u8PreVrrEnStatus[u8PortIndex]) {
		HDMIRX_MSG_INFO("** HDMI[%d] (Notify)vrr_en = %d->%d, VTEM_RECEIVE = %d\n",
					u8PortIndex, u8PreVrrEnStatus[u8PortIndex],
					u8NowVrrEnStatus, st_pk_status->bPKT_EM_VTEM_RECEIVE);
		v4l2_event->u.ctrl.changes = u8NowVrrEnStatus;
		u8PreVrrEnStatus[u8PortIndex] = u8NowVrrEnStatus;
		bReVal = TRUE;
	}

	kfree(pstPollingInfo);

	return bReVal;
}

enum v4l2_srccap_input_source mtk_hdmirx_search_hardware_port_num(int u8PortIndex)
{
	int cnt = 0;
	enum v4l2_srccap_input_source src;
	enum v4l2_srccap_input_source reval = V4L2_SRCCAP_INPUT_SOURCE_NONE;
	enum srccap_input_port eSrccapPort = SRCCAP_INPUT_PORT_NONE;

	for (cnt = 0; cnt < (INPUT_PORT_DVI_MAX-INPUT_PORT_DVI0); cnt++) {
		src = (enum v4l2_srccap_input_source)(
			V4L2_SRCCAP_INPUT_SOURCE_HDMI + cnt);
		eSrccapPort = v4l2src_to_srccap_src(src);
		if ((eSrccapPort - SRCCAP_INPUT_PORT_HDMI0) == u8PortIndex) {
			reval = src;
			break;
		}
	}

	return reval;
}

static bool mtk_hdmirx_no_vsif_pkt_monitor(struct mtk_srccap_dev *dev, int index)
{
	bool bReVal = FALSE;
	static __u8 u8PrePktReceiveCnt = HDMI_PACKET_RECEIVE_COUNT;
	__u8 u8PktReceiveCnt = HDMI_PACKET_RECEIVE_COUNT;
	__u32 u32VFrequency = mtk_hdmirx_get_datainfo(dev, E_HDMI_GET_PIXEL_CLOCK);
	__u32 u32HTTValue = mtk_hdmirx_get_datainfo(dev, E_HDMI_GET_HTT);
	__u32 u32VTTValue = mtk_hdmirx_get_datainfo(dev, E_HDMI_GET_VTT);

	if ((u32VFrequency > 0) && (u32HTTValue > 0) && (u32VTTValue > 0)) {
		u32VFrequency = (u32VFrequency * 1000) / u32HTTValue;
		u32VFrequency = u32VFrequency / u32VTTValue;

		if (u32VFrequency > 0) {
			u8PktReceiveCnt = ((1000 / u32VFrequency) + HDMI_POLLING_INTERVAL +
					HDMI_FRAME_RATE_OFFSET) / HDMI_POLLING_INTERVAL;
		}

		if (u8PktReceiveCnt > HDMI_PACKET_RECEIVE_COUNT)
			u8PktReceiveCnt = HDMI_PACKET_RECEIVE_COUNT;
	}

	if (u8PrePktReceiveCnt != u8PktReceiveCnt)
		u8PrePktReceiveCnt = u8PktReceiveCnt;

	if (dev->hdmirx_info.bNoHFVSIF[index] == TRUE) {
		if (dev->hdmirx_info.u32NoHFVSIFCnt[index] < u8PktReceiveCnt) {
			dev->hdmirx_info.u32NoHFVSIFCnt[index]++;
		} else {
			if (dev->hdmirx_info.u8ALLMMode[index]) {
				dev->hdmirx_info.u8ALLMMode[index] = 0;
				bReVal = TRUE;
			}
		}
	}

	return bReVal;
}

static int mtk_hdmirx_ctrl_packet_monitor_task(void *data)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	struct v4l2_event event;
	stCMD_HDMI_GET_PACKET_STATUS stGetPktStatus;
	MS_HDMI_EXTEND_PACKET_RECEIVE_t stPktStatus;
	stCMD_HDMI_GET_AVI_PARSING_INFO stGetAVI;
	stAVI_PARSING_INFO stAVIInfo;
	stCMD_HDMI_GET_GC_PARSING_INFO stGetGC;
	stGC_PARSING_INFO stGCInfo;
	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;
	struct hdmi_vsif_packet_info vsif_data[META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE];
	__u8 u8ALLMMode = 0;
	__u8 u8Index = 0;
	__u8 u8VSCount = 0;
	__u8 u8VSIndex = 0;
	enum v4l2_srccap_input_source src;

	memset(&stGetPktStatus, 0, sizeof(stCMD_HDMI_GET_PACKET_STATUS));
	memset(&stGetAVI, 0, sizeof(stCMD_HDMI_GET_AVI_PARSING_INFO));
	memset(vsif_data, 0, sizeof(vsif_data));
	memset(srccap_dev->hdmirx_info.AVI, 0xFF,
		   HDMI_PORT_MAX_NUM * sizeof(stAVI_PARSING_INFO));
	memset(srccap_dev->hdmirx_info.bAVMute, 0x00, HDMI_PORT_MAX_NUM * sizeof(bool));
	memset(srccap_dev->hdmirx_info.u8ALLMMode, 0x00, HDMI_PORT_MAX_NUM * sizeof(bool));

	stGetPktStatus.pExtendPacketReceive = &stPktStatus;
	stGetAVI.pstAVIParsingInfo = &stAVIInfo;
	stGetGC.pstGCParsingInfo = &stGCInfo;
	while (video_is_registered(srccap_dev->vdev)) {
		for (enInputPortType = INPUT_PORT_DVI0; enInputPortType < INPUT_PORT_DVI_MAX;
			 enInputPortType++) {

			u8Index = enInputPortType - INPUT_PORT_DVI0;
			src = mtk_hdmirx_search_hardware_port_num(u8Index);

			memset(&stPktStatus, 0, sizeof(MS_HDMI_EXTEND_PACKET_RECEIVE_t));
			memset(&stAVIInfo, 0, sizeof(stAVI_PARSING_INFO));
			memset(&stGCInfo, 0, sizeof(stGC_PARSING_INFO));
			memset(&event, 0, sizeof(struct v4l2_event));
			stGetPktStatus.enInputPortType = enInputPortType;
			stGetAVI.enInputPortType = enInputPortType;
			stGetGC.enInputPortType = enInputPortType;
			stPktStatus.u16Size = sizeof(MS_HDMI_EXTEND_PACKET_RECEIVE_t);


			if (MDrv_HDMI_Ctrl(E_HDMI_INTERFACE_CMD_GET_PACKET_STATUS,
							   &stGetPktStatus,
							   sizeof(stCMD_HDMI_GET_PACKET_STATUS))) {
				if (stPktStatus.bPKT_SPD_RECEIVE) {
					// HDMIRX_MSG_INFO("Receive SPD!\r\n");
					event.u.ctrl.changes |=
						V4L2_EVENT_CTRL_CH_HDMI_RX_RECEIVE_SPD;
				}
			}

			if (MDrv_HDMI_Ctrl(
							E_HDMI_INTERFACE_CMD_GET_GC_PARSING_INFO,
							&stGetGC,
							sizeof(stCMD_HDMI_GET_GC_PARSING_INFO))) {
				// HDMIRX_MSG_INFO("Receive GC!\r\n");
				if (srccap_dev->hdmirx_info.bAVMute[u8Index] !=
					stGCInfo.u8ControlAVMute) {
					// HDMIRX_MSG_INFO("AV Mute Changed : %d!\r\n",
					// stGCInfo.u8ControlAVMute);
                                        HDMIRX_MSG_INFO("** HDMI AV Mute Changed : %d, port %d!\r\n", stGCInfo.u8ControlAVMute, u8Index);
					srccap_dev->hdmirx_info.bAVMute[u8Index] =
						stGCInfo.u8ControlAVMute;
					event.u.ctrl.changes |=
						V4L2_EVENT_CTRL_CH_HDMI_RX_AVMUTE_CHANGE;
				}
			}

			if (MDrv_HDMI_Ctrl(
							E_HDMI_INTERFACE_CMD_GET_AVI_PARSING_INFO,
							&stGetAVI,
							sizeof(stCMD_HDMI_GET_AVI_PARSING_INFO))) {
				// HDMIRX_MSG_INFO("Receive AVI!\r\n");
				if ((srccap_dev->hdmirx_info.AVI[u8Index].enColorForamt !=
					 stAVIInfo.enColorForamt) |
					(srccap_dev->hdmirx_info.AVI[u8Index].enAspectRatio !=
					 stAVIInfo.enAspectRatio) |
					(srccap_dev->hdmirx_info.AVI[u8Index]
						 .enActiveFormatAspectRatio !=
					 stAVIInfo.enActiveFormatAspectRatio)) {
					// HDMIRX_MSG_INFO("AVI Changed!\r\n");
                                        HDMIRX_MSG_INFO("** HDMI AVI Changed, port %d!\r\n", u8Index);
					memcpy(&srccap_dev->hdmirx_info.AVI[u8Index], &stAVIInfo,
						   sizeof(stAVI_PARSING_INFO));
					event.u.ctrl.changes |=
						V4L2_EVENT_CTRL_CH_HDMI_RX_AVI_CHANGE;
				}
			}

			// fool-proof design for error hdmi port
			if (src == V4L2_SRCCAP_INPUT_SOURCE_NONE)
				continue;

			u8VSCount = mtk_srccap_hdmirx_pkt_get_VSIF(
				src, vsif_data,
				sizeof(struct hdmi_vsif_packet_info) *
					META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE);

			if (u8VSCount == 0) {
				srccap_dev->hdmirx_info.bNoHFVSIF[u8Index] = TRUE;
			}

			for (u8VSIndex = 0; u8VSIndex < u8VSCount; u8VSIndex++) {
				// HDMIRX_MSG_INFO("Receive VS!\r\n");

				if ((vsif_data[u8VSIndex].au8ieee[2] == 0xC4) &&
					(vsif_data[u8VSIndex].au8ieee[1] == 0x5D) &&
					(vsif_data[u8VSIndex].au8ieee[0] == 0xD8)) {
					if (vsif_data[u8VSIndex].au8payload[1] & BIT(1))
						u8ALLMMode = 1;
					else
						u8ALLMMode = 0;

					if (srccap_dev->hdmirx_info.u8ALLMMode[u8Index] !=
										u8ALLMMode) {
						srccap_dev->hdmirx_info.u8ALLMMode[u8Index] =
										u8ALLMMode;
						event.u.ctrl.changes |=
							V4L2_EVENT_CTRL_CH_HDMI_RX_ALLM_CHANGE;
					}
					srccap_dev->hdmirx_info.bNoHFVSIF[u8Index] = FALSE;
					srccap_dev->hdmirx_info.u32NoHFVSIFCnt[u8Index] = 0;
				} else {
					srccap_dev->hdmirx_info.bNoHFVSIF[u8Index] = TRUE;
				}
			}

			if (mtk_hdmirx_no_vsif_pkt_monitor(srccap_dev, u8Index)) {
				event.u.ctrl.changes |=
					V4L2_EVENT_CTRL_CH_HDMI_RX_ALLM_CHANGE;
			}

			if (event.u.ctrl.changes != 0) {
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_PKT_STATUS;
				event.u.ctrl.value = enInputPortType;
				v4l2_event_queue(srccap_dev->vdev, &event);
			}

			if ((srccap_dev->srccap_info.cap.u32HDMI_Srccap_HWVersion == 6) &&
			(mtk_hdmirx_vrr_monitor(enInputPortType, &stPktStatus, &event) == TRUE)) {
				v4l2_event_queue(srccap_dev->vdev, &event);
				HDMIRX_MSG_INFO("** HDMI notify HDMIRX_VTEM_VRR_EN_CHG\n");
			}
		}
		usleep_range(10000, 11000); // sleep 10ms

		if (kthread_should_stop()) {
			HDMIRX_MSG_ERROR("%s has been stopped.\n", __func__);

			return 0;
		}
	}

	return 0;
}

static int mtk_hdmirx_ctrl_cable_detect_monitor_task(void *data)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	struct v4l2_event event;
	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;
	__u8 u8Index = 0;

	set_freezable();
	while (video_is_registered(srccap_dev->vdev)) {
		for (enInputPortType = INPUT_PORT_DVI0; enInputPortType < INPUT_PORT_DVI_MAX;
			 enInputPortType++) {
			u8Index = enInputPortType - INPUT_PORT_DVI0;
			if (srccap_dev->hdmirx_info.bCableDetect[u8Index] !=
				KDrv_SRCCAP_HDMIRx_mac_get_scdccabledet(enInputPortType)) {
				srccap_dev->hdmirx_info.bCableDetect[u8Index] =
					KDrv_SRCCAP_HDMIRx_mac_get_scdccabledet(
						enInputPortType);
				HDMIRX_MSG_INFO("** HDMI port %d, 5V status = %d\r\n",
				u8Index, srccap_dev->hdmirx_info.bCableDetect[u8Index]);
				memset(&event, 0, sizeof(struct v4l2_event));
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_CABLE_DETECT;
				event.u.ctrl.changes =
					(srccap_dev->hdmirx_info.bCableDetect[u8Index]
						 ? V4L2_EVENT_CTRL_CH_HDMI_RX_CABLE_CONNECT
						 : V4L2_EVENT_CTRL_CH_HDMI_RX_CABLE_DISCONNECT);
				event.u.ctrl.value = enInputPortType;
				v4l2_event_queue(srccap_dev->vdev, &event);
			}
		}
		usleep_range(10000, 11000); // sleep 10ms

		try_to_freeze();
		if (kthread_should_stop()) {
			HDMIRX_MSG_ERROR("%s has been stopped.\n", __func__);

			return 0;
		}
	}

	return 0;
}

static int mtk_hdmirx_ctrl_hdcp2x_irq_monitor_task(void *data)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	struct v4l2_event event;
	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;

	set_freezable();
	while (video_is_registered(srccap_dev->vdev)) {
		for (enInputPortType = 0; enInputPortType < HDMI_PORT_MAX_NUM;
			 enInputPortType++) {
			if (KDrv_SRCCAP_HDMIRx_hdcp_22_polling_writedone(enInputPortType)) {
				// HDMIRX_MSG_ERROR("Get Write Done Status %d\r\n",
				// enInputPortType);
				memset(&event, 0, sizeof(struct v4l2_event));
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP2X_STATUS;
				event.u.ctrl.changes = V4L2_EVENT_CTRL_CH_HDMI_RX_HDCP2X_WRITE_DONE;
				event.u.ctrl.value = enInputPortType + INPUT_PORT_DVI0;
				v4l2_event_queue(srccap_dev->vdev, &event);
			}
#if DEF_READ_DONE_EVENT
			if (KDrv_SRCCAP_HDMIRx_hdcp_22_polling_readdone(enInputPortType)) {
				// HDMIRX_MSG_ERROR("Get Read Done Status %d\r\n",
				// enInputPortType);
				memset(&event, 0, sizeof(struct v4l2_event));
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP2X_STATUS;
				event.u.ctrl.changes = V4L2_EVENT_CTRL_CH_HDMI_RX_HDCP2X_READ_DONE;
				event.u.ctrl.value = enInputPortType + INPUT_PORT_DVI0;
				v4l2_event_queue(srccap_dev->vdev, &event);
			}
#endif
		}
		usleep_range(100, 110); // sleep 100us

		try_to_freeze();
		if (kthread_should_stop()) {
			HDMIRX_MSG_ERROR("%s has been stopped.\n", __func__);

			return 0;
		}
	}

	return 0;
}

static int mtk_hdmirx_ctrl_hdcp1x_irq_monitor_task(void *data)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	struct v4l2_event event;
	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;

	set_freezable();
	while (video_is_registered(srccap_dev->vdev)) {
		for (enInputPortType = 0; enInputPortType < HDMI_PORT_MAX_NUM;
			 enInputPortType++) {
			if (KDrv_SRCCAP_HDMIRx_hdcp_14_polling_r0_readdone(
					enInputPortType)) {
				memset(&event, 0, sizeof(struct v4l2_event));
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP1X_STATUS;
				event.u.ctrl.value = enInputPortType + INPUT_PORT_DVI0;
				v4l2_event_queue(srccap_dev->vdev, &event);
			}
		}
		usleep_range(100, 110); // sleep 100us

		try_to_freeze();
		if (kthread_should_stop()) {
			HDMIRX_MSG_ERROR("%s has been stopped.\n", __func__);

			return 0;
		}
	}

	return 0;
}

static int mtk_hdmirx_ctrl_hdmi_crc_monitor_task(void *data)
{
#define crc_sleep_min 20000
#define crc_sleep_max 21000
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	struct v4l2_event event;
	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;
	MS_DVI_CHANNEL_TYPE enChannelType = MS_DVI_CHANNEL_R;
	unsigned short usCrc = 0;
	__u8 u8Index = 0;
	__u8 u8Type = 0;

	set_freezable();
	while (video_is_registered(srccap_dev->vdev)) {
		for (enInputPortType = INPUT_PORT_DVI0; enInputPortType < INPUT_PORT_DVI_MAX;
			 enInputPortType++) {
			for (enChannelType = MS_DVI_CHANNEL_R; enChannelType <= MS_DVI_CHANNEL_B;
				 enChannelType++) {
				u8Index = enInputPortType - INPUT_PORT_DVI0;
				u8Type = enChannelType - MS_DVI_CHANNEL_R;
				if (MDrv_HDMI_GetCRCValue(enInputPortType, enChannelType, &usCrc)) {
					if (srccap_dev->hdmirx_info.usHDMICrc[u8Index][u8Type] !=
						usCrc) {
						memset(&event, 0, sizeof(struct v4l2_event));
						event.type = V4L2_EVENT_CTRL;
						event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_CRC;
						event.u.ctrl.value = enInputPortType;
						srccap_dev->hdmirx_info.usHDMICrc[u8Index][u8Type]
							 = usCrc;
						v4l2_event_queue(srccap_dev->vdev, &event);
					}
				}
			}
		}
		usleep_range(crc_sleep_min, crc_sleep_max);

		try_to_freeze();
		if (kthread_should_stop()) {
			HDMIRX_MSG_ERROR("%s has been stopped.\n", __func__);
			return 0;
		}
	}

	return 0;
}

static int mtk_hdmirx_ctrl_status_monitor_task(void *data)
{
#define hdmi_status_sleep_min 20000
#define hdmi_status_sleep_max 21000
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;
	__u8 u8Index = 0;

	set_freezable();
	while (video_is_registered(srccap_dev->vdev)) {
		for (enInputPortType = INPUT_PORT_DVI0; enInputPortType < INPUT_PORT_DVI_MAX;
			 enInputPortType++) {
			u8Index = enInputPortType - INPUT_PORT_DVI0;
			if (srccap_dev->hdmirx_info.u32MonitorEvent &
				V4L2_SRCCAP_HDMIRX_MONITOR_HDMIMODE)
				mtk_hdmirx_mode_change(srccap_dev, u8Index, enInputPortType);

			if (srccap_dev->hdmirx_info.u32MonitorEvent &
				V4L2_SRCCAP_HDMIRX_MONITOR_DESTATUS)
				mtk_hdmirx_DEstatus_change(srccap_dev, u8Index, enInputPortType);

			if (srccap_dev->hdmirx_info.u32MonitorEvent &
				V4L2_SRCCAP_HDMIRX_MONITOR_HDMISTATUS)
				mtk_hdmirx_status_change(srccap_dev, u8Index, enInputPortType);
		}
		usleep_range(hdmi_status_sleep_min, hdmi_status_sleep_max);

		try_to_freeze();
		if (kthread_should_stop()) {
			HDMIRX_MSG_ERROR("%s has been stopped.\n", __func__);

			return 0;
		}
	}

	return 0;
}

static int mtk_hdmirx_ctrl_hdcp_state_monitor_task(void *data)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	struct v4l2_event event;
	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;
	__u8 u8Index = 0;
	__u8 u8HDCPState = 0;

	set_freezable();
	memset(srccap_dev->hdmirx_info.ucHDCPState, 0x00,
		   HDMI_PORT_MAX_NUM * sizeof(unsigned char));
	while (video_is_registered(srccap_dev->vdev)) {
		for (enInputPortType = INPUT_PORT_DVI0; enInputPortType < INPUT_PORT_DVI_MAX;
			 enInputPortType++) {
			u8Index = enInputPortType - INPUT_PORT_DVI0;
			u8HDCPState = MDrv_HDMI_CheckHDCPENCState(enInputPortType);
			if (srccap_dev->hdmirx_info.ucHDCPState[u8Index] != u8HDCPState) {
				memset(&event, 0, sizeof(struct v4l2_event));
				srccap_dev->hdmirx_info.ucHDCPState[u8Index] = u8HDCPState;
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP_STATE;
				switch (srccap_dev->hdmirx_info.ucHDCPState[u8Index]) {
				case 0:
				default:
					event.u.ctrl.changes = V4L2_EXT_HDMI_HDCP_NO_ENCRYPTION;
					break;
				case 1:
					event.u.ctrl.changes = V4L2_EXT_HDMI_HDCP_1_4;
					break;
				case 2:
					event.u.ctrl.changes = V4L2_EXT_HDMI_HDCP_2_2;
					break;
				}
				event.u.ctrl.value = enInputPortType;
				v4l2_event_queue(srccap_dev->vdev, &event);
			}
		}
		usleep_range(100000, 110000); // sleep 100ms

		try_to_freeze();
		if (kthread_should_stop()) {
			HDMIRX_MSG_ERROR("%s has been stopped.\n", __func__);
			return 0;
		}
	}

	return 0;
}

static int mtk_hdmirx_ctrl_dsc_state_monitor_task(void *data)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	struct v4l2_event event;
	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;
	__u8 u8Index = 0;
	__u8 u8_tmp = 0;
	__u8 u8_diff = 0;

	set_freezable();

	if ((srccap_dev->srccap_info.cap.u32HDMI_Srccap_HWVersion != 1) &&
	(srccap_dev->srccap_info.cap.u32HDMI_Srccap_HWVersion != 4))
		return 0;

	memset(srccap_dev->hdmirx_info.ucDSCState, 0x00,
		   HDMI_PORT_MAX_NUM * sizeof(unsigned char));
	while (video_is_registered(srccap_dev->vdev)) {
		u8_diff = 0;
		for (enInputPortType = INPUT_PORT_DVI0; enInputPortType < INPUT_PORT_DVI_MAX;
			 enInputPortType++) {
			if (mtk_hdmirx_muxinput_to_v4l2src(enInputPortType) ==
				V4L2_SRCCAP_INPUT_SOURCE_NONE)
				break;
			u8Index = enInputPortType - INPUT_PORT_DVI0;
			u8_tmp = mtk_srccap_hdmirx_pkt_is_dsc(
				mtk_hdmirx_muxinput_to_v4l2src(enInputPortType));
			if (srccap_dev->hdmirx_info.ucDSCState[u8Index] != u8_tmp) {
				u8_diff |= (1 << u8Index);
				srccap_dev->hdmirx_info.ucDSCState[u8Index] = u8_tmp;
			}
			if (u8_tmp && u8_diff) {
				KDrv_SRCCAP_HDMIRx_mux_sel_dsc(enInputPortType, E_DSC_D0);
				break;
			}
		}
		if (u8_diff) {
			event.type = V4L2_EVENT_CTRL;
			event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_DSC_STATE;
			event.u.ctrl.changes = V4L2_EXT_HDMI_DSC_PKT_RECV_STATE_CHG;

			memcpy((void *)&event.u.ctrl.value,
				   (void *)&srccap_dev->hdmirx_info.ucDSCState[0],
				   min(sizeof(event.u.ctrl.value),
					   HDMI_PORT_MAX_NUM * sizeof(unsigned char)));
			v4l2_event_queue(srccap_dev->vdev, &event);
		}
		usleep_range(10000, 11000); // sleep 10ms

		try_to_freeze();
		if (kthread_should_stop()) {
			HDMIRX_MSG_ERROR("%s has been stopped.\n", __func__);
			return 0;
		}
	}

	return 0;
}

static int mtk_hdmirx_ctrl_vtem_info_monitor_task(
	void *data,
	E_MUX_INPUTPORT enInputPortType)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)data;
	struct srccap_hdmirx_info *p_hdmirx_info = &srccap_dev->hdmirx_info;

	struct s_pkt_emp_vtem *p_emp_vtem_prev
= &p_hdmirx_info->emp_vtem_info[enInputPortType - INPUT_PORT_DVI0];

	struct v4l2_event event;

	struct s_pkt_emp_vtem st_pkt_emp_vtem;

	MS_U32 u32_bitmap = p_hdmirx_info->u32Emp_vtem_info_subscribe_bitmap;

	enum v4l2_ext_hdmi_event_id v4l2_id
		= V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_VRR_EN_CHG;

	if (!u32_bitmap)
		return 0;

	memset((void *)&st_pkt_emp_vtem, 0,
		sizeof(struct s_pkt_emp_vtem));

	KDrv_SRCCAP_HDMIRx_pkt_get_emp_vtem_info(
		enInputPortType, &st_pkt_emp_vtem);

	if (memcmp((void *)&st_pkt_emp_vtem,
		(void *)p_emp_vtem_prev,
		sizeof(struct s_pkt_emp_vtem))) {
		// content diff

		event.type = V4L2_EVENT_CTRL;
		event.u.ctrl.value = enInputPortType;

		if (srccap_dev->srccap_info.cap.u32HDMI_Srccap_HWVersion != 6) {
			v4l2_id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_VRR_EN_CHG;
			if (st_pkt_emp_vtem.vrr_en
				!= p_emp_vtem_prev->vrr_en
			&& IS_VTEM_EVENT_SUBSCRIBE(u32_bitmap, v4l2_id)) {
				event.id = v4l2_id;
				event.u.ctrl.changes =
					st_pkt_emp_vtem.vrr_en;
				v4l2_event_queue(srccap_dev->vdev, &event);
			}
		}

		v4l2_id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_M_CONST_CHG;
		if (st_pkt_emp_vtem.m_const
			!= p_emp_vtem_prev->m_const
		&& IS_VTEM_EVENT_SUBSCRIBE(u32_bitmap, v4l2_id)) {
			event.id = v4l2_id;
			event.u.ctrl.changes =
				st_pkt_emp_vtem.m_const;
			v4l2_event_queue(srccap_dev->vdev, &event);
		}

		v4l2_id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_QMS_EN_CHG;
		if (st_pkt_emp_vtem.qms_en
			!= p_emp_vtem_prev->qms_en
		&& IS_VTEM_EVENT_SUBSCRIBE(u32_bitmap, v4l2_id)) {
			event.id = v4l2_id;
			event.u.ctrl.changes =
				st_pkt_emp_vtem.qms_en;
			v4l2_event_queue(srccap_dev->vdev, &event);
		}

		v4l2_id =
		V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_FVA_FACTOR_M1_CHG;
		if (st_pkt_emp_vtem.fva_factor_m1
			!= p_emp_vtem_prev->fva_factor_m1
		&& IS_VTEM_EVENT_SUBSCRIBE(u32_bitmap, v4l2_id)) {
			event.id = v4l2_id;
			event.u.ctrl.changes =
				st_pkt_emp_vtem.fva_factor_m1;
			v4l2_event_queue(srccap_dev->vdev, &event);
		}

		v4l2_id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_NEXT_TFR_CHG;
		if (st_pkt_emp_vtem.next_tfr
			!= p_emp_vtem_prev->next_tfr
		&& IS_VTEM_EVENT_SUBSCRIBE(u32_bitmap, v4l2_id)) {
			event.id = v4l2_id;
			event.u.ctrl.changes =
			(enum E_HDMI_NEXT_TFR)st_pkt_emp_vtem.next_tfr;
			v4l2_event_queue(srccap_dev->vdev, &event);
		}
	}

	memcpy((void *)p_emp_vtem_prev, (void *)&st_pkt_emp_vtem,
		sizeof(struct s_pkt_emp_vtem));
	return 0;
}

static EN_KDRV_HDMIRX_INT _mtk_hdmirx_getKDrvInt(enum srccap_irq_interrupts e_int)
{
	EN_KDRV_HDMIRX_INT e_ret = HDMI_IRQ_PHY;

	switch (e_int) {
	case SRCCAP_HDMI_IRQ_PHY:
		e_ret = HDMI_IRQ_PHY;
		break;

	case SRCCAP_HDMI_IRQ_MAC:
		e_ret = HDMI_IRQ_MAC;
		break;

	case SRCCAP_HDMI_IRQ_PKT_QUE:
		e_ret = HDMI_IRQ_PKT_QUE;
		break;

	case SRCCAP_HDMI_IRQ_PM_SQH_ALL_WK:
		e_ret = HDMI_IRQ_PM_SQH_ALL_WK;
		break;

	case SRCCAP_HDMI_IRQ_PM_SCDC:
		e_ret = HDMI_IRQ_PM_SCDC;
		break;

	case SRCCAP_HDMI_FIQ_CLK_DET_0:
		e_ret = HDMI_FIQ_CLK_DET_0;
		break;

	case SRCCAP_HDMI_FIQ_CLK_DET_1:
		e_ret = HDMI_FIQ_CLK_DET_1;
		break;

	case SRCCAP_HDMI_FIQ_CLK_DET_2:
		e_ret = HDMI_FIQ_CLK_DET_2;
		break;

	case SRCCAP_HDMI_FIQ_CLK_DET_3:
		e_ret = HDMI_FIQ_CLK_DET_3;
		break;

	default:
		HDMIRX_MSG_ERROR("Wring e_int[%d] !\r\n", e_int);
		e_ret = HDMI_IRQ_PHY;
		break;
	}

	return e_ret;
}

static void mtk_hdmirx_hdcp_isr(int irq, void *priv)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)priv;
	struct v4l2_event event;
	unsigned char ucHDCPWriteDoneIndex = 0;
	unsigned char ucHDCPReadDoneIndex = 0;
	unsigned char ucHDCPR0ReadDoneIndex = 0;
	unsigned char ucVersion = 0;

	ucVersion = MDrv_HDMI_HDCP_ISR(&ucHDCPWriteDoneIndex, &ucHDCPReadDoneIndex,
								   &ucHDCPR0ReadDoneIndex);

	if (ucVersion > 0) {
		E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;
		MS_U8 ucPortIndex = 0;

		memset(&event, 0, sizeof(struct v4l2_event));
		event.type = V4L2_EVENT_CTRL;

		for (enInputPortType = INPUT_PORT_DVI0;
			 enInputPortType <= INPUT_PORT_DVI_END; enInputPortType++) {
			ucPortIndex = HDMI_GET_PORT_SELECT(enInputPortType);
			// ucVersion = E_HDCP2X_WRITE_DONE
			if ((ucHDCPWriteDoneIndex & BIT(ucPortIndex)) != 0) {
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP2X_STATUS;
				event.u.ctrl.changes = V4L2_EVENT_CTRL_CH_HDMI_RX_HDCP2X_WRITE_DONE;
				event.u.ctrl.value = enInputPortType;
				v4l2_event_queue(srccap_dev->vdev, &event);
				// HDMIRX_MSG_ERROR("Get Write Done Status ISR \r\n");
			}
#if DEF_READ_DONE_EVENT
			// ucVersion = E_HDCP2X_READ_DONE
			if ((ucHDCPReadDoneIndex & BIT(ucPortIndex)) != 0) {
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP2X_STATUS;
				event.u.ctrl.changes = V4L2_EVENT_CTRL_CH_HDMI_RX_HDCP2X_READ_DONE;
				event.u.ctrl.value = enInputPortType;
				v4l2_event_queue(srccap_dev->vdev, &event);
				// HDMIRX_MSG_ERROR("Get Read Done Status ISR \r\n");
			}
#endif
			// ucVersion = E_HDCP1X_R0READ_DONE
			if ((ucHDCPR0ReadDoneIndex & BIT(ucPortIndex)) != 0) {
				event.id = V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP1X_STATUS;
				event.u.ctrl.changes = 0;
				event.u.ctrl.value = enInputPortType;
				v4l2_event_queue(srccap_dev->vdev, &event);
				// HDMIRX_MSG_ERROR("Get R0 Read Done Status ISR \r\n");
			}
		}
	}
}

int mtk_srccap_hdmirx_init(struct mtk_srccap_dev *srccap_dev)
{
	HDMIRX_MSG_DBG("mdbgin_%s :  start\r\n", __func__);
	HDMIRX_MSG_INFO("%s HDMI INIT!!.\n", __func__);

	return 0;
}

int mtk_srccap_hdmirx_release(struct mtk_srccap_dev *srccap_dev)
{
	HDMIRX_MSG_DBG("mdbgin_%s: start \r\n", __func__);
	HDMIRX_MSG_INFO("%s HDMI Release!!.\n", __func__);

	if (mutex_is_locked(&hdmirx_mutex)) {
		HDMIRX_MSG_INFO("%s unlock hdmirx_mutex\n", __func__);
		mutex_unlock(&hdmirx_mutex);
	}

	return 0;
}

static int _hdmirx_parse_dts(struct mtk_srccap_dev *srccap_dev,
							 struct platform_device *pdev)
{
	int ret = 0;
	struct device *property_dev = &pdev->dev;

	HDMIRX_MSG_DBG("mdbgin_%s :  start\r\n", __func__);
	HDMIRX_MSG_INFO("%s HDMI INIT!!.\n", __func__);

	/* parse register base */
	ret = mtk_srccap_hdmirx_parse_dts_reg(srccap_dev, property_dev);

	return ret;
}

int mtk_hdmirx_init(struct mtk_srccap_dev *srccap_dev, struct platform_device *pdev,
					int logLevel)
{
	int ret = 0;

	//hdmirxloglevel = LOG_LEVEL_DEBUG;
	KDrv_SRCCAP_HDMIRx_setloglevel(hdmirxloglevel);

	HDMIRX_MSG_DBG("mdbgin_%s :  start\r\n", __func__);
	HDMIRX_MSG_INFO("%s HDMI INIT!!.\n", __func__);

	ret = _hdmirx_parse_dts(srccap_dev, pdev);

	// mokona remove
	if (srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_4
		&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_5
		&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_6)
		ret = _hdmirx_enable(srccap_dev);
	ret = _hdmirx_hw_init(srccap_dev,
	srccap_dev->srccap_info.cap.u32HDMI_Srccap_HWVersion);

	_hdmirx_isr_handler_setup(&srccap_dev->hdmirx_info.hdmi_int_s);
	/* parse irq info and register irq */
	ret = _hdmirx_interrupt_register(srccap_dev, pdev);

	ret = mtk_srccap_hdmirx_sysfs_init(srccap_dev);

	memset(&_stPreHdmiMetaInfo, 0x0, sizeof(struct srccap_hdmirx_metainfo) * HDMI_PORT_MAX_NUM);

	return ret;
}

int mtk_hdmirx_deinit(struct mtk_srccap_dev *srccap_dev)
{
	HDMIRX_MSG_DBG("mdbgin_%s: start \r\n", __func__);
	HDMIRX_MSG_INFO("%s HDMI Release!!.\n", __func__);

	mtk_srccap_hdmirx_sysfs_deinit(srccap_dev);
	_hdmirx_hw_deinit(srccap_dev);
	// mokona remove
	if (srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_4
		&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_5
		&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_6)
		_hdmirx_disable(srccap_dev);
	_hdmirx_isr_handler_release(&srccap_dev->hdmirx_info.hdmi_int_s);

	if (mutex_is_locked(&hdmirx_mutex))
		mutex_unlock(&hdmirx_mutex);

	return 0;
}

int mtk_hdmirx_subscribe_ctrl_event(
		struct mtk_srccap_dev *srccap_dev,
		const struct v4l2_event_subscription *event_sub)
{
	int retval = 0;
	unsigned int u32_i = 0;
	struct srccap_hdmirx_info *p_hdmirx_info = &srccap_dev->hdmirx_info;

	HDMIRX_MSG_DBG("mdbgin_%s: start \r\n", __func__);
	if (event_sub == NULL) {
		HDMIRX_MSG_ERROR("NULL Event !\r\n");
		HDMIRX_MSG_ERROR("mdbgerr_ %s:  %d \r\n", __func__, -ENXIO);
		return -ENXIO;
	}

	mutex_lock(&hdmirx_subscribe_mutex);

	switch (event_sub->id) {
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_PKT_STATUS:
		// HDMIRX_MSG_INFO("Subscribe PKT_STATUS !\r\n");
		if (srccap_dev->hdmirx_info.ctrl_packet_monitor_task == NULL) {
			srccap_dev->hdmirx_info.ctrl_packet_monitor_task =
				kthread_create(mtk_hdmirx_ctrl_packet_monitor_task, srccap_dev,
							   "hdmi ctrl_packet_monitor_task");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.ctrl_packet_monitor_task)) {
				HDMIRX_MSG_ERROR("mdbgerr_ %s:  %d \r\n", __func__, -ENOMEM);
				mutex_unlock(&hdmirx_subscribe_mutex);
				return -ENOMEM;
			}
			wake_up_process(srccap_dev->hdmirx_info.ctrl_packet_monitor_task);
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_CABLE_DETECT:
		// HDMIRX_MSG_INFO("Subscribe CABLE_DETECT !\r\n");
		if (srccap_dev->hdmirx_info.ctrl_5v_detect_monitor_task == NULL) {
			memset(srccap_dev->hdmirx_info.bCableDetect, 0x00,
				   sizeof(srccap_dev->hdmirx_info.bCableDetect));
			srccap_dev->hdmirx_info.ctrl_5v_detect_monitor_task =
				kthread_create(
					mtk_hdmirx_ctrl_cable_detect_monitor_task,
					srccap_dev,
					"hdmi ctrl_5v_detect_monitor_task");
			if (IS_ERR_OR_NULL(
					srccap_dev->hdmirx_info.ctrl_5v_detect_monitor_task)) {
				HDMIRX_MSG_ERROR("mdbgerr_%s:  %d\r\n", __func__, -ENOMEM);
				mutex_unlock(&hdmirx_subscribe_mutex);
				return -ENOMEM;
			}
			wake_up_process(srccap_dev->hdmirx_info.ctrl_5v_detect_monitor_task);
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP2X_STATUS:
		// HDMIRX_MSG_INFO("Subscribe HDCP2X_STATUS !\r\n");
		if (!srccap_dev->hdmirx_info.ubHDCP22ISREnable) {
			// HDMIRX_MSG_ERROR("HDCP22 Status Thread Create !\r\n");
			if (srccap_dev->hdmirx_info.ctrl_hdcp2x_irq_monitor_task == NULL) {
				srccap_dev->hdmirx_info.ctrl_hdcp2x_irq_monitor_task =
					kthread_create(
						mtk_hdmirx_ctrl_hdcp2x_irq_monitor_task,
						srccap_dev,
						"hdmi ctrl_hdcp2x_irq_monitor_task");
				if (IS_ERR_OR_NULL(
					srccap_dev->hdmirx_info.ctrl_hdcp2x_irq_monitor_task)) {
					mutex_unlock(&hdmirx_subscribe_mutex);
					return -ENOMEM;
				}
				wake_up_process(
					srccap_dev->hdmirx_info.ctrl_hdcp2x_irq_monitor_task);
			}
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP1X_STATUS:
		// HDMIRX_MSG_INFO("Subscribe HDCP1X_STATUS !\r\n");
		if (!srccap_dev->hdmirx_info.ubHDCP14ISREnable) {
			// HDMIRX_MSG_ERROR("HDCP14 Status Thread Create !\r\n");
			if (srccap_dev->hdmirx_info.ctrl_hdcp1x_irq_monitor_task == NULL) {
				srccap_dev->hdmirx_info.ctrl_hdcp1x_irq_monitor_task =
					kthread_create(
						mtk_hdmirx_ctrl_hdcp1x_irq_monitor_task,
						srccap_dev,
						"hdmi ctrl_hdcp1x_irq_monitor_task");
				if (IS_ERR_OR_NULL(
					srccap_dev->hdmirx_info.ctrl_hdcp1x_irq_monitor_task)) {
					mutex_unlock(&hdmirx_subscribe_mutex);
					return -ENOMEM;
				}
				wake_up_process(
					srccap_dev->hdmirx_info.ctrl_hdcp1x_irq_monitor_task);
			}
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_CRC:
		// HDMIRX_MSG_INFO("Subscribe HDMI_CRC !\r\n");
		if (srccap_dev->hdmirx_info.ctrl_hdmi_crc_monitor_task == NULL) {
			srccap_dev->hdmirx_info.ctrl_hdmi_crc_monitor_task =
				kthread_create(mtk_hdmirx_ctrl_hdmi_crc_monitor_task, srccap_dev,
							   "hdmi ctrl_hdmi_crc_monitor_task");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.ctrl_hdmi_crc_monitor_task)) {
				mutex_unlock(&hdmirx_subscribe_mutex);
				return -ENOMEM;
			}
			wake_up_process(srccap_dev->hdmirx_info.ctrl_hdmi_crc_monitor_task);
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_MODE:
		// HDMIRX_MSG_INFO("Subscribe HDMI_MODE !\r\n");
		if (!(srccap_dev->hdmirx_info.u32MonitorEvent &
			V4L2_SRCCAP_HDMIRX_MONITOR_HDMIMODE)) {
			memset(srccap_dev->hdmirx_info.bHDMIMode, 0x00,
				   sizeof(srccap_dev->hdmirx_info.bHDMIMode));
			srccap_dev->hdmirx_info.u32MonitorEvent |=
				V4L2_SRCCAP_HDMIRX_MONITOR_HDMIMODE;
		}
		if (srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task == NULL) {
			srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task =
				kthread_create(mtk_hdmirx_ctrl_status_monitor_task, srccap_dev,
							   "hdmi ctrl_status_monitor_task");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task)) {
				mutex_unlock(&hdmirx_subscribe_mutex);
				return -ENOMEM;
			}
			wake_up_process(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task);
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP_STATE:
		// HDMIRX_MSG_INFO("Subscribe HDCP_STATE !\r\n");
		if (srccap_dev->hdmirx_info.ctrl_hdcp_state_monitor_task == NULL) {
			srccap_dev->hdmirx_info.ctrl_hdcp_state_monitor_task =
				kthread_create(mtk_hdmirx_ctrl_hdcp_state_monitor_task, srccap_dev,
							   "hdmi ctrl_hdcp_state_monitor_task");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.ctrl_hdcp_state_monitor_task)) {
				mutex_unlock(&hdmirx_subscribe_mutex);
				return -ENOMEM;
			}
			wake_up_process(srccap_dev->hdmirx_info.ctrl_hdcp_state_monitor_task);
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_DSC_STATE:
		SUBSCRIBE_CTRL_EVT(
			srccap_dev,
			srccap_dev->hdmirx_info.ctrl_dsc_state_monitor_task,
			mtk_hdmirx_ctrl_dsc_state_monitor_task,
			"hdmi ctrl_dsc_state_monitor_task",
			retval);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_VRR_EN_CHG:
		for (u32_i = 0 ; u32_i < HDMI_PORT_MAX_NUM; u32_i++)
			p_hdmirx_info->emp_vtem_info[u32_i].vrr_en = 0;
		p_hdmirx_info->u32Emp_vtem_info_subscribe_bitmap |=
			VTEM_EVENT_BITMAP(event_sub->id);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_M_CONST_CHG:
		for (u32_i = 0 ; u32_i < HDMI_PORT_MAX_NUM; u32_i++)
			p_hdmirx_info->emp_vtem_info[u32_i].m_const = 0;
		p_hdmirx_info->u32Emp_vtem_info_subscribe_bitmap |=
			VTEM_EVENT_BITMAP(event_sub->id);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_QMS_EN_CHG:
		for (u32_i = 0 ; u32_i < HDMI_PORT_MAX_NUM; u32_i++)
			p_hdmirx_info->emp_vtem_info[u32_i].qms_en = 0;
		p_hdmirx_info->u32Emp_vtem_info_subscribe_bitmap |=
			VTEM_EVENT_BITMAP(event_sub->id);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_FVA_FACTOR_M1_CHG:
		for (u32_i = 0 ; u32_i < HDMI_PORT_MAX_NUM; u32_i++)
			p_hdmirx_info->emp_vtem_info[u32_i].fva_factor_m1 = 0;
		p_hdmirx_info->u32Emp_vtem_info_subscribe_bitmap |=
			VTEM_EVENT_BITMAP(event_sub->id);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_NEXT_TFR_CHG:
		for (u32_i = 0 ; u32_i < HDMI_PORT_MAX_NUM; u32_i++)
			p_hdmirx_info->emp_vtem_info[u32_i].next_tfr = 0;
		p_hdmirx_info->u32Emp_vtem_info_subscribe_bitmap |=
			VTEM_EVENT_BITMAP(event_sub->id);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_DE_STATUS:
		// HDMIRX_MSG_INFO("Subscribe DE_STATUS !\r\n");
		if (!(srccap_dev->hdmirx_info.u32MonitorEvent &
			V4L2_SRCCAP_HDMIRX_MONITOR_DESTATUS)) {
			memset(srccap_dev->hdmirx_info.bDEStatus, 0x00,
				   sizeof(srccap_dev->hdmirx_info.bDEStatus));
			srccap_dev->hdmirx_info.u32MonitorEvent |=
				V4L2_SRCCAP_HDMIRX_MONITOR_DESTATUS;
		}
		if (srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task == NULL) {
			srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task =
				kthread_create(mtk_hdmirx_ctrl_status_monitor_task, srccap_dev,
							   "hdmi ctrl_status_monitor_task");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task)) {
				mutex_unlock(&hdmirx_subscribe_mutex);
				return -ENOMEM;
			}
			wake_up_process(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task);
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_STATUS:
		// HDMIRX_MSG_INFO("Subscribe HDMI_STATUS !\r\n");
		if (!(srccap_dev->hdmirx_info.u32MonitorEvent &
			V4L2_SRCCAP_HDMIRX_MONITOR_HDMISTATUS)) {
			memset(srccap_dev->hdmirx_info.bHDMIStatus, 0x00,
				   sizeof(srccap_dev->hdmirx_info.bHDMIStatus));
			memset(srccap_dev->hdmirx_info.bClkStatus, 0x00,
				   sizeof(srccap_dev->hdmirx_info.bClkStatus));
			memset(srccap_dev->hdmirx_info.bStable, 0x00,
				   sizeof(srccap_dev->hdmirx_info.bStable));
			memset(srccap_dev->hdmirx_info.u32HDE, 0x00,
				   sizeof(srccap_dev->hdmirx_info.u32HDE));
			memset(srccap_dev->hdmirx_info.u32VDE, 0x00,
				   sizeof(srccap_dev->hdmirx_info.u32VDE));
			srccap_dev->hdmirx_info.u32MonitorEvent |=
				V4L2_SRCCAP_HDMIRX_MONITOR_HDMISTATUS;
		}
		if (srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task == NULL) {
			srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task =
				kthread_create(mtk_hdmirx_ctrl_status_monitor_task, srccap_dev,
							   "hdmi ctrl_status_monitor_task");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task)) {
				mutex_unlock(&hdmirx_subscribe_mutex);
				return -ENOMEM;
			}
			wake_up_process(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task);
		}
		break;
	default:
		break;
	}

	mutex_unlock(&hdmirx_subscribe_mutex);

	return retval;
}

int mtk_hdmirx_unsubscribe_ctrl_event(
	struct mtk_srccap_dev *srccap_dev,
	const struct v4l2_event_subscription *event_sub)
{
	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);
	if (event_sub == NULL || srccap_dev == NULL) {
		HDMIRX_MSG_ERROR("NULL Event !\r\n");
		HDMIRX_MSG_ERROR("mdbgerr_%s:  %d \r\n", __func__, -ENXIO);
		return -ENXIO;
	}

	mutex_lock(&hdmirx_subscribe_mutex);

	switch (event_sub->id) {
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_PKT_STATUS:
		// HDMIRX_MSG_INFO("UnSubscribe PKT_STATUS !\r\n");
		UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_packet_monitor_task);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_CABLE_DETECT:
		// HDMIRX_MSG_INFO("UnSubscribe CABLE_DETECT !\r\n");
		UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_5v_detect_monitor_task);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP2X_STATUS:
		// HDMIRX_MSG_INFO("UnSubscribe HDCP2X_STATUS !\r\n");
		UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_hdcp2x_irq_monitor_task);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP1X_STATUS:
		// HDMIRX_MSG_INFO("UnSubscribe HDCP1X_STATUS !\r\n");
		UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_hdcp1x_irq_monitor_task);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_CRC:
		// HDMIRX_MSG_INFO("UnSubscribe HDMI_CRC !\r\n");
		UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_hdmi_crc_monitor_task);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_MODE:
		//HDMIRX_MSG_INFO("UnSubscribe HDMI_MODE !\r\n");
		srccap_dev->hdmirx_info.u32MonitorEvent &=
			~V4L2_SRCCAP_HDMIRX_MONITOR_HDMIMODE;
		if (srccap_dev->hdmirx_info.u32MonitorEvent == 0) {
			UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task);
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDCP_STATE:
		// HDMIRX_MSG_INFO("UnSubscribe HDCP_STATE !\r\n");
		UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_hdcp_state_monitor_task);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_DSC_STATE:
		UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_dsc_state_monitor_task);
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_VRR_EN_CHG:
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_M_CONST_CHG:
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_QMS_EN_CHG:
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_FVA_FACTOR_M1_CHG:
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_VTEM_NEXT_TFR_CHG:
		srccap_dev->hdmirx_info.u32Emp_vtem_info_subscribe_bitmap &=
			~((MS_U32)VTEM_EVENT_BITMAP(event_sub->id));
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_DE_STATUS:
		//HDMIRX_MSG_INFO("UnSubscribe DE Status !\r\n");
		srccap_dev->hdmirx_info.u32MonitorEvent &=
			~V4L2_SRCCAP_HDMIRX_MONITOR_DESTATUS;
		if (srccap_dev->hdmirx_info.u32MonitorEvent == 0) {
			UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task);
		}
		break;
	case V4L2_SRCCAP_CTRL_EVENT_HDMIRX_HDMI_STATUS:
		//HDMIRX_MSG_INFO("UnSubscribe HDMI Status !\r\n");
		srccap_dev->hdmirx_info.u32MonitorEvent &=
			~V4L2_SRCCAP_HDMIRX_MONITOR_HDMISTATUS;
		if (srccap_dev->hdmirx_info.u32MonitorEvent == 0) {
			UNSUBSCRIBE_CTRL_EVT(srccap_dev->hdmirx_info.ctrl_hdmi_status_monitor_task);
		}
		break;
	default:
		break;
	}

	mutex_unlock(&hdmirx_subscribe_mutex);

	return 0;
}

int mtk_hdmirx_set_current_port(struct mtk_srccap_dev *srccap_dev)
{
	E_MUX_INPUTPORT ePort = mtk_hdmirx_to_muxinputport(srccap_dev->srccap_info.src);
	stHDMI_POLLING_INFO polling = MDrv_HDMI_Get_HDMIPollingInfo(ePort);
	MS_U8 u8HDMIInfoSource = KDrv_SRCCAP_HDMIRx_mux_inport_2_src(ePort);

	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);
	HDMIRX_MSG_DBG("%s: dev_id[%d] src[%d] ePort[%d] \r\n", __func__,
				   srccap_dev->dev_id, srccap_dev->srccap_info.src, ePort);
	KDrv_SRCCAP_HDMIRx_mux_sel_video(srccap_dev->dev_id, ePort);

	KDrv_SRCCAP_HDMIRx_pkt_packet_info(u8HDMIInfoSource, &polling);

	return 0;
}

int mtk_hdmirx_g_edid(struct v4l2_edid *edid)
{
#define EDID_BLOCK_MAX 4
#define EDID_BLOCK_SIZE 128
	XC_DDCRAM_PROG_INFO pstDDCRam_Info;
	MS_U8 u8EDID[EDID_BLOCK_MAX * EDID_BLOCK_SIZE];
	HDMIRX_MSG_DBG("mdbgin_%s: start \r\n", __func__);
	if ((edid != NULL) && (edid->edid != NULL)) {
		if ((edid->start_block >= EDID_BLOCK_MAX) ||
			(edid->blocks > EDID_BLOCK_MAX))
			return -EINVAL;

		if (((edid->start_block + edid->blocks) > EDID_BLOCK_MAX) ||
			(((edid->start_block + edid->blocks) * EDID_BLOCK_SIZE) >
			 (EDID_BLOCK_MAX * EDID_BLOCK_SIZE)))
			return -EINVAL;

		pstDDCRam_Info.EDID = u8EDID;
		pstDDCRam_Info.u16EDIDSize =
			(edid->start_block + edid->blocks) * EDID_BLOCK_SIZE;
		pstDDCRam_Info.eDDCProgType = edid->pad;
		MDrv_HDMI_READ_DDCRAM(&pstDDCRam_Info);
		memcpy(edid->edid, (u8EDID + (edid->start_block * EDID_BLOCK_SIZE)),
			   edid->blocks * EDID_BLOCK_SIZE);
	} else {
		HDMIRX_MSG_ERROR("mdbgerr_%s:  %d \r\n", __func__, -EINVAL);
		return -EINVAL;
	}

	return 0;
}

int mtk_hdmirx_s_edid(struct v4l2_edid *edid)
{
	XC_DDCRAM_PROG_INFO pstDDCRam_Info;
	//HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);
	if ((edid != NULL) && (edid->edid != NULL)) {
		if (edid->blocks > 4)
			return -E2BIG;

		pstDDCRam_Info.EDID = edid->edid;
		pstDDCRam_Info.u16EDIDSize = edid->blocks * 128;
		pstDDCRam_Info.eDDCProgType = edid->pad;
		MDrv_HDMI_PROG_DDCRAM(&pstDDCRam_Info);
                HDMIRX_MSG_DBG("** HDMI EDID has been set, port %d\n", pstDDCRam_Info.eDDCProgType);
	} else {
		HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, -EINVAL);
		return -EINVAL;
	}

	return 0;
}

int mtk_hdmirx_g_audio_channel_status(struct mtk_srccap_dev *srccap_dev,
									  struct v4l2_audio *audio)
{
	int tmp = 0;

	if (audio != NULL) {
		E_MUX_INPUTPORT ePort =
			mtk_hdmirx_to_muxinputport(srccap_dev->srccap_info.src);
		MS_U8 u8HDMIInfoSource = KDrv_SRCCAP_HDMIRx_mux_inport_2_src(ePort);
		// index which byte to get.
		tmp = snprintf(audio->name, sizeof(audio->name), "byte %d", audio->index);
		if (tmp < 0) {
			HDMIRX_MSG_ERROR("Fail to do snprintf, val = %d\n", tmp);
			return -EINVAL;
		}

		audio->index = KDrv_SRCCAP_HDMIRx_pkt_audio_channel_status(
			u8HDMIInfoSource, audio->index, NULL);
	} else {
		HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, -EINVAL);
		return -EINVAL;
	}

	return 0;
}

int mtk_hdmirx_g_dv_timings(struct mtk_srccap_dev *srccap_dev,
							struct v4l2_dv_timings *timings)
{
	if ((timings != NULL) && (isV4L2PortAsHDMI(srccap_dev->srccap_info.src))) {
		E_MUX_INPUTPORT ePort =
			mtk_hdmirx_to_muxinputport(srccap_dev->srccap_info.src);
		stHDMI_POLLING_INFO polling = MDrv_HDMI_Get_HDMIPollingInfo(ePort);
		MS_U8 u8HDMIInfoSource = KDrv_SRCCAP_HDMIRx_mux_inport_2_src(ePort);

		timings->bt.width = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_HDE, u8HDMIInfoSource, &polling);
		timings->bt.hfrontporch = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_HFRONT, u8HDMIInfoSource, &polling);
		timings->bt.hsync = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_HSYNC, u8HDMIInfoSource, &polling);
		timings->bt.hbackporch = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_HBACK, u8HDMIInfoSource, &polling);
		timings->bt.height = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_VDE, u8HDMIInfoSource, &polling);
		timings->bt.vfrontporch = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_VFRONT, u8HDMIInfoSource, &polling);
		timings->bt.vsync = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_VSYNC, u8HDMIInfoSource, &polling);
		timings->bt.vbackporch = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_VBACK, u8HDMIInfoSource, &polling);
		timings->bt.interlaced = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(
			E_HDMI_GET_INTERLACE, u8HDMIInfoSource, &polling);
		timings->bt.pixelclock = KDrv_SRCCAP_HDMIRx_mac_getratekhz(
			ePort, E_HDMI_PIX,
			(polling.ucSourceVersion == HDMI_SOURCE_VERSION_HDMI21 ? TRUE : FALSE));
		timings->bt.polarities =
			(KDrv_SRCCAP_HDMIRx_mac_get_datainfo(E_HDMI_GET_H_POLARITY,
					u8HDMIInfoSource, &polling) & 0x1);
		timings->bt.polarities |=
			(KDrv_SRCCAP_HDMIRx_mac_get_datainfo(E_HDMI_GET_V_POLARITY,
					u8HDMIInfoSource, &polling) & 0x1) << 4;
		timings->bt.polarities |=
			(mtk_hdmirx_get_datainfo(srccap_dev,
			E_HDMI_GET_H_POLARITY_BY_SRC) & 0x1) << 8;
		timings->bt.polarities |=
			(mtk_hdmirx_get_datainfo(srccap_dev,
			E_HDMI_GET_V_POLARITY_BY_SRC) & 0x1) << 12;
	} else {
		HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, -EINVAL);
		return -EINVAL;
	}

	return 0;
}

__u32 mtk_hdmirx_get_datainfo(struct mtk_srccap_dev *srccap_dev,
	__u8 u8DataType)
{
	if ((srccap_dev != NULL) &&
		(isV4L2PortAsHDMI(srccap_dev->srccap_info.src))) {
		E_MUX_INPUTPORT ePort =
			mtk_hdmirx_to_muxinputport(srccap_dev->srccap_info.src);
		stHDMI_POLLING_INFO polling = MDrv_HDMI_Get_HDMIPollingInfo(ePort);
		MS_U8 u8HDMIInfoSource = KDrv_SRCCAP_HDMIRx_mux_inport_2_src(ePort);

		return KDrv_SRCCAP_HDMIRx_mac_get_datainfo(u8DataType,
			u8HDMIInfoSource, &polling);
	} else
		return -ENXIO;
}

int mtk_hdmirx_store_color_fmt(struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (isV4L2PortAsHDMI(srccap_dev->srccap_info.src))
		srccap_dev->hdmirx_info.color_format =
			mtk_hdmirx_Get_ColorFormat(srccap_dev->srccap_info.src);
	return 0;
}

int mtk_hdmirx_SetPowerState(struct mtk_srccap_dev *srccap_dev,
							 EN_POWER_MODE ePowerState)
{
	if (srccap_dev->dev_id == 0) {
		if (ePowerState == E_POWER_SUSPEND) {
			_hdmirx_irq_suspend(srccap_dev);
			MDrv_HDMI_STREventProc(E_POWER_SUSPEND, &stHDMIRxBank);
			// mokona remove
			if (srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_4
				&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_5
				&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_6)
				_hdmirx_disable(srccap_dev);
		} else if (ePowerState == E_POWER_RESUME) {
			// mokona remove
			if (srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_4
				&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_5
				&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_6)
				_hdmirx_enable(srccap_dev);
			MDrv_HDMI_STREventProc(E_POWER_RESUME, &stHDMIRxBank);
			_hdmirx_irq_resume(srccap_dev);
		} else {
			HDMIRX_MSG_ERROR("Not support this Power State(%d).\n", ePowerState);
			return -EINVAL;
		}
	}
	return 0;
}

int _mtk_hdmirx_ctrl_process(__u32 u32Cmd, void *pBuf, __u32 u32BufSize)
{
	switch (u32Cmd) {
	case V4L2_EXT_HDMI_INTERFACE_CMD_GET_HDCP_STATE: {
		unsigned long ret;
		stCMD_HDMI_CHECK_HDCP_STATE stHDCP_State;

		ret = copy_from_user(&stHDCP_State,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stHDCP_State.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDCP_State.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_State, u32BufSize);
#if DEF_TEST_USE
			stHDCP_State.ucHDCPState = DEF_TEST_CONST2 + stHDCP_State.enInputPortType;
#endif
		} else {
			HDMIRX_MSG_ERROR("copy_from_user HDCP State failed\n"
							 "\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stHDCP_State, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user HDCP State failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_INTERFACE_CMD_WRITE_X74: {
		unsigned long ret;
		stCMD_HDCP_WRITE_X74 stHDCP_WriteX74;

		ret = copy_from_user(&stHDCP_WriteX74,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stHDCP_WriteX74.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDCP_WriteX74.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_WriteX74, u32BufSize);
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Write X74 failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_INTERFACE_CMD_READ_X74: {
		unsigned long ret;
		stCMD_HDCP_READ_X74 stHDCP_ReadX74;

		ret = copy_from_user(&stHDCP_ReadX74,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stHDCP_ReadX74.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDCP_ReadX74.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_ReadX74, u32BufSize);
#if DEF_TEST_USE
			stHDCP_ReadX74.ucRetData = DEF_TEST_CONST2 + stHDCP_ReadX74.enInputPortType;
#endif
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Read X74 failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stHDCP_ReadX74, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Read X74 failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_INTERFACE_CMD_SET_REPEATER: {
		unsigned long ret;
		stCMD_HDCP_SET_REPEATER stHDCP_SetRepeater;

		ret = copy_from_user(&stHDCP_SetRepeater,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stHDCP_SetRepeater.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDCP_SetRepeater.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_SetRepeater, u32BufSize);
		} else {
			HDMIRX_MSG_ERROR("copy_from_user HDCP Set Repeater\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_SET_BSTATUS: {
		unsigned long ret;
		stCMD_HDCP_SET_BSTATUS stHDCP_SetBstatus;

		ret = copy_from_user(&stHDCP_SetBstatus,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stHDCP_SetBstatus.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDCP_SetBstatus.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_SetBstatus, u32BufSize);
		} else {
			HDMIRX_MSG_ERROR("copy_from_user HDCP Set BStatus\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_SET_HDMI_MODE: {
		unsigned long ret;
		stCMD_HDCP_SET_HDMI_MODE stHDMI_SetMode;

		ret = copy_from_user(&stHDMI_SetMode,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stHDMI_SetMode.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDMI_SetMode.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDMI_SetMode, u32BufSize);
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Set HDMI Mode\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_INTERRUPT_STATUS: {
		unsigned long ret;
		stCMD_HDCP_GET_INTERRUPT_STATUS stHDCP_IRQStatus;

		ret = copy_from_user(&stHDCP_IRQStatus,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stHDCP_IRQStatus.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDCP_IRQStatus.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_IRQStatus, u32BufSize);
#if DEF_TEST_USE
			stHDCP_IRQStatus.ucRetIntStatus =
				DEF_TEST_CONST2 + stHDCP_IRQStatus.enInputPortType;
#endif
		} else {
			HDMIRX_MSG_ERROR("copy_from_user HDCP IRQ Status\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stHDCP_IRQStatus, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user HDCP IRQ Status\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_WRITE_KSV_LIST: {
		//"Untrusted value as argument"
		// Passing tainted variable "stHDCP_KSVList.ulLen" to a tainted sink.

		unsigned long ret;
		stCMD_HDCP_WRITE_KSV_LIST stHDCP_KSVList;
		__u8 *u8KSVList = NULL;

		ret = copy_from_user(&stHDCP_KSVList,
					 pBuf,
					 u32BufSize);

		if (ret == 0) {
			if ((stHDCP_KSVList.ulLen > 0) && (stHDCP_KSVList.ulLen <= 635)) {
				u8KSVList = kmalloc(stHDCP_KSVList.ulLen, GFP_KERNEL);
				if (u8KSVList == NULL) {
					HDMIRX_MSG_ERROR("Allocate KSVList\n"
									 "Memory Failed!\r\n");
					return -ENOMEM;
				}
				ret = copy_from_user(
						u8KSVList,
						stHDCP_KSVList.pucKSV,
						stHDCP_KSVList.ulLen);

				if (ret == 0) {
					stHDCP_KSVList.pucKSV = u8KSVList;
					stHDCP_KSVList.enInputPortType = mtk_hdmirx_to_muxinputport(
						(enum v4l2_srccap_input_source)
							stHDCP_KSVList.enInputPortType);
					MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_KSVList, u32BufSize);
					kfree(u8KSVList);
				} else {
					HDMIRX_MSG_ERROR("copy_from_user KSV\n"
									 "List failed\r\n");
					kfree(u8KSVList);
					return -EINVAL;
				}

			} else {
				HDMIRX_MSG_ERROR("KSV Length is < 0\r\n");
				return -EINVAL;
			}
		} else {
			HDMIRX_MSG_ERROR("copy_from_user KSV Struct\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_SET_VPRIME: {
		unsigned long ret;
		stCMD_HDCP_SET_VPRIME stHDCP_Vprime;
		__u8 *u8Vprime = NULL;

		ret = copy_from_user(&stHDCP_Vprime,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			u8Vprime = kmalloc(HDMI_HDCP_VPRIME_LENGTH, GFP_KERNEL);
			if (u8Vprime == NULL) {
				HDMIRX_MSG_ERROR("Allocate Vprime Memory\n"
								 "Failed!\r\n");
				return -ENOMEM;
			}
			ret = copy_from_user(u8Vprime, stHDCP_Vprime.pucVPrime,
								 HDMI_HDCP_VPRIME_LENGTH);

			if (ret == 0) {
				stHDCP_Vprime.pucVPrime = u8Vprime;
				stHDCP_Vprime.enInputPortType = mtk_hdmirx_to_muxinputport(
					(enum v4l2_srccap_input_source)
						stHDCP_Vprime.enInputPortType);
				MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_Vprime, u32BufSize);
				kfree(u8Vprime);
			} else {
				HDMIRX_MSG_ERROR("copy_from_user Vprime\n"
								 "failed\r\n");
				kfree(u8Vprime);
				return -EINVAL;
			}
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Vprime Struct\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_DATA_RTERM_CONTROL: {
		unsigned long ret;
		stCMD_HDMI_DATA_RTERM_CONTROL stData_Rterm;

		ret = copy_from_user(&stData_Rterm,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stData_Rterm.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stData_Rterm.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stData_Rterm, u32BufSize);
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Data Rterm\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_SCDC_VALUE: {
		unsigned long ret;
		stCMD_HDMI_GET_SCDC_VALUE stSCDC_Value;

		ret = copy_from_user(&stSCDC_Value,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stSCDC_Value.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stSCDC_Value.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stSCDC_Value, u32BufSize);
#if DEF_TEST_USE
			stSCDC_Value.ucRetData = DEF_TEST_CONST2 + stSCDC_Value.enInputPortType;
#endif
		} else {
			HDMIRX_MSG_ERROR("copy_from_user SCDC Value\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stSCDC_Value, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user SCDC Value failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_TMDS_RATES_KHZ: {
		unsigned long ret;
		stCMD_HDMI_GET_TMDS_RATES_KHZ stTMDS_Rates;

		ret = copy_from_user(&stTMDS_Rates,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stTMDS_Rates.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stTMDS_Rates.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stTMDS_Rates, u32BufSize);
#if DEF_TEST_USE
			stTMDS_Rates.ulRetRates = DEF_TEST_CONST2 + stTMDS_Rates.enInputPortType;
#endif
		} else {
			HDMIRX_MSG_ERROR("copy_from_user TMDS Rates\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stTMDS_Rates, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user TMDS Rates failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_CABLE_DETECT: {
		unsigned long ret;
		stCMD_HDMI_GET_SCDC_CABLE_DETECT stCable_Detect;

		ret = copy_from_user(&stCable_Detect,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stCable_Detect.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stCable_Detect.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stCable_Detect, u32BufSize);
#if DEF_TEST_USE
			stCable_Detect.bCableDetectFlag = TRUE;
#endif
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Cable Detect\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stCable_Detect, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Cable Detect\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_PACKET_STATUS: {
		unsigned long ret;
		stCMD_HDMI_GET_PACKET_STATUS stPkt_Status;
		MS_HDMI_EXTEND_PACKET_RECEIVE_t stPktReceStatus;
		MS_HDMI_EXTEND_PACKET_RECEIVE_t *pstUser_PktReceStatus;

		ret = copy_from_user(&stPkt_Status, pBuf, u32BufSize);
		if (ret == 0) {
			// get user Addr
			pstUser_PktReceStatus = stPkt_Status.pExtendPacketReceive;
			ret = copy_from_user(
					&stPktReceStatus,
					stPkt_Status.pExtendPacketReceive,
					sizeof(MS_HDMI_EXTEND_PACKET_RECEIVE_t));
			if (ret == 0) {
				stPkt_Status.pExtendPacketReceive = &stPktReceStatus;
				stPkt_Status.enInputPortType = mtk_hdmirx_to_muxinputport(
					(enum v4l2_srccap_input_source)
						stPkt_Status.enInputPortType);
				MDrv_HDMI_Ctrl(u32Cmd, (void *)&stPkt_Status, u32BufSize);
#if DEF_TEST_USE
				stPkt_Status.pExtendPacketReceive->bPKT_GC_RECEIVE = TRUE;
#endif
				ret = copy_to_user(
						pstUser_PktReceStatus,
						stPkt_Status.pExtendPacketReceive,
						sizeof(MS_HDMI_EXTEND_PACKET_RECEIVE_t));
				if (ret == 0)
					stPkt_Status.pExtendPacketReceive = pstUser_PktReceStatus;
				else {
					HDMIRX_MSG_ERROR("copy_to_user Packet\n"
									 "Status failed\r\n");
					return -EINVAL;
				}
			} else {
				HDMIRX_MSG_ERROR("copy_form_user Packet\n"
								 "Status failed\r\n");
				return -EINVAL;
			}
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Get Packet Status\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stPkt_Status, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Get Packet Status\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_PACKET_CONTENT: {
		//"Untrusted value as argument"
		// Passing tainted variable "stPkt_Content.u8PacketLength"
		//						to a tainted sink.

		unsigned long ret;
		stCMD_HDMI_GET_PACKET_CONTENT stPkt_Content;
		__u8 *u8PktContent = NULL;

		ret = copy_from_user(&stPkt_Content,
					 pBuf,
					 u32BufSize);
		u8PktContent = stPkt_Content.pu8PacketContent; // get user Addr
		if (ret == 0) {
			if ((stPkt_Content.u8PacketLength > 0) &&
				(stPkt_Content.u8PacketLength <= 121)) {
				stPkt_Content.pu8PacketContent =
					kmalloc(stPkt_Content.u8PacketLength, GFP_KERNEL);
				if (stPkt_Content.pu8PacketContent == NULL) {
					HDMIRX_MSG_ERROR("Allocate Packet\n"
									 "Content Memory Failed!\r\n");
					return -ENOMEM;
				}

				stPkt_Content.enInputPortType = mtk_hdmirx_to_muxinputport(
					(enum v4l2_srccap_input_source)
						stPkt_Content.enInputPortType);
				MDrv_HDMI_Ctrl(u32Cmd, (void *)&stPkt_Content, u32BufSize);
#if DEF_TEST_USE
				*stPkt_Content.pu8PacketContent =
					DEF_TEST_CONST2 + stPkt_Content.enInputPortType;
#endif
				ret = copy_to_user(
						u8PktContent,
						stPkt_Content.pu8PacketContent,
						stPkt_Content.u8PacketLength);
				if (ret == 0) {
					kfree(stPkt_Content.pu8PacketContent);
					stPkt_Content.pu8PacketContent = u8PktContent;
				} else {
					HDMIRX_MSG_ERROR("copy_to_user\n"
									 "Packet Content failed\r\n");
					kfree(stPkt_Content.pu8PacketContent);
					return -EINVAL;
				}
			} else {
				HDMIRX_MSG_ERROR("Packet Length Error\r\n");
				return -EINVAL;
			}
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Get Packet Content\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stPkt_Content, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Get Packet Content\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_HDR_METADATA: {
		unsigned long ret;
		stCMD_HDMI_GET_HDR_METADATA stHDR_GetData;
		sHDR_METADATA stHDR_Meta;
		sHDR_METADATA *pstUser_HDRMeta;

		ret = copy_from_user(&stHDR_GetData,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			// get user Addr
			pstUser_HDRMeta = stHDR_GetData.pstHDRMetadata;
			ret = copy_from_user(&stHDR_Meta, stHDR_GetData.pstHDRMetadata,
								 sizeof(sHDR_METADATA));
			if (ret == 0) {
				stHDR_GetData.pstHDRMetadata = &stHDR_Meta;
				stHDR_GetData.enInputPortType = mtk_hdmirx_to_muxinputport(
					(enum v4l2_srccap_input_source)
						stHDR_GetData.enInputPortType);
				MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDR_GetData, u32BufSize);
#if DEF_TEST_USE
				stHDR_Meta.u8Length =
					DEF_TEST_CONST2 + stHDR_GetData.enInputPortType;
#endif
				ret = copy_to_user(
						pstUser_HDRMeta,
						stHDR_GetData.pstHDRMetadata,
						sizeof(sHDR_METADATA));
				if (ret == 0) {
					stHDR_GetData.pstHDRMetadata = pstUser_HDRMeta;
				} else {
					HDMIRX_MSG_ERROR("copy_to_user HDR\n"
									 "Meta Data failed\r\n");
					return -EINVAL;
				}
			} else {
				HDMIRX_MSG_ERROR("copy_form_user HDR Meta\n"
								 "Data failed\r\n");
				return -EINVAL;
			}
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Get HDR failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stHDR_GetData, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Get HDR failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_AVI_PARSING_INFO: {
		unsigned long ret;
		stCMD_HDMI_GET_AVI_PARSING_INFO stAVI_GetInfo;
		stAVI_PARSING_INFO stAVI_ParseInfo;
		stAVI_PARSING_INFO *pstUser_ParseAVIInfo;

		ret = copy_from_user(&stAVI_GetInfo,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			// get user Addr
			pstUser_ParseAVIInfo = stAVI_GetInfo.pstAVIParsingInfo;
			ret =
				copy_from_user(
					&stAVI_ParseInfo, stAVI_GetInfo.pstAVIParsingInfo,
					sizeof(stAVI_PARSING_INFO));
			if (ret == 0) {
				stAVI_GetInfo.pstAVIParsingInfo = &stAVI_ParseInfo;
				stAVI_GetInfo.enInputPortType = mtk_hdmirx_to_muxinputport(
					(enum v4l2_srccap_input_source)
						stAVI_GetInfo.enInputPortType);
				MDrv_HDMI_Ctrl(u32Cmd, (void *)&stAVI_GetInfo, u32BufSize);
#if DEF_TEST_USE
				stAVI_ParseInfo.u8Length =
					DEF_TEST_CONST2 + stAVI_GetInfo.enInputPortType;
#endif
				ret = copy_to_user(
						pstUser_ParseAVIInfo,
						stAVI_GetInfo.pstAVIParsingInfo,
						sizeof(stAVI_PARSING_INFO));
				if (ret == 0)
					stAVI_GetInfo.pstAVIParsingInfo = pstUser_ParseAVIInfo;
				else {
					HDMIRX_MSG_ERROR("copy_to_user AVI\n"
									 "Parse Info failed\r\n");
					return -EINVAL;
				}
			} else {
				HDMIRX_MSG_ERROR("copy_form_user AVI Parse\n"
								 "Info failed\r\n");
				return -EINVAL;
			}
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Get AVI Info\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stAVI_GetInfo, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Get AVI Info\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_VS_PARSING_INFO: {
		unsigned long ret;
		stCMD_HDMI_GET_VS_PARSING_INFO stVS_GetInfo;
		stVS_PARSING_INFO stVS_Info;
		stVS_PARSING_INFO *pstUser_VSInfo;

		ret = copy_from_user(&stVS_GetInfo,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			// get user Addr
			pstUser_VSInfo = stVS_GetInfo.pstVSParsingInfo;
			ret = copy_from_user(&stVS_Info, stVS_GetInfo.pstVSParsingInfo,
								 sizeof(stVS_PARSING_INFO));
			if (ret == 0) {
				stVS_GetInfo.pstVSParsingInfo = &stVS_Info;
				stVS_GetInfo.enInputPortType = mtk_hdmirx_to_muxinputport(
					(enum v4l2_srccap_input_source)
						stVS_GetInfo.enInputPortType);
				MDrv_HDMI_Ctrl(u32Cmd, (void *)&stVS_GetInfo, u32BufSize);
#if DEF_TEST_USE
				stVS_Info.u8Length = DEF_TEST_CONST2 + stVS_GetInfo.enInputPortType;
#endif
				ret = copy_to_user(
						pstUser_VSInfo,
						stVS_GetInfo.pstVSParsingInfo,
						sizeof(stVS_PARSING_INFO));
				if (ret == 0)
					stVS_GetInfo.pstVSParsingInfo = pstUser_VSInfo;
				else {
					HDMIRX_MSG_ERROR("copy_to_user VS\n"
									 "Parse Info failed\r\n");
					return -EINVAL;
				}
			} else {
				HDMIRX_MSG_ERROR("copy_form_user VS Parse\n"
								 "Info failed\r\n");
				return -EINVAL;
			}
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Get VS Info\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stVS_GetInfo, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Get VS Info\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_GC_PARSING_INFO: {
		unsigned long ret;
		stCMD_HDMI_GET_GC_PARSING_INFO stGC_GetInfo;
		stGC_PARSING_INFO stGC_Info;
		stGC_PARSING_INFO *pstUser_GCInfo;

		ret = copy_from_user(&stGC_GetInfo,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			// get user Addr
			pstUser_GCInfo = stGC_GetInfo.pstGCParsingInfo;
			ret = copy_from_user(&stGC_Info, stGC_GetInfo.pstGCParsingInfo,
								 sizeof(stGC_PARSING_INFO));
			if (ret == 0) {
				stGC_GetInfo.pstGCParsingInfo = &stGC_Info;
				stGC_GetInfo.enInputPortType = mtk_hdmirx_to_muxinputport(
					(enum v4l2_srccap_input_source)
						stGC_GetInfo.enInputPortType);
				MDrv_HDMI_Ctrl(u32Cmd, (void *)&stGC_GetInfo, u32BufSize);
#if DEF_TEST_USE
				stGC_Info.enColorDepth =
					MS_HDMI_COLOR_DEPTH_8_BIT +
					stGC_GetInfo.enInputPortType - 80;
#endif
				ret = copy_to_user(
						pstUser_GCInfo,
						stGC_GetInfo.pstGCParsingInfo,
						sizeof(stGC_PARSING_INFO));
				if (ret == 0)
					stGC_GetInfo.pstGCParsingInfo = pstUser_GCInfo;
				else {
					HDMIRX_MSG_ERROR("copy_to_user GC\n"
									 "Parse Info failed\r\n");
					return -EINVAL;
				}
			} else {
				HDMIRX_MSG_ERROR("copy_form_user GC Parse\n"
								 "Info failed\r\n");
				return -EINVAL;
			}
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Get GC Info\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stGC_GetInfo, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Get GC Info failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_TIMING_INFO: {
		unsigned long ret;
		stCMD_HDMI_GET_TIMING_INFO stTiming_Info;

		ret = copy_from_user(&stTiming_Info,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stTiming_Info.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stTiming_Info.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stTiming_Info, u32BufSize);
#if DEF_TEST_USE
			stTiming_Info.u16TimingInfo = 1080;
#endif
		} else {
			HDMIRX_MSG_ERROR("copy_from_user Timing Info\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stTiming_Info, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user Timing Info\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	case V4L2_EXT_HDMI_HDMI_INTERFACE_CMD_GET_HDCP_AUTHVERSION: {
		unsigned long ret;
		stCMD_HDMI_GET_HDCP_AUTHVERSION stHDCP_AuthVer;

		ret = copy_from_user(&stHDCP_AuthVer,
					 pBuf,
					 u32BufSize);
		if (ret == 0) {
			stHDCP_AuthVer.enInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDCP_AuthVer.enInputPortType);
			MDrv_HDMI_Ctrl(u32Cmd, (void *)&stHDCP_AuthVer, u32BufSize);
#if DEF_TEST_USE
			stHDCP_AuthVer.enHDCPAuthVersion = E_HDCP_2_2;
#endif
		} else {
			HDMIRX_MSG_ERROR("copy_from_user HDCP Auth Version\n"
							 "failed\r\n");
			return -EINVAL;
		}
		ret = copy_to_user(pBuf, &stHDCP_AuthVer, u32BufSize);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user HDCP Auth Version\n"
							 "failed\r\n");
			return -EINVAL;
		}
	} break;

	default:
		HDMIRX_MSG_ERROR("Command ID Not Found! \r\n");
		return -EINVAL;
	}

	return 0;
}


static int
_hdmirx_gctrl_v4l2_cid_hdmi_rx_get_it_content_type(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	srccap_dev =
		container_of(ctrl->handler, struct mtk_srccap_dev, hdmirx_ctrl_handler);

	ctrl->val = mtk_hdmirx_Content_Type(srccap_dev->srccap_info.src);
#if DEF_TEST_USE
	ctrl->val = DEF_TEST_CONST1;
#endif
	return 0;
}

static int
_hdmirx_gctrl_v4l2_cid_hdmi_rx_get_sim_mode(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool *pv4lCable_bool;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	pv4lCable_bool = (void *)ctrl->p_new.p;

	pv4lCable_bool->bRet = KDrv_SRCCAP_HDMIRx_isSimMode();
	return 0;
}

static int
_hdmirx_gctrl_v4l2_cid_hdmi_rx_get_cable_detect(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev;
	int i = 0;
	stCMD_HDMI_GET_SCDC_CABLE_DETECT stCable_Detect;
	struct v4l2_ext_hdmi_cmd_get_cable_detect *v4lCable_Detect;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	srccap_dev =
		container_of(ctrl->handler, struct mtk_srccap_dev, hdmirx_ctrl_handler);

	v4lCable_Detect =
			(void *)ctrl->p_new.p;

	for (i = 0; i < srccap_dev->hdmirx_info.port_count; i++) {
		stCable_Detect.enInputPortType = INPUT_PORT_DVI0 + i;
		MDrv_HDMI_Ctrl(
				E_HDMI_INTERFACE_CMD_GET_SCDC_CABLE_DETECT,
				(void *)&(stCable_Detect),
				sizeof(stCMD_HDMI_GET_SCDC_CABLE_DETECT));
			v4lCable_Detect[i].enInputPortType =
				V4L2_SRCCAP_INPUT_SOURCE_HDMI + i;
			v4lCable_Detect[i].bCableDetectFlag =
				stCable_Detect.bCableDetectFlag;
	}

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_irq_eanble(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	srccap_dev =
		container_of(ctrl->handler, struct mtk_srccap_dev, hdmirx_ctrl_handler);

	if (ctrl->val > 0) {
		srccap_dev->hdmirx_info.ubHDCP22ISREnable = TRUE;
		if (srccap_dev->hdmirx_info.ctrl_hdcp2x_irq_monitor_task != NULL) {
			HDMIRX_MSG_ERROR("Enable HDCP22 ISR but thread has created !\r\n");
			kthread_stop(srccap_dev->hdmirx_info.ctrl_hdcp2x_irq_monitor_task);
			srccap_dev->hdmirx_info.ctrl_hdcp2x_irq_monitor_task = NULL;
		}
	} else
		srccap_dev->hdmirx_info.ubHDCP22ISREnable = FALSE;

	MDrv_HDCP22_IRQEnable(srccap_dev->hdmirx_info.ubHDCP22ISREnable);
	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_portinit(struct v4l2_ctrl *ctrl)
{
	if (ctrl == NULL)
		return -EINVAL;

	MDrv_HDCP22_PortInit(ctrl->val);
	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_sendmsg(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_hdcp22_msg stHDCPMsg;
	MS_U8 *u8Msg = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&stHDCPMsg, ctrl->p_new.p_u8,
		sizeof(struct v4l2_ext_hdmi_hdcp22_msg));
	if (stHDCPMsg.dwDataLen == 0)
		return 0;

	u8Msg = kmalloc(stHDCPMsg.dwDataLen, GFP_KERNEL);

	if (u8Msg != NULL) {
		ret = copy_from_user(
			u8Msg, stHDCPMsg.pucData,
			stHDCPMsg.dwDataLen);
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_from_user\n"
							 "failed\r\n");
			kfree(u8Msg);
			return -EINVAL;
		}

		MDrv_HDCP22_SendMsg(0, stHDCPMsg.ucPortIdx, u8Msg,
							stHDCPMsg.dwDataLen, NULL);
		kfree(u8Msg);

	} else
		return -ENOMEM;

	return 0;
}
static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp14_r0irqenable(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	srccap_dev =
			container_of(ctrl->handler, struct mtk_srccap_dev, hdmirx_ctrl_handler);

	if (ctrl->val > 0) {
		srccap_dev->hdmirx_info.ubHDCP14ISREnable = TRUE;
		if (srccap_dev->hdmirx_info.ctrl_hdcp1x_irq_monitor_task != NULL) {
			HDMIRX_MSG_ERROR("Enable HDCP14 ISR but thread has created !\r\n");
			kthread_stop(srccap_dev->hdmirx_info.ctrl_hdcp1x_irq_monitor_task);
			srccap_dev->hdmirx_info.ctrl_hdcp1x_irq_monitor_task = NULL;
		}
	} else
		srccap_dev->hdmirx_info.ubHDCP14ISREnable = FALSE;

	MDrv_HDCP14_ReadR0IRQEnable(srccap_dev->hdmirx_info.ubHDCP14ISREnable);
	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_set_eq_to_port(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_eq stHDMI_EQ;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&stHDMI_EQ, ctrl->p_new.p_u8, sizeof(struct v4l2_ext_hdmi_eq));
	eInputPortType = mtk_hdmirx_to_muxinputport(
			(enum v4l2_srccap_input_source)stHDMI_EQ.enInputPortType);
	MDrv_HDMI_Set_EQ_To_Port((MS_HDMI_EQ)stHDMI_EQ.enEq, stHDMI_EQ.u8EQValue,
								 eInputPortType);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_audio_status_clear(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool stHDMI_Bool;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Bool, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_bool));

	stHDMI_Bool.bRet =
		mtk_srccap_hdmirx_pkt_audio_status_clr(stHDMI_Bool.enInputPortType);
#if DEF_TEST_USE
		stHDMI_Bool.bRet = TRUE;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Bool, sizeof(stHDMI_Bool));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_hpd_control(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_hpd stHDMI_HPD;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&stHDMI_HPD, ctrl->p_new.p_u8, sizeof(struct v4l2_ext_hdmi_hpd));
	eInputPortType = mtk_hdmirx_to_muxinputport(
			(enum v4l2_srccap_input_source)stHDMI_HPD.enInputPortType);
	MDrv_HDMI_pullhpd(stHDMI_HPD.bHighLow, eInputPortType,
							  stHDMI_HPD.bInverse);
        HDMIRX_MSG_INFO("** HDMI HPD ctrl by v4l2, port %d, HPD = %d, bInverse = %d\r\n", eInputPortType, stHDMI_HPD.bHighLow, stHDMI_HPD.bInverse);
	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_sw_reset(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_sw_reset stHDMI_Reset;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Reset, ctrl->p_new.p_u8, sizeof(struct v4l2_ext_hdmi_sw_reset));
	eInputPortType = mtk_hdmirx_to_muxinputport(
			(enum v4l2_srccap_input_source)stHDMI_Reset.enInputPortType);
	MDrv_DVI_Software_Reset(eInputPortType, stHDMI_Reset.u16Reset);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_reset(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt_reset stHDMI_PKT_Reset;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&stHDMI_PKT_Reset, ctrl->p_new.p_u8,
			sizeof(struct v4l2_ext_hdmi_pkt_reset));
	eInputPortType = mtk_hdmirx_to_muxinputport(
			(enum v4l2_srccap_input_source)stHDMI_PKT_Reset.enInputPortType);
	MDrv_HDMI_PacketReset(eInputPortType, stHDMI_PKT_Reset.enResetType);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_clkrterm_control(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_clkrterm_ctrl stHDMI_CLK;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&stHDMI_CLK, ctrl->p_new.p_u8, sizeof(struct v4l2_ext_hdmi_clkrterm_ctrl));
	eInputPortType = mtk_hdmirx_to_muxinputport(
				(enum v4l2_srccap_input_source)stHDMI_CLK.enInputPortType);
	MDrv_DVI_ClkPullLow(stHDMI_CLK.bPullLow, eInputPortType);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_arcpin_control(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_enable  stHDCP_ENABLE;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&stHDCP_ENABLE, ctrl->p_new.p_u8, sizeof(struct v4l2_ext_hdmi_enable));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDCP_ENABLE.enInputPortType);
	MDrv_HDMI_ARC_PINControl(eInputPortType,
		stHDCP_ENABLE.bEnable, 0);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_audio_mute(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_mux_ioctl_s my_mux_s;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy((void *)&my_mux_s, ctrl->p_new.p_u8,
		sizeof(struct v4l2_ext_hdmi_mux_ioctl_s));

	my_mux_s.bRet = mtk_srccap_hdmirx_pkt_audio_mute_en(
		my_mux_s.enInputPortType, my_mux_s.u32_param[0] ? true : false);
	memcpy(ctrl->p_new.p, (void *)&my_mux_s,
		sizeof(struct v4l2_ext_hdmi_mux_ioctl_s));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_set_current_port(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev;

	if (ctrl == NULL)
		return -EINVAL;

	srccap_dev =
		container_of(ctrl->handler, struct mtk_srccap_dev, hdmirx_ctrl_handler);

	mtk_hdmirx_set_current_port(srccap_dev);
	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_readdone(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_hdcp_readdone stReaddone;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stReaddone, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_hdcp_readdone));
	stReaddone.bReadFlag = MDrv_HDCP22_PollingReadDone(stReaddone.u8PortIdx);
	#if DEF_TEST_USE
		stReaddone.bReadFlag = TRUE;
	#endif
	memcpy(ctrl->p_new.p, &stReaddone, sizeof(struct v4l2_ext_hdmi_hdcp_readdone));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_gc_info(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_gc_info stGCinfo;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stGCinfo, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_gc_info));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stGCinfo.enInputPortType);
	stGCinfo.u16GCdata = MDrv_HDMI_GC_Info
		(eInputPortType, (HDMI_GControl_INFO_t)stGCinfo.gcontrol);
	#if DEF_TEST_USE
		stGCinfo.u16GCdata = DEF_TEST_CONST2 + stGCinfo.enInputPortType;
	#endif
	memcpy(ctrl->p_new.p, &stGCinfo, sizeof(struct v4l2_ext_hdmi_gc_info));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_err_status(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_err_status stErrStatus;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stErrStatus, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_err_status));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stErrStatus.enInputPortType);
	stErrStatus.u8RetValue = MDrv_HDMI_err_status_update
		(eInputPortType, stErrStatus.u8value, stErrStatus.bread);
#if DEF_TEST_USE
		stErrStatus.u8RetValue = DEF_TEST_CONST2 + stErrStatus.enInputPortType;
#endif
	memcpy(ctrl->p_new.p, &stErrStatus, sizeof(struct v4l2_ext_hdmi_err_status));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_acp(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_byte stHDMI_Byte;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Byte, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_byte));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Byte.enInputPortType);
	stHDMI_Byte.u8RetValue =
		MDrv_HDMI_audio_cp_hdr_info(eInputPortType);
#if DEF_TEST_USE
			stHDMI_Byte.u8RetValue = DEF_TEST_CONST2 + stHDMI_Byte.enInputPortType;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Byte, sizeof(struct v4l2_ext_hdmi_byte));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_pixel_repetition(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_byte stHDMI_Byte;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Byte, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_byte));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Byte.enInputPortType);
	stHDMI_Byte.u8RetValue =
		MDrv_HDMI_Get_Pixel_Repetition(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Byte.u8RetValue = DEF_TEST_CONST2 + stHDMI_Byte.enInputPortType;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Byte, sizeof(struct v4l2_ext_hdmi_byte));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_avi_ver(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_byte stHDMI_Byte;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Byte, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_byte));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Byte.enInputPortType);
	stHDMI_Byte.u8RetValue =
	MDrv_HDMI_Get_AVIInfoFrameVer(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Byte.u8RetValue = DEF_TEST_CONST2 + stHDMI_Byte.enInputPortType;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Byte, sizeof(struct v4l2_ext_hdmi_byte));

	return 0;

}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_avi_activeinfo(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool stHDMI_Bool;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Bool, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_bool));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Bool.enInputPortType);
	stHDMI_Bool.bRet =
		MDrv_HDMI_Get_AVIInfoActiveInfoPresent(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Bool.bRet = TRUE;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Bool, sizeof(struct v4l2_ext_hdmi_bool));

	return 0;

}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_ishdmimode(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool stHDMI_Bool;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Bool, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_bool));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Bool.enInputPortType);
	stHDMI_Bool.bRet = MDrv_HDMI_IsHDMI_Mode(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Bool.bRet = TRUE;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Bool, sizeof(struct v4l2_ext_hdmi_bool));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_check_4k2k(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool stHDMI_Bool;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Bool, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_bool));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Bool.enInputPortType);
	stHDMI_Bool.bRet = MDrv_HDMI_Check4K2K(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Bool.bRet = TRUE;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Bool, sizeof(stHDMI_Bool));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_check_addition_fmt(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_byte stHDMI_Byte;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Byte, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_byte));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Byte.enInputPortType);
	stHDMI_Byte.u8RetValue = MDrv_HDMI_Check_Additional_Format(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Byte.u8RetValue = DEF_TEST_CONST2 + stHDMI_Byte.enInputPortType;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Byte, sizeof(struct v4l2_ext_hdmi_byte));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_3d_structure(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_byte stHDMI_Byte;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Byte, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_byte));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Byte.enInputPortType);
	stHDMI_Byte.u8RetValue = MDrv_HDMI_Get_3D_Structure(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Byte.u8RetValue = DEF_TEST_CONST2 + stHDMI_Byte.enInputPortType;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Byte, sizeof(struct v4l2_ext_hdmi_byte));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_3d_ext_data(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_byte stHDMI_Byte;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Byte, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_byte));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Byte.enInputPortType);
	stHDMI_Byte.u8RetValue = MDrv_HDMI_Get_3D_Ext_Data(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Byte.u8RetValue = DEF_TEST_CONST2 + stHDMI_Byte.enInputPortType;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Byte, sizeof(struct v4l2_ext_hdmi_byte));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_3d_meta_field(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_meta_filed stMeta_field;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stMeta_field, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_meta_filed));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stMeta_field.enInputPortType);
	MDrv_HDMI_Get_3D_Meta_Field(
				eInputPortType,
				(sHDMI_3D_META_FIELD *)&stMeta_field.st3D_META_DATA);
#if DEF_TEST_USE
		stMeta_field.st3D_META_DATA.t3D_Metadata_Type = E_3D_META_DATA_MAX;
		stMeta_field.st3D_META_DATA.u83D_Metadata_Length =
			DEF_TEST_CONST2 + stMeta_field.enInputPortType;
		stMeta_field.st3D_META_DATA.b3D_Meta_Present = TRUE;
#endif
	memcpy(ctrl->p_new.p, &stMeta_field, sizeof(struct v4l2_ext_hdmi_meta_filed));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_source_ver(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_byte stHDMI_Byte;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Byte, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_byte));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Byte.enInputPortType);

	stHDMI_Byte.u8RetValue = MDrv_HDMI_GetSourceVersion(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Byte.u8RetValue = DEF_TEST_CONST2 + stHDMI_Byte.enInputPortType;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Byte, sizeof(struct v4l2_ext_hdmi_byte));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_de_stable_status(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool stHDMI_Bool;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Bool, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_bool));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Bool.enInputPortType);
	stHDMI_Bool.bRet = MDrv_HDMI_GetDEStableStatus(eInputPortType);
#if DEF_TEST_USE
		stHDMI_Bool.bRet = TRUE;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_Bool, sizeof(stHDMI_Bool));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_crc(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_crc_value stHDMI_CRC_Value;
	__u16 u16crc = 0;
	E_MUX_INPUTPORT eInputPortType;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_CRC_Value, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_crc_value));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_CRC_Value.enInputPortType);
	stHDMI_CRC_Value.bIsValid =
		MDrv_HDMI_GetCRCValue(
			eInputPortType,
			(MS_DVI_CHANNEL_TYPE)stHDMI_CRC_Value.u8Channel, &u16crc);
#if DEF_TEST_USE
		stHDMI_CRC_Value.bIsValid = TRUE;
		u16crc = DEF_TEST_CONST2 + stHDMI_CRC_Value.enInputPortType;
#endif
	ret = copy_to_user(stHDMI_CRC_Value.u16CRCVal, &u16crc, sizeof(__u16));

	if (ret != 0) {
		HDMIRX_MSG_ERROR("copy_to_user failed\r\n");
		return -EINVAL;
	}

	memcpy(ctrl->p_new.p, &stHDMI_CRC_Value, sizeof(stHDMI_CRC_Value));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_emp(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_emp_info stHDMI_EMP;
	__u8 u8TotalNum = 0;
	__u8 u8EmpData[HDMI_SINGLE_PKT_SIZE] = {0};
	unsigned long ret = 0;
	bool bret = FALSE;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_EMP, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_emp_info));

	HDMIRX_MSG_INFO(
		"Input Port : %d, EMP Type : %X\r\n",
		stHDMI_EMP.enInputPortType, stHDMI_EMP.enEmpType);

	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_EMP.enInputPortType);

	bret = MDrv_HDMI_Get_EMP(
			eInputPortType, (E_HDMI_EMPACKET_TYPE)stHDMI_EMP.enEmpType,
			stHDMI_EMP.u8CurrentPacketIndex, &u8TotalNum, u8EmpData);
#if DEF_TEST_USE
		stHDMI_EMP.u8CurrentPacketIndex =
				DEF_TEST_CONST2 + stHDMI_EMP.enInputPortType - DEF_TEST_CONST3;
		u8TotalNum = stHDMI_EMP.enInputPortType;
		u8EmpData[0] = DEF_TEST_CONST2 + stHDMI_EMP.enInputPortType;
		bret = TRUE;
#endif
	if (bret) {
		ret = copy_to_user(
			stHDMI_EMP.pu8TotalPacketNumber,
			&u8TotalNum, sizeof(__u8));
	}
	if (ret != 0) {
		HDMIRX_MSG_ERROR("copy_to_user EMP total\n"
			"number failed\r\n");
		return -EINVAL;
	}
	if (bret) {
		ret = copy_to_user(
			stHDMI_EMP.pu8EmPacket,
			(void *)&u8EmpData[0],
			sizeof(u8EmpData));
	}
	if (ret != 0) {
		HDMIRX_MSG_ERROR("copy_to_user EMP data\n"
			"failed\r\n");
		return -EINVAL;
	}

	memcpy(ctrl->p_new.p, &stHDMI_EMP, sizeof(struct v4l2_ext_hdmi_emp_info));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_ctrl stCtrl;
	int ret = 0;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stCtrl, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_ctrl));

	ret = _mtk_hdmirx_ctrl_process(
		stCtrl.u32Cmd,
		stCtrl.pBuf,
		stCtrl.u32BufSize);

	memcpy(ctrl->p_new.p, &stCtrl, sizeof(struct v4l2_ext_hdmi_ctrl));
	return ret;

}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_fs(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_word stHDMI_WORD;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDMI_WORD, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_word));

	stHDMI_WORD.u16RetValue = MDrv_HDMI_GetDataInfoByPort(
		E_HDMI_GET_FS,
		mtk_hdmirx_to_muxinputport(stHDMI_WORD.enInputPortType));
#if DEF_TEST_USE
		stHDMI_WORD.u16RetValue = DEF_TEST_CONST4;
#endif
	memcpy(ctrl->p_new.p, &stHDMI_WORD, sizeof(struct v4l2_ext_hdmi_word));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_set_dither(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_dither stHDMI_Dither;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&stHDMI_Dither, ctrl->p_new.p_u8, sizeof(struct v4l2_ext_hdmi_dither));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)stHDMI_Dither.enInputPortType);
	MDrv_HDMI_Dither_Set(eInputPortType,
		(E_DITHER_TYPE)stHDMI_Dither.enDitherType,
		stHDMI_Dither.ubRoundEnable);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_getmsg(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_hdcp22_msg stHDCPMsg;
	MS_U8 ucMsgData[HDMI_HDCP22_MESSAGE_LENGTH + 1] = {0};
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy(&stHDCPMsg, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_hdcp22_msg));

	MDrv_HDCP22_RecvMsg(stHDCPMsg.ucPortIdx, ucMsgData);
	stHDCPMsg.dwDataLen
		= ucMsgData[HDMI_HDCP22_MESSAGE_LENGTH];
#if DEF_TEST_USE
		stHDCPMsg.dwDataLen = DEF_TEST_CONST5;
		ucMsgData[0] = DEF_TEST_CONST5;
		ucMsgData[1] = DEF_TEST_CONST2 + stHDCPMsg.ucPortIdx;
#endif

	if (stHDCPMsg.dwDataLen > 0) {
		ret = copy_to_user(
				stHDCPMsg.pucData,
				(void *)&ucMsgData[0],
				sizeof(ucMsgData));
		if (ret != 0) {
			HDMIRX_MSG_ERROR("copy_to_user HDCP22\n"
							 "MSG failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, &stHDCPMsg, sizeof(struct v4l2_ext_hdmi_hdcp22_msg));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_avi(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_AVI(
		my_pkt_s.enInputPortType, (struct hdmi_avi_packet_info *)buf_ptr,
		my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
				sizeof(struct hdmi_avi_packet_info));
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR("copy_to_user PKT_GET_AVI failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_vsif(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_VSIF(
			my_pkt_s.enInputPortType, (struct hdmi_vsif_packet_info *)buf_ptr,
			my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
				sizeof(struct hdmi_vsif_packet_info) *
				my_pkt_s.u8Ret_n);
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR("copy_to_user PKT_GET_VSIF failed\r\n");
			return -EIO;
		}
	}

	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_gcp(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_GCP(
		my_pkt_s.enInputPortType, (struct hdmi_gc_packet_info *)buf_ptr,
		my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
				sizeof(struct hdmi_gc_packet_info) *
				my_pkt_s.u8Ret_n);
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR("copy_to_user GET_GCP failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_hdrif(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_HDRIF(
		my_pkt_s.enInputPortType, (struct hdmi_hdr_packet_info *)buf_ptr,
		my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
				sizeof(struct hdmi_hdr_packet_info) *
				my_pkt_s.u8Ret_n);
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR("copy_to_user GET_HDRIF failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_vs_emp(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_VS_EMP(
		my_pkt_s.enInputPortType, (struct hdmi_emp_packet_info *)buf_ptr,
		my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
				sizeof(struct hdmi_emp_packet_info) *
				my_pkt_s.u8Ret_n);
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR("copy_to_user GET_VS_EMP failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_dsc_emp(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_DSC_EMP(
		my_pkt_s.enInputPortType, (struct hdmi_emp_packet_info *)buf_ptr,
		my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
				sizeof(struct hdmi_emp_packet_info) *
				my_pkt_s.u8Ret_n);
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR("copy_to_user GET_DSC_EMP failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_dynamic_hdr_emp(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_Dynamic_HDR_EMP(
		my_pkt_s.enInputPortType, (struct hdmi_emp_packet_info *)buf_ptr,
		my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
				sizeof(struct hdmi_emp_packet_info) *
				my_pkt_s.u8Ret_n);
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR(
				"copy_to_user GET_DYNAMIC_HDR_EMP failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_vrr_emp(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_VRR_EMP(
		my_pkt_s.enInputPortType, (struct hdmi_emp_packet_info *)buf_ptr,
		my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
				sizeof(struct hdmi_emp_packet_info) *
				my_pkt_s.u8Ret_n);
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR("copy_to_user GET_VRR_EMP failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_report(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	struct v4l2_ext_hdmi_pkt_report_s report_buf;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	my_pkt_s.u8Ret_n =
		(MS_U8)mtk_srccap_hdmirx_pkt_get_report(
			my_pkt_s.enInputPortType,
			my_pkt_s.e_pkt_type, &report_buf);
	ret = copy_to_user(
			my_pkt_s.pBuf, (void *)&report_buf,
			sizeof(struct v4l2_ext_hdmi_pkt_report_s));
	if (ret != 0) {
		HDMIRX_MSG_ERROR("copy_to_user PKT_GET_REPORT failed\r\n");
		return -EIO;
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_is_dsc(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool my_bool_s;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_bool_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_bool));
	my_bool_s.bRet = mtk_srccap_hdmirx_pkt_is_dsc(my_bool_s.enInputPortType);
	memcpy(ctrl->p_new.p, (void *)&my_bool_s,
		   sizeof(struct v4l2_ext_hdmi_bool));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_is_vrr(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool my_bool_s;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_bool_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_bool));
	my_bool_s.bRet = mtk_srccap_hdmirx_pkt_is_vrr(my_bool_s.enInputPortType);
	memcpy(ctrl->p_new.p, (void *)&my_bool_s,
		   sizeof(struct v4l2_ext_hdmi_bool));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_dsc_get_pps_info(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_dsc_info report_buf;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&report_buf, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_dsc_info));
	report_buf.bRet = mtk_srccap_hdmirx_dsc_get_pps_info(
						  report_buf.enInputPortType,
						  &report_buf) == TRUE
						  ? 1
						  : 0;
	memcpy(ctrl->p_new.p, (void *)&report_buf,
		   sizeof(struct v4l2_ext_hdmi_dsc_info));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_mux_sel_dsc(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_mux_ioctl_s my_mux_s;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_mux_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_mux_ioctl_s));
	my_mux_s.bRet = mtk_srccap_hdmirx_mux_sel_dsc(
						my_mux_s.enInputPortType,
						my_mux_s.dsc_mux_n);
	memcpy(ctrl->p_new.p, (void *)&my_mux_s,
		   sizeof(struct v4l2_ext_hdmi_mux_ioctl_s));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_mux_query_dsc(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_mux_ioctl_s my_mux_s;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_mux_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_mux_ioctl_s));
	my_mux_s.enInputPortType =
		mtk_srccap_hdmirx_mux_query_dsc(my_mux_s.dsc_mux_n);
	memcpy(ctrl->p_new.p, (void *)&my_mux_s,
		   sizeof(struct v4l2_ext_hdmi_mux_ioctl_s));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_gnl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_pkt my_pkt_s;
	MS_U8 *buf_ptr = NULL;
	unsigned long ret;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_pkt_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_pkt));

	buf_ptr = kmalloc(my_pkt_s.u32BufSize, GFP_KERNEL);
	if (!buf_ptr) {
		HDMIRX_MSG_ERROR("%s,%d, FAIL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	my_pkt_s.u8Ret_n = mtk_srccap_hdmirx_pkt_get_gnl(
		my_pkt_s.enInputPortType, my_pkt_s.e_pkt_type,
		(struct hdmi_packet_info *)buf_ptr, my_pkt_s.u32BufSize);
	if (my_pkt_s.u8Ret_n) {
		ret = copy_to_user(
				my_pkt_s.pBuf, (void *)buf_ptr,
					sizeof(struct hdmi_packet_info) *
					my_pkt_s.u8Ret_n);
		if (ret != 0) {
			kfree(buf_ptr);
			HDMIRX_MSG_ERROR("copy_to_user PKT_GET_GNL failed\r\n");
			return -EIO;
		}
	}
	memcpy(ctrl->p_new.p, (void *)&my_pkt_s,
		   sizeof(struct v4l2_ext_hdmi_pkt));
	kfree(buf_ptr);

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_set_sim_mode(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool shdmi_bool;
	struct mtk_srccap_dev *srccap_dev;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	srccap_dev =
			container_of(ctrl->handler, struct mtk_srccap_dev, hdmirx_ctrl_handler);

	memcpy(&shdmi_bool, ctrl->p_new.p, sizeof(struct v4l2_ext_hdmi_bool));
	HDMIRX_MSG_DBG("bRet [%d] \r\n", shdmi_bool.bRet);

	if (shdmi_bool.bRet) {
		// u8_save = 1
		_mtk_srccap_hdmirx_irq_save_restore(1);
		_hdmirx_irq_suspend(srccap_dev);
	}

	KDrv_SRCCAP_HDMIRx_setSimMode(shdmi_bool.bRet);

	MDrv_HDMI_Set_Init_Stub();

	if (!shdmi_bool.bRet) {
		_hdmirx_irq_resume(srccap_dev);
		// u8_save = 0
		_mtk_srccap_hdmirx_irq_save_restore(0);
	}

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_set_sim_data(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_sim_data *pshdmi_sim_data;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	pshdmi_sim_data = kmalloc(sizeof(struct v4l2_ext_hdmi_sim_data), GFP_KERNEL);
	if (pshdmi_sim_data == NULL) {
		HDMIRX_MSG_ERROR("Get buffer memory Error\r\n");
		return -ENOMEM;
	}
	memcpy(pshdmi_sim_data, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_sim_data));
	HDMIRX_MSG_DBG("u8data [%p] \r\n", pshdmi_sim_data->u8data);
	KDrv_SRCCAP_HDMIRx_setSimData(pshdmi_sim_data->u8data);
	kfree(pshdmi_sim_data);

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_audio_fs(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_get_audio_sampling_freq my_audio_fs;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_audio_fs,
		ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_get_audio_sampling_freq));

	my_audio_fs.u32RetValue_hz =
		mtk_srccap_hdmirx_pkt_get_audio_fs_hz(my_audio_fs.enInputPortType);

	memcpy(ctrl->p_new.p, (void *)&my_audio_fs,
		sizeof(struct v4l2_ext_hdmi_get_audio_sampling_freq));

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_set_pwr_saving_onoff(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_set_pwr_saving_onoff my_set_pwr_onoff;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_set_pwr_onoff,
		ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_set_pwr_saving_onoff));

	my_set_pwr_onoff.bRet =
		mtk_srccap_hdmirx_set_pwr_saving_onoff(
			my_set_pwr_onoff.enInputPortType,
			my_set_pwr_onoff.b_on);

	memcpy(ctrl->p_new.p, (void *)&my_set_pwr_onoff,
		sizeof(struct v4l2_ext_hdmi_set_pwr_saving_onoff));

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_pwr_saving_onoff(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_get_pwr_saving_onoff my_get_pwr_onoff;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_get_pwr_onoff, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_get_pwr_saving_onoff));

	my_get_pwr_onoff.bRet_hwdef
		= mtk_srccap_hdmirx_get_pwr_saving_onoff_hwdef(
		my_get_pwr_onoff.enInputPortType);

	my_get_pwr_onoff.bRet_swprog
		= mtk_srccap_hdmirx_get_pwr_saving_onoff(
		my_get_pwr_onoff.enInputPortType);

	memcpy(ctrl->p_new.p, (void *)&my_get_pwr_onoff,
		sizeof(struct v4l2_ext_hdmi_get_pwr_saving_onoff));

	return 0;
}


static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_hdmi_status(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool my_bool_s;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_bool_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_bool));
	my_bool_s.bRet =
		mtk_srccap_hdmirx_get_HDMIStatus(my_bool_s.enInputPortType);
	memcpy(ctrl->p_new.p, (void *)&my_bool_s,
		   sizeof(struct v4l2_ext_hdmi_bool));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_clk_status(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_bool my_bool_s;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_bool_s, ctrl->p_new.p,
		   sizeof(struct v4l2_ext_hdmi_bool));
	my_bool_s.bRet =
		mtk_srccap_hdmirx_get_ClkStatus(my_bool_s.enInputPortType);
	memcpy(ctrl->p_new.p, (void *)&my_bool_s,
		   sizeof(struct v4l2_ext_hdmi_bool));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_get_freesync_info(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_get_freesync_info my_get_freesync_info;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_get_freesync_info, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_get_freesync_info));
	my_get_freesync_info.bRet = mtk_srccap_hdmirx_get_freesync_info(
		my_get_freesync_info.enInputPortType, &my_get_freesync_info);
	memcpy(ctrl->p_new.p, (void *)&my_get_freesync_info,
		sizeof(struct v4l2_ext_hdmi_get_freesync_info));

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_set_scdcval(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_set_scdc_value my_set_scdc;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p == NULL)
		return -EINVAL;

	memcpy((void *)&my_set_scdc, ctrl->p_new.p,
		sizeof(struct v4l2_ext_hdmi_set_scdc_value));
	eInputPortType = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)my_set_scdc.enInputPortType);
	mtk_srccap_hdmirx_set_scdcval(eInputPortType,
		my_set_scdc.ucOffset, my_set_scdc.ucData);

	return 0;
}

static int
_hdmirx_sctrl_v4l2_cid_hdmi_rx_ddc_bus_control(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ext_hdmi_ddc_ctrl my_ddc_ctrl;
	E_MUX_INPUTPORT eInputPortType;

	if (ctrl == NULL || ctrl->p_new.p_u8 == NULL)
		return -EINVAL;

	memcpy(&my_ddc_ctrl, ctrl->p_new.p_u8, sizeof(struct v4l2_ext_hdmi_ddc_ctrl));
	eInputPortType = mtk_hdmirx_to_muxinputport(
			(enum v4l2_srccap_input_source)my_ddc_ctrl.enInputPortType);
	mtk_srccap_hdmirx_ddc_bus_control(my_ddc_ctrl.bEnable, eInputPortType);

	return 0;
}

static int mtk_hdmirx_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	if (ctrl == NULL) {
		HDMIRX_MSG_ERROR("Get ctrls Error\r\n");
		return -EINVAL;
	}

	if (mutex_is_locked(&hdmirx_mutex))
		HDMIRX_MSG_INFO("%s locked priv_lock_id = 0x%x\r\n", __func__, priv_lock_id);

	mutex_lock(&hdmirx_mutex);
	priv_lock_id = ctrl->id;

	switch (ctrl->id) {
	case V4L2_CID_HDMI_RX_GET_IT_CONTENT_TYPE: {
		ret = _hdmirx_gctrl_v4l2_cid_hdmi_rx_get_it_content_type(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_SIM_MODE: {
		ret = _hdmirx_gctrl_v4l2_cid_hdmi_rx_get_sim_mode(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_CABLE_DETECT: {
		ret = _hdmirx_gctrl_v4l2_cid_hdmi_rx_get_cable_detect(ctrl);
	} break;

	default:
		HDMIRX_MSG_ERROR("Wrong CID\r\n");
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&hdmirx_mutex);
	return ret;
}


static int mtk_hdmirx_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	if (ctrl == NULL) {
		HDMIRX_MSG_ERROR("Set ctrls Error\r\n");
		return -EINVAL;
	}

	if (mutex_is_locked(&hdmirx_mutex))
		HDMIRX_MSG_INFO("%s locked priv_lock_id = 0x%x\r\n", __func__, priv_lock_id);

	mutex_lock(&hdmirx_mutex);
	priv_lock_id = ctrl->id;


	switch (ctrl->id) {
	case V4L2_CID_HDMI_RX_HDCP22_IRQ_EANBLE: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_irq_eanble(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_HDCP22_PORTINIT: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_portinit(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_HDCP22_SENDMSG: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_sendmsg(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_HDCP14_R0IRQENABLE: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp14_r0irqenable(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_SET_EQ_TO_PORT: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_set_eq_to_port(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_AUDIO_STATUS_CLEAR: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_audio_status_clear(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_HPD_CONTROL: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_hpd_control(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_SW_RESET: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_sw_reset(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_RESET: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_reset(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_CLKRTERM_CONTROL: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_clkrterm_control(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_ARCPIN_CONTROL: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_arcpin_control(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_AUDIO_MUTE: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_audio_mute(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_SET_CURRENT_PORT: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_set_current_port(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_HDCP22_READDONE: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_readdone(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_GC_INFO: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_gc_info(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_ERR_STATUS: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_err_status(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_ACP: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_acp(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_PIXEL_REPETITION: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_pixel_repetition(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_AVI_VER: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_avi_ver(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_AVI_ACTIVEINFO: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_avi_activeinfo(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_ISHDMIMODE: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_ishdmimode(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_CHECK_4K2K: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_check_4k2k(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_CHECK_ADDITION_FMT: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_check_addition_fmt(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_3D_STRUCTURE: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_3d_structure(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_3D_EXT_DATA: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_3d_ext_data(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_3D_META_FIELD: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_3d_meta_field(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_SOURCE_VER: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_source_ver(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_DE_STABLE_STATUS: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_de_stable_status(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_CRC: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_crc(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_EMP: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_emp(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_CTRL: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_ctrl(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_FS: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_fs(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_SET_DITHER: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_set_dither(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_HDCP22_GETMSG: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_hdcp22_getmsg(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_AVI: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_avi(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_VSIF: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_vsif(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_GCP: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_gcp(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_HDRIF: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_hdrif(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_VS_EMP: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_vs_emp(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_DSC_EMP: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_dsc_emp(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_DYNAMIC_HDR_EMP: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_dynamic_hdr_emp(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_VRR_EMP: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_vrr_emp(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_REPORT: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_report(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_IS_DSC: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_is_dsc(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_IS_VRR: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_is_vrr(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_DSC_GET_PPS_INFO: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_dsc_get_pps_info(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_MUX_SEL_DSC: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_mux_sel_dsc(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_MUX_QUERY_DSC: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_mux_query_dsc(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_PKT_GET_GNL: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_pkt_get_gnl(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_SET_SIM_MODE: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_set_sim_mode(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_SET_SIM_DATA: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_set_sim_data(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_AUDIO_FS: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_audio_fs(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_SET_PWR_SAVING_ONOFF: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_set_pwr_saving_onoff(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_PWR_SAVING_ONOFF: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_pwr_saving_onoff(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_HDMI_STATUS: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_hdmi_status(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_FREESYNC_INFO: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_freesync_info(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_GET_CLK_STATUS: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_get_clk_status(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_SET_SCDCVAL: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_set_scdcval(ctrl);
	} break;

	case V4L2_CID_HDMI_RX_DDC_BUS_CONTROL: {
		ret = _hdmirx_sctrl_v4l2_cid_hdmi_rx_ddc_bus_control(ctrl);
	} break;

	default:
		HDMIRX_MSG_ERROR("Wrong CID\r\n");
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&hdmirx_mutex);
	return ret;
}

static const struct v4l2_ctrl_ops hdmirx_ctrl_ops = {
	.g_volatile_ctrl = mtk_hdmirx_g_ctrl,
	.s_ctrl = mtk_hdmirx_s_ctrl,
};

static const struct v4l2_ctrl_config hdmirx_ctrl[] = {
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_IT_CONTENT_TYPE,
		.name = "HDMI Get IT Content Type",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_HDCP22_READDONE,
		.name = "HDMI Get HDCP22 Read Done",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_hdcp_readdone)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_GC_INFO,
		.name = "HDMI Get GC",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_gc_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_ERR_STATUS,
		.name = "HDMI Get Error Status",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_err_status)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_ACP,
		.name = "HDMI Get ACP",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_byte)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_PIXEL_REPETITION,
		.name = "HDMI Get Pexel Repetition",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_byte)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_AVI_VER,
		.name = "HDMI Get AVI Version",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_byte)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_AVI_ACTIVEINFO,
		.name = "HDMI Get AVI ActiveInfo",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_ISHDMIMODE,
		.name = "HDMI Get Is HDMI mode",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_CHECK_4K2K,
		.name = "HDMI Get 4k2k info",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_CHECK_ADDITION_FMT,
		.name = "HDMI Get Addition Format",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_byte)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_3D_STRUCTURE,
		.name = "HDMI Get 3D Structure",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_byte)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_3D_EXT_DATA,
		.name = "HDMI Get 3D Ext Data",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_byte)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_3D_META_FIELD,
		.name = "HDMI Get 3D Meta Field",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_3d_meta_filed)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_SOURCE_VER,
		.name = "HDMI Get Source Version",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_byte)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_DE_STABLE_STATUS,
		.name = "HDMI Get DE Stable",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_CRC,
		.name = "HDMI Get CRC",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_crc_value)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_EMP,
		.name = "HDMI Get EMP Packet contetn",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_emp_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_FS,
		.name = "HDMI Rx Get FS",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_word)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_CTRL,
		.name = "HDMI Rx Ctrl Command",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_ctrl)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_CABLE_DETECT,
		.name = "HDMI Rx Get Cable Detect",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_cmd_get_cable_detect) * 4},
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_SET_CURRENT_PORT,
		.name = "HDMI Port Set",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 3,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_HDCP22_IRQ_EANBLE,
		.name = "HDCP22 IRQ ENABLE",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 255,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_HDCP22_PORTINIT,
		.name = "HDCP22 Port Init",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 3,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_HDCP22_GETMSG,
		.name = "HDMI Rx Get HDCP22 MSG",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_hdcp22_msg)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_HDCP22_SENDMSG,
		.name = "HDCP22 Send MSG",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_hdcp22_msg)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_HDCP14_R0IRQENABLE,
		.name = "HDCP14 R0IRQ Enable",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 255,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_SET_EQ_TO_PORT,
		.name = "HDMI Set EQ",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_eq)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_AUDIO_STATUS_CLEAR,
		.name = "Audio Status Clear",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_HPD_CONTROL,
		.name = "HPD Control",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_hpd)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_SW_RESET,
		.name = "HDMI SE Reset",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(ST_HDMI_RESET)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_RESET,
		.name = "HDMI PKT Reset",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(ST_HDMI_PACKET_RESET)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_CLKRTERM_CONTROL,
		.name = "HDMI Clock Control",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_clkrterm_ctrl)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_ARCPIN_CONTROL,
		.name = "HDMI ARC Control",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_enable)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_AUDIO_MUTE,
		.name = "HDMI Audio Mute",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_mux_ioctl_s)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_SET_DITHER,
		.name = "HDMI Set Dither",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_dither)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_AVI,
		.name = "HDMI Get PKT AVI",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_VSIF,
		.name = "HDMI Get PKT VSIF",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_GCP,
		.name = "HDMI Get PKT GCP",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_HDRIF,
		.name = "HDMI Get PKT HDRIF",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_VS_EMP,
		.name = "HDMI Get PKT VS EMP",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_DSC_EMP,
		.name = "HDMI Get PKT DSC EMP",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_DYNAMIC_HDR_EMP,
		.name = "HDMI Get PKT DYNAMIC HDR EMP",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_VRR_EMP,
		.name = "HDMI Get PKT VRR EMP",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_REPORT,
		.name = "HDMI Get PKT REPORT",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_IS_DSC,
		.name = "HDMI PKT IS DSC",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_IS_VRR,
		.name = "HDMI PKT IS VRR",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_DSC_GET_PPS_INFO,
		.name = "HDMI DSC GET PPS INFO",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_dsc_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_MUX_SEL_DSC,
		.name = "HDMI MUX SEL DSC",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_mux_ioctl_s)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_MUX_QUERY_DSC,
		.name = "HDMI MUX QUERY DSC",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_mux_ioctl_s)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_PKT_GET_GNL,
		.name = "HDMI  Get PKT General",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_pkt)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_SET_SIM_MODE,
		.name = "HDMI Set Sim Mode",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_SET_SIM_DATA,
		.name = "HDMI Set Sim Data",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_sim_data)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_SIM_MODE,
		.name = "HDMI Get Sim Mode",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_AUDIO_FS,
		.name = "HDMI GET AUDIO SAMPLING FREQUENCY HZ",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_get_audio_sampling_freq)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_SET_PWR_SAVING_ONOFF,
		.name = "HDMI SET PWR SAVING ON/OFF",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_set_pwr_saving_onoff)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_PWR_SAVING_ONOFF,
		.name = "HDMI GET PWR SAVING ON/OFF",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_get_pwr_saving_onoff)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_HDMI_STATUS,
		.name = "HDMI GET HDMI STATUS",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_FREESYNC_INFO,
		.name = "HDMI GET FREESYNC INFO",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_get_freesync_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_GET_CLK_STATUS,
		.name = "HDMI GET CLK STATUS",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_bool)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_SET_SCDCVAL,
		.name = "HDMI SET SCDC VALUE",
		.type = V4L2_CTRL_COMPOUND_TYPES,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_set_scdc_value)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &hdmirx_ctrl_ops,
		.id = V4L2_CID_HDMI_RX_DDC_BUS_CONTROL,
		.name = "DDC Bus Control",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_ext_hdmi_ddc_ctrl)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops hdmirx_sd_ops = {};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops hdmirx_sd_internal_ops = {};

int mtk_srccap_register_hdmirx_subdev(
	struct v4l2_device *v4l2_dev,
	struct v4l2_subdev *hdmirx_subdev,
	struct v4l2_ctrl_handler *hdmirx_ctrl_handler)
{
	int ret = 0;
	u32 ctrl_count;
	u32 ctrl_num = sizeof(hdmirx_ctrl) / sizeof(struct v4l2_ctrl_config);
	HDMIRX_MSG_DBG("mdbgin_%s: start \r\n", __func__);
	v4l2_ctrl_handler_init(hdmirx_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(hdmirx_ctrl_handler, &hdmirx_ctrl[ctrl_count], NULL);
	}

	ret = hdmirx_ctrl_handler->error;
	if (ret) {
		SRCCAP_MSG_ERROR("failed to create hdmirx ctrl handler\n");
		goto exit;
	}

	hdmirx_subdev->ctrl_handler = hdmirx_ctrl_handler;

	v4l2_subdev_init(hdmirx_subdev, &hdmirx_sd_ops);
	hdmirx_subdev->internal_ops = &hdmirx_sd_internal_ops;
	strlcpy(hdmirx_subdev->name, "mtk-hdmirx", sizeof(hdmirx_subdev->name));

	ret = v4l2_device_register_subdev(v4l2_dev, hdmirx_subdev);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to register hdmirx subdev\n");
		goto exit;
	}
	return 0;

exit:
	v4l2_ctrl_handler_free(hdmirx_ctrl_handler);
	HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, ret);

	return ret;
}

void mtk_srccap_unregister_hdmirx_subdev(struct v4l2_subdev *hdmirx_subdev)
{
	HDMIRX_MSG_DBG("mdbgin_%s: start \r\n", __func__);
	v4l2_ctrl_handler_free(hdmirx_subdev->ctrl_handler);
	v4l2_device_unregister_subdev(hdmirx_subdev);
}

static int mtk_srccap_read_dts_u32(struct device *property_dev,
			struct device_node *node, char *s, u32 *val)
{
	int ret = 0;

	if (node == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (s == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (val == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	ret = of_property_read_u32(node, s, val);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	HDMIRX_MSG_INFO("%s = 0x%x(%u)\n", s, *val, *val);
	return ret;
}

int mtk_srccap_hdmirx_parse_dts_reg(struct mtk_srccap_dev *srccap_dev,
									struct device *property_dev)
{
#define BUF_LEN_MAX 20
	int ret = 0;
	struct device_node *hdmirx_node;
	struct device_node *capability_node;
	struct device_node *bank_node;

	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);
	if (srccap_dev->dev_id == 0) {
		struct of_mmap_info_data of_mmap_info_hdmi = {0};

		memset(&stHDMIRxBank, 0, sizeof(stHDMIRx_Bank));

		hdmirx_node = of_find_node_by_name(property_dev->of_node, "hdmirx");
		if (hdmirx_node == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, -ENOENT);
			return -ENOENT;
		}

		of_mtk_get_reserved_memory_info_by_idx(hdmirx_node, 0, &of_mmap_info_hdmi);
		stHDMIRxBank.pExchangeDataPA = (u8 *)of_mmap_info_hdmi.start_bus_address;
		stHDMIRxBank.u32ExchangeDataSize = of_mmap_info_hdmi.buffer_size;

		HDMIRX_MSG_INFO("ExchangeDataPA is 0x%llX\n",
						(u64)stHDMIRxBank.pExchangeDataPA);
		HDMIRX_MSG_INFO("ExchangeDataSize is 0x%X\n",
						(u32)stHDMIRxBank.u32ExchangeDataSize);

		ret |= get_memory_info((u64 *)&stHDMIRxBank.pMemoryBusBase);
		HDMIRX_MSG_INFO("MemoryBusBase is 0x%llX\n",
						(u64)stHDMIRxBank.pMemoryBusBase);

		capability_node = of_find_node_by_name(property_dev->of_node, "capability");
		if (capability_node == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, -ENOENT);
			return -ENOENT;
		}

		bank_node = of_find_node_by_name(hdmirx_node, "banks");
		if (bank_node == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, -ENOENT);
			return -ENOENT;
		}

		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_hdmirx",
					&stHDMIRxBank.bank_hdmirx);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_hdmirx_end",
					&stHDMIRxBank.bank_hdmirx_end);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_pm_slp",
					&stHDMIRxBank.bank_pm_slp);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_pm_ioctl",
					&stHDMIRxBank.bank_pm_ioctl);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_ddc",
					&stHDMIRxBank.bank_ddc);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_scdc_p0",
					&stHDMIRxBank.bank_scdc_p0);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_scdc_xa8_p0",
					&stHDMIRxBank.bank_scdc_xa8_p0);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_scdc_p1",
					&stHDMIRxBank.bank_scdc_p1);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_scdc_xa8_p1",
					&stHDMIRxBank.bank_scdc_xa8_p1);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_scdc_p2",
					&stHDMIRxBank.bank_scdc_p2);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_scdc_xa8_p2",
					&stHDMIRxBank.bank_scdc_xa8_p2);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_scdc_p3",
					&stHDMIRxBank.bank_scdc_p3);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_scdc_xa8_p3",
					&stHDMIRxBank.bank_scdc_xa8_p3);
		ret |=
			mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_hdmirxphy_pm_p0",
					&stHDMIRxBank.bank_hdmirxphy_pm_p0);
		ret |=
			mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_hdmirxphy_pm_p1",
					&stHDMIRxBank.bank_hdmirxphy_pm_p1);
		ret |=
			mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_hdmirxphy_pm_p2",
					&stHDMIRxBank.bank_hdmirxphy_pm_p2);
		ret |=
			mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_hdmirxphy_pm_p3",
					&stHDMIRxBank.bank_hdmirxphy_pm_p3);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_efuse_0",
					&stHDMIRxBank.bank_efuse_0);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_pm_top",
					&stHDMIRxBank.bank_pm_top);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_pmux",
					 &stHDMIRxBank.bank_pmux);
		ret |= mtk_srccap_read_dts_u32(property_dev, bank_node, "bank_dig_mux",
					&stHDMIRxBank.bank_dig_mux);

		ret |= mtk_srccap_read_dts_u32(property_dev, capability_node, "hdmi_count",
					&srccap_dev->hdmirx_info.port_count);
		stHDMIRxBank.port_num = srccap_dev->hdmirx_info.port_count;

		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, -ENOENT);

			return -ENOENT;
		}
		// mokona remove
		if (srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_4
			&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_5
			&& srccap_dev->srccap_info.cap.hw_version != SRCCAP_HW_VERSION_6) {
			srccap_dev->hdmirx_info.hdmi_r2_ck =
				devm_clk_get(property_dev, "hdmi_r2_ck");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.hdmi_r2_ck)) {
				HDMIRX_MSG_ERROR("Get HDMIRx get hdmi_r2_ck failed!!\r\n");
				ret = PTR_ERR(srccap_dev->hdmirx_info.hdmi_r2_ck);
				HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, ret);

				return ret;
			}
			srccap_dev->hdmirx_info.hdmi_mcu_ck =
				devm_clk_get(property_dev, "hdmi_mcu_ck");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.hdmi_mcu_ck)) {
				HDMIRX_MSG_ERROR("Get HDMIRx get hdmi_mcu_ck failed!!\r\n");
				ret = PTR_ERR(srccap_dev->hdmirx_info.hdmi_mcu_ck);
				HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, ret);

				return ret;
			}
			srccap_dev->hdmirx_info.hdmi_en_clk_xtal =
				devm_clk_get(property_dev, "hdmi_en_clk_xtal");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.hdmi_en_clk_xtal)) {
				HDMIRX_MSG_ERROR("Get HDMIRx get hdmi_en_clk_xtal failed!!\r\n");
				ret = PTR_ERR(srccap_dev->hdmirx_info.hdmi_en_clk_xtal);
				HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, ret);

				return ret;
			}
			srccap_dev->hdmirx_info.hdmi_en_clk_r2_hdmi2mac =
				devm_clk_get(property_dev, "hdmi_en_clk_r2_hdmi2mac");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.hdmi_en_clk_r2_hdmi2mac)) {
				HDMIRX_MSG_ERROR("Get HDMIRx hdmi_en_clk_r2_hdmi2mac failed!!\r\n");
				ret = PTR_ERR(srccap_dev->hdmirx_info.hdmi_en_clk_r2_hdmi2mac);
				HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, ret);

				return ret;
			}
			srccap_dev->hdmirx_info.hdmi_en_smi2mac =
				devm_clk_get(property_dev, "hdmi_en_smi2mac");
			if (IS_ERR_OR_NULL(srccap_dev->hdmirx_info.hdmi_en_smi2mac)) {
				HDMIRX_MSG_ERROR("Get HDMIRx get hdmi_en_smi2mac failed!!\r\n");
				ret = PTR_ERR(srccap_dev->hdmirx_info.hdmi_en_smi2mac);
				HDMIRX_MSG_ERROR("mdbgerr_%s: %d\r\n", __func__, ret);
			}
		}
	}

	return ret;
}

static int _hdmirx_enable(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u32 hdmi_mcu_clk_rate = 0;
	u32 hdmi_r2_clk_rate = 0;

	ret = clk_prepare_enable(srccap_dev->hdmirx_info.hdmi_mcu_ck);
	hdmi_mcu_clk_rate =
		clk_round_rate(srccap_dev->hdmirx_info.hdmi_mcu_ck, 24000000);
	ret = clk_set_rate(srccap_dev->hdmirx_info.hdmi_mcu_ck, hdmi_mcu_clk_rate);
	clk_prepare_enable(srccap_dev->hdmirx_info.hdmi_en_clk_xtal);
	clk_prepare_enable(srccap_dev->hdmirx_info.hdmi_en_smi2mac);

	clk_prepare_enable(srccap_dev->hdmirx_info.hdmi_r2_ck);
	hdmi_r2_clk_rate = clk_round_rate(srccap_dev->hdmirx_info.hdmi_r2_ck, 432000000);
	ret = clk_set_rate(srccap_dev->hdmirx_info.hdmi_r2_ck, hdmi_r2_clk_rate);
	clk_prepare_enable(srccap_dev->hdmirx_info.hdmi_en_clk_r2_hdmi2mac);

	return ret;
}

static int _hdmirx_disable(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);

	clk_disable_unprepare(srccap_dev->hdmirx_info.hdmi_en_clk_r2_hdmi2mac);
	clk_disable_unprepare(srccap_dev->hdmirx_info.hdmi_r2_ck);
	clk_disable_unprepare(srccap_dev->hdmirx_info.hdmi_en_smi2mac);
	clk_disable_unprepare(srccap_dev->hdmirx_info.hdmi_en_clk_xtal);
	clk_disable_unprepare(srccap_dev->hdmirx_info.hdmi_mcu_ck);

	return ret;
}

int _hdmirx_hw_init(struct mtk_srccap_dev *srccap_dev, int hw_version)
{
	int ret = 0;

	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);

	if (!gHDMIRxInitFlag) {
		pV4l2SrccapPortMap = (struct srccap_port_map *)srccap_dev->srccap_info.map;

//#ifndef CONFIG_MSTAR_ARM_BD_FPGA
		MDrv_HDMI_init(hw_version, &stHDMIRxBank);
//#endif
		gHDMIRxInitFlag = TRUE;
	}

	return ret;
}

static int _hdmirx_hw_deinit(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);

	if (gHDMIRxInitFlag) {
		if (!MDrv_HDMI_deinit(
			srccap_dev->srccap_info.cap.u32HDMI_Srccap_HWVersion,
			&stHDMIRxBank))
			ret = -1;

		gHDMIRxInitFlag = FALSE;
	} else
		ret = -1;
	return ret;
}

static irqreturn_t mtk_hdmirx_isr_phy(int irq, void *priv)
{
	unsigned int mask_save =
		MDrv_HDMI_IRQ_MASK_SAVE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_PHY));

	MDrv_HDMI_ISR_PHY();
	MDrv_HDMI_IRQ_MASK_RESTORE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_PHY),
							   mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}


static MS_BOOL mtk_hdmirx_hdcp_irq_evt(void)
{

	E_MUX_INPUTPORT enInputPortType = INPUT_PORT_DVI0;
	MS_BOOL ret_b = FALSE;

	for (enInputPortType = INPUT_PORT_DVI0;
			 enInputPortType <= INPUT_PORT_DVI_END;
			 enInputPortType++) {
		if (KDrv_SRCCAP_HDMIRx_hdcp_irq_evt(enInputPortType)) {
			ret_b = TRUE;
			break;
		}
	}
	return ret_b;
}

static void _irq_handler_mac_dec_misc(
	void *priv,
	E_MUX_INPUTPORT enInputPortType,
	enum E_MAC_TOP_ISR_EVT e_evt)
{
	// px_dec handler
	KDrv_SRCCAP_HDMIRx_mac_isr(enInputPortType, e_evt);
	// drv_hdmirx_irq_frl();
	// drv_hdmirx_irq_pkt();
	// drv_hdmirx_irq_tmds();
	// drv_hdmirx_irq_hdcp();
}

static void _irq_handler_mac_dep_misc(
	void *priv,
	E_MUX_INPUTPORT enInputPortType,
	enum E_MAC_TOP_ISR_EVT e_evt)
{

	MS_BOOL b_save = KDrv_SRCCAP_HDMIRx_pkt_get_emp_irq_onoff(
			enInputPortType,
			E_EMP_MTW_PULSE);

	MS_U16 u16_sts = KDrv_SRCCAP_HDMIRx_pkt_get_emp_evt(
			enInputPortType,
			E_EMP_MTW_PULSE);

	KDrv_SRCCAP_HDMIRx_pkt_set_emp_irq_onoff(
			enInputPortType,
			E_EMP_MTW_PULSE,
			FALSE);

	if (b_save && u16_sts) {
		KDrv_SRCCAP_HDMIRx_pkt_clr_emp_evt(
			enInputPortType,
			E_EMP_MTW_PULSE);
#ifdef HDCP22_REAUTH_ACCU
		KDrv_SRCCAP_HDMIRx_hdcp_isr(
			(MS_U8)(enInputPortType - INPUT_PORT_DVI0),
			E_HDMI_HDCP22_CLR_BCH_ERR,
			(void *)NULL);
#endif
		mtk_hdmirx_ctrl_vtem_info_monitor_task(
			priv, enInputPortType);
	}
	KDrv_SRCCAP_HDMIRx_mac_isr(enInputPortType, e_evt);
	// drv_hdmirx_irq_pkt();

	KDrv_SRCCAP_HDMIRx_pkt_set_emp_irq_onoff(
			enInputPortType,
			E_EMP_MTW_PULSE,
			b_save);
}

static void _irq_handler_mac_dtop_misc(void *priv)
{
}

static void _irq_handler_mac_inner_misc(void *priv)
{
}

static void _irq_handler_mac_dscd(void *priv)
{
}

static void _irq_handler_mac_dec_misc_p0(void *priv)
{
	_irq_handler_mac_dec_misc(priv, INPUT_PORT_DVI0, E_DEC_MISC_P0);
}

static void _irq_handler_mac_dec_misc_p1(void *priv)
{
	_irq_handler_mac_dec_misc(priv, INPUT_PORT_DVI1, E_DEC_MISC_P1);
}
static void _irq_handler_mac_dec_misc_p2(void *priv)
{
	_irq_handler_mac_dec_misc(priv, INPUT_PORT_DVI2, E_DEC_MISC_P2);
}

static void _irq_handler_mac_dec_misc_p3(void *priv)
{
	_irq_handler_mac_dec_misc(priv, INPUT_PORT_DVI3, E_DEC_MISC_P3);
}

static void _irq_handler_mac_dep_misc_p0(void *priv)
{
	_irq_handler_mac_dep_misc(priv, INPUT_PORT_DVI0, E_DEP_MISC_P0);
}

static void _irq_handler_mac_dep_misc_p1(void *priv)
{
	_irq_handler_mac_dep_misc(priv, INPUT_PORT_DVI1, E_DEP_MISC_P1);
}
static void _irq_handler_mac_dep_misc_p2(void *priv)
{
	_irq_handler_mac_dep_misc(priv, INPUT_PORT_DVI2, E_DEP_MISC_P2);
}

static void _irq_handler_mac_dep_misc_p3(void *priv)
{
	_irq_handler_mac_dep_misc(priv, INPUT_PORT_DVI3, E_DEP_MISC_P3);
}

static void _irq_handler_pkt_parser_emp(void *priv)
{
	MS_BOOL b_save = KDrv_SRCCAP_HDMIRx_pkt_get_emp_irq_onoff(INPUT_PORT_DVI0,
			E_EMP_MTW_PULSE);

	MS_U16 u16_sts = KDrv_SRCCAP_HDMIRx_pkt_get_emp_evt(INPUT_PORT_DVI0,
			E_EMP_MTW_PULSE);

	KDrv_SRCCAP_HDMIRx_pkt_set_emp_irq_onoff(INPUT_PORT_DVI0,
			E_EMP_MTW_PULSE,
			FALSE);

	if (b_save && u16_sts) {
		KDrv_SRCCAP_HDMIRx_pkt_clr_emp_evt(INPUT_PORT_DVI0,
			E_EMP_MTW_PULSE);
		mtk_hdmirx_ctrl_vtem_info_monitor_task(priv, INPUT_PORT_DVI0);
	}

	KDrv_SRCCAP_HDMIRx_pkt_set_emp_irq_onoff(INPUT_PORT_DVI0,
		E_EMP_MTW_PULSE,
		b_save);
}
static irqreturn_t mtk_hdmirx_isr_mac(int irq, void *priv)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)priv;
	unsigned int mask_save =
		KDrv_SRCCAP_HDMIRx_irq_mask_save_global(
		_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_MAC));

	unsigned int evt = KDrv_SRCCAP_HDMIRx_irq_status(
			_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_MAC),
			INPUT_PORT_DVI0);

	unsigned int evt_2 = (~mask_save) & evt;
	unsigned char u8_bit = 0;

	if (srccap_dev->srccap_info.cap.u32HDMI_Srccap_HWVersion == (1 + 1 + 1)) {
		if (evt_2 & (1 << E_COMBO_GP_IRQ_PKT_PARSER_EMP))
			_irq_handler_pkt_parser_emp(priv);

		if (evt_2 & (1 << E_COMBO_GP_IRQ_HDCP_DTOP)) {
			if (mtk_hdmirx_hdcp_irq_evt())
				mtk_hdmirx_hdcp_isr(irq, priv);
		}
	} else {
		if (evt_2 &
		((1<<E_DEC_MISC_P0) | (1<<E_DEC_MISC_P1)
		| (1<<E_DEC_MISC_P2) | (1<<E_DEC_MISC_P3))) {

			if (mtk_hdmirx_hdcp_irq_evt())
				mtk_hdmirx_hdcp_isr(irq, priv);

		}

		while (evt_2 && u8_bit < E_MAC_TOP_ISR_EVT_N) {
			if (evt_2 & HDMI_BIT0) {
				if (_irq_handler_mac_top[u8_bit])
					_irq_handler_mac_top[u8_bit](priv);
				else
					HDMIRX_MSG_ERROR("\r\n");
			}
			evt_2 >>= 1;
			u8_bit++;
		}
	}

	evt_2 = (~mask_save) & evt;

	KDrv_SRCCAP_HDMIRx_irq_clr(
		_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_MAC),
		INPUT_PORT_DVI0,
		evt_2);

	KDrv_SRCCAP_HDMIRx_irq_mask_restore_global(
		_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_MAC),
		mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_hdmirx_isr_pkt_que(int irq, void *priv)
{
	unsigned int mask_save =
		MDrv_HDMI_IRQ_MASK_SAVE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_PKT_QUE));

	MDrv_HDMI_ISR_PKT_QUE();
	MDrv_HDMI_IRQ_MASK_RESTORE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_PKT_QUE),
							   mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_hdmirx_isr_scdc(int irq, void *priv)
{
	unsigned int mask_save =
		MDrv_HDMI_IRQ_MASK_SAVE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_PM_SCDC));

	MDrv_HDMI_ISR_SCDC();
	MDrv_HDMI_IRQ_MASK_RESTORE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_PM_SCDC),
							   mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_hdmirx_isr_sqh_all_wk(int irq, void *priv)
{
	unsigned int mask_save = MDrv_HDMI_IRQ_MASK_SAVE(
		_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_PM_SQH_ALL_WK));

	MDrv_HDMI_ISR_SQH_ALL_WK();
	MDrv_HDMI_IRQ_MASK_RESTORE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_IRQ_PM_SQH_ALL_WK),
							   mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_hdmirx_isr_clk_det_0(int irq, void *priv)
{
	unsigned int mask_save =
		MDrv_HDMI_IRQ_MASK_SAVE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_FIQ_CLK_DET_0));

	MDrv_HDMI_ISR_CLK_DET_0();
	MDrv_HDMI_IRQ_MASK_RESTORE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_FIQ_CLK_DET_0),
							   mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_hdmirx_isr_clk_det_1(int irq, void *priv)
{
	unsigned int mask_save =
		MDrv_HDMI_IRQ_MASK_SAVE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_FIQ_CLK_DET_1));

	MDrv_HDMI_ISR_CLK_DET_1();
	MDrv_HDMI_IRQ_MASK_RESTORE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_FIQ_CLK_DET_1),
							   mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_hdmirx_isr_clk_det_2(int irq, void *priv)
{
	unsigned int mask_save =
		MDrv_HDMI_IRQ_MASK_SAVE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_FIQ_CLK_DET_2));

	MDrv_HDMI_ISR_CLK_DET_2();
	MDrv_HDMI_IRQ_MASK_RESTORE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_FIQ_CLK_DET_2),
							   mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_hdmirx_isr_clk_det_3(int irq, void *priv)
{
	unsigned int mask_save =
		MDrv_HDMI_IRQ_MASK_SAVE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_FIQ_CLK_DET_3));

	MDrv_HDMI_ISR_CLK_DET_3();
	MDrv_HDMI_IRQ_MASK_RESTORE(_mtk_hdmirx_getKDrvInt(SRCCAP_HDMI_FIQ_CLK_DET_3),
							   mask_save);
	// HDMIRX_MSG_DBG("%s,irq=%d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_srccap_hdmirx_isr_entry(int irq, void *priv)
{
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)priv;
	struct srccap_hdmirx_interrupt *p_hdmi_int_s =
		&srccap_dev->hdmirx_info.hdmi_int_s;
	irqreturn_t ret_val = IRQ_NONE;
	int idx = 0;

	for (idx = SRCCAP_HDMI_IRQ_START; idx < SRCCAP_HDMI_IRQ_END; idx++) {
		if (irq == p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START]) {
			if (p_hdmi_int_s->int_func[idx - SRCCAP_HDMI_IRQ_START]) {
				ret_val =
				p_hdmi_int_s->int_func[idx - SRCCAP_HDMI_IRQ_START](irq, priv);
			}
			break;
		}
	}
	return ret_val;
}

static void _hdmirx_isr_handler_setup(struct srccap_hdmirx_interrupt *p_hdmi_int_s)
{
	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);
	if (p_hdmi_int_s) {
		p_hdmi_int_s->int_func[SRCCAP_HDMI_IRQ_PHY - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_phy;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_IRQ_MAC - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_mac;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_IRQ_PKT_QUE - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_pkt_que;
		p_hdmi_int_s
			->int_func[SRCCAP_HDMI_IRQ_PM_SQH_ALL_WK - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_sqh_all_wk;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_IRQ_PM_SCDC - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_scdc;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_FIQ_CLK_DET_0 - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_clk_det_0;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_FIQ_CLK_DET_1 - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_clk_det_1;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_FIQ_CLK_DET_2 - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_clk_det_2;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_FIQ_CLK_DET_3 - SRCCAP_HDMI_IRQ_START] =
			mtk_hdmirx_isr_clk_det_3;
	}
}

static void _hdmirx_isr_handler_release(struct srccap_hdmirx_interrupt *p_hdmi_int_s)
{
	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);
	if (p_hdmi_int_s) {
		p_hdmi_int_s->int_func[SRCCAP_HDMI_IRQ_PHY - SRCCAP_HDMI_IRQ_START] = NULL;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_IRQ_MAC - SRCCAP_HDMI_IRQ_START] = NULL;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_IRQ_PKT_QUE - SRCCAP_HDMI_IRQ_START] =
			NULL;
		p_hdmi_int_s
			->int_func[SRCCAP_HDMI_IRQ_PM_SQH_ALL_WK - SRCCAP_HDMI_IRQ_START] = NULL;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_IRQ_PM_SCDC - SRCCAP_HDMI_IRQ_START] =
			NULL;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_FIQ_CLK_DET_0 - SRCCAP_HDMI_IRQ_START] =
			NULL;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_FIQ_CLK_DET_1 - SRCCAP_HDMI_IRQ_START] =
			NULL;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_FIQ_CLK_DET_2 - SRCCAP_HDMI_IRQ_START] =
			NULL;
		p_hdmi_int_s->int_func[SRCCAP_HDMI_FIQ_CLK_DET_3 - SRCCAP_HDMI_IRQ_START] =
			NULL;
	}
}

static int _hdmirx_interrupt_register(struct mtk_srccap_dev *srccap_dev,
				struct platform_device *pdev)
{
	struct srccap_hdmirx_interrupt *p_hdmi_int_s =
		&srccap_dev->hdmirx_info.hdmi_int_s;
	int ret = 0;
	int ret_val = 0;
	int idx = 0;
	HDMIRX_MSG_DBG("mdbgin_%s:  start \r\n", __func__);
	if (srccap_dev->dev_id == 0) {
		for (idx = SRCCAP_HDMI_IRQ_START; idx < SRCCAP_HDMI_IRQ_END; idx++) {
			p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START] =
				platform_get_irq(pdev, idx);
			if (p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START] < 0)
				HDMIRX_MSG_ERROR("Failed to get irq, hdmi index=%d\r\n", idx);
			else {
				HDMIRX_MSG_INFO("irq[%d]=0x%x\r\n", idx,
					p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START]);
				if (idx == SRCCAP_HDMI_IRQ_PM_SQH_ALL_WK) {
					HDMIRX_MSG_INFO("skip SRCCAP_HDMI_IRQ_PM_SQH_ALL_WK\r\n");
					continue;
				}
				ret = devm_request_irq(
					&pdev->dev,
					p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START],
					mtk_srccap_hdmirx_isr_entry, IRQF_ONESHOT, pdev->name,
					srccap_dev);
				if (ret) {
					HDMIRX_MSG_ERROR(
						"Failed to request irq[%d]=0x%x\r\n", idx,
						p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START]);
					HDMIRX_MSG_ERROR("mdbgerr_%s:  %d\r\n", __func__, -EINVAL);
					ret_val = -EINVAL;
				}
			}
		}
	}

	return ret_val;
}

static int _hdmirx_irq_suspend(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	int idx = 0;

	struct srccap_hdmirx_interrupt *p_hdmi_int_s =
		&srccap_dev->hdmirx_info.hdmi_int_s;

	for (idx = SRCCAP_HDMI_IRQ_START; idx < SRCCAP_HDMI_IRQ_END; idx++) {
		if (p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START] < 0) {
			HDMIRX_MSG_ERROR("Wrong irq, failed = %d\r\n", idx);
			ret = -EINVAL;
		} else {
			disable_irq(p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START]);
		}
	}

	return ret;
}

static int _hdmirx_irq_resume(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	int idx = 0;

	struct srccap_hdmirx_interrupt *p_hdmi_int_s =
		&srccap_dev->hdmirx_info.hdmi_int_s;

	for (idx = SRCCAP_HDMI_IRQ_START; idx < SRCCAP_HDMI_IRQ_END; idx++) {
		if (p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START] < 0) {
			HDMIRX_MSG_ERROR("Wrong irq, hdmi index = %d\r\n", idx);
			ret = -EINVAL;
		} else {
			enable_irq(p_hdmi_int_s->int_id[idx - SRCCAP_HDMI_IRQ_START]);
		}
	}

	return ret;
}

bool mtk_srccap_hdmirx_isHDMIMode(enum v4l2_srccap_input_source src)
{
	E_MUX_INPUTPORT ePort = mtk_hdmirx_to_muxinputport(src);

	return MDrv_HDMI_IsHDMI_Mode(ePort);
}

bool mtk_srccap_hdmirx_get_pwr_saving_onoff_hwdef(
	enum v4l2_srccap_input_source src)
{
	return KDrv_SRCCAP_HDMIRx_query_module_hw_param(
			E_HDMI_IMMESWITCH_E,
			E_HDMI_PARM_HW_DEF,
		mtk_hdmirx_to_muxinputport(src)) ? true : false;
}

bool mtk_srccap_hdmirx_get_pwr_saving_onoff(
	enum v4l2_srccap_input_source src)
{
	return KDrv_SRCCAP_HDMIRx_query_module_hw_param(
			E_HDMI_IMMESWITCH_E,
			E_HDMI_PARM_SW_PROG,
		mtk_hdmirx_to_muxinputport(src)) ? true : false;
}

bool mtk_srccap_hdmirx_set_pwr_saving_onoff(
	enum v4l2_srccap_input_source src,
	bool b_on)
{
	return KDrv_SRCCAP_HDMIRx_set_module_hw_param_sw_prog(
		E_HDMI_IMMESWITCH_E,
		mtk_hdmirx_to_muxinputport(src),
		b_on ? 1 : 0);
}

bool mtk_srccap_hdmirx_get_HDMIStatus(
	enum v4l2_srccap_input_source src)
{
	bool bret = FALSE;
	E_MUX_INPUTPORT ePort =
			mtk_hdmirx_to_muxinputport(src);
	MS_U8 u8HDMIInfoSource = KDrv_SRCCAP_HDMIRx_mux_inport_2_src(ePort);
	MS_U8 u8ErrorStatus = 0;
	stHDMI_POLLING_INFO polling = MDrv_HDMI_Get_HDMIPollingInfo(ePort);

	MS_U32 u32_mute_evt = 0;

	if (ePort == INPUT_PORT_NONE_PORT || u8HDMIInfoSource >= HDMI_INFO_SOURCE_MAX)
		return bret;

	u8ErrorStatus = KDrv_SRCCAP_HDMIRx_pkt_get_errstatus(u8HDMIInfoSource,
		u8ErrorStatus, TRUE, &polling);

	if (KDrv_SRCCAP_HDMIRx_phy_get_clklosedet(ePort)
		|| (!KDrv_SRCCAP_HDMIRx_mac_get_destable(ePort))
		|| (u8ErrorStatus & (E_HDMI_BCH_ERROR | E_HDMI_PHY_STATE_NOT_READY))
		|| (MDrv_HDMI_sw_mute(ePort, &u32_mute_evt) == TRUE)) {
		bret = FALSE;
	} else {
		bret = TRUE;
	}

	// clear errstatus
	u8ErrorStatus = KDrv_SRCCAP_HDMIRx_pkt_get_errstatus(u8HDMIInfoSource,
		u8ErrorStatus, FALSE, &polling);

	return bret;
}

bool mtk_srccap_hdmirx_get_ClkStatus(
	enum v4l2_srccap_input_source src)
{
	bool bret = FALSE;
	E_MUX_INPUTPORT ePort =
			mtk_hdmirx_to_muxinputport(src);

	if (KDrv_SRCCAP_HDMIRx_phy_get_clklosedet(ePort))
		bret = FALSE;
	else
		bret = TRUE;

	return bret;
}

bool mtk_srccap_hdmirx_get_freesync_info(
	enum v4l2_srccap_input_source src,
	struct v4l2_ext_hdmi_get_freesync_info *p_info)
{
	int pkt_count = 0;
	bool bval = false;
	struct hdmi_packet_info *gnl_ptr = NULL;

	gnl_ptr = kzalloc(
	sizeof(struct hdmi_packet_info)*META_SRCCAP_HDMIRX_PKT_COUNT_MAX_SIZE,
	GFP_KERNEL);

	if (gnl_ptr == NULL)
		return -ENOMEM;

	pkt_count = mtk_srccap_hdmirx_pkt_get_gnl(src, V4L2_EXT_HDMI_PKT_SPD,
			gnl_ptr, META_SRCCAP_HDMIRX_PKT_MAX_SIZE);

	if (pkt_count > 0) {
		int i;

		p_info->u8_fsinfo = 0;

		for (i = 0; i < pkt_count; i++) {
			if ((gnl_ptr[i].pb[1] == 0x1A) && (gnl_ptr[i].pb[2] == 0x00)
			&& (gnl_ptr[i].pb[3] == 0x00)) {
				if (gnl_ptr[i].pb[6] & BIT(0)) {
					p_info->u8_fsinfo =
					p_info->u8_fsinfo | V4L2_SRCCAP_FREESYNC_SUPPORTED;
				}
				if (gnl_ptr[i].pb[6] & BIT(1)) {
					p_info->u8_fsinfo =
					p_info->u8_fsinfo | V4L2_SRCCAP_FREESYNC_ENABLED;
				}
				if (gnl_ptr[i].pb[6] & BIT(2)) {
					p_info->u8_fsinfo =
					p_info->u8_fsinfo | V4L2_SRCCAP_FREESYNC_ACTIVE;
				}

				if ((gnl_ptr[i].pb[6] & BIT(0)) && (gnl_ptr[i].pb[6] & BIT(1)))
					bval = true;
			}
		}
	}
	kfree(gnl_ptr);
	return bval;
}

int mtk_srccap_hdmirx_get_pixel_freq(
	enum v4l2_srccap_input_source src)
{
	MS_U32 pixel_freq = 0;

	pixel_freq = MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_PIXEL_CLOCK,
			mtk_hdmirx_to_muxinputport(src));
	return pixel_freq;
}

enum m_freesync_type mtk_srccap_hdmirx_pkt_get_freesync_type(
	enum v4l2_srccap_input_source src,
	bool NativeColorActive,
	int FpsMultiple)
{
	bool freesync = FALSE;
	struct v4l2_ext_hdmi_get_freesync_info freesync_info;

	memset(&freesync_info, 0, sizeof(struct v4l2_ext_hdmi_get_freesync_info));
	freesync = mtk_srccap_hdmirx_get_freesync_info(src, &freesync_info);

	if (freesync) {
		if (NativeColorActive && FpsMultiple > FREESYNC_MULTIPLE)
			return TYPE_FREESYNC_PREMIUM_PRO;
		else if (FpsMultiple > FREESYNC_MULTIPLE)
			return TYPE_FREESYNC_PREMIUM;
		else
			return TYPE_FREESYNC;
	} else
		return TYPE_NFREESYNC;
}

int mtk_srccap_hdmirx_pkt_get_spd(
	enum v4l2_srccap_input_source src,
	u8 u8DevId,
	struct meta_srccap_spd_pkt *p_info)
{
	int pkt_count = 0;
	int fps_multiple = 0;
	INDEX_E index;
	static struct hdmi_packet_info
			buf_gnl_ptr[SRCCAP_DEV_NUM][META_SRCCAP_HDMIRX_PKT_COUNT_MAX_SIZE];

	struct hdmi_packet_info *gnl_ptr = NULL;

	index = E_INDEX_0;

	if (p_info == NULL)
		return -ENXIO;

	if (u8DevId >= SRCCAP_DEV_NUM) {
		pr_err("Invalid dev_id %d\r\n", u8DevId);
		return -ENODEV;
	}

	gnl_ptr = buf_gnl_ptr[u8DevId];
	pkt_count = mtk_srccap_hdmirx_pkt_get_gnl(src, V4L2_EXT_HDMI_PKT_SPD,
			gnl_ptr, META_SRCCAP_HDMIRX_PKT_MAX_SIZE);

	if (pkt_count > 0) {
		int i;

		p_info->native_color_space_active = 0;
		p_info->ld_disable_control = 0;
		p_info->brightness_control_active = 0;
		p_info->sRGB_EOTF_active = 0;
		p_info->BT709_EOTF_active = 0;
		p_info->gamma_22_EOTF_active = 0;
		p_info->gamma_26_EOTF_active = 0;
		p_info->PQ_EOTF_active = 0;
		p_info->brightness_control = 0;
		p_info->bRet = false;

		for (i = 0; i < pkt_count; i++) {
			if ((gnl_ptr[i].pb[E_INDEX_1] == SPD_PKT_HEAD)
			&& (gnl_ptr[i].pb[E_INDEX_2] == 0x00)
			&& (gnl_ptr[i].pb[E_INDEX_3] == 0x00)) {
				p_info->bRet = true;
				if (gnl_ptr[i].pb[E_INDEX_6] & BIT(E_INDEX_3))
					p_info->native_color_space_active = 1;
				if (gnl_ptr[i].pb[E_INDEX_6] & BIT(E_INDEX_4))
					p_info->brightness_control_active = 1;
				if (gnl_ptr[i].pb[E_INDEX_6] & BIT(E_INDEX_5))
					p_info->ld_disable_control  = 1;
				if (gnl_ptr[i].pb[E_INDEX_9] & BIT(E_INDEX_0))
					p_info->sRGB_EOTF_active  = 1;
				if (gnl_ptr[i].pb[E_INDEX_9] & BIT(E_INDEX_1))
					p_info->BT709_EOTF_active  = 1;
				if (gnl_ptr[i].pb[E_INDEX_9] & BIT(E_INDEX_2))
					p_info->gamma_22_EOTF_active  = 1;
				if (gnl_ptr[i].pb[E_INDEX_9] & BIT(E_INDEX_3))
					p_info->gamma_26_EOTF_active  = 1;
				if (gnl_ptr[i].pb[E_INDEX_9] & BIT(E_INDEX_5))
					p_info->PQ_EOTF_active  = 1;
				if (gnl_ptr[i].pb[E_INDEX_10] != 0x00)
					p_info->brightness_control = gnl_ptr[i].pb[E_INDEX_10];
				if (gnl_ptr[i].pb[E_INDEX_8] != 0x00 &&
					gnl_ptr[i].pb[E_INDEX_7] != 0x00){
					fps_multiple = (gnl_ptr[i].pb[E_INDEX_8] * VFREQX1000) /
									gnl_ptr[i].pb[E_INDEX_7];
				}
			}
		}
	p_info->freesync_type = mtk_srccap_hdmirx_pkt_get_freesync_type(src,
						p_info->native_color_space_active, fps_multiple);
	}
	return 0;
}

bool mtk_srccap_hdmirx_set_scdcval(E_MUX_INPUTPORT enInputPortType,
	MS_U8 u8Offset, MS_U8 u8SCDCValue)
{
	return KDrv_SRCCAP_HDMIRx_mac_set_scdcval(
		enInputPortType, u8Offset, u8SCDCValue);
}

bool mtk_srccap_hdmirx_ddc_bus_control(MS_BOOL bEnable,
	E_MUX_INPUTPORT enInputPortType)
{
	return KDrv_SRCCAP_HDMIRx_edid_ddc_bus_ctrl(
		bEnable, enInputPortType);
}

bool mtk_srccap_hdmirx_get_event_change(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	bool b_event_chage = FALSE;
	E_MUX_INPUTPORT ePort = mtk_hdmirx_to_muxinputport(srccap_dev->srccap_info.src);
	MS_U8 ucPortIndex = 0;
	struct hdmi_avi_packet_info pkt_avi;
	struct hdmi_gc_packet_info pkt_gcp;

	if (ePort == INPUT_PORT_NONE_PORT)
		return FALSE;
	ucPortIndex = HDMI_GET_PORT_SELECT(ePort);

	memset(&pkt_avi, 0, sizeof(struct hdmi_avi_packet_info));
	memset(&pkt_gcp, 0, sizeof(struct hdmi_gc_packet_info));

	// AVI event
	ret = mtk_srccap_hdmirx_pkt_get_AVI(srccap_dev->srccap_info.src,
		&pkt_avi, sizeof(struct hdmi_avi_packet_info));
	if (ret) {
		CHECK_META_VALUE_UPDATED(HDMI_META_AVI_CONTENT_TYPE, ucPortIndex,
			_stPreHdmiMetaInfo[ucPortIndex].eContentType,
			pkt_avi.u8CN10Value, b_event_chage);	// IT Contents Type
		//CHECK_META_VALUE_UPDATED(HDMI_META_AVI_COLOR_FORFAT, ucPortIndex,
		//	_stPreHdmiMetaInfo[ucPortIndex].eHdmiColorformat,
		//	pkt_avi.enColorForamt, b_event_chage);
		//	// Color format - RGB or YCbCr
		CHECK_META_VALUE_UPDATED(HDMI_META_AVI_COLOR_RANGE, ucPortIndex,
			_stPreHdmiMetaInfo[ucPortIndex].eHdmiColorRange,
			pkt_avi.enColorRange, b_event_chage);	// RGB Quantization Range
		CHECK_META_VALUE_UPDATED(HDMI_META_AVI_AFD, ucPortIndex,
			_stPreHdmiMetaInfo[ucPortIndex].eHdmiAfd,
			pkt_avi.enActiveFormatAspectRatio,
			b_event_chage);	// AFD - Active Portion Aspect Ratio
		CHECK_META_VALUE_UPDATED(HDMI_META_AVI_SCAN_INFO, ucPortIndex,
			_stPreHdmiMetaInfo[ucPortIndex].eScanInfo,
			pkt_avi.u8S10Value, b_event_chage);	// Scan Information
		CHECK_META_VALUE_UPDATED(HDMI_META_AVI_COLORIMETRY, ucPortIndex,
			_stPreHdmiMetaInfo[ucPortIndex].eColorimetry,
			pkt_avi.enColormetry, b_event_chage);	// Colormetry
		//CHECK_META_VALUE_UPDATED(HDMI_META_AVI_EXT_COLORIMETRY, ucPortIndex,
		//	_stPreHdmiMetaInfo[ucPortIndex].eExtColorimetry,
		//	pkt_avi.enColormetry, b_event_chage);
		//	// Extended Colormetry (same as Colormetry)
		CHECK_META_VALUE_UPDATED(HDMI_META_AVI_PIC_AR, ucPortIndex,
			_stPreHdmiMetaInfo[ucPortIndex].ePicAr,
			pkt_avi.enAspectRatio, b_event_chage);	// Picture Aspect Ratio
	}

	// GCP event
	ret = mtk_srccap_hdmirx_pkt_get_GCP(srccap_dev->srccap_info.src,
		&pkt_gcp, sizeof(struct hdmi_gc_packet_info));
	if (ret) {
		CHECK_META_VALUE_UPDATED(HDMI_META_COLOR_DEPTH, ucPortIndex,
			_stPreHdmiMetaInfo[ucPortIndex].eHdmiColorDepth,
			pkt_gcp.u8CDValue, b_event_chage);	// Color Depth
		CHECK_META_VALUE_UPDATED(HDMI_META_GC_AVMUTE, ucPortIndex,
			_stPreHdmiMetaInfo[ucPortIndex].u8HdmiAvmute,
			pkt_gcp.u8ControlAVMute, b_event_chage);	// AVMute
	}

	// HDMI/DVI mode
	CHECK_META_VALUE_UPDATED(HDMI_META_HDMI_MODE, ucPortIndex,
		_stPreHdmiMetaInfo[ucPortIndex].bHdmiMode,
		mtk_srccap_hdmirx_isHDMIMode(srccap_dev->srccap_info.src), b_event_chage);

	CHECK_META_VALUE_UPDATED(HDMI_META_ALLM_MODE, ucPortIndex,
		_stPreHdmiMetaInfo[ucPortIndex].u8Allm,
		srccap_dev->hdmirx_info.u8ALLMMode[ucPortIndex], b_event_chage);

	if (b_event_chage)
		HDMIRX_MSG_DBG("[META] HDMI trigger EVENT_CHG\r\n");

	return b_event_chage;
}

// only for sim mode enable/disable
static void _mtk_srccap_hdmirx_irq_save_restore(MS_U8 u8_save)
{
	static MS_U32
		u32_irq_save_phy[INPUT_PORT_DVI_MAX-INPUT_PORT_DVI0] = {0};

	static MS_U32 u32_irq_save_mac;

	static MS_U32
		u32_irq_save_pktq[INPUT_PORT_DVI_MAX-INPUT_PORT_DVI0] = {0};
	static MS_U32
		u32_irq_save_sqh[INPUT_PORT_DVI_MAX-INPUT_PORT_DVI0] = {0};
	static MS_U32
		u32_irq_save_scdc[INPUT_PORT_DVI_MAX-INPUT_PORT_DVI0] = {0};
	static MS_U32
		u32_irq_save_clk_det[HDMI_FIQ_CLK_DET_3 - HDMI_FIQ_CLK_DET_0 + 1] = {0};

	MS_U32 u32_idx_p = 0;

	if (u8_save) {
		u32_irq_save_mac =
			KDrv_SRCCAP_HDMIRx_irq_mask_save(
				HDMI_IRQ_MAC,
				INPUT_PORT_DVI0);
		for (u32_idx_p = 0;
			u32_idx_p < (INPUT_PORT_DVI_MAX-INPUT_PORT_DVI0);
			u32_idx_p++) {
			u32_irq_save_phy[u32_idx_p] =
				KDrv_SRCCAP_HDMIRx_irq_mask_save(
					HDMI_IRQ_PHY,
					INPUT_PORT_DVI0 + u32_idx_p);
			u32_irq_save_pktq[u32_idx_p] =
				KDrv_SRCCAP_HDMIRx_irq_mask_save(
					HDMI_IRQ_PKT_QUE,
					INPUT_PORT_DVI0 + u32_idx_p);
			u32_irq_save_sqh[u32_idx_p] =
				KDrv_SRCCAP_HDMIRx_irq_mask_save(
					HDMI_IRQ_PM_SQH_ALL_WK,
					INPUT_PORT_DVI0 + u32_idx_p);
			u32_irq_save_scdc[u32_idx_p] =
				KDrv_SRCCAP_HDMIRx_irq_mask_save(
					HDMI_IRQ_PM_SCDC,
					INPUT_PORT_DVI0 + u32_idx_p);
			KDrv_SRCCAP_HDMIRx_pkt_suspend(
				INPUT_PORT_DVI0 + u32_idx_p);
		}
		for (u32_idx_p = 0;
		u32_idx_p < (HDMI_FIQ_CLK_DET_3 - HDMI_FIQ_CLK_DET_0 + 1);
			u32_idx_p++) {
			u32_irq_save_clk_det[u32_idx_p] =
				KDrv_SRCCAP_HDMIRx_irq_mask_save(
					HDMI_FIQ_CLK_DET_0 + u32_idx_p,
					INPUT_PORT_DVI0);
		}
	} else {
		for (u32_idx_p = 0;
			u32_idx_p < (INPUT_PORT_DVI_MAX-INPUT_PORT_DVI0);
			u32_idx_p++) {
			KDrv_SRCCAP_HDMIRx_irq_mask_restore(
				HDMI_IRQ_PHY,
				INPUT_PORT_DVI0 + u32_idx_p,
				u32_irq_save_phy[u32_idx_p]);
			KDrv_SRCCAP_HDMIRx_irq_mask_restore(
				HDMI_IRQ_PKT_QUE,
				INPUT_PORT_DVI0 + u32_idx_p,
				u32_irq_save_pktq[u32_idx_p]);
			KDrv_SRCCAP_HDMIRx_irq_mask_restore(
				HDMI_IRQ_PM_SQH_ALL_WK,
				INPUT_PORT_DVI0 + u32_idx_p,
				u32_irq_save_sqh[u32_idx_p]);
			KDrv_SRCCAP_HDMIRx_irq_mask_restore(
				HDMI_IRQ_PM_SCDC,
				INPUT_PORT_DVI0 + u32_idx_p,
				u32_irq_save_scdc[u32_idx_p]);
			KDrv_SRCCAP_HDMIRx_pkt_resume(
				INPUT_PORT_DVI0 + u32_idx_p);
		}
		for (u32_idx_p = 0;
		u32_idx_p < (HDMI_FIQ_CLK_DET_3 - HDMI_FIQ_CLK_DET_0 + 1);
			u32_idx_p++) {
			KDrv_SRCCAP_HDMIRx_irq_mask_restore(
				HDMI_FIQ_CLK_DET_0 + u32_idx_p,
				INPUT_PORT_DVI0,
				u32_irq_save_clk_det[u32_idx_p]);
		}
		KDrv_SRCCAP_HDMIRx_irq_mask_restore(
			HDMI_IRQ_MAC,
			INPUT_PORT_DVI0,
			u32_irq_save_mac);
	}
}

