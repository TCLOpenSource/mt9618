// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/version.h>
#include <drm/mtk_tv_drm.h>

#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_pqu_metadata.h"
#include "mtk_tv_drm_global_pq.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_pqu_wrapper.h"
#include "mtk-reserved-memory.h"
#include "m_pqu_render.h"
#include "pqu_render.h"

//--------------------------------------------------------------------------------------------------
//  Local Defines
//--------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_METABUF
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

//--------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//--------------------------------------------------------------------------------------------------
enum mmap_index {		// MMAP name
	MMAP_INDEX_GOP,		// MI_GOP_RPMSG
	MMAP_INDEX_DEMURA,	// MI_DISPOUT_DEMURA
	MMAP_INDEX_PQU,		// MI_RENDER_METADATA
	MMAP_INDEX_AUTOGEN,	// MI_DISP_PQ_DF_MEMORY_LAYER
	MMAP_INDEX_MAX
};

struct pqubuf_layout {
	// PQU metadata: mtk_tv_drm_pqu_metadata.h
	struct drm_mtk_tv_pqu_metadata pqu_metadata[MTK_TV_DRM_PQU_METADATA_COUNT];

	// gamma data : M_pqu_render.h
	struct meta_render_pqu_pq_info gamma;

	// Global PQ: mtk_tv_drm_global_pq.h
	char global_pq[MTK_GLOBAL_PQ_STRING_LEN_MAX];

	// panel infor data : M_pqu_render.h
	struct meta_render_pqu_cfd_context panel_info;

	// share info : pqu_render.h
	struct render_pqu_frame_info frameinfo;

	// share info : pqu_render.h
	struct pqu_render_backlight panel_backlight;
};

struct metabuf {
	// common
	uint64_t cpu_emi_base;
	bool rv55_enable;

	// pqu-rv55 only
	struct {
		uint32_t mem_size;
		uint64_t bus_addr;
	} mmap[MMAP_INDEX_MAX];

	// pqu-lite only
	struct {
		void *virt_addr;
		uint64_t bus_addr;
	} shm;
};

//--------------------------------------------------------------------------------------------------
//  Local Variables
//--------------------------------------------------------------------------------------------------
static int _init;
static struct metabuf _metabuf;

//--------------------------------------------------------------------------------------------------
//  Local Functions
//--------------------------------------------------------------------------------------------------
static inline int _mmap_alloc(struct metabuf *mb,
	enum mmap_index midx, uint32_t offset, uint32_t size,
	void **virt, uint64_t *bus, uint64_t *phys)
{
	uint64_t ba, pa;
	void *va;
	bool is_pfn;
	int sz = size;

	if (mb->rv55_enable == true || size == 0) { // PQU-RV55 || using MMAP
		if (offset == 0 && size == 0) { // use all mmap
			sz = mb->mmap[midx].mem_size;
		} else if (offset + size > mb->mmap[midx].mem_size) {
			ERR("offset(%u) + size(%u) > limit(%u)",
				offset, size, mb->mmap[midx].mem_size);
			return -EINVAL;
		}
		ba  = mb->mmap[midx].bus_addr + offset;
		pa = ba - mb->cpu_emi_base;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
		is_pfn = pfn_is_map_memory(__phys_to_pfn(ba));
#else
		is_pfn = pfn_valid(__phys_to_pfn(ba));
#endif
		va =  (is_pfn) ? __va(ba) : ioremap_wc(ba, sz);
		if (IS_ERR_OR_NULL(va)) {
			ERR("convert bus to virt fail, pfn: %d, bus: 0x%llx, size: 0x%x",
				is_pfn, ba, sz);
			return -EPERM;
		}
	} else {
		va = mb->shm.virt_addr + offset;
		ba = mb->shm.bus_addr  + offset;
		pa = mb->shm.bus_addr  + offset - mb->cpu_emi_base;
	}
	*virt = va;
	*phys = pa;
	*bus  = ba;
	return sz; // return allocated memory size
}

static inline int _mmap_free(struct metabuf *mb, void *virt, uint64_t bus)
{
	// Only free the memory which allocate from @ioremap
	//   In PQU-Lite, we don't need to free memory because we use kernel memory pool.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	if (!pfn_is_map_memory(__phys_to_pfn(bus)))
#else
	if (!pfn_valid(__phys_to_pfn(bus)))
#endif
		iounmap(virt);
	return 0;
}

static inline int _ion_alloc(struct metabuf *mb, int ion_fd,
	void **virt, struct dma_buf **dma_buf)
{
	struct dma_buf *db = NULL;
	void *va;
	int sz;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	struct dma_buf_map mp;
	int ret = 0;
#endif
	db = dma_buf_get(ion_fd);
	if (IS_ERR_OR_NULL(db)) {
		ERR("dma_buf_get fail, ret = %ld", PTR_ERR(db));
		return PTR_ERR(db);
	}
	sz = db->size;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	ret = dma_buf_vmap(db, &mp);
	if (ret) {
		ERR("dma_buf_vmap fail, ret = 0x%x", ret);
		dma_buf_put(db);
		return ret;
	}
	va = mp.vaddr;
#else
	va = dma_buf_vmap(db);
	if (IS_ERR_OR_NULL(va)) {
		ERR("dma_buf_vmap fail, ret = %ld", PTR_ERR(va));
		dma_buf_put(db);
		return PTR_ERR(va);
	}
#endif
	*virt = va;
	*dma_buf = db;
	return sz; // return allocated memory size
}

static inline int _ion_free(struct metabuf *mb, void *virt, struct dma_buf *dma_buf)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	struct dma_buf_map mp;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	dma_buf_map_set_vaddr(&mp, virt);
	dma_buf_vunmap(dma_buf, &mp);
#else
	dma_buf_vunmap(dma_buf, virt);
#endif
	dma_buf_put(dma_buf);
	return 0;
}

static inline int _destroy(struct metabuf *mb)
{
	if (mb->shm.virt_addr)
		kvfree(mb->shm.virt_addr);
	memset(mb, 0, sizeof(struct metabuf));
	return 0;
}

static inline int _create(struct metabuf *mb, bool rv55_enable)
{
	struct device_node *np = NULL;
	struct of_mmap_info_data mmap_data;
	uint32_t len = 0;
	__be32 *p = NULL;
	uint32_t i;
	int ret = 0;

	// get cpu_emi_base
	np = of_find_node_by_name(NULL, "memory_info");
	if (np == NULL) {
		ERR("Find node memory_info fail");
		return -ENODEV;
	}
	p = (__be32 *)of_get_property(np, "cpu_emi0_base", &len);
	if (p == NULL) {
		of_node_put(np);
		ERR("Find prop cpu_emi0_base fail");
		return -ENODEV;
	}
	mb->cpu_emi_base = be32_to_cpup(p);
	LOG("cpu_emi_base = 0x%llx", mb->cpu_emi_base);
	of_node_put(np);

	// get mmap info
	np = of_find_compatible_node(NULL, NULL, MTK_DRM_TV_KMS_COMPATIBLE_STR);
	if (np == NULL) {
		ERR("find node %s failed", MTK_DRM_TV_KMS_COMPATIBLE_STR);
		return -ENODEV;
	}
	for (i = 0; i < MMAP_INDEX_MAX; ++i) {
		memset(&mmap_data, 0, sizeof(struct of_mmap_info_data));
		ret = of_mtk_get_reserved_memory_info_by_idx(np, i, &mmap_data);
		if (ret) {
			ERR("get mmap %d failed, ret = %d", i, ret);
			continue;
		}
		mb->mmap[i].mem_size = mmap_data.buffer_size;
		mb->mmap[i].bus_addr = mmap_data.start_bus_address;
		LOG("mmap %d size = 0x%llx", i, mmap_data.buffer_size);
		LOG("mmap %d bus  = 0x%llx", i, mmap_data.start_bus_address);
	}
	of_node_put(np);

	// in pqu-list, use kernel memory as shared memory
	mb->rv55_enable = rv55_enable;
	if (rv55_enable == false) {
		/*
		 * When we use MMAP on metabuf, any metabuf with the same @mmap_type can access the
		 *   same physical area and share data.
		 * However, when we use kernel memory on metabuf, we cannot have the above feature.
		 * In order to keep the behavior, we allocate a static kernel memory pool as MMAP.
		 */
		mb->shm.virt_addr = kvmalloc(sizeof(struct pqubuf_layout), GFP_KERNEL);
		if (mb->shm.virt_addr == NULL) {
			ERR("kmalloc(%zu) fail", sizeof(struct pqubuf_layout));
			return -ENOMEM;
		}
		mb->shm.bus_addr = (uint64_t)virt_to_phys(mb->shm.virt_addr);
	} else {
		mb->shm.virt_addr = NULL;
		mb->shm.bus_addr = 0;
	}

	return 0;
}

static int _trigger_pqu(struct metabuf *mb)
{
	struct msg_render_os_wrapper_init_info msg;
	struct pqu_render_os_wrapper_init_info pqu;
	int ret;

	memset(&msg, 0, sizeof(struct msg_render_os_wrapper_init_info));
	memset(&pqu, 0, sizeof(struct pqu_render_os_wrapper_init_info));
	msg.cpu_emi_base = pqu.cpu_emi_base = mb->cpu_emi_base;
	ret = MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_OS_WRAPPER_INIT, &msg, &pqu);
	if (ret)
		ERR("Trigger PQU fail, ret = %d", ret);
	return ret;
}

//--------------------------------------------------------------------------------------------------
//  Global Functions
//--------------------------------------------------------------------------------------------------
int mtk_tv_drm_metabuf_init(void)
{
	struct metabuf *mb = &_metabuf;
	int ret;

	if (_init == true)
		return 0;

	ret = _create(mb, bPquEnable);
	if (ret)
		return ret;

	ret = _trigger_pqu(mb);
	if (ret)
		return ret;

	_init = true;
	return 0;
}

int mtk_tv_drm_metabuf_deinit(void)
{
	return (_init == true) ? _destroy(&_metabuf) : 0;
}

int mtk_tv_drm_metabuf_suspend(void)
{
	// do nothing
	return 0;
}

int mtk_tv_drm_metabuf_resume(void)
{
	return (_init == true) ? _trigger_pqu(&_metabuf) : 0;
}

int mtk_tv_drm_metabuf_alloc_by_mmap(
	struct mtk_tv_drm_metabuf *drm_metabuf,
	enum mtk_tv_drm_metabuf_mmap_type mmap_type)
{
	struct metabuf *mb = &_metabuf;
	enum mmap_index index = MMAP_INDEX_MAX;
	uint64_t bus = 0, phys = 0;
	int offset = 0, size = 0;
	void *virt = NULL;
	int ret = 0;

	if (unlikely(_init == false)) {
		ret = _create(mb, bPquEnable);
		if (ret)
			return ret;
		_init = true;
	}

	if (mmap_type >= MTK_TV_DRM_METABUF_MMAP_TYPE_MAX || drm_metabuf == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}

	// get memory basic info
	switch (mmap_type) {
	case MTK_TV_DRM_METABUF_MMAP_TYPE_GOP:
		index  = MMAP_INDEX_GOP;
		offset = size = 0; // use all mmap
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_DEMURA:
		index  = MMAP_INDEX_DEMURA;
		offset = size = 0; // use all mmap
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_PQU:
		index  = MMAP_INDEX_PQU;
		offset = size = 0; // use all mmap
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_MATADATA:
		index  = MMAP_INDEX_PQU;
		offset = offsetof(struct pqubuf_layout, pqu_metadata);
		size   = sizeof(((struct pqubuf_layout *)0)->pqu_metadata);
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA:
		index  = MMAP_INDEX_PQU;
		offset = offsetof(struct pqubuf_layout, gamma);
		size   = sizeof(((struct pqubuf_layout *)0)->gamma);
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GLOBAL_PQ:
		index  = MMAP_INDEX_PQU;
		offset = offsetof(struct pqubuf_layout, global_pq);
		size   = sizeof(((struct pqubuf_layout *)0)->global_pq);
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_AUTOGEN:
		index  = MMAP_INDEX_AUTOGEN;
		offset = size = 0; // use all mmap
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_VDO_PNL:
		index  = MMAP_INDEX_PQU;
		offset = offsetof(struct pqubuf_layout, panel_info);
		size   = sizeof(((struct pqubuf_layout *)0)->panel_info);
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_FRAMEINFO:
		index  = MMAP_INDEX_PQU;
		offset = offsetof(struct pqubuf_layout, frameinfo);
		size   = sizeof(((struct pqubuf_layout *)0)->frameinfo);
		break;

	case MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_BACKLIGHT:
		index  = MMAP_INDEX_PQU;
		offset = offsetof(struct pqubuf_layout, panel_backlight);
		size   = sizeof(((struct pqubuf_layout *)0)->panel_backlight);
		break;

	default:
		ERR("Unknown metabuf type %d", mmap_type);
		return -EINVAL;
	}
	LOG("Input=mmap_type:%d, index:%d, offset:%x, size:0x%x", mmap_type, index, offset, size);

	// get memory address info
	ret = _mmap_alloc(mb, index, offset, size, &virt, &bus, &phys);
	if (ret < size) {
		ERR("_mmap_alloc fail, ret = %d, size = %d", ret, size);
		return -EINVAL;
	}
	size = ret;
	LOG("Output=size:0x%x, virt:0x%p, phys:0x%llx, bus:0x%llx", size, virt, phys, bus);

	memset(drm_metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	drm_metabuf->addr = virt;
	drm_metabuf->size = size;
	drm_metabuf->metabuf_type = MTK_TV_DRM_METABUF_TYPE_MMAP;
	drm_metabuf->mmap_info.mmap_type = mmap_type;
	drm_metabuf->mmap_info.bus_addr  = bus;
	drm_metabuf->mmap_info.phy_addr  = phys;
	return 0;
}

int mtk_tv_drm_metabuf_alloc_by_ion(
	struct mtk_tv_drm_metabuf *drm_metabuf,
	int ion_fd)
{
	struct metabuf *mb = &_metabuf;
	struct dma_buf *dma_buf = NULL;
	void *virt = NULL;
	int size = 0;

	if (drm_metabuf == NULL || ion_fd < 0) {
		ERR("Invalid input");
		return -EINVAL;
	}
	LOG("Input=ion_fd:%d", ion_fd);

	size = _ion_alloc(mb, ion_fd, &virt, &dma_buf);
	if (size <= 0) {
		ERR("_ion_alloc fail, ret = %d", size);
		return -EINVAL;
	}
	LOG("Output=size:0x%x, virt:0x%p", size, virt);

	drm_metabuf->addr = virt;
	drm_metabuf->size = size;
	drm_metabuf->metabuf_type = MTK_TV_DRM_METABUF_TYPE_ION;
	drm_metabuf->ion_info.ion_fd  = ion_fd;
	drm_metabuf->ion_info.dma_buf = dma_buf;
	return 0;
}

int mtk_tv_drm_metabuf_free(
	struct mtk_tv_drm_metabuf *drm_metabuf)
{
	struct metabuf *mb = &_metabuf;

	if (drm_metabuf == NULL || drm_metabuf->addr == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}
	switch (drm_metabuf->metabuf_type) {
	case MTK_TV_DRM_METABUF_TYPE_ION:
		_ion_free(mb, drm_metabuf->addr, drm_metabuf->ion_info.dma_buf);
		LOG("_ion_free=ion_fd:%d, virt:0x%p",
			drm_metabuf->ion_info.ion_fd, drm_metabuf->addr);
		break;

	case MTK_TV_DRM_METABUF_TYPE_MMAP:
		_mmap_free(mb, drm_metabuf->addr, drm_metabuf->mmap_info.bus_addr);
		LOG("_mmap_free=mmap_type:%d, virt:0x%p, bus: 0x%llx, phys: 0x%llx",
			drm_metabuf->mmap_info.mmap_type, drm_metabuf->addr,
			drm_metabuf->mmap_info.bus_addr, drm_metabuf->mmap_info.phy_addr);
		break;

	default:
		ERR("Unknown metabuf type %d", drm_metabuf->metabuf_type);
		return -EINVAL;
	}
	memset(drm_metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	return 0;
}
