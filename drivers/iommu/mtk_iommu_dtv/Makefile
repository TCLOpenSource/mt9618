# SPDX-License-Identifier: GPL-2.0

ccflags-y	+= -I$(srctree)/mm
ccflags-y      +=  -I$(srctree)/drivers/dma-buf/heaps
ccflags-y      +=  -I$(srctree)/drivers/iommu/mtk_iommu_dtv
ccflags-y += -Wall -Werror

mtk_iommu-objs := mtk_iommu_dtv.o \
		mtk_iommu_trace.o \
		mtk_iommu_helper.o \
		mtk_iommu_tee_interface.o \
		mtk_iommu_of.o \
		mtk_iommu_sysfs.o \
		mtk_iommu_defer_free.o

ifdef CONFIG_DMABUF_HEAPS_SYSTEM
mtk_iommu-objs += mtk_iommu_dmaheap.o \
		dma-heaps/mtk_iommu_heap.o \
		dma-heaps/mtk_iommu_carveout_dmaheap.o \
		dma-heaps/mtk_iommu_sys_dmaheap.o \
		dma-heaps/mtk_iommu_agile_dmaheap.o \
		dma-heaps/mtk_iommu_cma_dmaheap.o \
		dma-heaps/mtk_iommu_dmaheap_ops.o \
		dma-heaps/mtk_iommu_page_pool.o
endif

ifdef CONFIG_ION
mtk_iommu-objs += mtk_iommu_ion.o \
		ion-heaps/mtk_iommu_sys_heap.o \
		ion-heaps/mtk_iommu_cma_heap.o \
		ion-heaps/mtk_iommu_ion_heap.o
ifndef CONFIG_DMABUF_HEAPS_SYSTEM
mtk_iommu-objs += ion-heaps/mtk_iommu_carveout_heap.o
endif
endif

mtk_iommu-objs += mtk_iommu_seal.o
mtk_iommu-objs += mtk_iommu_test.o

obj-$(CONFIG_MTK_IOMMU_DTV) += mtk_iommu.o
