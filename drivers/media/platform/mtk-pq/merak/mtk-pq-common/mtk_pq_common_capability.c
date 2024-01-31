/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
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
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/clk-provider.h>
#include <linux/timekeeping.h>
#include <linux/genalloc.h>
#include <linux/dma-resv.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-fence.h>
#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>

#include "mtk-efuse.h"
#include "mtk-dramc.h"

#include "hwreg_pq_common.h"

#include "mtk_pq.h"
#include "mtk_pq_common.h"

/*-----------------------------------------------------------------------------*/
/* Local Define */
/*-----------------------------------------------------------------------------*/
#define CAP_PAGE_SIZE 4096

/*-----------------------------------------------------------------------------*/
/* Local Functions*/
/*-----------------------------------------------------------------------------*/

int mtk_pq_common_capability_parse(struct mtk_pq_platform_device *pqdev)
{
	uint32_t value = 0;
	unsigned int freqIdx1 = 0, freqIdx2 = 0, channelNum = 0;
	uint32_t u32OPPath = 0;
	int ret = 0;

	struct device *property_dev = NULL;
	struct device_node *bank_node = NULL;

	if (pqdev == NULL) {
		PQ_MSG_ERROR("pqdev NULL\r\n");
		return -EINVAL;
	}
	property_dev = pqdev->dev;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "capability");

	if (bank_node == NULL) {
		PQ_MSG_ERROR("bank_node NULL\r\n");
		return -EINVAL;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter capability_parse\n");

	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	/* efuse for i_view */
	ret = drv_hwreg_pq_common_get_efuse(E_EFUSE_IDX_112, &value);
	if (ret != 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "hwreg get efuse(E_EFUSE_IDX_112) fail\n");
		value = 0;
		return -EINVAL;
	}
	pqdev->pqcaps.u8support_i_view = value;

	ret = drv_hwreg_pq_common_get_efuse(E_EFUSE_IDX_113, &value);
	if (ret != 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "hwreg get efuse(E_EFUSE_IDX_113) fail\n");
		value = 0;
		return -EINVAL;
	}
	pqdev->pqcaps.u8support_i_view_pip = value;

	/* dramc */
	ret |= of_property_read_u32(bank_node, "DRAMC_FREQ_IDX_1", &freqIdx1);
	PQ_CAPABILITY_CHECK_RET(ret, "DRAMC_Freq_IDX_1");

	ret |= of_property_read_u32(bank_node, "DRAMC_FREQ_IDX_2", &freqIdx2);
	PQ_CAPABILITY_CHECK_RET(ret, "DRAMC_Freq_IDX_2");

	channelNum = mtk_dramc_get_support_channel_num();

	freqIdx1 = (channelNum <= freqIdx1) ? (channelNum) : (freqIdx1);
	freqIdx2 = (channelNum <= freqIdx2) ? (channelNum) : (freqIdx2);

	pqdev->pqcaps.u32dramc_freq_1 = (uint32_t)mtk_dramc_get_freq(freqIdx1);
	pqdev->pqcaps.u32dramc_freq_2 = (uint32_t)mtk_dramc_get_freq(freqIdx2);
	pqdev->pqcaps.u32dramc_size   = (uint32_t)mtk_dramc_get_channel_total_size();
	pqdev->pqcaps.u32dramc_type   = (uint32_t)mtk_dramc_get_ddr_type();
	pqdev->pqcaps.u8dramc_pkg     = (uint8_t)mtk_dramc_get_ddrpkg();

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"Idx1 %d, Idx2 %d, freq_1 %d, freq_2 %d, size %d, type %d, pkg %d\n",
		freqIdx1, freqIdx2, pqdev->pqcaps.u32dramc_freq_1, pqdev->pqcaps.u32dramc_freq_2,
		pqdev->pqcaps.u32dramc_size, pqdev->pqcaps.u32dramc_type,
		pqdev->pqcaps.u8dramc_pkg);

	/* op path throughput */
	ret |= of_property_read_u32(bank_node, "OP_PATH_THROUGHPUT", &u32OPPath);
	PQ_CAPABILITY_CHECK_RET(ret, "OP_PATH_THROUGHPUT");
	pqdev->pqcaps.u32op_path_throughput = u32OPPath;

	return ret;
}

/*-----------------------------------------------------------------------------*/

/* efuse for i_view */
static ssize_t support_i_view_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = snprintf(buf, CAP_PAGE_SIZE, "%u\n", pqdev->pqcaps.u8support_i_view);

	return ((ret < 0) || (ret > CAP_PAGE_SIZE)) ? (-EINVAL) : (ret);
}

static ssize_t support_i_view_pip_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = snprintf(buf, CAP_PAGE_SIZE, "%u\n", pqdev->pqcaps.u8support_i_view_pip);

	return ((ret < 0) || (ret > CAP_PAGE_SIZE)) ? (-EINVAL) : (ret);
}

/* dramc */
static ssize_t dramc_freq_1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = snprintf(buf, CAP_PAGE_SIZE, "%u\n", pqdev->pqcaps.u32dramc_freq_1);

	return ((ret < 0) || (ret > CAP_PAGE_SIZE)) ? (-EINVAL) : (ret);
}

static ssize_t dramc_freq_2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = snprintf(buf, CAP_PAGE_SIZE, "%u\n", pqdev->pqcaps.u32dramc_freq_2);

	return ((ret < 0) || (ret > CAP_PAGE_SIZE)) ? (-EINVAL) : (ret);
}

static ssize_t dramc_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = snprintf(buf, CAP_PAGE_SIZE, "%u\n", pqdev->pqcaps.u32dramc_size);

	return ((ret < 0) || (ret > CAP_PAGE_SIZE)) ? (-EINVAL) : (ret);
}

static ssize_t dramc_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = snprintf(buf, CAP_PAGE_SIZE, "%u\n", pqdev->pqcaps.u32dramc_type);

	return ((ret < 0) || (ret > CAP_PAGE_SIZE)) ? (-EINVAL) : (ret);
}

static ssize_t dramc_pkg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = snprintf(buf, CAP_PAGE_SIZE, "%u\n", pqdev->pqcaps.u8dramc_pkg);

	return ((ret < 0) || (ret > CAP_PAGE_SIZE)) ? (-EINVAL) : (ret);
}

/* op path throughput */
static ssize_t op_path_throughput_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = snprintf(buf, CAP_PAGE_SIZE, "%u\n", pqdev->pqcaps.u32op_path_throughput);

	return ((ret < 0) || (ret > CAP_PAGE_SIZE)) ? (-EINVAL) : (ret);
}

/*-----------------------------------------------------------------------------*/

/* efuse for i_view */
static DEVICE_ATTR_RO(support_i_view);
static DEVICE_ATTR_RO(support_i_view_pip);

/* dramc */
static DEVICE_ATTR_RO(dramc_freq_1);
static DEVICE_ATTR_RO(dramc_freq_2);
static DEVICE_ATTR_RO(dramc_size);
static DEVICE_ATTR_RO(dramc_type);
static DEVICE_ATTR_RO(dramc_pkg);

/* op path throughput */
static DEVICE_ATTR_RO(op_path_throughput);

/*-----------------------------------------------------------------------------*/

static struct attribute *mtk_pq_capability_attrs[] = {
	/* efuse for i_view */
	&dev_attr_support_i_view.attr,
	&dev_attr_support_i_view_pip.attr,

	/* dramc */
	&dev_attr_dramc_freq_1.attr,
	&dev_attr_dramc_freq_2.attr,
	&dev_attr_dramc_size.attr,
	&dev_attr_dramc_type.attr,
	&dev_attr_dramc_pkg.attr,

	/* op path throughput */
	&dev_attr_op_path_throughput.attr,

	NULL,
};

/*-----------------------------------------------------------------------------*/

static const struct attribute_group mtk_pq_capability_attrs_group = {
	.name = "pq_capability",
	.attrs = mtk_pq_capability_attrs
};

void mtk_pq_common_capability_create_sysfs(struct device *pdv)
{
	int ret = 0;

	PQ_MSG_INFO("Device_create_file initialized\n");

	ret = sysfs_create_group(&pdv->kobj, &mtk_pq_capability_attrs_group);
	if (ret)
		dev_err(pdv, "[%d]Fail to create sysfs files: %d\n", __LINE__, ret);
}

void mtk_pq_common_capability_remove_sysfs(struct device *pdv)
{
	dev_info(pdv, "Remove device attribute files");
	sysfs_remove_group(&pdv->kobj, &mtk_pq_capability_attrs_group);
}

