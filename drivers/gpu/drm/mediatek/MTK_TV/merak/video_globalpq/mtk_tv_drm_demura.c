// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/miscdevice.h>
#include <drm/mtk_tv_drm.h>
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_crtc.h"
#include "mtk_tv_drm_demura.h"
#include "mtk_tv_drm_pqu_wrapper.h"
#include "mtk_tv_drm_video_panel.h"
#include "mtk_tv_drm_log.h"
#include "pqu_msg.h"
#include "ext_command_render_if.h"
#include "hwreg_render_stub.h"

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_DEMURA
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

#define DEMURA_U8ARRAY_TO_U32(hh, hl, lh, ll) \
	(((u32)(hh & 0xff) << 24) | ((u32)(hl & 0xff) << 16) | ((u32)(lh & 0xff) << 8) | \
	(u32)(ll & 0xff))

#define DEMURA_U8ARRAY_TO_U16(h, l) \
	(((u16)(h & 0xff) << 8) | (l & 0xff))

#define HEADER_GET_LUT_SIZE(pheader) DEMURA_U8ARRAY_TO_U32( \
			pheader->lut_size[0], \
			pheader->lut_size[1], \
			pheader->lut_size[2], \
			pheader->lut_size[3])

#define HEADER_GET_HEADER_SIZE(pheader) DEMURA_U8ARRAY_TO_U32( \
			pheader->header_size[0], \
			pheader->header_size[1], \
			pheader->header_size[2], \
			pheader->header_size[3])

#define HEADER_GET_VERSION(pheader) \
	DEMURA_U8ARRAY_TO_U32(pheader->version[0], \
			pheader->version[1], \
			pheader->version[2], \
			pheader->version[3])

#define HEADER_GET_H_SIZE(pheader) \
	DEMURA_U8ARRAY_TO_U16(pheader->h_size[0], \
			pheader->h_size[1])

#define HEADER_GET_V_SIZE(pheader) \
	DEMURA_U8ARRAY_TO_U16(pheader->v_size[0], \
			pheader->v_size[1])

#define HEADER_GET_R_OFFSET(pheader, idx) \
	DEMURA_U8ARRAY_TO_U16(pheader->offset_r[idx][0], \
			pheader->offset_r[idx][1])

#define HEADER_GET_G_OFFSET(pheader, idx) \
	DEMURA_U8ARRAY_TO_U16(pheader->offset_g[idx][0], \
			pheader->offset_g[idx][1])

#define HEADER_GET_B_OFFSET(pheader, idx) \
	DEMURA_U8ARRAY_TO_U16(pheader->offset_b[idx][0], \
			pheader->offset_b[idx][1])

#define HEADER_GET_PLANE(pheader, idx) \
	DEMURA_U8ARRAY_TO_U16(pheader->plane[idx][0], \
			pheader->plane[idx][1])

#define HEADER_GET_BLACKLIMIT(pheader) \
	DEMURA_U8ARRAY_TO_U16(pheader->black_limit[0], \
			pheader->black_limit[1])

#define HEADER_GET_WHITELIMIT(pheader) \
	DEMURA_U8ARRAY_TO_U16(pheader->white_limit[0], \
			pheader->white_limit[1])

#define HEADER_GET_HEADERCRC(pheader) \
	DEMURA_U8ARRAY_TO_U32(pheader->header_crc[0], \
			pheader->header_crc[1], \
			pheader->header_crc[2], \
			pheader->header_crc[3])

static uint8_t Main_slot;
static uint8_t DLG_slot;
static uint8_t pre_DLG_mode;

static struct miscdevice demura_misc_dev;
struct mtk_tv_drm_metabuf metabuf = {0};


//-------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Local Functions
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------

static int demura_mmap_open(struct inode *inode, struct file *file)
{
	return 0;
}


static int demura_mmap_release(struct inode *inode,
		struct file *file)
{
	return 0;
}

static int demura_userva_mmap(struct file *filp,
		struct vm_area_struct *vma)
{
	int ret;
	unsigned long start, len, offset;
	unsigned long pfn, pages;
	pgprot_t prot;

	if (!metabuf.mmap_info.phy_addr) {
		ERR("Invalid input");
		return -EIO;
	}

	LOG("mmap bus address  = 0x%llx", metabuf.mmap_info.bus_addr);
	LOG("mmap phys address = 0x%llx", metabuf.mmap_info.phy_addr);
	LOG("mmap virt address = 0x%p", metabuf.addr);

	// step_2: mmap to user_va
	len = vma->vm_end - vma->vm_start;
	offset = vma->vm_pgoff << PAGE_SHIFT;
	start = (unsigned long)metabuf.mmap_info.bus_addr;
	prot = vma->vm_page_prot;

	vma->vm_page_prot = pgprot_decrypted(prot);

	if (start + len < start)
		return -EINVAL;

	if (vma->vm_start > vma->vm_end)
		return -EINVAL;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	if (offset > len)
		return -EINVAL;

	if (offset > metabuf.size)
		return -EINVAL;

	if (len > metabuf.size - offset)
		return -EINVAL;

	len += start & ~PAGE_MASK;
	pfn = start >> PAGE_SHIFT;
	pages = (len + ~PAGE_MASK) >> PAGE_SHIFT;

	if (pfn + pages < pfn)
		return -EINVAL;

	if (vma->vm_pgoff > pages)
		return -EINVAL;

	pfn += vma->vm_pgoff;
	pages -= vma->vm_pgoff;

	if ((len >> PAGE_SHIFT) > pages)
		return -EINVAL;

	if (pfn < (start >> PAGE_SHIFT))
		return -EINVAL;

	len = vma->vm_end - vma->vm_start;

	ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);

	if (ret) {
		ERR("[%s] vm_iomap_memory failed with (%d)\n", __func__, ret);
		return -EAGAIN;
	}

	return ret;
}

static const struct file_operations demura_mmap_fops = {
	.owner = THIS_MODULE,
	.open = demura_mmap_open,
	.release = demura_mmap_release,
	.mmap = demura_userva_mmap,
};

int mtk_tv_drm_demura_init(
	struct mtk_demura_context *demura, struct mtk_panel_context *pctx_pnl)
{
	void *va = NULL;
	uint64_t pa = 0;
	uint32_t len = 0;
	uint32_t i;
	int ret = 0;

	if (!demura) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (demura->init == true) {
		LOG("demura is already inited");
		return 0;
	}
	memset(demura, 0, sizeof(struct mtk_demura_context));

	// get MMAP va pa info
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_DEMURA)) {
		ERR("metabuf get MTK_TV_DRM_METABUF_MMAP_TYPE_DEMURA fail");
		demura->init = -ENODEV;
		return 0;
	}
	len = metabuf.size / MTK_DEMURA_SLOT_NUM;
	va  = metabuf.addr;
	pa  = metabuf.mmap_info.phy_addr;

	// setup info
	memcpy(&demura->metabuf, &metabuf, sizeof(struct mtk_tv_drm_metabuf));
	for (i = 0; i < MTK_DEMURA_SLOT_NUM; ++i) {
		demura->config_slot[i].bin_va = va;
		demura->config_slot[i].bin_pa = pa;
		va += len;
		pa += len;
	}
	demura->curr_slot_idx = -1;
	demura->bin_max_size = len;
	demura->init = true;

	LOG("mmap bus address  = 0x%llx", demura->metabuf.mmap_info.bus_addr);
	LOG("mmap phys address = 0x%llx", demura->metabuf.mmap_info.phy_addr);
	LOG("mmap virt address = 0x%p", demura->metabuf.addr);
	LOG("mmap size         = %u", demura->metabuf.size);
	LOG("demura slot num   = %d",  MTK_DEMURA_SLOT_NUM);
	LOG("demura bin size   = %u", demura->bin_max_size);

	if (pctx_pnl->cus.dlg_en == 1)
		mtk_tv_drm_DLG_demura_slot_init(demura, pctx_pnl);
	else
		mtk_tv_drm_demura_slot_init(demura);

	//register misc mmap info
	demura_misc_dev.minor = MISC_DYNAMIC_MINOR;
	demura_misc_dev.name = "demura_mmap";
	demura_misc_dev.fops = &demura_mmap_fops;

	ret = misc_register(&demura_misc_dev);
	LOG("%s:misc_register %d\n", __func__, ret);

	return 0;
}

int mtk_tv_drm_demura_deinit(
	struct mtk_demura_context *demura)
{
	if (!demura) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (demura->init == false) {
		LOG("render demura is not inited");
		return 0;
	}
	mtk_tv_drm_metabuf_free(&demura->metabuf);
	memset(demura, 0, sizeof(struct mtk_demura_context));
	return 0;
}

int mtk_tv_drm_demura_set_config(
	struct mtk_demura_context *demura,
	struct drm_mtk_demura_config *config)
{
	struct mtk_demura_slot *slot = NULL;
	struct msg_render_demura_info info;
	struct pqu_render_demura_param param;
	uint32_t idx = 0;

	if (!demura || !config) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (demura->init != true) {
		ERR("Render demura is not inited");
		return demura->init;
	}
	drv_STUB("DEMURA_ID",       config->id);
	drv_STUB("DEMURA_DISABLE",  config->disable);
	drv_STUB("DEMURA_BIN_SIZE", config->binary_size);

	// setup slot
	if (config->binary_size >= demura->bin_max_size) {
		ERR("demura binary too large, input(%u) >= max(%u)",
			config->binary_size, demura->bin_max_size);
		return -EINVAL;
	}
	idx = (demura->curr_slot_idx >= 0) ? (demura->curr_slot_idx + 1) % MTK_DEMURA_SLOT_NUM : 1;
	slot = &demura->config_slot[idx];
	slot->id      = idx;
	slot->disable = config->disable;
	slot->bin_len = config->binary_size;
	if (config->disable == false && config->binary_size > 0)
		memcpy(slot->bin_va, config->binary_data, config->binary_size);
	else
		memset(slot->bin_va, 0, demura->bin_max_size);
	demura->curr_slot_idx = idx;
	LOG("slot idx  = %d", idx);
	LOG("  id      = %d", slot->id);
	LOG("  disable = %d", slot->disable);
	LOG("  bin len = %u", slot->bin_len);
	LOG("  bin va  = 0x%p", slot->bin_va);
	LOG("  bin pa  = 0x%llx", slot->bin_pa);

	// notify pqu
	memset(&info, 0, sizeof(struct msg_render_demura_info));
	memset(&param, 0, sizeof(struct pqu_render_demura_param));
	info.binary_pa  = param.binary_pa  = slot->bin_pa;
	info.binary_len = param.binary_len = slot->bin_len;
	info.disable    = param.disable    = slot->disable;
	info.hw_index   = param.hw_index   = slot->id;
	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_DEMURA, &info, &param);
	return 0;
}

int mtk_tv_drm_demura_check_buffer(
	void *buffer,
	uint32_t size)
{
	struct drm_mtk_demura_config *config = (struct drm_mtk_demura_config *)buffer;

	return ((sizeof(struct drm_mtk_demura_config) + config->binary_size) <= size) ? 0 : -1;
}

int mtk_tv_drm_demura_suspend(
	struct mtk_demura_context *demura)
{
	// do nothing
	return 0;
}

int mtk_tv_drm_demura_resume(
	struct mtk_demura_context *demura)
{
	struct mtk_demura_slot *slot;
	struct msg_render_demura_info info;
	struct pqu_render_demura_param param;
	static uint64_t next_bin_addr;

	if (!demura) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (demura->init != true) {
		ERR("Render demura is not inited");
		return demura->init;
	}

	// get previous slot
	if (demura->curr_slot_idx >= 0) {
		slot = &demura->config_slot[demura->curr_slot_idx];
	} else { // use binary when uboot load
		slot = &demura->config_slot[0];
		slot->id = 0;
		slot->bin_len = demura->bin_max_size; // don't know binary size, use max value
		slot->disable = false;
	}
	next_bin_addr = slot->bin_pa + DEMURA_BUFFER_DESC_SIZE;

	// notify pqu
	memset(&info, 0, sizeof(struct msg_render_demura_info));
	memset(&param, 0, sizeof(struct pqu_render_demura_param));
	info.binary_pa  = param.binary_pa  = next_bin_addr;
	info.binary_len = param.binary_len = slot->bin_len;
	info.disable    = param.disable    = slot->disable;
	info.hw_index   = param.hw_index   = slot->id;
	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_DEMURA, &info, &param);
	return 0;
}

void mtk_tv_drm_dump_demura_ddr_data(void *bin_va)
{
	struct mtk_demura_header *pheader;
	uint32_t lut_size = 0, header_size = 0, version = 0;
	u16 h_size, v_size;
	u8  mode;
	u8  plane_num;
	u8  h_blk_size, v_blk_size;
	u16 black_limit, white_limit;
	u32 header_crc;
	static void *next_bin_addr;

	next_bin_addr = bin_va + DEMURA_BUFFER_DESC_SIZE;
	pheader = (struct mtk_demura_header *)next_bin_addr;
	header_size = HEADER_GET_HEADER_SIZE(pheader);//sizeof(struct mtk_demura_header);
	version = HEADER_GET_VERSION(pheader);
	lut_size = HEADER_GET_LUT_SIZE(pheader);
	header_crc = HEADER_GET_HEADERCRC(pheader);
	h_size      = HEADER_GET_H_SIZE(pheader);
	v_size      = HEADER_GET_V_SIZE(pheader);
	mode        = pheader->mode;
	plane_num   = pheader->plane_num;
	h_blk_size = pheader->h_blk_size;
	v_blk_size = pheader->v_blk_size;
	black_limit = HEADER_GET_BLACKLIMIT(pheader);
	white_limit = HEADER_GET_WHITELIMIT(pheader);

	LOG("======DUMP Demura DDR info=====\n");
	LOG("next_bin_addr = %p\n", next_bin_addr);
	LOG("version = %x\n", version);
	LOG("header_size = %x\n", header_size);
	LOG("lut_size = %x\n", lut_size);
	LOG("header_crc = %x\n", header_crc);
	LOG("h_size = %x\n", h_size);
	LOG("v_size = %x\n", v_size);
	LOG("mode = %x\n", mode);
	LOG("plane_num = %x\n", plane_num);
	LOG("h_blk_size = %x\n", h_blk_size);
	LOG("v_blk_size = %x\n", v_blk_size);
	LOG("black_limit = %x\n", black_limit);
	LOG("white_limit = %x\n", white_limit);
	LOG("===============================\n");
}

unsigned int mtk_tv_drm_demura_get_bin_size(void *bin_va)
{
	struct mtk_demura_header *pheader;
	uint32_t lut_size = 0, header_size = 0;
	static void *next_bin_addr;

	next_bin_addr = bin_va + DEMURA_BUFFER_DESC_SIZE;
	pheader = (struct mtk_demura_header *)next_bin_addr;
	header_size = sizeof(struct mtk_demura_header);
	lut_size = HEADER_GET_LUT_SIZE(pheader);

	if (header_size == 0)
		ERR("[%s] Invalid header_size!!", __func__);
	if (lut_size == 0)
		ERR("[%s] Invalid lut_size!!", __func__);
	return header_size + lut_size;
}

int mtk_tv_drm_DLG_demura_slot_init(
	struct mtk_demura_context *demura, struct mtk_panel_context *pctx_pnl)
{
	struct mtk_demura_slot *slot = NULL;
	uint32_t i;
	uint32_t idx = 0;

	if (!demura) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (demura->init != true) {
		ERR("Render demura is not inited");
		return demura->init;
	}

	//main demura always use slot 0
	//DLG demura always use slot 1
	Main_slot = 0;
	DLG_slot = 1;

	if (pctx_pnl->info.dlg_on == 0)
		idx = Main_slot;
	else
		idx = DLG_slot;
	demura->curr_slot_idx = idx;
	pre_DLG_mode = pctx_pnl->info.dlg_on;

	for (i = 0; i < MTK_DEMURA_SLOT_NUM; ++i) {
		slot = &demura->config_slot[i];
		slot->id      = i;
		slot->disable = false;
		slot->bin_len = mtk_tv_drm_demura_get_bin_size(slot->bin_va);

		LOG("[%s] slot idx  = %d", __func__, i);
		LOG("  id      = %d", slot->id);
		LOG("  disable = %d", slot->disable);
		LOG("  bin len = %x", slot->bin_len);
		LOG("  bin va  = 0x%p", slot->bin_va);
		LOG("  bin pa  = 0x%llx", slot->bin_pa);

		mtk_tv_drm_dump_demura_ddr_data(slot->bin_va);
	}

	return 0;
}

int mtk_tv_drm_demura_slot_init(
	struct mtk_demura_context *demura)
{
	struct mtk_demura_slot *slot = NULL;
	uint32_t i;

	if (!demura) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (demura->init != true) {
		ERR("Render demura is not inited");
		return demura->init;
	}

	demura->curr_slot_idx = 0;
	for (i = 0; i < MTK_DEMURA_SLOT_NUM; ++i) {
		slot = &demura->config_slot[i];
		slot->id      = i;
		slot->disable = false;
		slot->bin_len = mtk_tv_drm_demura_get_bin_size(slot->bin_va);

		LOG("[%s] slot idx  = %d", __func__, i);
		LOG("  id      = %d", slot->id);
		LOG("  disable = %d", slot->disable);
		LOG("  bin len = %x", slot->bin_len);
		LOG("  bin va  = 0x%p", slot->bin_va);
		LOG("  bin pa  = 0x%llx", slot->bin_pa);
		mtk_tv_drm_dump_demura_ddr_data(slot->bin_va);
	}

	return 0;
}

int mtk_tv_drm_demura_slot_init(
	struct mtk_demura_context *demura)
{
	struct mtk_demura_slot *slot = NULL;
	uint32_t i;

	if (!demura) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (demura->init != true) {
		ERR("Render demura is not inited");
		return demura->init;
	}

	demura->curr_slot_idx = 0;
	for (i = 0; i < MTK_DEMURA_SLOT_NUM; ++i) {
		slot = &demura->config_slot[i];
		slot->id      = i;
		slot->disable = false;
		slot->bin_len = mtk_tv_drm_demura_get_bin_size(slot->bin_va);

		LOG("[%s] slot idx  = %d", __func__, i);
		LOG("  id      = %d", slot->id);
		LOG("  disable = %d", slot->disable);
		LOG("  bin len = %x", slot->bin_len);
		LOG("  bin va  = 0x%p", slot->bin_va);
		LOG("  bin pa  = 0x%llx", slot->bin_pa);
	}

	return 0;
}

int mtk_tv_drm_DLG_demura_set_config(
	struct mtk_demura_context *demura, bool bDLG_mode)
{
	struct mtk_demura_slot *slot = NULL;
	struct msg_render_demura_info info;
	struct pqu_render_demura_param param;
	static uint64_t next_bin_addr;
	uint32_t idx = 0;

	if (pre_DLG_mode == bDLG_mode) {	//no DLG mode change
		ERR("no DLG mode change\n");
		return 0;
	}
	pre_DLG_mode = bDLG_mode;

	// setup slot
	idx = (bDLG_mode) ? DLG_slot : Main_slot;
	slot = &demura->config_slot[idx];
	demura->curr_slot_idx = idx;
	next_bin_addr = slot->bin_pa + DEMURA_BUFFER_DESC_SIZE;
	LOG("[%s] slot idx  = %d", __func__, idx);
	LOG("  id      = %d", slot->id);
	LOG("  disable = %d", slot->disable);
	LOG("  bin len = %u", slot->bin_len);
	LOG("  bin va  = 0x%p", slot->bin_va);
	LOG("  bin pa  = 0x%llx", slot->bin_pa);
	LOG("  next_bin_addr  = 0x%llx", next_bin_addr);

	// notify pqu
	memset(&info, 0, sizeof(struct msg_render_demura_info));
	memset(&param, 0, sizeof(struct pqu_render_demura_param));
	info.binary_pa  = param.binary_pa  = next_bin_addr;
	info.binary_len = param.binary_len = slot->bin_len;
	info.disable    = param.disable    = slot->disable;
	info.hw_index   = param.hw_index   = slot->id;
	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_DEMURA, &info, &param);
	return 0;
}

int mtk_tv_drm_set_demura_enable(
	struct mtk_demura_context *demura, bool bEnable)
{
	struct mtk_demura_slot *slot = NULL;
	struct msg_render_demura_info info;
	struct pqu_render_demura_param param;
	static uint64_t next_bin_addr;

	if (demura == NULL) {
		ERR("pctx_demura is NULL!!!");
		return -EINVAL;
	}

	slot = &demura->config_slot[demura->curr_slot_idx];
	next_bin_addr = slot->bin_pa + DEMURA_BUFFER_DESC_SIZE;
	LOG("[%s] slot idx  = %d", __func__, demura->curr_slot_idx);
	LOG("  id      = %d", slot->id);
	LOG("  disable = %d", !bEnable);
	LOG("  bin len = %u", slot->bin_len);
	LOG("  bin va  = 0x%p", slot->bin_va);
	LOG("  bin pa  = 0x%llx", slot->bin_pa);
	LOG("  next_bin_addr  = 0x%llx", next_bin_addr);

	if (slot->bin_len > demura->bin_max_size) {
		ERR("demura binary too large, input(%u) >= max(%u)",
			slot->bin_len, demura->bin_max_size);
		demura->init = false;
		return demura->init;
	}

	// notify pqu
	memset(&info, 0, sizeof(struct msg_render_demura_info));
	memset(&param, 0, sizeof(struct pqu_render_demura_param));
	info.binary_pa  = param.binary_pa  = next_bin_addr;
	info.binary_len = param.binary_len = slot->bin_len;
	info.disable    = param.disable    = !bEnable;
	info.hw_index   = param.hw_index   = slot->id;
	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_DEMURA, &info, &param);
	return 0;
}
