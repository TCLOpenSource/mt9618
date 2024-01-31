# SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)

ccflags-y      +=  -I$(srctree)/mm
ccflags-y      +=  -I$(srctree)/drivers/dma-buf/heaps
ccflags-y      +=  -Werror

obj-$(CONFIG_MTK_MEMORY) = mtk-memory.o
mtk-memory-objs = mtk-reserved-memory.o
ifdef CONFIG_DMA_CMA
mtk-memory-objs += mtk-cma.o
endif
ifdef CONFIG_ION
mtk-memory-objs += mtk-null-memory.o
endif
ifdef CONFIG_DMABUF_HEAPS_SYSTEM
mtk-memory-objs += mtk-null-memory-dmaheap.o
endif
