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
#include <linux/clk-provider.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>

#include <linux/mtk-v4l2-srccap.h>
#include "mtk_srccap.h"
#include "show_param.h"
#include "sti_msos.h"
#include "avd-ex.h"

#include <linux/ion.h>
#include <linux/syscalls.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include "mtk_iommu_common.h"
#include "mtk-reserved-memory.h"
#include "mtk-cma.h"

/* ============================================================================================== */
/* -------------------------------------- Global Type Definitions ------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* -------------------------------------- Macros and Defines ------------------------------------ */
/* ============================================================================================== */
#define AVD_MAX_CMD_LENGTH (0xFF)
#define AVD_MAX_ARG_NUM (64)
#define AVD_CMD_REMOVE_LEN (2)
#define AVD_CLOCK_MAX_COUNT (1)
#define AVD_CLOCK_MIN_COUNT (0)
#define SRCCAP_AVD_TASK_SLEEP_5000 (5000)
#define SRCCAP_AVD_TASK_SLEEP_5100 (5100)
#define SRCCAP_AVD_TASK_SLEEP_50000 (50000)
#define SRCCAP_AVD_TASK_SLEEP_51000 (51000)

/* ============================================================================================== */
/* -------------------------------------- Local Functions --------------------------------------- */
/* ============================================================================================== */
static struct v4l2_ext_avd_scan_hsyc_check pHsync;
static enum v4l2_ext_avd_factory_para eFactoryPara;

static const char *V4L2InputSourceToString(
	enum v4l2_srccap_input_source eInputSource)
{
	char *src = NULL;

	switch (eInputSource) {
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		src = "Cvbs";
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		src = "Svideo";
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		src = "Atv";
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		src = "Scart";
		break;
	default:
		src = "Other";
		break;
	}

	return src;
}

static int mtk_avd_ctrl_macrovision_monitor_task(void *data)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	struct v4l2_event event;
	enum v4l2_srccap_input_source eSrc;
	enum v4l2_timingdetect_status eSignalStatus;
	enum V4L2_AVD_SIGNAL_STAGE eVdStage;
	bool bScan = 0;
	bool bDetect = FALSE;
	int ret = 0;
	uint32_t u32TaskSleepIntervelRangeStart, u32TaskSleepIntervelRangeEnd;

	if (data == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	srccap_dev = (struct mtk_srccap_dev *)data;
	while (!kthread_should_stop()) {
		eSrc = srccap_dev->srccap_info.src;
		eSignalStatus = srccap_dev->avd_info.enVdSignalStatus;
		bScan = srccap_dev->avd_info.bIsATVScanMode;
		eVdStage = srccap_dev->avd_info.enSignalStage;
		//Init task interval range to 50ms.
		u32TaskSleepIntervelRangeStart = SRCCAP_AVD_TASK_SLEEP_50000;
		u32TaskSleepIntervelRangeEnd = SRCCAP_AVD_TASK_SLEEP_51000;

		//Signal no stable then skip monitor.
		if ((eSignalStatus != V4L2_TIMINGDETECT_STABLE_SYNC) &&
			(eVdStage != V4L2_SIGNAL_KEEP_TRUE))
			goto task_sleep;

		//In Atv source not play mode then skip monitor.
		if ((bScan == TRUE) && (eSrc == V4L2_SRCCAP_INPUT_SOURCE_ATV))
			goto task_sleep;

		//Reduce task interval range to 5ms. When the detection condition is met.
		u32TaskSleepIntervelRangeStart = SRCCAP_AVD_TASK_SLEEP_5000;
		u32TaskSleepIntervelRangeEnd = SRCCAP_AVD_TASK_SLEEP_5100;

		memset(&bDetect, 0, sizeof(bool));
		ret = mtk_avd_macro_vision_detect(&bDetect);
		if (ret < 0)
			goto task_sleep;

		if (srccap_dev->avd_info.bMacroVersionDetect != bDetect) {
			srccap_dev->avd_info.bMacroVersionDetect = bDetect;
			SRCCAP_AVD_MSG_INFO("[Input:%s][bScan = %d]bMacroVisionDetect = %d",
			V4L2InputSourceToString(eSrc),
			bScan,
			srccap_dev->avd_info.bMacroVersionDetect);
			memset(&event, 0, sizeof(struct v4l2_event));
			event.type = V4L2_EVENT_CTRL;
			event.id = V4L2_SRCCAP_CTRL_AVD_MACROVISION_DETECT;
			v4l2_event_queue(srccap_dev->vdev, &event);
		}

task_sleep:
		usleep_range(u32TaskSleepIntervelRangeStart, u32TaskSleepIntervelRangeEnd);
	}

	if (kthread_should_stop())
		SRCCAP_AVD_MSG_INFO("%s has been stopped.\n", __func__);

	return 0;
}

static int mtk_avd_ctrl_cgms_monitor_task(void *data)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	struct v4l2_event event;
	enum v4l2_srccap_input_source eSrc;
	enum v4l2_timingdetect_status eSignalStatus;
	enum V4L2_AVD_SIGNAL_STAGE eVdStage;
	bool bScan = 0;
	bool bDetect = FALSE;
	int ret = 0;
	uint32_t u32TaskSleepIntervelRangeStart, u32TaskSleepIntervelRangeEnd;

	if (data == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	srccap_dev = (struct mtk_srccap_dev *)data;
	while (!kthread_should_stop()) {
		eSrc = srccap_dev->srccap_info.src;
		eSignalStatus = srccap_dev->avd_info.enVdSignalStatus;
		bScan = srccap_dev->avd_info.bIsATVScanMode;
		eVdStage = srccap_dev->avd_info.enSignalStage;
		//Init task interval range to 50ms.
		u32TaskSleepIntervelRangeStart = SRCCAP_AVD_TASK_SLEEP_50000;
		u32TaskSleepIntervelRangeEnd = SRCCAP_AVD_TASK_SLEEP_51000;

		//Signal no stable then skip monitor.
		if ((eSignalStatus != V4L2_TIMINGDETECT_STABLE_SYNC) &&
			(eVdStage != V4L2_SIGNAL_KEEP_TRUE))
			goto task_sleep;

		//In Atv source not play mode then skip monitor.
		if ((bScan == TRUE) && (eSrc == V4L2_SRCCAP_INPUT_SOURCE_ATV))
			goto task_sleep;

		//Reduce task interval range to 5ms. When the detection condition is met.
		u32TaskSleepIntervelRangeStart = SRCCAP_AVD_TASK_SLEEP_5000;
		u32TaskSleepIntervelRangeEnd = SRCCAP_AVD_TASK_SLEEP_5100;

		memset(&bDetect, 0, sizeof(bool));
		ret = mtk_avd_vbi_CGMS_detect(&bDetect);
		if (ret < 0)
			goto task_sleep;

		if (srccap_dev->avd_info.bCgmsDetect != bDetect) {
			srccap_dev->avd_info.bCgmsDetect = bDetect;
			SRCCAP_AVD_MSG_INFO("[Input:%s][bScan = %d]bCgmsDetect = %d",
			V4L2InputSourceToString(eSrc),
			bScan,
			srccap_dev->avd_info.bCgmsDetect);
			memset(&event, 0, sizeof(struct v4l2_event));
			event.type = V4L2_EVENT_CTRL;
			event.id = V4L2_SRCCAP_CTRL_AVD_CGMS_DETECT;
			v4l2_event_queue(srccap_dev->vdev, &event);
		}

task_sleep:
		usleep_range(u32TaskSleepIntervelRangeStart, u32TaskSleepIntervelRangeEnd);
	}

	if (kthread_should_stop())
		SRCCAP_AVD_MSG_INFO("%s has been stopped.\n", __func__);

	return 0;
}

static int mtk_avd_subscribe_ctrl_event_macrovision_detect(
		struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_macro_version);

	SRCCAP_AVD_MSG_INFO("Subscribe MACROVISION_DETECT !");
	if (srccap_dev->avd_info.ctrl_macrovision_monitor_task == NULL) {
		srccap_dev->avd_info.ctrl_macrovision_monitor_task = kthread_create(
			mtk_avd_ctrl_macrovision_monitor_task,
			srccap_dev,
			"avd_ctrl_macrovision_monitor_task");
		if (srccap_dev->avd_info.ctrl_macrovision_monitor_task == ERR_PTR(-ENOMEM)) {
			SRCCAP_AVD_MSG_ERROR("Subscribe MACROVISION_DETECT event fail.");
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_macro_version);
			return -ENOMEM;
		}
		wake_up_process(srccap_dev->avd_info.ctrl_macrovision_monitor_task);
		get_task_struct(srccap_dev->avd_info.ctrl_macrovision_monitor_task);
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_macro_version);
	return 0;
}

static int mtk_avd_subscribe_ctrl_event_cgms_detect(
		struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_cgms);

	SRCCAP_AVD_MSG_INFO("Subscribe CGMS_DETECT !");
	if (srccap_dev->avd_info.ctrl_cgms_monitor_task == NULL) {
		srccap_dev->avd_info.ctrl_cgms_monitor_task = kthread_create(
			mtk_avd_ctrl_cgms_monitor_task,
			srccap_dev,
			"avd_ctrl_cgms_monitor_task");
		if (srccap_dev->avd_info.ctrl_cgms_monitor_task == ERR_PTR(-ENOMEM)) {
			SRCCAP_AVD_MSG_ERROR("Subscribe CGMS_DETECT event fail.");
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_cgms);
			return -ENOMEM;
		}
		wake_up_process(srccap_dev->avd_info.ctrl_cgms_monitor_task);
		get_task_struct(srccap_dev->avd_info.ctrl_cgms_monitor_task);
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_cgms);
	return 0;
}

static int mtk_avd_unsubscribe_ctrl_event_macrovision_detect(
		struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_macro_version);

	SRCCAP_AVD_MSG_INFO("Unsubscribe MACROVISION_DETECT !");
	if (srccap_dev->avd_info.ctrl_macrovision_monitor_task != NULL) {
		if (srccap_dev->avd_info.ctrl_macrovision_monitor_task == ERR_PTR(-ENOMEM)) {
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_macro_version);
			return -ENOMEM;
		}
		kthread_stop(srccap_dev->avd_info.ctrl_macrovision_monitor_task);
		put_task_struct(srccap_dev->avd_info.ctrl_macrovision_monitor_task);
		srccap_dev->avd_info.ctrl_macrovision_monitor_task = NULL;
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_macro_version);
	return 0;
}

static int mtk_avd_unsubscribe_ctrl_event_cgms_detect(
		struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_cgms);

	SRCCAP_AVD_MSG_INFO("Unsubscribe CGMS_DETECT !");
	if (srccap_dev->avd_info.ctrl_cgms_monitor_task != NULL) {
		if (srccap_dev->avd_info.ctrl_cgms_monitor_task == ERR_PTR(-ENOMEM)) {
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_cgms);
			return -ENOMEM;
		}
		kthread_stop(srccap_dev->avd_info.ctrl_cgms_monitor_task);
		put_task_struct(srccap_dev->avd_info.ctrl_cgms_monitor_task);
		srccap_dev->avd_info.ctrl_cgms_monitor_task = NULL;
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_cgms);
	return 0;
}

int mtk_avd_subscribe_ctrl_event(
		struct mtk_srccap_dev *srccap_dev,
		const struct v4l2_event_subscription *event_sub)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (event_sub == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (event_sub->id) {
	case V4L2_SRCCAP_CTRL_AVD_MACROVISION_DETECT:
		ret = mtk_avd_subscribe_ctrl_event_macrovision_detect(srccap_dev);
		if (ret < 0) {
			v4l2_err(&srccap_dev->v4l2_dev,
				"failed to subscribe event %d\n",
				event_sub->id);
			return -EPERM;
		}
		break;
	case V4L2_SRCCAP_CTRL_AVD_CGMS_DETECT:
		ret = mtk_avd_subscribe_ctrl_event_cgms_detect(srccap_dev);
		if (ret < 0) {
			v4l2_err(&srccap_dev->v4l2_dev,
				"failed to subscribe event %d\n",
				event_sub->id);
			return -EPERM;
		}
		break;
	default:
		break;
	}

	return 0;
}

int mtk_avd_unsubscribe_ctrl_event(
	struct mtk_srccap_dev *srccap_dev,
	const struct v4l2_event_subscription *event_sub)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (event_sub == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (event_sub->id) {
	case V4L2_SRCCAP_CTRL_AVD_MACROVISION_DETECT:
		ret = mtk_avd_unsubscribe_ctrl_event_macrovision_detect(srccap_dev);
		if (ret < 0) {
			v4l2_err(&srccap_dev->v4l2_dev,
				"failed to unsubscribe event %d\n",
				event_sub->type);
			return -EPERM;
		}
		break;
	case V4L2_SRCCAP_CTRL_AVD_CGMS_DETECT:
		ret = mtk_avd_unsubscribe_ctrl_event_cgms_detect(srccap_dev);
		if (ret < 0) {
			v4l2_err(&srccap_dev->v4l2_dev,
				"failed to unsubscribe event %d\n",
				event_sub->type);
			return -EPERM;
		}
		break;
	default:
		break;
	}

	return 0;
}

#if defined(CONFIG_CMA) || defined(CONFIG_DD_USE_CMA)
bool comb_get_memory(struct mtk_srccap_dev *srccap_dev,
	struct platform_device *pdev, u64 *pAddr,
	unsigned int *pSize, unsigned int *cma_id)
{
	struct device_node *np;
	struct of_mmap_info_data mmap_info;
	struct platform_device *vd_pdev;
	struct of_mmap_info_data of_cma_info = {0};
	unsigned long attrs = 0;
	dma_addr_t  bus_addr_offset;
	void *cookie;
	int ret = 0;

	np = of_find_compatible_node(NULL, NULL, "mediatek,dtv-mmap-vd-comb-buf");
	if (np == NULL) {
		SRCCAP_AVD_MSG_ERROR("[COMB]of_find_compatible_node failed \r\n");
		return false;
	}

	vd_pdev = of_find_device_by_node(np);
	if (!vd_pdev) {
		pr_emerg("[COMB][CMA] cannot find device (dtv-mmap-vd-comb-buf)\n");
		return false;
	}

	of_mtk_get_reserved_memory_info_by_idx(np, 0, &mmap_info);
	SRCCAP_AVD_MSG_INFO("[COMB][CMA] mmap_info addr 0x%llx, mmap_info len 0x%llx",
		mmap_info.start_bus_address, mmap_info.buffer_size);

	of_mtk_get_reserved_memory_info_by_idx(np, 1, &of_cma_info);
	SRCCAP_AVD_MSG_INFO("[COMB][CMA] cma start address = 0x%llx\n",
		(unsigned long long)of_cma_info.start_bus_address);

	attrs |= DMA_ATTR_NO_KERNEL_MAPPING;
	bus_addr_offset = mmap_info.start_bus_address - of_cma_info.start_bus_address;

	cookie = dma_alloc_attrs(&vd_pdev->dev, mmap_info.buffer_size, &bus_addr_offset,
		GFP_KERNEL, attrs);

	if ((long)cookie == -ENOMEM) {
		pr_emerg("[COMB][CMA] failed to allocate %llx byte dma buffer",
			(unsigned long long)mmap_info.buffer_size);
		return false;
	}
	srccap_dev->avd_info.cma_buf_info.DmaAddr = bus_addr_offset;
	srccap_dev->avd_info.cma_buf_info.u32Size = mmap_info.buffer_size;
	srccap_dev->avd_info.cma_buf_info.dma_attrs = attrs;
	srccap_dev->avd_info.cma_buf_info.virtAddr = cookie;
	SRCCAP_AVD_MSG_INFO("[COMB][CMA]buf = {va : %llx, addr : %llx, size = %lx}\n",
		(unsigned long long)srccap_dev->avd_info.cma_buf_info.virtAddr,
		srccap_dev->avd_info.cma_buf_info.DmaAddr,
		(unsigned long)srccap_dev->avd_info.cma_buf_info.u32Size);

	ret = dma_mapping_error(&pdev->dev, bus_addr_offset);
	if (ret) {
		SRCCAP_AVD_MSG_ERROR("[COMB][CMA] dma_alloc_attrs fail\n");
		return false;
	}

	*pAddr = mmap_info.start_bus_address - COMB_BUS_OFFSET;
	*pSize = mmap_info.buffer_size;
	*cma_id = mmap_info.cma_id;

	SRCCAP_AVD_MSG_INFO("[COMB][CMA]get OK addr 0x%llx, len 0x%lx cma_id = %d",
		*pAddr, (unsigned long)(*pSize), (int)(*cma_id));

	return true;
}
#endif

#if defined(CONFIG_CMA) || defined(CONFIG_DD_USE_CMA)
int mtk_avd_allco_vd_cma_buf(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	bool result = false;
	struct platform_device *pdev;
	uint32_t len = 0;
	uint32_t cma_id = 0;
	u64 addr;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	pdev = to_platform_device(srccap_dev->dev);

	if (srccap_dev->avd_info.Comb3dBufHeapId == 0) {
		SRCCAP_AVD_MSG_WARNING("It's to use Static memory. Head ID = %d\n",
			srccap_dev->avd_info.Comb3dBufHeapId);
		return ret;
	}

	if (srccap_dev->avd_info.bCMAInit == TRUE) {
		SRCCAP_AVD_MSG_WARNING("[CMA][VD]CMA is already registered. Pass it .\n");
		return ret;
	}

	SRCCAP_AVD_MSG_INFO("[START][CMA][VD]Get VD comb buffer.\n");
	result = comb_get_memory(srccap_dev, pdev, &addr, &len, &cma_id);
	if (result != true) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	srccap_dev->avd_info.Comb3dBufAddr = addr;
	srccap_dev->avd_info.Comb3dBufSize = len;
	srccap_dev->avd_info.Comb3dBufHeapId = cma_id;
	//Set alloc Flag
	srccap_dev->avd_info.bCMAInit = TRUE;
	SRCCAP_AVD_MSG_INFO("[CMA][VD]addr = 0x%llx size = 0x%lx cma_id = %d\n",
		addr, (unsigned long)len, (int)cma_id);
	SRCCAP_AVD_MSG_INFO("[END][CMA][VD]Get VD comb buffer.\n");

	if (mtk_avd_InitHWinfo(srccap_dev) < 0) {
		SRCCAP_AVD_MSG_ERROR("Init AVD RegBase & Bank failed!!\r\n");
		ret = -ENOENT;
	}

	return ret;
}
#endif

#if defined(CONFIG_CMA) || defined(CONFIG_DD_USE_CMA)
int mtk_avd_free_vd_cma_buf(
	struct mtk_srccap_dev *srccap_dev)
{
	struct device_node *np;
	struct platform_device *vd_pdev;
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->avd_info.Comb3dBufHeapId == 0) {
		SRCCAP_AVD_MSG_WARNING("It's to use Static memory. Head ID = %d\n",
			srccap_dev->avd_info.Comb3dBufHeapId);
		return ret;
	}

	if (srccap_dev->avd_info.bCMAInit == FALSE) {
		SRCCAP_AVD_MSG_WARNING("[CMA][VD]CMA is already Free. Pass it .\n");
		return ret;
	}

	np = of_find_compatible_node(NULL, NULL, "mediatek,dtv-mmap-vd-comb-buf");
	if (np == NULL) {
		SRCCAP_AVD_MSG_ERROR("[COMB]of_find_compatible_node failed \r\n");
		return -ENOENT;
	}

	vd_pdev = of_find_device_by_node(np);
	if (!vd_pdev) {
		pr_emerg("[COMB][CMA] cannot find device (dtv-mmap-vd-comb-buf)\n");
		return -ENOENT;
	}

	//VD cma release
	if (srccap_dev->avd_info.cma_buf_info.DmaAddr != 0) {
		SRCCAP_AVD_MSG_INFO("[COMB][CMA]buf = {va : %llx, addr : %llx, size = %lx}\n",
			(unsigned long long)(srccap_dev->avd_info.cma_buf_info.virtAddr),
			srccap_dev->avd_info.cma_buf_info.DmaAddr,
			(unsigned long)srccap_dev->avd_info.cma_buf_info.u32Size);
		dma_free_attrs(&vd_pdev->dev,
			srccap_dev->avd_info.cma_buf_info.u32Size,
			srccap_dev->avd_info.cma_buf_info.virtAddr,
			srccap_dev->avd_info.cma_buf_info.DmaAddr,
			srccap_dev->avd_info.cma_buf_info.dma_attrs);
		srccap_dev->avd_info.cma_buf_info.DmaAddr = 0;
		srccap_dev->avd_info.cma_buf_info.virtAddr = NULL;
		SRCCAP_AVD_MSG_INFO("[CMA][VD]Free VD cma memory done.\n");
	}

	//Set free flag
	srccap_dev->avd_info.bCMAInit = FALSE;

	return ret;
}
#endif

static int avd_debug_set_pypass_pq(struct device *dev, const char *args[], int arg_num)
{
	int ret = 0;
	int input = 0;
	struct mtk_srccap_dev *srccap_dev = dev_get_drvdata(dev);

	if (!srccap_dev) {
		SRCCAP_LOG(LOG_LEVEL_ERROR, "srccap_dev is NULL!\n");
		return -EINVAL;
	}

	if (arg_num < 1)
		goto exit;

	ret = kstrtoint(args[0], 0, &input);
	if (ret < 0)
		goto exit;

	srccap_dev->avd_info.bIsPassPQ = input;

	SRCCAP_AVD_MSG_INFO("Input =  %d. bIsPassPQ = %d\n", input, srccap_dev->avd_info.bIsPassPQ);

exit:
	return ret;
}

static int avd_debug_set_pypass_colorsystem(struct device *dev, const char *args[], int arg_num)
{
	int ret = 0;
	int input = 0;
	struct mtk_srccap_dev *srccap_dev = dev_get_drvdata(dev);

	if (!srccap_dev) {
		SRCCAP_LOG(LOG_LEVEL_ERROR, "srccap_dev is NULL!\n");
		return -EINVAL;
	}

	if (arg_num < 1)
		goto exit;

	ret = kstrtoint(args[0], 0, &input);
	if (ret < 0)
		goto exit;

	srccap_dev->avd_info.bIsPassColorsys = input;

	SRCCAP_AVD_MSG_INFO("Input =  %d. bIsPassColorsys = %d\n",
		input, srccap_dev->avd_info.bIsPassColorsys);

exit:
	return ret;
}

static int avd_debug_set_ForceCropScreenInfo(struct device *dev, const char *args[], int arg_num)
{
	int ret = 0;
	int input = 0;
	struct mtk_srccap_dev *srccap_dev = dev_get_drvdata(dev);

	if (!srccap_dev) {
		SRCCAP_LOG(LOG_LEVEL_ERROR, "srccap_dev is NULL!\n");
		return -EINVAL;
	}

	if (arg_num < 1)
		goto exit;

	ret = kstrtoint(args[0], 0, &input);
	if (ret < 0)
		goto exit;

	srccap_dev->avd_info.bIsForceCropScreenInfo = input;

	SRCCAP_AVD_MSG_INFO("Input =  %d. bIsForceCropScreenInfo = %d\n",
		input, srccap_dev->avd_info.bIsForceCropScreenInfo);

exit:
	return ret;
}

static int avd_debug_print_help(void)
{
	int ret = 0;

	SRCCAP_AVD_MSG_ERROR(
		"----------------Avd debug commands help start----------------\n");
	SRCCAP_AVD_MSG_ERROR(
		"//Set bypasspq = 1 is bypass VD pqmap, default bypasspq is '0'\n");
	SRCCAP_AVD_MSG_ERROR(
		"echo bypasspq=1 > sys/devices/platform/mtk_srccap0/mtk_dbg/avd\n");
	SRCCAP_AVD_MSG_ERROR(
		"----------------Avd debug commands help end----------------\n");

	return  ret;
}

static int avd_parse_cmd_helper(char *buf, char *sep[], int max_cnt)
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
	return (cnt - AVD_CMD_REMOVE_LEN);
}

int mtk_avd_store(
	struct device *dev,
	const char *buf)
{
	int ret = 0;
	char cmd[AVD_MAX_CMD_LENGTH];
	char *args[AVD_MAX_ARG_NUM] = {NULL};
	int arg_num;

	if ((dev == NULL) || (buf == NULL)) {
		SRCCAP_AVD_MSG_ERROR("NULL pointer\n");
		goto exit;
	}

	if (strncpy(cmd, buf, AVD_MAX_CMD_LENGTH) == NULL) {
		SRCCAP_AVD_MSG_ERROR("strcpy Fail\n");
		goto exit;
	}

	cmd[AVD_MAX_CMD_LENGTH - 1] = '\0';
	arg_num = avd_parse_cmd_helper(cmd, args, AVD_MAX_ARG_NUM);
	SRCCAP_AVD_MSG_INFO("cmd: %s, num: %d.\n", cmd, arg_num);

	if (strncmp(cmd, "bypasspq", AVD_MAX_CMD_LENGTH) == 0) {
		ret = avd_debug_set_pypass_pq(dev, (const char **)&args[1], arg_num);
	} else if (strncmp(cmd, "bypassColorsys", AVD_MAX_CMD_LENGTH) == 0) {
		ret = avd_debug_set_pypass_colorsystem(dev, (const char **)&args[1], arg_num);
	} else if (strncmp(cmd, "ForceCropScreenInfo", AVD_MAX_CMD_LENGTH) == 0) {
		ret = avd_debug_set_ForceCropScreenInfo(dev, (const char **)&args[1], arg_num);

	} else if (strncmp(cmd, "help", AVD_MAX_CMD_LENGTH) == 0) {
		ret = avd_debug_print_help();
	} else {
		SRCCAP_AVD_MSG_ERROR("No match cmd: %s, num: %d.\n", cmd, arg_num);
		goto exit;
	}

	if (ret < 0) {
		SRCCAP_AVD_MSG_ERROR("Sysfs cmd fail.\n");
		goto exit;
	}

exit:
	return ret;
}

static const char *PowerModeToString(
	EN_POWER_MODE ePowerState)
{
	char *mode = NULL;

	switch (ePowerState) {
	case E_POWER_SUSPEND:
		mode = "Suspend";
		break;
	case E_POWER_RESUME:
		mode = "Resume";
		break;
	default:
		mode = "Other";
		break;
	}

	return mode;
}

int mtk_avd_SetPowerState(struct mtk_srccap_dev *srccap_dev, EN_POWER_MODE ePowerState)
{
	int ret = 0;
	enum v4l2_srccap_input_source eSrc = V4L2_SRCCAP_INPUT_SOURCE_NONE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	eSrc = srccap_dev->srccap_info.src;

	//Check Current is Vd source and device id is Zero.
	if (((eSrc > V4L2_SRCCAP_INPUT_SOURCE_SCART2) ||
		(eSrc < V4L2_SRCCAP_INPUT_SOURCE_CVBS) ||
		((eSrc < V4L2_SRCCAP_INPUT_SOURCE_ATV) &&
		(eSrc > V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4))) ||
		(srccap_dev->dev_id != 0)) {
		SRCCAP_AVD_MSG_INFO(
			"[%s]Not VD source : %s(%d). DevID:(%d) Do not perform this action.\n",
			PowerModeToString(ePowerState),
			V4L2InputSourceToString(eSrc),
			eSrc,
			srccap_dev->dev_id);
		return V4L2_EXT_AVD_OK;
	}

	if (ePowerState == E_POWER_SUSPEND) {
		SRCCAP_AVD_MSG_INFO("[%s]Start, Source: %s(%d), DevID:(%d)\n",
			PowerModeToString(ePowerState),
			V4L2InputSourceToString(eSrc),
			eSrc,
			srccap_dev->dev_id);
		Api_AVD_SetPowerState(E_POWER_SUSPEND);
		ret = mtk_avd_disable_clock(srccap_dev);
		ret |= mtk_avd_free_vd_cma_buf(srccap_dev);
		if (ret > 0)
			return V4L2_EXT_AVD_FAIL;

		SRCCAP_AVD_MSG_INFO("[%s]End\n",
			PowerModeToString(ePowerState));
	} else if (ePowerState == E_POWER_RESUME) {
		SRCCAP_AVD_MSG_INFO("[%s]Start, Source: %s(%d), DevID:(%d)\n",
			PowerModeToString(ePowerState),
			V4L2InputSourceToString(eSrc),
			eSrc,
			srccap_dev->dev_id);
		ret = mtk_avd_eanble_clock(srccap_dev);
		ret |= mtk_avd_SetClockSource(srccap_dev);
		ret |= mtk_avd_allco_vd_cma_buf(srccap_dev);
		if (ret > 0)
			return V4L2_EXT_AVD_FAIL;

		Api_AVD_SetPowerState(E_POWER_RESUME);
		SRCCAP_AVD_MSG_INFO("[%s]End\n",
			PowerModeToString(ePowerState));
	} else {
		SRCCAP_AVD_MSG_DEBUG("[%s]Not support this Power State(%d), DevID:(%d).\n",
			PowerModeToString(ePowerState),
			ePowerState,
			srccap_dev->dev_id);
	}

	return V4L2_EXT_AVD_OK;
}

int mtk_avd_SetClockSource(struct mtk_srccap_dev *srccap_dev)
{
	struct clk *clk = NULL;
	int ret = 0;

	SRCCAP_AVD_MSG_INFO("in\n");
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	clk = srccap_dev->avd_info.stclock.clk_buf_sel;

	if (srccap_dev->avd_info.eInputSource == V4L2_SRCCAP_INPUT_SOURCE_ATV) {
		SRCCAP_AVD_MSG_INFO("(%s)ATV source\n", __func__);
		ret = clk_set_parent(clk, srccap_dev->avd_info.stclock.clk_vd_atv_input);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	} else if ((srccap_dev->avd_info.eInputSource >= V4L2_SRCCAP_INPUT_SOURCE_CVBS) &&
		(srccap_dev->avd_info.eInputSource <= V4L2_SRCCAP_INPUT_SOURCE_CVBS8)) {
		SRCCAP_AVD_MSG_INFO("(%s)CVBS(Others) source\n", __func__);
		ret = clk_set_parent(clk, srccap_dev->avd_info.stclock.clk_vd_cvbs_input);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}
	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

static struct srccap_avd_clock gAvdClk = {0};

int mtk_avd_1st_store_clk(struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	gAvdClk.cmbai2vd = srccap_dev->avd_info.stclock.cmbai2vd;
	gAvdClk.cmbao2vd = srccap_dev->avd_info.stclock.cmbao2vd;
	gAvdClk.cmbbi2vd = srccap_dev->avd_info.stclock.cmbbi2vd;
	gAvdClk.cmbbo2vd = srccap_dev->avd_info.stclock.cmbbo2vd;
	gAvdClk.mcu_mail02vd = srccap_dev->avd_info.stclock.mcu_mail02vd;
	gAvdClk.mcu_mail12vd = srccap_dev->avd_info.stclock.mcu_mail12vd;
	gAvdClk.smi2mcu_m2mcu = srccap_dev->avd_info.stclock.smi2mcu_m2mcu;
	gAvdClk.smi2vd = srccap_dev->avd_info.stclock.smi2vd;
	gAvdClk.vd2x2vd = srccap_dev->avd_info.stclock.vd2x2vd;
	gAvdClk.vd_32fsc2vd = srccap_dev->avd_info.stclock.vd_32fsc2vd;
	gAvdClk.vd2vd = srccap_dev->avd_info.stclock.vd2vd;
	gAvdClk.xtal_12m2vd = srccap_dev->avd_info.stclock.xtal_12m2vd;
	gAvdClk.xtal_12m2mcu_m2riu = srccap_dev->avd_info.stclock.xtal_12m2mcu_m2riu;
	gAvdClk.xtal_12m2mcu_m2mcu = srccap_dev->avd_info.stclock.xtal_12m2mcu_m2mcu;
	gAvdClk.clk_vd_atv_input = srccap_dev->avd_info.stclock.clk_vd_atv_input;
	gAvdClk.clk_vd_cvbs_input = srccap_dev->avd_info.stclock.clk_vd_cvbs_input;
	gAvdClk.clk_buf_sel = srccap_dev->avd_info.stclock.clk_buf_sel;

	return 0;

}

int _mtk_avd_chk_clk_null(struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->dev_id != 1) {
		SRCCAP_AVD_MSG_INFO("Support FD1 only\n");
		return -ENXIO;
	}

	if (srccap_dev->avd_info.stclock.cmbai2vd == NULL)
		srccap_dev->avd_info.stclock.cmbai2vd = gAvdClk.cmbai2vd;
	if (srccap_dev->avd_info.stclock.cmbao2vd == NULL)
		srccap_dev->avd_info.stclock.cmbao2vd = gAvdClk.cmbao2vd;
	if (srccap_dev->avd_info.stclock.cmbbi2vd == NULL)
		srccap_dev->avd_info.stclock.cmbbi2vd = gAvdClk.cmbbi2vd;
	if (srccap_dev->avd_info.stclock.cmbbo2vd == NULL)
		srccap_dev->avd_info.stclock.cmbbo2vd = gAvdClk.cmbbo2vd;
	if (srccap_dev->avd_info.stclock.mcu_mail02vd == NULL)
		srccap_dev->avd_info.stclock.mcu_mail02vd = gAvdClk.mcu_mail02vd;
	if (srccap_dev->avd_info.stclock.mcu_mail12vd == NULL)
		srccap_dev->avd_info.stclock.mcu_mail12vd = gAvdClk.mcu_mail12vd;
	if (srccap_dev->avd_info.stclock.smi2mcu_m2mcu == NULL)
		srccap_dev->avd_info.stclock.smi2mcu_m2mcu = gAvdClk.smi2mcu_m2mcu;
	if (srccap_dev->avd_info.stclock.smi2vd == NULL)
		srccap_dev->avd_info.stclock.smi2vd = gAvdClk.smi2vd;
	if (srccap_dev->avd_info.stclock.vd2x2vd == NULL)
		srccap_dev->avd_info.stclock.vd2x2vd = gAvdClk.vd2x2vd;
	if (srccap_dev->avd_info.stclock.vd_32fsc2vd == NULL)
		srccap_dev->avd_info.stclock.vd_32fsc2vd = gAvdClk.vd_32fsc2vd;
	if (srccap_dev->avd_info.stclock.vd2vd == NULL)
		srccap_dev->avd_info.stclock.vd2vd = gAvdClk.vd2vd;
	if (srccap_dev->avd_info.stclock.xtal_12m2vd == NULL)
		srccap_dev->avd_info.stclock.xtal_12m2vd = gAvdClk.xtal_12m2vd;
	if (srccap_dev->avd_info.stclock.xtal_12m2mcu_m2riu == NULL)
		srccap_dev->avd_info.stclock.xtal_12m2mcu_m2riu = gAvdClk.xtal_12m2mcu_m2riu;
	if (srccap_dev->avd_info.stclock.xtal_12m2mcu_m2mcu == NULL)
		srccap_dev->avd_info.stclock.xtal_12m2mcu_m2mcu = gAvdClk.xtal_12m2mcu_m2mcu;
	if (srccap_dev->avd_info.stclock.clk_vd_atv_input == NULL)
		srccap_dev->avd_info.stclock.clk_vd_atv_input = gAvdClk.clk_vd_atv_input;
	if (srccap_dev->avd_info.stclock.clk_vd_cvbs_input == NULL)
		srccap_dev->avd_info.stclock.clk_vd_cvbs_input = gAvdClk.clk_vd_cvbs_input;
	if (srccap_dev->avd_info.stclock.clk_buf_sel == NULL)
		srccap_dev->avd_info.stclock.clk_buf_sel = gAvdClk.clk_buf_sel;

	return 0;
}

int mtk_avd_eanble_clock(struct mtk_srccap_dev *srccap_dev)
{
	struct clk *clk = NULL;
	int res;

	SRCCAP_AVD_MSG_INFO("in\n");

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->avd_info.en_count == AVD_CLOCK_MAX_COUNT) {
		SRCCAP_AVD_MSG_INFO("Reach Clock Max count Skip.\n");
		return V4L2_EXT_AVD_OK;
	}

	/*Count plus one.*/
	srccap_dev->avd_info.en_count++;

	SRCCAP_AVD_MSG_INFO("Current count : %d.\n",
	srccap_dev->avd_info.en_count);
	_mtk_avd_chk_clk_null(srccap_dev);

	/* enable register : reg_sw_en_cmbai2vd*/
	clk = srccap_dev->avd_info.stclock.cmbai2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_CMBAI2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_cmbao2vd */
	clk = srccap_dev->avd_info.stclock.cmbao2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_CMBAO2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register :  reg_sw_en_cmbbi2vd*/
	clk = srccap_dev->avd_info.stclock.cmbbi2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_CMBBI2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register :  reg_sw_en_cmbbo2vd*/
	clk = srccap_dev->avd_info.stclock.cmbbo2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_CMBBO2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_mcu_mail02vd*/
	clk = srccap_dev->avd_info.stclock.mcu_mail02vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_MCU_MAIL02VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register  : reg_sw_en_mcu_mail12vd*/
	clk = srccap_dev->avd_info.stclock.mcu_mail12vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_MCU_MAIL12VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register :  reg_sw_en_smi2mcu_m2mcu*/
	clk = srccap_dev->avd_info.stclock.smi2mcu_m2mcu;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_SMI2MCU_M2MCU: %d\n", res);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_smi2vd*/
	clk = srccap_dev->avd_info.stclock.smi2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_SMI2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register  : reg_sw_en_vd2x2vd*/
	clk = srccap_dev->avd_info.stclock.vd2x2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_VD2X2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register  : reg_sw_en_vd_32fsc2vd*/
	clk = srccap_dev->avd_info.stclock.vd_32fsc2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_VD_32FSC2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_vd2vd*/
	clk = srccap_dev->avd_info.stclock.vd2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_VD2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register  : reg_sw_en_xtal_12m2vd*/
	clk = srccap_dev->avd_info.stclock.xtal_12m2vd;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_XTAL_12M2VD: %d\n", res);
		return -EINVAL;
	}

	/* enable register : reg_sw_en_xtal_12m2mcu_m2riu*/
	clk = srccap_dev->avd_info.stclock.xtal_12m2mcu_m2riu;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_XTAL_12M2MCU_M2RIU: %d\n", res);
		return -EINVAL;
	}

	/* enable register :  reg_sw_en_xtal_12m2mcu_m2mcu*/
	clk = srccap_dev->avd_info.stclock.xtal_12m2mcu_m2mcu;
	res = clk_prepare_enable(clk);
	if (res) {
		dev_err(srccap_dev->dev, "Failed to enable VD_EN_XTAL_12M2MCU_M2MCU: %d\n", res);
		return -EINVAL;
	}

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

int mtk_avd_disable_clock(struct mtk_srccap_dev *srccap_dev)
{
	struct clk *clk = NULL;

	SRCCAP_AVD_MSG_INFO("in\n");

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev->avd_info.en_count == AVD_CLOCK_MIN_COUNT) {
		SRCCAP_AVD_MSG_INFO("Reach Clock Min count Skip.\n");
		return V4L2_EXT_AVD_OK;
	}

	/*Count minus one.*/
	srccap_dev->avd_info.en_count--;

	SRCCAP_AVD_MSG_INFO("Current count : %d.\n",
		srccap_dev->avd_info.en_count);

	/* disable register  : reg_sw_en_xtal_12m2vd*/
	clk = srccap_dev->avd_info.stclock.xtal_12m2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_xtal_12m2mcu_m2riu*/
	clk = srccap_dev->avd_info.stclock.xtal_12m2mcu_m2riu;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register :  reg_sw_en_xtal_12m2mcu_m2mcu*/
	clk = srccap_dev->avd_info.stclock.xtal_12m2mcu_m2mcu;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_mcu_mail02vd*/
	clk = srccap_dev->avd_info.stclock.mcu_mail02vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register  : reg_sw_en_mcu_mail12vd*/
	clk = srccap_dev->avd_info.stclock.mcu_mail12vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_cmbai2vd*/
	clk = srccap_dev->avd_info.stclock.cmbai2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_cmbao2vd */
	clk = srccap_dev->avd_info.stclock.cmbao2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register :  reg_sw_en_cmbbi2vd*/
	clk = srccap_dev->avd_info.stclock.cmbbi2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register :  reg_sw_en_cmbbo2vd*/
	clk = srccap_dev->avd_info.stclock.cmbbo2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_vd2vd*/
	clk = srccap_dev->avd_info.stclock.vd2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register  : reg_sw_en_vd2x2vd*/
	clk = srccap_dev->avd_info.stclock.vd2x2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register  : reg_sw_en_vd_32fsc2vd*/
	clk = srccap_dev->avd_info.stclock.vd_32fsc2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register : reg_sw_en_smi2vd*/
	clk = srccap_dev->avd_info.stclock.smi2vd;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	/* disable register :  reg_sw_en_smi2mcu_m2mcu*/
	clk = srccap_dev->avd_info.stclock.smi2mcu_m2mcu;
	if (__clk_is_enabled(clk) == true)
		clk_disable_unprepare(clk);

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

int mtk_avd_init_customer_setting(struct mtk_srccap_dev *srccap_dev,
	struct v4l2_ext_avd_init_data *init_data)
{
	SRCCAP_AVD_MSG_INFO("in\n");

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	SRCCAP_AVD_MSG_INFO(
		"vd_sensibility = %d, vd_patchflag = %d, vd_factory = %d\n",
		srccap_dev->avd_info.cus_setting.sensibility.init,
		srccap_dev->avd_info.cus_setting.patchflag.init,
		srccap_dev->avd_info.cus_setting.factory.init);

	if (srccap_dev->avd_info.cus_setting.sensibility.init == 1) {
		init_data->eVDHsyncSensitivityNormal.u8DetectWinBeforeLock =
			srccap_dev->avd_info.cus_setting.sensibility.normal_detect_win_before_lock;
		init_data->eVDHsyncSensitivityNormal.u8DetectWinAfterLock =
			srccap_dev->avd_info.cus_setting.sensibility.noamrl_detect_win_after_lock;
		init_data->eVDHsyncSensitivityNormal.u8CNTRFailBeforeLock =
			srccap_dev->avd_info.cus_setting.sensibility.normal_cntr_fail_before_lock;
		init_data->eVDHsyncSensitivityNormal.u8CNTRSyncBeforeLock =
			srccap_dev->avd_info.cus_setting.sensibility.normal_cntr_sync_before_lock;
		init_data->eVDHsyncSensitivityNormal.u8CNTRSyncAfterLock =
			srccap_dev->avd_info.cus_setting.sensibility.normal_cntr_sync_after_lock;
		init_data->eVDHsyncSensitivityTuning.u8DetectWinBeforeLock =
			srccap_dev->avd_info.cus_setting.sensibility.scan_detect_win_before_lock;
		init_data->eVDHsyncSensitivityTuning.u8DetectWinAfterLock =
			srccap_dev->avd_info.cus_setting.sensibility.scan_detect_win_after_lock;
		init_data->eVDHsyncSensitivityTuning.u8CNTRFailBeforeLock =
			srccap_dev->avd_info.cus_setting.sensibility.scan_cntr_fail_before_lock;
		init_data->eVDHsyncSensitivityTuning.u8CNTRSyncBeforeLock =
			srccap_dev->avd_info.cus_setting.sensibility.scan_cntr_sync_before_lock;
		init_data->eVDHsyncSensitivityTuning.u8CNTRSyncAfterLock =
			srccap_dev->avd_info.cus_setting.sensibility.scan_cntr_sync_after_lock;
	}

	if (srccap_dev->avd_info.cus_setting.patchflag.init == 1) {
		init_data->u32VDPatchFlag =
			srccap_dev->avd_info.cus_setting.patchflag.init_patch_flag;
	}

	if (srccap_dev->avd_info.cus_setting.factory.init == 1) {
		init_data->u8ColorKillHighBound =
			srccap_dev->avd_info.cus_setting.factory.color_kill_high_bound;
		init_data->u8ColorKillLowBound =
			srccap_dev->avd_info.cus_setting.factory.color_kill_low_bound;
	}

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

int mtk_avd_reboot(struct mtk_srccap_dev *srccap_dev)
{
	int ret;

	SRCCAP_AVD_MSG_INFO("in\n");
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = mtk_avd_exit();
	ret |= mtk_avd_disable_clock(srccap_dev);

	if (ret > 0)
		return V4L2_EXT_AVD_FAIL;

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

static int _avd_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	struct v4l2_timing ptiming;
	int ret;
	bool bResult;
	struct v4l2_ext_avd_factory_data stFactoryData;
	struct v4l2_ext_avd_standard_detection pDetection;
	struct v4l2_ext_avd_info pinfo;
	struct v4l2_ext_avd_crop_screen_info stCropScreen;
	__u32 u32AVDFlag;
	__u16 u16vdstatus;
	__u8 u8HsyncEdge, u8NoiseMag, u8lock;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler,
					struct mtk_srccap_dev, avd_ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_AVD_HSYNC_EDGE:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_HSYNC_EDGE\n");
		memset(&u8HsyncEdge, 0, sizeof(__u8));
		ret = mtk_avd_hsync_edge(&u8HsyncEdge);
		memcpy((void *)ctrl->p_new.p_u8, &u8HsyncEdge, sizeof(__u8));
		break;
	case V4L2_CID_AVD_FLAG:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_FLAG\n");
		memset(&u32AVDFlag, 0, sizeof(__u32));
		ret = mtk_avd_g_flag(&u32AVDFlag);
		memcpy((void *)ctrl->p_new.p_u8, &u32AVDFlag, sizeof(__u32));
		break;
	case V4L2_CID_AVD_V_TOTAL:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_V_TOTAL\n");
		memset(&ptiming, 0, sizeof(struct v4l2_timing));
		ret = mtk_avd_v_total(&ptiming);
		memcpy((void *)ctrl->p_new.p_u8, &ptiming,
					sizeof(struct v4l2_timing));
		break;
	case V4L2_CID_AVD_NOISE_MAG:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_NOISE_MAG\n");
		memset(&u8NoiseMag, 0, sizeof(__u8));
		ret = mtk_avd_noise_mag(&u8NoiseMag);
		memcpy((void *)ctrl->p_new.p_u8, &u8NoiseMag, sizeof(__u8));
		break;
	case V4L2_CID_AVD_IS_SYNCLOCKED:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_IS_SYNCLOCKED\n");
		memset(&u8lock, 0, sizeof(__u8));
		ret = mtk_avd_is_synclocked(&u8lock);
		memcpy((void *)ctrl->p_new.p_u8, &u8lock, sizeof(__u8));
		break;
	case V4L2_CID_AVD_STANDARD_DETETION:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_STANDARD_DETETION\n");
		memset(&pDetection, 0,
			sizeof(struct v4l2_ext_avd_standard_detection));
		ret = mtk_avd_standard_detetion(&pDetection);
		memcpy((void *)ctrl->p_new.p_u8, &pDetection,
			sizeof(struct v4l2_ext_avd_standard_detection));
		break;
	case V4L2_CID_AVD_STATUS:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_STATUS\n");
		memset(&u16vdstatus, 0, sizeof(__u16));
		ret = mtk_avd_status(&u16vdstatus);
		memcpy((void *)ctrl->p_new.p_u8, &u16vdstatus, sizeof(__u16));
		break;
	case V4L2_CID_AVD_SCAN_HSYNC_CHECK:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_SCAN_HSYNC_CHECK\n");
		ret = mtk_avd_scan_hsync_check(&pHsync);
		memcpy((void *)ctrl->p_new.p_u8, &pHsync,
				sizeof(struct v4l2_ext_avd_scan_hsyc_check));
		break;
	case V4L2_CID_AVD_VERTICAL_FREQ:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_VERTICAL_FREQ\n");
		memset(&ptiming, 0, sizeof(struct v4l2_timing));
		ret = mtk_avd_vertical_freq(&ptiming);
		memcpy((void *)ctrl->p_new.p_u8, &ptiming,
						sizeof(struct v4l2_timing));
		break;
	case V4L2_CID_AVD_INFO:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_INFO\n");
		memset(&pinfo, 0, sizeof(struct v4l2_ext_avd_info));
		ret = mtk_avd_info(&pinfo);
		memcpy((void *)ctrl->p_new.p_u8, &pinfo,
					sizeof(struct v4l2_ext_avd_info));
		break;
	case V4L2_CID_AVD_IS_LOCK_AUDIO_CARRIER:
		SRCCAP_AVD_MSG_DEBUG(
		"[V4L2]V4L2_CID_AVD_IS_LOCK_AUDIO_CARRIER\n");
		memset(&bResult, 0, sizeof(bool));
		ret = 0;
		mtk_avd_is_lock_audio_carrier(&bResult);
		memcpy((void *)ctrl->p_new.p_u8, &bResult, sizeof(bool));

		break;
	case V4L2_CID_AVD_ALIVE_CHECK:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_ALIVE_CHECK\n");
		memset(&bResult, 0, sizeof(bool));
		ret = 0;
		mtk_avd_alive_check(&bResult);
		memcpy((void *)ctrl->p_new.p_u8, &bResult, sizeof(bool));
		break;
	case V4L2_CID_AVD_FACTORY_PARA:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_FACTORY_PARA\n");
		memset(&stFactoryData, 0,
			sizeof(struct v4l2_ext_avd_factory_data));
		stFactoryData.eFactoryPara = eFactoryPara;
		ret = mtk_avd_get_factory_para(&stFactoryData);
		memcpy((void *)ctrl->p_new.p_u8, &stFactoryData,
			sizeof(struct v4l2_ext_avd_factory_data));
		break;
	case V4L2_CID_AVD_VBI_CGMS_DETECT:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_VBI_CGMS_DETECT\n");
		memset(&bResult, 0, sizeof(bool));
		ret = mtk_avd_vbi_CGMS_detect(&bResult);
		memcpy((void *)ctrl->p_new.p_u8, &bResult, sizeof(bool));
		break;
	case V4L2_CID_AVD_AFEC_MACRO_VISION_DETECT:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_AFEC_MACRO_VISION_DETECT\n");
		memset(&bResult, 0, sizeof(bool));
		ret = mtk_avd_macro_vision_detect(&bResult);
		memcpy((void *)ctrl->p_new.p_u8, &bResult, sizeof(bool));
		break;
	case V4L2_CID_AVD_CROP_SCREEN_INFO:
		SRCCAP_AVD_MSG_INFO("[V4L2]V4L2_CID_AVD_CROP_SCREEN_INFO\n");
		memset(&stCropScreen, 0, sizeof(struct v4l2_ext_avd_crop_screen_info));
		ret = mtk_avd_get_crop_screen_info(srccap_dev, &stCropScreen);
		memcpy((void *)ctrl->p_new.p_u8, &stCropScreen,
			sizeof(struct v4l2_ext_avd_crop_screen_info));
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int _avd_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;
	struct v4l2_ext_avd_init_data pParam;
	v4l2_std_id u64Videostandardtype;
	struct v4L2_ext_avd_video_standard pVideoStandard;
	enum v4l2_ext_avd_freerun_freq pFreerunFreq;
	struct v4l2_ext_avd_still_image_param pImageParam;
	struct v4l2_ext_avd_factory_data stFactoryData;
	struct v4l2_ext_avd_3d_comb_speed pSpeed;
	struct v4l2_ext_avd_info info;
	struct v4l2_ext_avd_set_input pSetInput;
	struct v4l2_ext_avd_crop_screen_info stCropScreen;
	enum v4l2_srccap_input_source eSrc;
	bool ScanMode;
	bool bCombEanble, bTuningEanble, bCancelEanble, bAtvSnow;
	__u32 u32VDPatchFlag;
	__u16 u16Htt;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler,
				struct mtk_srccap_dev, avd_ctrl_handler);

	eSrc = srccap_dev->avd_info.eInputSource;

	switch (ctrl->id) {
	case V4L2_CID_AVD_INIT:
		SRCCAP_AVD_MSG_INFO("[V4L2]V4L2_CID_AVD_INIT\n");
		memset(&pParam, 0, sizeof(struct v4l2_ext_avd_init_data));
		memcpy(&pParam, (void *)ctrl->p_new.p_u8,
					sizeof(struct v4l2_ext_avd_init_data));
		ret = mtk_avd_eanble_clock(srccap_dev);
		ret |= mtk_avd_allco_vd_cma_buf(srccap_dev);
		if (ret >= 0) {
			ret |= mtk_avd_init_customer_setting(srccap_dev, &pParam);
			ret |= mtk_avd_init(pParam);
		}
		srccap_dev->avd_info.bAtvResetParameter = TRUE;
		break;
	case V4L2_CID_AVD_EXIT:
		SRCCAP_AVD_MSG_INFO("[V4L2]V4L2_CID_AVD_EXIT\n");
		ret = mtk_avd_exit();
		ret |= mtk_avd_disable_clock(srccap_dev);
		ret |= mtk_avd_free_vd_cma_buf(srccap_dev);
		break;
	case V4L2_CID_AVD_INPUT:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_INPUT\n");
		memset(&pSetInput, 0,
				sizeof(struct v4l2_ext_avd_set_input));
		memcpy(&pSetInput, (void *)ctrl->p_new.p_u8,
				sizeof(struct v4l2_ext_avd_set_input));

		srccap_dev->avd_info.eInputSource = pSetInput.eInputSource;
		SRCCAP_AVD_MSG_INFO("[V4L2]V4L2_CID_AVD_INPUT src = %u\n",
			srccap_dev->avd_info.eInputSource);
		mtk_avd_SetClockSource(srccap_dev);
		ret = mtk_avd_input(&pSetInput.u8ScartFB, srccap_dev);
		break;
	case V4L2_CID_AVD_FLAG:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_FLAG\n");
		memset(&u32VDPatchFlag, 0, sizeof(__u32));
		memcpy(&u32VDPatchFlag, (void *)ctrl->p_new.p_u8,
							sizeof(__u32));
		ret = mtk_avd_s_flag(&u32VDPatchFlag);
		break;
	case V4L2_CID_AVD_FORCE_VIDEO_STANDARD:
		SRCCAP_AVD_MSG_DEBUG(
			"[V4L2]V4L2_CID_AVD_FORCE_VIDEO_STANDARD\n");
		memset(&u64Videostandardtype, 0, sizeof(v4l2_std_id));
		memcpy(&u64Videostandardtype, (void *)ctrl->p_new.p_u8,
							sizeof(v4l2_std_id));
		ret = mtk_avd_force_video_standard(&u64Videostandardtype);
		break;
	case V4L2_CID_AVD_VIDEO_STANDARD:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_VIDEO_STANDARD\n");
		memset(&pVideoStandard, 0,
				sizeof(struct v4L2_ext_avd_video_standard));
		memcpy(&pVideoStandard, (void *)ctrl->p_new.p_u8,
				sizeof(struct v4L2_ext_avd_video_standard));
		mtk_avd_SetCurForceSystem(srccap_dev, &pVideoStandard.u64Videostandardtype);
		mtk_avd_SetSamplingByHtt(srccap_dev, &pVideoStandard.u64Videostandardtype);
		ret = mtk_avd_video_standard(&pVideoStandard);
		break;
	case V4L2_CID_AVD_FREERUN_FREQ:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_FREERUN_FREQ\n");
		memset(&pFreerunFreq, 0,
			sizeof(enum v4l2_ext_avd_freerun_freq));
		memcpy(&pFreerunFreq, (void *)ctrl->p_new.p_u8,
			sizeof(enum v4l2_ext_avd_freerun_freq));
		ret = mtk_avd_freerun_freq(&pFreerunFreq);
		break;
	case V4L2_CID_AVD_STILL_IMAGE_PARAM:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_STILL_IMAGE_PARAM\n");
		memset(&pImageParam, 0,
			sizeof(struct v4l2_ext_avd_still_image_param));
		memcpy(&pImageParam, (void *)ctrl->p_new.p_u8,
			sizeof(struct v4l2_ext_avd_still_image_param));
		ret = mtk_avd_still_image_param(&pImageParam);
		break;
	case V4L2_CID_AVD_FACTORY_PARA:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_FACTORY_PARA\n");
		memset(&stFactoryData, 0,
			sizeof(struct v4l2_ext_avd_factory_data));
		memcpy(&stFactoryData, (void *)ctrl->p_new.p_u8,
			sizeof(struct v4l2_ext_avd_factory_data));
		if (stFactoryData.eFactoryOperate == V4L2_EXT_AVD_FACTORY_MODE_SET_VALUE)
			ret = mtk_avd_set_factory_para(&stFactoryData);
		else {
			eFactoryPara = stFactoryData.eFactoryPara;
			ret = 0;
		}
		break;
	case V4L2_CID_AVD_3D_COMB_SPEED:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_3D_COMB_SPEED\n");
		memset(&pSpeed, 0,
			sizeof(struct v4l2_ext_avd_3d_comb_speed));
		memcpy(&pSpeed, (void *)ctrl->p_new.p_u8,
			sizeof(struct v4l2_ext_avd_3d_comb_speed));
		ret = mtk_avd_3d_comb_speed(&pSpeed);
		break;
	case V4L2_CID_AVD_3D_COMB:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_3D_COMB\n");
		memset(&bCombEanble, 0, sizeof(bool));
		memcpy(&bCombEanble, (void *)ctrl->p_new.p_u8, sizeof(bool));
		ret = mtk_avd_3d_comb(&bCombEanble);
		break;
	case V4L2_CID_AVD_REG_FROM_DSP:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_REG_FROM_DSP\n");
		ret = mtk_avd_reg_from_dsp();
		break;
	case V4L2_CID_AVD_HTT_USER_MD:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_HTT_USER_MD\n");
		memset(&u16Htt, 0, sizeof(__u16));
		memcpy(&u16Htt, (void *)ctrl->p_new.p_u8, sizeof(__u16));
		ret = mtk_avd_htt_user_md(&u16Htt);
		break;
	case V4L2_CID_AVD_HSYNC_DETECTION_FOR_TUNING:
		SRCCAP_AVD_MSG_DEBUG(
		"[V4L2]V4L2_CID_AVD_HSYNC_DETECTION_FOR_TUNING\n");
		memset(&bTuningEanble, 0, sizeof(bool));
		memcpy(&bTuningEanble, (void *)ctrl->p_new.p_u8,
							sizeof(bool));
		ret = mtk_avd_hsync_detect_detetion_for_tuning(&bTuningEanble);
		srccap_dev->avd_info.bIsATVScanMode = bTuningEanble;
		SRCCAP_AVD_MSG_INFO("bIsATVScanMode = %d\n", bTuningEanble);
		break;
	case V4L2_CID_AVD_CHANNEL_CHANGE:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_CHANNEL_CHANGE\n");
		ret = mtk_avd_channel_change();
		break;
	case V4L2_CID_AVD_MCU_RESET:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_MCU_RESET\n");
		ret = mtk_avd_mcu_reset();
		ScanMode = srccap_dev->avd_info.bIsATVScanMode;
		//Start vd detect at this time.
		SRCCAP_AVD_MSG_INFO(
		"[AVD]ScanMode = %d, eSrc = %d, VifLock = %d ,bAtvVdDetect = %d\n",
			ScanMode, eSrc, srccap_dev->avd_info.bIsVifLock,
			srccap_dev->avd_info.bStrAtvVdDet);
		if ((ScanMode == SCAN_MODE) && (eSrc == V4L2_SRCCAP_INPUT_SOURCE_ATV)) {
			/* Checck current detect finish or not*/
			if (srccap_dev->avd_info.bStrAtvVdDet == TRUE) {
				/*Stop current Atv vd detect process if detect not finish*/
				srccap_dev->avd_info.bStopAtvVdDet = TRUE;
				SRCCAP_AVD_MSG_INFO("[AVD][Cancel]bStopAtvVdDet = (%d)\n",
				srccap_dev->avd_info.bStopAtvVdDet);
			}
			srccap_dev->avd_info.bIsVifLock = TRUE;
			srccap_dev->avd_info.bStrAtvVdDet = TRUE;
			SRCCAP_AVD_MSG_INFO("[AVD]bIsVifLock = (%d),bStrAtvVdDet = (%d)\n",
				srccap_dev->avd_info.bIsVifLock,
				srccap_dev->avd_info.bStrAtvVdDet);
		}
		break;
	case V4L2_CID_AVD_DSP_RESET:
		SRCCAP_AVD_MSG_DEBUG("[V4L2][%s][%d]V4L2_CID_AVD_DSP_RESET\n",
									__func__, __LINE__);
		ret = mtk_avd_dsp_reset();
		break;
	case V4L2_CID_AVD_START_AUTO_STANDARD_DETECTION:
		SRCCAP_AVD_MSG_DEBUG(
		"[V4L2]V4L2_CID_AVD_START_AUTO_STANDARD_DETECTION\n");
		memset(&u64Videostandardtype, 0, sizeof(v4l2_std_id));
		u64Videostandardtype = V4L2_STD_UNKNOWN;
		mtk_avd_SetCurForceSystem(srccap_dev, &u64Videostandardtype);
		ret = mtk_avd_start_auto_standard_detetion();
		break;
	case V4L2_CID_AVD_3D_COMB_SPEED_UP:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_3D_COMB_SPEED_UP\n");
		ret = mtk_avd_3d_comb_speed_up();
		break;
	case V4L2_CID_AVD_SCAN_HSYNC_CHECK:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_SCAN_HSYNC_CHECK\n");
		memset(&pHsync, 0,
			sizeof(struct v4l2_ext_avd_scan_hsyc_check));
		memcpy(&pHsync, (void *)ctrl->p_new.p_u8,
			sizeof(struct v4l2_ext_avd_scan_hsyc_check));
		if (srccap_dev->avd_info.cus_setting.sensibility.init == 1) {
			pHsync.u8HtotalTolerance =
				srccap_dev->avd_info.cus_setting.sensibility.scan_hsync_check_count;
			//Customer setting init flag enable.
			SRCCAP_AVD_MSG_INFO(
			"[AVD]scan_hsync_check_count = %d\n", pHsync.u8HtotalTolerance);
		}
		show_v4l2_ext_avd_scan_hsyc_check(&pHsync);
		ret = 0;
		break;
	case V4L2_CID_AVD_INFO:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_INFO\n");
		memset(&info, 0, sizeof(struct v4l2_ext_avd_info));
		memcpy(&info, (void *)ctrl->p_new.p_u8,
			sizeof(struct v4l2_ext_avd_info));
		ret = mtk_avd_setting_info(srccap_dev, &info);
		break;
	case V4L2_CID_AVD_75IRE_SETUP_CANCEL:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_75IRE_SETUP_CANCEL\n");
		memset(&bCancelEanble, 0, sizeof(bool));
		memcpy(&bCancelEanble, (void *)ctrl->p_new.p_u8, sizeof(bool));
		srccap_dev->avd_info.bCancelIreSetting = bCancelEanble;
		ret = mtk_avd_75ire_setup_cancel(&bCancelEanble);
		break;
	case V4L2_CID_AVD_CROP_SCREEN_INFO:
		SRCCAP_AVD_MSG_INFO("[V4L2]V4L2_CID_AVD_CROP_SCREEN_INFO\n");
		memset(&stCropScreen, 0, sizeof(struct v4l2_ext_avd_crop_screen_info));
		memcpy(&stCropScreen, (void *)ctrl->p_new.p_u8,
					sizeof(struct v4l2_ext_avd_crop_screen_info));
		ret = mtk_avd_set_crop_screen_info(srccap_dev, &stCropScreen);
		break;
	case V4L2_CID_AVD_SNOW_FLOW:
		SRCCAP_AVD_MSG_DEBUG("[V4L2]V4L2_CID_AVD_SNOW_FLOW\n");
		memset(&bAtvSnow, 0, sizeof(bool));
		memcpy(&bAtvSnow, (void *)ctrl->p_new.p_u8, sizeof(bool));
		srccap_dev->avd_info.bAtvSnow = bAtvSnow;

		if(bAtvSnow) {
			//give a default timing used to query sampling table.
			srccap_dev->avd_info.eStableTvsystem = V4L2_EXT_AVD_STD_NTSC_M;
		} else {
			//Reset parameter.
			srccap_dev->avd_info.eStableTvsystem = V4L2_EXT_AVD_STD_NOTSTANDARD;
		}
		SRCCAP_AVD_MSG_INFO("[SnowFlow]bAtvSnow = %d, eStableTvsystem = %d\n",
			bAtvSnow, srccap_dev->avd_info.eStableTvsystem);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops avd_ctrl_ops = {
	.g_volatile_ctrl = _avd_g_ctrl,
	.s_ctrl = _avd_s_ctrl,
};

static const struct v4l2_ctrl_config avd_ctrl[] = {
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_INIT,
		.name = "AVD Init",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4l2_ext_avd_init_data)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_EXIT,
		.name = "AVD exit",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_INPUT,
		.name = "AVD_INPUT",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4l2_ext_avd_set_input)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_FLAG,
		.name = "AVD_FLAG",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags =
		V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(unsigned int)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_SCAN_HSYNC_CHECK,
		.name = "AVD_SCAN_HSYNC_CHECK",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags =
		V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4l2_ext_avd_scan_hsyc_check)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_FORCE_VIDEO_STANDARD,
		.name = "AVD_FORCE_VIDEO_STANDARD",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(v4l2_std_id)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_VIDEO_STANDARD,
		.name = "AVD_VIDEO_STANDARD",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4L2_ext_avd_video_standard)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_FREERUN_FREQ,
		.name = "AVD_FREERUN_FREQ",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(enum v4l2_ext_avd_freerun_freq)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_STILL_IMAGE_PARAM,
		.name = "AVD_STILL_IMAGE_PARAM",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4l2_ext_avd_still_image_param)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_FACTORY_PARA,
		.name = "AVD_FACTORY_PARA",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4l2_ext_avd_factory_data)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_3D_COMB_SPEED,
		.name = "AVD_3D_COMB_SPEED",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4l2_ext_avd_3d_comb_speed)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_3D_COMB,
		.name = "AVD_3D_COMB",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(bool)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_REG_FROM_DSP,
		.name = "AVD_REG_FROM_DSP",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_HTT_USER_MD,
		.name = "AVD_HTT_USER_MD",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(unsigned short)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_HSYNC_DETECTION_FOR_TUNING,
		.name = "AVD_HSYNC_DETECTION_FOR_TUNING",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(bool)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_CHANNEL_CHANGE,
		.name = "AVD_CHANNEL_CHANGE",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_MCU_RESET,
		.name = "AVD_MCU_RESET",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_DSP_RESET,
		.name = "AVD_DSP_RESET",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_START_AUTO_STANDARD_DETECTION,
		.name = "AVD_START_AUTO_STANDARD_DETECTION",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_3D_COMB_SPEED_UP,
		.name = "AVD_3D_COMB_SPEED_UP",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_HSYNC_EDGE,
		.name = "AVD_HSYNC_EDGE",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(unsigned char)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_FLAG,
		.name = "AVD_FLAG",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(unsigned int)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_V_TOTAL,
		.name = "AVD_V_TOTAL",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(struct v4l2_timing)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_NOISE_MAG,
		.name = "AVD_NOISE_MAG",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(unsigned char)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_IS_SYNCLOCKED,
		.name = "AVD_NOISE_MAG",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(unsigned char)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_STANDARD_DETETION,
		.name = "AVD_STANDARD_DETETION",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(struct v4l2_ext_avd_standard_detection)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_STATUS,
		.name = "AVD_STATUS",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(unsigned short)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_SCAN_HSYNC_CHECK,
		.name = "AVD_SCAN_HSYNC_CHECK",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(struct v4l2_ext_avd_scan_hsyc_check)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_VERTICAL_FREQ,
		.name = "AVD_VERTICAL_FREQ",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(struct v4l2_timing)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_INFO,
		.name = "AVD_INFO",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4l2_ext_avd_info)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_IS_LOCK_AUDIO_CARRIER,
		.name = "AVD_IS_LOCK_AUDIO_CARRIER",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(bool)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_ALIVE_CHECK,
		.name = "AVD_ALIVE_CHECK",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(bool)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_VBI_CGMS_DETECT,
		.name = "AVD_VBI_CGMS_DETECT",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(bool)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_AFEC_MACRO_VISION_DETECT,
		.name = "AVD_AFEC_MACRO_VISION_DETECT",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.dims = {sizeof(bool)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_75IRE_SETUP_CANCEL,
		.name = "AVD_75IRE_SETUP_CANCEL",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(bool)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_CROP_SCREEN_INFO,
		.name = "AVD_CROP_SCREEN_INFO",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct v4l2_ext_avd_crop_screen_info)},
	},
	{
		.ops = &avd_ctrl_ops,
		.id = V4L2_CID_AVD_SNOW_FLOW,
		.name = "AVD_SNOW_FLOW",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(bool)},
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops avd_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops avd_sd_internal_ops = {
};

/* ============================================================================================== */
/* -------------------------------------- Global Functions -------------------------------------- */
/* ============================================================================================== */
int mtk_srccap_register_avd_subdev(
	struct v4l2_device *v4l2_dev, struct v4l2_subdev *subdev_avd,
	struct v4l2_ctrl_handler *avd_ctrl_handler)
{
	int ret = 0;
	u32 ctrl_count;
	u32 ctrl_num = sizeof(avd_ctrl)/sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(avd_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(avd_ctrl_handler, &avd_ctrl[ctrl_count],
									NULL);
	}

	ret = avd_ctrl_handler->error;
	if (ret) {
		SRCCAP_MSG_ERROR("failed to create avd ctrl handler\n");
		goto exit;
	}
	subdev_avd->ctrl_handler = avd_ctrl_handler;

	v4l2_subdev_init(subdev_avd, &avd_sd_ops);
	subdev_avd->internal_ops = &avd_sd_internal_ops;
	strlcpy(subdev_avd->name, "mtk-avd", sizeof(subdev_avd->name));

	ret = v4l2_device_register_subdev(v4l2_dev, subdev_avd);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to register avd subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(avd_ctrl_handler);
	return ret;
}

void mtk_srccap_unregister_avd_subdev(struct v4l2_subdev *subdev_avd)
{
	v4l2_ctrl_handler_free(subdev_avd->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_avd);
}
