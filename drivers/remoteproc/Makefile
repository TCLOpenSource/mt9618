# SPDX-License-Identifier: GPL-2.0
#
# Generic framework for controlling remote processors
#

ccflags-y += -I$(srctree)/drivers/soc/mediatek/mtk-memory

obj-$(CONFIG_REMOTEPROC)		+= remoteproc.o
remoteproc-y				:= remoteproc_core.o
remoteproc-y				+= remoteproc_debugfs.o
remoteproc-y				+= remoteproc_sysfs.o
remoteproc-y				+= remoteproc_virtio.o
remoteproc-y				+= remoteproc_elf_loader.o
obj-$(CONFIG_IMX_REMOTEPROC)		+= imx_rproc.o
obj-$(CONFIG_OMAP_REMOTEPROC)		+= omap_remoteproc.o
obj-$(CONFIG_MTK_CCD_REMOTEPROC)	+= mtk_ccd.o mtk_ccd_mem.o
obj-$(CONFIG_WKUP_M3_RPROC)		+= wkup_m3_rproc.o
obj-$(CONFIG_DA8XX_REMOTEPROC)		+= da8xx_remoteproc.o
obj-$(CONFIG_KEYSTONE_REMOTEPROC)	+= keystone_remoteproc.o
obj-$(CONFIG_QCOM_RPROC_COMMON)		+= qcom_common.o
obj-$(CONFIG_QCOM_Q6V5_COMMON)		+= qcom_q6v5.o
obj-$(CONFIG_QCOM_Q6V5_ADSP)		+= qcom_q6v5_adsp.o
obj-$(CONFIG_QCOM_Q6V5_MSS)		+= qcom_q6v5_mss.o
obj-$(CONFIG_QCOM_Q6V5_PAS)		+= qcom_q6v5_pas.o
obj-$(CONFIG_QCOM_Q6V5_WCSS)		+= qcom_q6v5_wcss.o
obj-$(CONFIG_QCOM_SYSMON)		+= qcom_sysmon.o
obj-$(CONFIG_QCOM_WCNSS_PIL)		+= qcom_wcnss_pil.o
qcom_wcnss_pil-y			+= qcom_wcnss.o
qcom_wcnss_pil-y			+= qcom_wcnss_iris.o
obj-$(CONFIG_ST_REMOTEPROC)		+= st_remoteproc.o
obj-$(CONFIG_ST_SLIM_REMOTEPROC)	+= st_slim_rproc.o
obj-$(CONFIG_STM32_RPROC)		+= stm32_rproc.o
obj-$(CONFIG_MTK_PMU_RPROC)		+= mtk_pmu_rproc.o
obj-$(CONFIG_MTK_PQU_RPROC)		+= mtk_pqu_rproc.o
mtk_pqu_rproc-y				+= mtk_pqu_remoteproc.o mtk_pqu_fdt.o
mtk_pqu_rproc-$(CONFIG_DMABUF_HEAPS)	+= mtk_pqu_cma_heaps.o
obj-$(CONFIG_MTK_AUDIO_MCU_RPROC)	+= mtk_audio_mcu_rproc.o
obj-$(CONFIG_MTK_FRC_MCU_RPROC)	        += mtk_frc_mcu_rproc.o
