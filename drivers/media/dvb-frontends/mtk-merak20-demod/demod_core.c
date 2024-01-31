// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek	Inc. (C) 2020. All rights	reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/cma.h>
#include "demod_core.h"
#include "demod_common.h"
#include "demod_drv_dvbt_t2.h"
#include "demod_drv_dvbc.h"
#include "demod_drv_isdbt.h"
#include "demod_drv_dtmb.h"
#include "demod_drv_atsc.h"
#include "demod_drv_vif.h"
#include "demod_drv_dvbs.h"
#include "cfm.h"
#include "../../../../mm/cma.h"
#include "../../../soc/mediatek/mtk-memory/mtk-cma.h"
#include "../../../soc/mediatek/mtk-memory/mtk-reserved-memory.h"
#include "mdrv_adapter.h"

#include "../austin_demod/austin-demod.h"
extern struct austin_demod_dev *p_austin_dev;

static const char *pinctrl_ts;
static const char *decouple_demod;
static u16 demod_unique_id;
static bool b_diseqc_envelope_mode_enable;
static bool lock_ever_flag;
static struct cfm_properties cfm_props_status = { 0 };
static struct cfm_properties cfm_props_configuration = { 0 };
#define MAX_PARAM_NUM    10
static struct cfm_property cfm_prop_status[MAX_PARAM_NUM];
static struct cfm_property cfm_prop_configuration[MAX_PARAM_NUM];
static struct page **pages_km;
/* 200ms, dvb_frontend_thread wake up time */
#define THREAD_WAKE_UP_TIME    5
#define MIU_BUS_ADDRESS_OFFSET 0x20000000
#define	transfer_unit	1000
#define PWS_SUCCESS "DMD BIG RESET HAS DONE\n"
#define PWS_FAIL    "DMD BIG RESET Failed\n"
#define PARALLEL_BIT    8
#define TS_PACKET_BIT   188
//#define PRINT_COUNT			100

#define DEMOD_FILE_PATH_LENGTH    256

#define DMDIP_UN       0
#define DMDIP_DVBT     1
#define DMDIP_DVBS     2
#define DMDIP_DVBC     3
#define DMDIP_ATSC     4
#define DMDIP_J83B     5
#define DMDIP_ISDBT    6
#define DMDIP_DTMB     7
#define DMDIP_VIF      8
#define DMDIP_ISDBS3   9
#define DMDIP_ATSC30   10

#define FE_SOUND_BG   (FE_SOUND_B | FE_SOUND_G)
#define FE_SOUND_DK   (FE_SOUND_D | FE_SOUND_K)
#define FE_SOUND_MN   (FE_SOUND_M | FE_SOUND_N)

struct DMD_DTMB_InitData sDMD_DTMB_InitData;
struct dmd_isdbt_initdata sdmd_isdbt_initdata;
struct DMD_ATSC_INITDATA sdmd_atsc_initdata;
struct vif_initial_in sdmd_vif_initdata;
static int bs_flag;
static int dvbs_init_flag;
static u8 delivery_sys_rec;
static DRIVER_ATTR_RO(dtmb_get_information);
static DRIVER_ATTR_RO(dvbt_t2_get_information);
static DRIVER_ATTR_RO(dvbc_get_information);
static DRIVER_ATTR_RO(dvbs_get_information);
//static bool demod_init_get_memory_flag;
static void (*demod_notify)(int, enum cfm_notify_event_type_t, struct cfm_properties*);
static void demod_status_monitor_n_notify(struct dvb_frontend *fe, enum fe_status *status);

static struct attribute *demod_attrs[] = {
	&driver_attr_dtmb_get_information.attr,
	&driver_attr_dvbt_t2_get_information.attr,
	&driver_attr_dvbc_get_information.attr,
	&driver_attr_dvbs_get_information.attr,
	NULL
};

static struct attribute_group demod_attrs_group = {
	.attrs = demod_attrs,
};

static const struct attribute_group *dmd_groups[] = {
	&demod_attrs_group,
	NULL
};

static ssize_t pws_dbg_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
		u32 temp1;

		temp1 = readl(sdmd_atsc_initdata.virt_clk_base_addr + 0x14e4);

		pr_info("[DMD PWS DBG Value] = 0x%X\n",  temp1);

		if ((temp1 & 0x00000040) == 0x00000040)
			return scnprintf(buf, PAGE_SIZE, "%s", PWS_FAIL);
		else
			return scnprintf(buf, PAGE_SIZE, "%s", PWS_SUCCESS);
}

static DEVICE_ATTR_RO(pws_dbg);

static struct attribute *mtk_dmd_pws_attrs[] = {
	&dev_attr_pws_dbg.attr,
	NULL,
};

static const struct attribute_group mtk_dmd_pws_attr_group = {
	.name = "mtk_dmd_pws_dbg",
	.attrs = mtk_dmd_pws_attrs,
};

static int mtk_dmd_pws_create_sysfs_attr(struct platform_device *pdev)
{
	struct device *dev = &(pdev->dev);
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!pdev) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] pdev is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	/* Create device attribute files */
	dev_info(dev, "Create device attribute group\n");

	ret = sysfs_create_group(&dev->kobj, &mtk_dmd_pws_attr_group);
	if (ret)
		goto err_out;
	return ret;

err_out:
	/* Remove device attribute files */
	dev_info(dev, "Error Handling: Remove device attribute group\n");
	sysfs_remove_group(&dev->kobj, &mtk_dmd_pws_attr_group);
	return ret;
}

static int mtk_dmd_pws_remove_sysfs_attr(struct platform_device *pdev)
{
	struct device *dev = &(pdev->dev);
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!pdev) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] pdev is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	/* Remove device attribute files */
	dev_info(dev, "Remove device attribute group\n");
	sysfs_remove_group(&dev->kobj, &mtk_dmd_pws_attr_group);

	return ret;
}

static struct device *mtk_demod_mmap;

static void mtk_demod_get_memory_buffer(struct mtk_demod_dev *demod_dev)
{
	dma_addr_t bus_addr = 0;
	//struct of_mmap_info_data of_mmap_info_cma = {0};
	struct of_mmap_info_data of_mmap_info_demod = {0};
	int npages = 0;
	u64 start_bus_pfn;
	pgprot_t pgprot;
	void *vaddr;
	int i = 0;
	struct page **pages = NULL;
	struct page **tmp = NULL;

	mtk_demod_mmap = &demod_dev->pdev->dev;

	/* get Demod buffer */
	of_mtk_get_reserved_memory_info_by_idx(mtk_demod_mmap->of_node, 1, &of_mmap_info_demod);

	mtkcma_presetting_v2(mtk_demod_mmap, 0);
	bus_addr = of_mmap_info_demod.start_bus_address -
		((mtk_demod_mmap->cma_area->base_pfn)<<PAGE_SHIFT);
	pr_info("########## bus_addr = 0x%llx\n", bus_addr);
	dma_alloc_attrs(mtk_demod_mmap, of_mmap_info_demod.buffer_size,
		&bus_addr, GFP_KERNEL, DMA_ATTR_NO_KERNEL_MAPPING);

	npages = PAGE_ALIGN(of_mmap_info_demod.buffer_size) / PAGE_SIZE;
	pages = vmalloc(sizeof(struct page *) * npages);
	tmp = pages;
	pages_km = pages;

	start_bus_pfn = of_mmap_info_demod.start_bus_address >> PAGE_SHIFT;

	if (of_mmap_info_demod.is_cache)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	pr_info("start_bus_pfn is 0x%llX\n", start_bus_pfn);
	pr_info("npages is %d\n", npages);

	for (i = 0; i < npages; ++i) {
		*(tmp++) = __pfn_to_page(start_bus_pfn);
		start_bus_pfn++;
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	demod_dev->virt_dram_base_addr = vaddr;

	pr_info("@@@@@@@@@ pa (bus address) = 0x%llx !!!\n", bus_addr);
	demod_dev->dram_base_addr = bus_addr - MIU_BUS_ADDRESS_OFFSET;
	pr_info("@@@@@@@@@ pa (miu address) = 0x%lx !!!\n", demod_dev->dram_base_addr);
	pr_info("@@@@@@@@@ va = 0x%p !!!\n", demod_dev->virt_dram_base_addr);
}
/*
 *static void mtk_demod_release_memory_buffer(struct mtk_demod_dev *demod_dev)
 *{
 *	dma_addr_t bus_addr = 0;
 *	struct of_mmap_info_data of_mmap_info_demod = {0};
 *
 *	mtk_demod_mmap = &demod_dev->pdev->dev;
 *
 *	// get Demod buffer
 *	of_mtk_get_reserved_memory_info_by_idx(mtk_demod_mmap->of_node, 1, &of_mmap_info_demod);
 *
 *	mtkcma_presetting_v2(mtk_demod_mmap, 0);
 *
 *	vunmap(demod_dev->virt_dram_base_addr);
 *
 *	bus_addr = demod_dev->dram_base_addr + MIU_BUS_ADDRESS_OFFSET;
 *	dma_free_attrs(mtk_demod_mmap, of_mmap_info_demod.buffer_size,
 *		demod_dev->virt_dram_base_addr, bus_addr, DMA_ATTR_NO_KERNEL_MAPPING);
 *
 *	vfree(pages_km);
 *}
 */

static int mtk_demod_read_status(struct dvb_frontend *fe,
				enum fe_status *status);

static int mtk_demod_set_frontend_vif(struct dvb_frontend *fe,
	u32 if_freq)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	enum vif_freq_band vif_rf_band;
	enum vif_sound_sys vif_sound_std;
	u8 scan_flag = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	if (c->frequency < 200*1000000)
		vif_rf_band = FREQ_VHF_L;
	else if (c->frequency < 470*1000000)
		vif_rf_band = FREQ_VHF_H;
	else
		vif_rf_band = FREQ_UHF;

	if (c->ana_sound_std == FE_SOUND_BG)
		c->ana_sound_std = FE_SOUND_B;

	if (c->ana_sound_std == FE_SOUND_DK)
		c->ana_sound_std = FE_SOUND_D;

	if (c->ana_sound_std == FE_SOUND_MN)
		c->ana_sound_std = FE_SOUND_M;

	switch (c->ana_sound_std) {
	case FE_SOUND_B:
	case FE_SOUND_G:
		//< 300 MHz is B, > 300 MHz is G
		if (c->frequency < 300*1000000)
			vif_sound_std = VIF_SOUND_B;
		else
			vif_sound_std = VIF_SOUND_GH;
		break;
	case FE_SOUND_D:
	case FE_SOUND_K:
		vif_sound_std = VIF_SOUND_DK2;
		break;
	case FE_SOUND_I:
		vif_sound_std = VIF_SOUND_I;
		break;
	case FE_SOUND_M:
	case FE_SOUND_N:
		vif_sound_std = VIF_SOUND_MN;
		break;
	case FE_SOUND_L:
		vif_sound_std = VIF_SOUND_L;
		break;
	case FE_SOUND_L_P:
		vif_sound_std = VIF_SOUND_LL;
		break;
	default:
		//TODO, should return error?
		vif_sound_std = VIF_SOUND_DK2;
		break;
	}

	drv_vif_set_freqband(vif_rf_band); //471250kHz
	drv_vif_set_sound_sys(vif_sound_std, if_freq); //dk
	drv_vif_set_scan(scan_flag); //is_scan = 0

	return 0;
}

static int mtk_demod_ip_mapping(enum fe_delivery_system del_sys)
{
	int curr_system = 0;

	if ((del_sys == SYS_DVBT) || (del_sys == SYS_DVBT2))
		curr_system = DMDIP_DVBT;
	else if ((del_sys == SYS_DVBS) || (del_sys == SYS_DVBS2))
		curr_system = DMDIP_DVBS;
	else if (del_sys == SYS_DVBC_ANNEX_A)
		curr_system = DMDIP_DVBC;
	else if (del_sys == SYS_ATSC)
		curr_system =  DMDIP_ATSC;
	else if (del_sys == SYS_DVBC_ANNEX_B)
		curr_system = DMDIP_J83B;
	else if (del_sys == SYS_ISDBT)
		curr_system = DMDIP_ISDBT;
	else if (del_sys == SYS_DTMB)
		curr_system = DMDIP_DTMB;
	else if (del_sys == SYS_ANALOG)
		curr_system = DMDIP_VIF;
	else if (del_sys == SYS_ISDBS3)
		curr_system = DMDIP_ISDBS3;
	else if (del_sys == SYS_ATSC30)
		curr_system = DMDIP_ATSC30;
	else
		curr_system = DMDIP_UN;

	return curr_system;
}

static int mtk_demod_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = NULL;
	struct mtk_demod_dev *demod_prvi = NULL;
	struct cfm_tuner_pri_data *cfm_tuner_priv = NULL;
	u32 u32IFFreq = 0;
	int curr_system = 0;
	int prev_system = 0;
	int ret = 0;
	u64 cnt;
	u8 i = 0;
	u64 tmp = 0;

	/*fake para*/
	//struct analog_parameters a_para = {0};

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	if (lock_ever_flag == 1) {
		cfm_prop_status[0].cmd = CFM_STREAMING_STATUS;
		cfm_prop_status[0].u.data = 0;
		cfm_props_status.num = 1;
		cfm_props_status.props = &cfm_prop_status[0];
		demod_notify(demod_unique_id,
			STREAMING_STATUS_CHANGED,
			&cfm_props_status);

		cfm_prop_configuration[0].cmd = CFM_STREAM_CLOCK_RATE;
		cfm_prop_configuration[0].u.data = 0;
		cfm_props_configuration.num = 1;
		cfm_props_configuration.props = &cfm_prop_configuration[0];
		demod_notify(demod_unique_id,
			STREAM_CONFIGURATION_CHANGED,
			&cfm_props_configuration);

		lock_ever_flag = 0;
		pr_info("[%s][%d] demod lost lock!\n", __func__, __LINE__);
	}

	c = &fe->dtv_property_cache;
	demod_prvi = fe->demodulator_priv;
	cfm_tuner_priv = fe->tuner_priv;

	u32IFFreq = (u32) cfm_tuner_priv->common_parameter.IF_frequency;
	demod_prvi->spectrum_inversion = cfm_tuner_priv->common_parameter.spectrum_inversion;
	demod_prvi->agc_polarity = cfm_tuner_priv->common_parameter.AGC_inversion;
	pr_info("######## IF Freq = %d KHz\n", u32IFFreq);
	pr_info("######## Spectrum Inversion = %d\n", demod_prvi->spectrum_inversion);
	pr_info("######## AGC Polarity = %d\n", demod_prvi->agc_polarity);

	pr_info("pre_system = %d, now_system = %d\n",
		demod_prvi->previous_system, c->delivery_system);

	prev_system = mtk_demod_ip_mapping(demod_prvi->previous_system);
	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	/* system change.  source change / first time probe / other situation */
	if (prev_system != curr_system) {
		/* exit previous system */
		switch (prev_system) {
		case DMDIP_DVBT:
			ret = dmd_drv_dvbt_t2_exit(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DVBS:
			ret = mdrv_dmd_dvbs_exit(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] MDrv_DMD_DVBS_Exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			dvbs_init_flag = 0;
			break;
		case DMDIP_DVBC:
			ret = dmd_drv_dvbc_exit(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbc_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_J83B:
		case DMDIP_ATSC:
			pr_info("ATSC_exit in remove is called\n");
			ret = _mdrv_dmd_atsc_md_setpowerstate(0, E_POWER_SUSPEND);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] _mdrv_dmd_atsc_md_exit fail !\n",
					__func__, __LINE__);
			return ret;
			}
			break;
		case DMDIP_ISDBT:
			pr_info("ISDBT_exit in remove is called\n");
			ret = !_mdrv_dmd_isdbt_md_exit(0);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] _MDrv_DMD_ISDBT_MD_Exit fail !\n",
				    __func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DTMB:
			pr_info("[%s][%d] DTMB_exit is called\n",
					__func__, __LINE__);
			ret = !_mdrv_dmd_dtmb_md_exit(0);
			break;
		case DMDIP_VIF:
			pr_info("vif_exit is called\n");
			ret = !drv_vif_exit();
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] drv_vif_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_UN:
		default:
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
			__func__, __LINE__);
			break;
		}
		demod_prvi->fw_downloaded = 0;

		/* update system */
		demod_prvi->previous_system = c->delivery_system;

		if (demod_prvi->virt_dram_base_addr == NULL ||
			demod_prvi->dram_base_addr + MIU_BUS_ADDRESS_OFFSET == 0) {
			pr_err("[%s][%d] Get DMD memory buffer NULL !!!!\n", __func__, __LINE__);
			//mtk_demod_release_memory_buffer(demod_prvi);
			//demod_init_get_memory_flag = 0;
			return -ETIMEDOUT;
		}

		/* init new system */
		switch (curr_system) {
		case DMDIP_DVBT:
			ret = dmd_drv_dvbt_t2_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DVBS:
			ret = mdrv_dmd_dvbs_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] MDrv_DMD_DVBS_Init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			dvbs_init_flag = 1;
			break;

		case DMDIP_DVBC:
			ret = dmd_drv_dvbc_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbc_init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_J83B:
		case DMDIP_ATSC:
		case DMDIP_ATSC30:
			// Init Parameter
			sdmd_atsc_initdata.biqswap =
			cfm_tuner_priv->common_parameter.spectrum_inversion;
			sdmd_atsc_initdata.btunergaininvert =
			cfm_tuner_priv->common_parameter.AGC_inversion;
			sdmd_atsc_initdata.if_khz =
			cfm_tuner_priv->common_parameter.IF_frequency;
			sdmd_atsc_initdata.vsbagclockchecktime   = 100;
			sdmd_atsc_initdata.vsbprelockchecktime   = 300;
			sdmd_atsc_initdata.vsbfsynclockchecktime = 600;
			sdmd_atsc_initdata.vsbfeclockchecktime   = 4000;
			sdmd_atsc_initdata.qamagclockchecktime   = 60;
			sdmd_atsc_initdata.qamprelockchecktime   = 1000;
			sdmd_atsc_initdata.qammainlockchecktime  = 2000;

			if (curr_system == DMDIP_ATSC ||
				curr_system == DMDIP_J83B) {
				sdmd_atsc_initdata.bisextdemod = FALSE;
				ret = !_mdrv_dmd_atsc_md_init(0, &sdmd_atsc_initdata,
				sizeof(sdmd_atsc_initdata));
			} else {
				sdmd_atsc_initdata.bIsUseSspiLoadCode = false;
				sdmd_atsc_initdata.bis_qpad = false;
				sdmd_atsc_initdata.bIsSspiUseTsPin = false;

				tmp = *((u64 *)&c->plp_array[0]);
				for (cnt = 0; cnt < 64; cnt++) {
					if (tmp & ((u64)1 << cnt)) {
						sdmd_atsc_initdata.u8PLP_id[i] = cnt;
						i++;
					}
				}
				sdmd_atsc_initdata.u8PLP_num = i;

				if (sdmd_atsc_initdata.u8PLP_num == 0) {
					sdmd_atsc_initdata.u8PLP_id[0] = 0xFF;
					sdmd_atsc_initdata.u8PLP_num = 1;
				}
				sdmd_atsc_initdata.u8PLP_layer[0] = 0;
				sdmd_atsc_initdata.u8PLP_layer[1] = 0;
				sdmd_atsc_initdata.u8PLP_layer[2] = 0;
				sdmd_atsc_initdata.u8PLP_layer[3] = 0;
				sdmd_atsc_initdata.u8Minor_Version = 0;
				sdmd_atsc_initdata.u8IsKRMode = 0;
				sdmd_atsc_initdata.bisextdemod = TRUE;

				pr_info("plp num = %d, plp_id - [0,1,2,3]= %d %d %d %d\n",
				sdmd_atsc_initdata.u8PLP_num,
				sdmd_atsc_initdata.u8PLP_id[0], sdmd_atsc_initdata.u8PLP_id[1],
				sdmd_atsc_initdata.u8PLP_id[2], sdmd_atsc_initdata.u8PLP_id[3]);
				ret = !_mdrv_dmd_atsc_md_init(1, &sdmd_atsc_initdata,
				sizeof(sdmd_atsc_initdata));
			}
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_atsc_init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_ISDBT:
			sdmd_isdbt_initdata.is_ext_demod = 0;
			sdmd_isdbt_initdata.if_khz =
			cfm_tuner_priv->common_parameter.IF_frequency;
			sdmd_isdbt_initdata.agc_reference_value = 0x400;
			sdmd_isdbt_initdata.tuner_gain_invert =
			cfm_tuner_priv->common_parameter.AGC_inversion;
			ret = !_mdrv_dmd_isdbt_md_init(0, &sdmd_isdbt_initdata,
				sizeof(sdmd_isdbt_initdata));
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] _MDrv_DMD_ISDBT_MD_Init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DTMB:
			sDMD_DTMB_InitData.bIQSwap =
			cfm_tuner_priv->common_parameter.spectrum_inversion;
			sDMD_DTMB_InitData.bisextdemod = 0;
			sDMD_DTMB_InitData.uif_khz16 =
			cfm_tuner_priv->common_parameter.IF_frequency;
			sDMD_DTMB_InitData.btunergaininvert =
			cfm_tuner_priv->common_parameter.AGC_inversion;
			ret = !_mdrv_dmd_dtmb_md_init(0, &sDMD_DTMB_InitData,
				sizeof(sDMD_DTMB_InitData));
			if (ret) {
				pr_info("[%s][%d] _MDrv_DMD_DTMB_MD_Init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_VIF:
			pr_info("vif_init is called\n");
			sdmd_vif_initdata.tuner_gain_inv =
			cfm_tuner_priv->common_parameter.AGC_inversion;
			sdmd_vif_initdata.spectrum_inv =
			cfm_tuner_priv->common_parameter.spectrum_inversion;
			sdmd_vif_initdata.if_khz =
			cfm_tuner_priv->common_parameter.IF_frequency;
			ret = !drv_vif_init(&sdmd_vif_initdata, sizeof(sdmd_vif_initdata));
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] drv_vif_init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_UN:
		default:
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
			__func__, __LINE__);
			break;
		}
		demod_prvi->fw_downloaded = 1;
	}

	/* Program demod */
	switch (curr_system) {
	case DMDIP_DVBT:
		ret = dmd_drv_dvbt_t2_config(fe, u32IFFreq);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_config fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_DVBS:
		ret = mdrv_dmd_dvbs_config(fe);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] MDrv_DMD_DVBS_Config fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_DVBC:
		ret = dmd_drv_dvbc_config(fe, u32IFFreq);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbc_config fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_J83B:
		ret = !_mdrv_dmd_atsc_md_setconfig(0,
					DMD_ATSC_DEMOD_ATSC_256QAM, TRUE);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_j83b_config fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_ATSC:
		ret = !_mdrv_dmd_atsc_md_setconfig(0,
				DMD_ATSC_DEMOD_ATSC_VSB, TRUE);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_atsc_config fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_ATSC30:
		ret = !_mdrv_dmd_atsc_md_setconfig(1,
		DMD_ATSC_DEMOD_ATSC_OFDM, TRUE);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_atsc_config fail !\n",
			__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_ISDBT:
		if (c->bandwidth_hz == 8000000)
			ret = !_mdrv_dmd_isdbt_md_advsetconfig(0, DMD_ISDBT_DEMOD_8M, TRUE);
		else
			ret = !_mdrv_dmd_isdbt_md_advsetconfig(0, DMD_ISDBT_DEMOD_6M, TRUE);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] _MDrv_DMD_ISDBT_MD_AdvSetConfig fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_DTMB:
		ret = !_mdrv_dmd_dtmb_md_setconfig(0,
				DMD_DTMB_DEMOD_DTMB, TRUE);
		if (ret) {
			pr_info("[%s][%d] _MDrv_DMD_DTMB_MD_SetConfig fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_VIF:
		pr_info("SYS_ANALOG set freqency to %d\n", (c->frequency));
		demod_prvi->start_time = mtk_demod_get_time();
		ret = mtk_demod_set_frontend_vif(fe, u32IFFreq);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] mtk_demod_set_frontend_vif fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_UN:
	default:
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
			__func__, __LINE__);
		break;
	}

	return ret;
}

static int mtk_demod_get_frontend(struct dvb_frontend *fe,
				 struct dtv_frontend_properties *p)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int ret = 0;
	int curr_system = 0;
	enum fe_status status = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	switch (curr_system) {
	case DMDIP_DVBT:
		ret = dmd_drv_dvbt_t2_get_frontend(fe, p);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_get_frontend fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_DVBS:
		if (dvbs_init_flag == 1) {
			mtk_demod_read_status(fe, &status);
			if ((status&FE_HAS_LOCK) != FE_HAS_LOCK) {
				p->strength.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
				p->strength.stat[1].uvalue = 0;
				p->strength.stat[1].scale = FE_SCALE_NOT_AVAILABLE;
				p->quality.stat[0].uvalue = 0;
				p->quality.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
				p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
				p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
				p->post_bit_error.stat[0].scale =
					FE_SCALE_NOT_AVAILABLE;
				p->post_bit_count.stat[0].scale =
					FE_SCALE_NOT_AVAILABLE;
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] demod lock fail !\n",
					__func__, __LINE__);
			} else {
				ret = mdrv_dmd_dvbs_get_frontend(fe, p);
				if (ret) {
					pr_err("[mdbgerr_merak_demod_dd][%s][%d]dvbs_get_frontend fail!\n",
						__func__, __LINE__);
					return ret;
				}
			}
		} else {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dvbs doesn't load code !\n",
				__func__, __LINE__);
		}
		break;
	case DMDIP_DVBC:
		ret = dmd_drv_dvbc_get_frontend(fe, p);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbc_get_frontend fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_J83B:
	case DMDIP_ATSC:
	{
		pr_info("Steve DBG:[%s],[%d]\n", __func__, __LINE__);

		mtk_demod_read_status(fe, &status);

		if (status == 0) {
			p->strength.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->post_bit_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->post_bit_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		} else if (status & FE_HAS_LOCK == 0x10) {
			ret = _mdrv_dmd_atsc_md_get_info(fe, p);

			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_atsc_get_frontend fail !\n",
				__func__, __LINE__);
			return ret;
			}
		}
	}
		break;
	case DMDIP_ATSC30:
	{
		pr_err("[%s][%d]\n", __func__, __LINE__);
		mtk_demod_read_status(fe, &status);
		pr_err("[%s],[%d] status = 0x%x\n", __func__, __LINE__, status);
		if (status == 0) {
			p->strength.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->post_bit_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->post_bit_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		} else {
			ret = _mdrv_dmd_atsc3_md_get_info(fe, p);

			if (ret) {
				pr_err("[%s][%d] dmd_drv_atsc3_get_frontend fail !\n",
				__func__, __LINE__);
			return ret;
			}
		}
	}
		break;
	case DMDIP_ISDBT:
		mtk_demod_read_status(fe, &status);

		if (status == 0) {
			p->strength.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->post_bit_error.stat[0].scale =
					FE_SCALE_NOT_AVAILABLE;
			p->post_bit_count.stat[0].scale =
					FE_SCALE_NOT_AVAILABLE;
		} else {
			ret = _mdrv_dmd_isdbt_md_get_information(0, fe, p);
		}
		break;
	case DMDIP_DTMB:
		mtk_demod_read_status(fe, &status);
		if (status == 0) {
			p->strength.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
			p->post_bit_error.stat[0].scale =
					FE_SCALE_NOT_AVAILABLE;
			p->post_bit_count.stat[0].scale =
					FE_SCALE_NOT_AVAILABLE;
		} else {
			ret = _mdrv_dmd_drv_dtmb_get_frontend(fe, p);

		if (ret)
			return ret;
		}
		break;
	case DMDIP_VIF:
		mtk_demod_read_status(fe, &status);

		if (status != 0)
			ret = drv_vif_get_information(fe, p);
		break;
	default:
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
			__func__, __LINE__);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mtk_demod_init(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = NULL;
	struct mtk_demod_dev *demod_prvi = NULL;
	int curr_system = 0;
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	if (lock_ever_flag == 1) {
		cfm_prop_status[0].cmd = CFM_STREAMING_STATUS;
		cfm_prop_status[0].u.data = 0;
		cfm_props_status.num = 1;
		cfm_props_status.props = &cfm_prop_status[0];
		demod_notify(demod_unique_id,
			STREAMING_STATUS_CHANGED,
			&cfm_props_status);

		cfm_prop_configuration[0].cmd = CFM_STREAM_CLOCK_RATE;
		cfm_prop_configuration[0].u.data = 0;
		cfm_props_configuration.num = 1;
		cfm_props_configuration.props = &cfm_prop_configuration[0];
		demod_notify(demod_unique_id,
			STREAM_CONFIGURATION_CHANGED,
			&cfm_props_configuration);

		lock_ever_flag = 0;
		pr_info("[%s][%d] demod lost lock!\n", __func__, __LINE__);
	}

	c = &fe->dtv_property_cache;
	demod_prvi = fe->demodulator_priv;

	mtk_demod_init_riuaddr(fe);
/*
 *	// get DRAM addr from dts.
 *	if (demod_init_get_memory_flag != 1) {
 *		mtk_demod_get_memory_buffer(demod_prvi);
 *		pr_info("@@@@@@@@@ dram_base_addr = 0x%lx !!!\n", demod_prvi->dram_base_addr);
 *		pr_info("@@@@@@@@@ virt_base_addr = 0x%p !!!\n", demod_prvi->virt_dram_base_addr);
 *
 *		if (demod_prvi->virt_dram_base_addr == NULL ||
 *			demod_prvi->dram_base_addr + MIU_BUS_ADDRESS_OFFSET == 0) {
 *			pr_err("[%s][%d] Get DMD memory buffer NULL !!!!\n", __func__, __LINE__);
 *			mtk_demod_release_memory_buffer(demod_prvi);
 *			demod_init_get_memory_flag = 0;
 *			return -ETIMEDOUT;
 *		}
 *
 *		sdmd_isdbt_initdata.tdi_start_addr = (demod_prvi->dram_base_addr) / 16;
 *		sdmd_isdbt_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *		sDMD_DTMB_InitData.utdistartaddr32 = (demod_prvi->dram_base_addr) / 16;
 *		sDMD_DTMB_InitData.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *		sdmd_atsc_initdata.utdistartaddr32 = (demod_prvi->dram_base_addr);
 *		sdmd_atsc_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *		sdmd_vif_initdata.dram_base_addr = (demod_prvi->dram_base_addr) / 16;
 *		sdmd_vif_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *		demod_init_get_memory_flag = 1;
 *	}
 */
	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	if (fe->exit == DVB_FE_DEVICE_RESUME) {
		switch (curr_system) {
		case DMDIP_DVBT:
			ret = dmd_drv_dvbt_t2_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			demod_prvi->fw_downloaded = 1;
			/* update system */
			demod_prvi->previous_system = c->delivery_system;
			break;
		case DMDIP_DVBS:
			ret = mdrv_dmd_dvbs_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] MDrv_DMD_DVBS_Init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			dvbs_init_flag = 1;
			demod_prvi->fw_downloaded = 1;
			/* update system */
			demod_prvi->previous_system = c->delivery_system;
			break;
		case DMDIP_DVBC:
			ret = dmd_drv_dvbc_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbc_init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			demod_prvi->fw_downloaded = 1;
			/* update system */
			demod_prvi->previous_system = c->delivery_system;
			break;
		case DMDIP_J83B:
		case DMDIP_ATSC:
			ret = _mdrv_dmd_atsc_md_setpowerstate(0, E_POWER_RESUME);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] ATSC_resume_fail !\n",
					__func__, __LINE__);
				return ret;
			}
			demod_prvi->fw_downloaded = 1;
			/* update system */
			demod_prvi->previous_system = c->delivery_system;
			break;
		case DMDIP_ATSC30:
			ret = _mdrv_dmd_atsc_md_setpowerstate(1, E_POWER_RESUME);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] ATSC3_resume_fail !\n",
				__func__, __LINE__);
				return ret;
			}
			demod_prvi->fw_downloaded = 1;
			/* update system */
			demod_prvi->previous_system = c->delivery_system;
			break;
		case DMDIP_ISDBT:
			ret = _mdrv_dmd_isdbt_md_setpowerstate(0, E_POWER_RESUME);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] ISDBT_resume_fail !\n",
					__func__, __LINE__);
				return ret;
			}
			demod_prvi->fw_downloaded = 1;
			/* update system */
			demod_prvi->previous_system = c->delivery_system;
			break;
		case DMDIP_DTMB:
			ret = _mdrv_dmd_dtmb_md_setpowerstate(0, E_POWER_RESUME);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] DTMB_resume_fail !\n",
					__func__, __LINE__);
				return ret;
			}
			demod_prvi->fw_downloaded = 1;
			demod_prvi->previous_system = c->delivery_system;
			break;
		case DMDIP_VIF:
			ret = drv_vif_set_power_state(E_POWER_RESUME);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] VIF_resume_fail !\n",
					__func__, __LINE__);
				return ret;
			}
			demod_prvi->fw_downloaded = 1;
			/* update system */
			demod_prvi->previous_system = c->delivery_system;
			break;
		case DMDIP_UN:
		default:
			pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
				__func__, __LINE__);
			demod_prvi->fw_downloaded = 0;
			break;
		}
	}

	if (fe->exit != DVB_FE_DEVICE_RESUME && curr_system == DMDIP_UN) {
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n", __func__, __LINE__);
		demod_prvi->fw_downloaded = 0;
	}
	return ret;
}

static int mtk_demod_sleep(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = NULL;
	struct mtk_demod_dev *demod_prvi = NULL;
	int curr_system = 0;
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	if (lock_ever_flag == 1) {
		cfm_prop_status[0].cmd = CFM_STREAMING_STATUS;
		cfm_prop_status[0].u.data = 0;
		cfm_props_status.num = 1;
		cfm_props_status.props = &cfm_prop_status[0];
		demod_notify(demod_unique_id,
			STREAMING_STATUS_CHANGED,
			&cfm_props_status);

		cfm_prop_configuration[0].cmd = CFM_STREAM_CLOCK_RATE;
		cfm_prop_configuration[0].u.data = 0;
		cfm_props_configuration.num = 1;
		cfm_props_configuration.props = &cfm_prop_configuration[0];
		demod_notify(demod_unique_id,
			STREAM_CONFIGURATION_CHANGED,
			&cfm_props_configuration);

		lock_ever_flag = 0;
		pr_info("[%s][%d] demod lost lock!\n", __func__, __LINE__);
	}

	c = &fe->dtv_property_cache;
	demod_prvi = fe->demodulator_priv;

	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	/* exit system */
	if (demod_prvi->fw_downloaded || dvbs_init_flag) {
		switch (curr_system) {
		case DMDIP_DVBT:
			ret = dmd_drv_dvbt_t2_exit(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DVBS:
			ret = mdrv_dmd_dvbs_exit(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d]MDrv_DMD_DVBS_Exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			dvbs_init_flag = 0;
			break;
		case DMDIP_DVBC:
			ret = dmd_drv_dvbc_exit(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d]dmd_drv_dvbc_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_J83B:
		case DMDIP_ATSC:
			ret = _mdrv_dmd_atsc_md_setpowerstate(0, E_POWER_SUSPEND);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] ATSC_suspend_fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_ATSC30:
			ret = _mdrv_dmd_atsc_md_setpowerstate(1, E_POWER_SUSPEND);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] ATSC_suspend_fail !\n",
				__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_ISDBT:
			ret = _mdrv_dmd_isdbt_md_setpowerstate(0, E_POWER_SUSPEND);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d]ISDBT_suspend_fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DTMB:
			ret = _mdrv_dmd_dtmb_md_setpowerstate(0, E_POWER_SUSPEND);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] DTMB_suspend_fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_VIF:
			ret = drv_vif_set_power_state(E_POWER_SUSPEND);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d]VIF_suspend_fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_UN:
		default:
			pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
				__func__, __LINE__);
			break;
		}
	}
	demod_prvi->fw_downloaded = 0;
	demod_prvi->previous_system = SYS_UNDEFINED;
	c->delivery_system = SYS_UNDEFINED;
/*
 *	// release DRAM addr
 *	if (demod_init_get_memory_flag == 1) {
 *		mtk_demod_release_memory_buffer(demod_prvi);
 *		demod_init_get_memory_flag = 0;
 *	}
 */
	bs_flag = 0;
	return ret;
}

static int mtk_demod_get_tune_settings(struct dvb_frontend *fe,
				struct dvb_frontend_tune_settings *settings)
{
	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}
/*
 *	when we use DVBFE_ALGO_SW, aka sw zigzag, we define some para here
 *	otherwise this is useless
 *	Example
 *	settings->min_delay_ms = HZ / 20;
 *	settings->max_drift = 1000000+1000; //1*MHz+1*kHz;
 *	settings->step_size = 1000000; //1*MHz;
 */
	settings->min_delay_ms = HZ / 20;
	settings->max_drift = 1000000+1000; //1*MHz+1*kHz;
	settings->step_size = 1000000; //1*MHz;

	return 0;
}

static int mtk_demod_stop_tune(struct dvb_frontend *fe, enum fe_status *status)
{
	int ret = 0;
	u8 *virt_cvbs_addr;
	u8 *virt_cvbs_load;
	u32 temp1;
	*status = 0;

	virt_cvbs_addr = ioremap(0x1ce03c88, 0x400); //0x1c000000+0x701e00*2+0x22*4
	virt_cvbs_load = ioremap(0x1ce03c04, 0x10); //0x1c000000+0x701e00*2+0x0*4
	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}
	pr_info("[%s][%d]ready to set stop tune, lock ever flag is :%d\n", __func__, __LINE__, lock_ever_flag);

	temp1 = readl(virt_cvbs_addr);
	temp1 = temp1 | 0x00000200;
	pr_info("[%s][%d]reg_clampgain_gain_overwrite bit 9 = 1\n", __func__, __LINE__);
	writel(temp1, virt_cvbs_addr);
	pr_info("[%s][%d]reg_clampgain_gain_value = 0\n", __func__, __LINE__);
	writel(0x00000000, virt_cvbs_addr+0x8); //0x1c000000+0x701e00*2+0x24*4
	writel(0x00000001, virt_cvbs_load);
	demod_status_monitor_n_notify(fe, status);

	return ret;
}

static int mtk_demod_read_status(struct dvb_frontend *fe,
					enum fe_status *status)
{
	struct dtv_frontend_properties *c = NULL;
	struct mtk_demod_dev *demod_prvi = NULL;
	int ret = 0;
	int lock_stat;
	int curr_system = 0;
	enum DMD_ATSC_LOCK_STATUS atsclockstatus = DMD_ATSC_NULL;
	enum dmd_isdbt_lock_status isdbt_lock_status = DMD_ISDBT_NULL;
	enum DMD_DTMB_LOCK_STATUS DtmbLockStatus  = DMD_DTMB_NULL;
	/* foe could be here or at get frontend*/
	u8 foe;
	u8 agc_value = 0;
	//static u8 print_count;
	int atv_time_out = 0;

	//pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	c = &fe->dtv_property_cache;
	demod_prvi = fe->demodulator_priv;

	//if (print_count == 0xff)
	//	print_count = 0;

	//print_count++;

	//if ((print_count % PRINT_COUNT) == 0)
	//	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	switch (curr_system) {
	case DMDIP_DVBT:
		ret = dmd_drv_dvbt_t2_read_status(fe, status);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_read_status fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_DVBS:
		if (dvbs_init_flag == 1) {
			ret = mdrv_dmd_dvbs_read_status(fe, status);
		} else {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dvbs doesn't load code !\n",
				__func__, __LINE__);
			ret = -EINVAL;
		}
		break;
	case DMDIP_DVBC:
		ret = dmd_drv_dvbc_read_status(fe, status);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbc_read_status fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_J83B:
	case DMDIP_ATSC:
	case DMDIP_ATSC30:
		if (curr_system == DMDIP_ATSC30)
			atsclockstatus = _mdrv_dmd_atsc_md_getlock(1, DMD_ATSC_GETLOCK);
		else
			atsclockstatus = _mdrv_dmd_atsc_md_getlock(0, DMD_ATSC_GETLOCK);
		*status = 0;
		if (atsclockstatus == DMD_ATSC_UNLOCK)
			*status = FE_TIMEDOUT;
		else if (atsclockstatus == DMD_ATSC_CHECKING)
			*status = FE_HAS_SIGNAL | FE_HAS_CARRIER;
		else if (atsclockstatus == DMD_ATSC_LOCK)
			*status = FE_HAS_SIGNAL | FE_HAS_CARRIER
				| FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK;
		break;
	case DMDIP_ISDBT:
		isdbt_lock_status = _mdrv_dmd_isdbt_md_getlock(0,
				DMD_ISDBT_GETLOCK);
		*status = 0;
		if (isdbt_lock_status == DMD_ISDBT_UNLOCK)
			*status = FE_TIMEDOUT;
		else if (isdbt_lock_status == DMD_ISDBT_CHECKING)
			*status = FE_HAS_SIGNAL | FE_HAS_CARRIER;
		else if (isdbt_lock_status == DMD_ISDBT_LOCK)
			*status = FE_HAS_SIGNAL | FE_HAS_CARRIER
				| FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK;
		break;
	case DMDIP_DTMB:
		DtmbLockStatus = _mdrv_dmd_dtmb_md_getlock(0,
				DMD_DTMB_GETLOCK);
		*status = 0;
		if (DtmbLockStatus == DMD_DTMB_UNLOCK)
			*status = FE_TIMEDOUT;//FE_NONE;
		else if (DtmbLockStatus == DMD_DTMB_CHECKING)
			*status = FE_HAS_SIGNAL | FE_HAS_CARRIER;
		else if (DtmbLockStatus == DMD_DTMB_LOCK)
			*status = FE_HAS_SIGNAL | FE_HAS_CARRIER
				| FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK;
		break;
	case DMDIP_VIF:
		drv_vif_read_statics(&agc_value);
		//if (polarity) agc_value = 0xFF - agc_value;
		c->agc_val = agc_value;

		atv_time_out = 80;
		if ((c->ana_sound_std == FE_SOUND_L_P) || (c->ana_sound_std == FE_SOUND_L))
			atv_time_out = 200;//120;

		lock_stat = drv_vif_read_cr_lock_status();
		if (lock_stat & 0x01)
			*status = FE_HAS_SIGNAL | FE_HAS_CARRIER
				| FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK;
		//else if (mtk_demod_dvb_time_diff(demod_prvi->start_time) > 30)
		//else if (mtk_demod_dvb_time_diff(demod_prvi->start_time) > 80)
		else if (mtk_demod_dvb_time_diff(demod_prvi->start_time) > atv_time_out)
			*status = FE_TIMEDOUT;
		else
			*status = FE_NONE;

		foe = drv_vif_read_cr_foe();
		if (*status & FE_HAS_LOCK) {
			c->frequency_offset = drv_vif_foe2hz(foe, 62500/4);

			if (c->ana_sound_std == FE_SOUND_L_P)
				c->frequency_offset *= -1;

			// force unlock for too big foe
			if ((c->frequency_offset > 1000000) || (c->frequency_offset<-1000000))
				*status = FE_TIMEDOUT;

			// force unlock when AFT is on for foe
			if (c->pilot == PILOT_OFF && c->rolloff == ROLLOFF_20) {
				if ((c->frequency_offset > 700000) || (c->frequency_offset<-700000))
					*status = FE_TIMEDOUT;
			}

		}
		break;
	default:
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
			__func__, __LINE__);
		ret = -EINVAL;
		break;
	}

	demod_status_monitor_n_notify(fe, status);

	return ret;
}

// sample code to get demod lock/unlock
static void demod_status_monitor_n_notify(struct dvb_frontend *fe, enum fe_status *status)
{
	int ret;
	u32 ts_rate;

	if ((((*status) & FE_HAS_LOCK) != 0) && (lock_ever_flag == 0)) {
		cfm_prop_status[0].cmd = CFM_STREAMING_STATUS;
		cfm_prop_status[0].u.data = 1;
		cfm_props_status.num = 1;
		cfm_props_status.props = &cfm_prop_status[0];
		demod_notify(demod_unique_id, STREAMING_STATUS_CHANGED, &cfm_props_status);

		ret = mtk_demod_ts_get_clk_rate(fe, &ts_rate);
		cfm_prop_configuration[0].cmd = CFM_STREAM_CLOCK_RATE;
		cfm_prop_configuration[0].u.data = ts_rate;
		cfm_props_configuration.num = 1;
		cfm_props_configuration.props = &cfm_prop_configuration[0];
		demod_notify(demod_unique_id,
			STREAM_CONFIGURATION_CHANGED,
			&cfm_props_configuration);

		lock_ever_flag = 1;
		pr_info("[%s][%d] demod lock!, ts clock rate = %d\n", __func__, __LINE__, ts_rate);
	}

	if ((((*status) & FE_HAS_LOCK) == 0) && (lock_ever_flag == 1)) {
		cfm_prop_status[0].cmd = CFM_STREAMING_STATUS;
		cfm_prop_status[0].u.data = 0;
		cfm_props_status.num = 1;
		cfm_props_status.props = &cfm_prop_status[0];
		demod_notify(demod_unique_id,
			STREAMING_STATUS_CHANGED,
			&cfm_props_status);

		cfm_prop_configuration[0].cmd = CFM_STREAM_CLOCK_RATE;
		cfm_prop_configuration[0].u.data = 0;
		cfm_props_configuration.num = 1;
		cfm_props_configuration.props = &cfm_prop_configuration[0];
		demod_notify(demod_unique_id,
			STREAM_CONFIGURATION_CHANGED,
			&cfm_props_configuration);

		lock_ever_flag = 0;
		pr_info("[%s][%d] demod lost lock!\n", __func__, __LINE__);
	}
}

static enum dvbfe_algo mtk_demod_get_frontend_algo(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = NULL;
	int curr_system = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	c = &fe->dtv_property_cache;

	curr_system = mtk_demod_ip_mapping(c->delivery_system);
/*
 * pr_info("[%s][%d], delivery system is %d\n", __func__, __LINE__, (c->delivery_system));
 * pr_info("[%s][%d], pilot is %d\n", __func__, __LINE__, (c->pilot));
 * pr_info("[%s][%d], rolloff is %d\n", __func__, __LINE__, (c->rolloff));
 */
	if (curr_system == DMDIP_VIF) {
		if (c->pilot == PILOT_OFF && c->rolloff == ROLLOFF_20)
			return DVBFE_ALGO_SW;
	}

	return DVBFE_ALGO_HW;
}

static int mtk_demod_tune(struct dvb_frontend *fe,
			bool re_tune,
			unsigned int mode_flags,
			unsigned int *delay,
			enum fe_status *status)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int curr_system = 0;
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	if (re_tune) {
		switch (curr_system) {
		case DMDIP_DVBT:
		case DMDIP_DVBC:
		case DMDIP_J83B:
		case DMDIP_ATSC:
		case DMDIP_ISDBT:
		case DMDIP_DTMB:
		case DMDIP_ATSC30:
			ret = mtk_demod_set_frontend(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d]set frontend failed, c->delivery_system = %d !\n",
					__func__, __LINE__, c->delivery_system);
				return ret;
			}
			break;
		case DMDIP_DVBS:
			if (bs_flag)
				ret = mdrv_dmd_dvbs_blindscan_config(fe);
			else
				ret = mtk_demod_set_frontend(fe);

			break;
		case DMDIP_VIF:
			ret = mtk_demod_set_frontend(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d]set frontend failed, c->delivery_system = %d !\n",
					__func__, __LINE__, c->delivery_system);
				return ret;
			}
			break;
		default:
			ret = -EINVAL;
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
			__func__, __LINE__);
			break;
		}
	}

	*delay = HZ / 5;
	delivery_sys_rec = curr_system;
	return mtk_demod_read_status(fe, status);
}

static int mtk_demod_set_tone(struct dvb_frontend *fe,
				enum fe_sec_tone_mode tone)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int ret = 0;
	struct mtk_demod_dev *demod_prvi = NULL;
	struct cfm_tuner_pri_data *cfm_tuner_priv = NULL;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	demod_prvi = fe->demodulator_priv;
	cfm_tuner_priv = fe->tuner_priv;
	demod_prvi->agc_polarity = cfm_tuner_priv->common_parameter.AGC_inversion;
	pr_info("[mdbgin_merak_demod_dd][%s][%d] AGC Polarity = %d\n",
		__func__, __LINE__, demod_prvi->agc_polarity);

	if (demod_prvi->fw_downloaded == 0) {
		ret = mdrv_dmd_dvbs_init(fe);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] MDrv_DMD_DVBS_Init fail !\n",
				__func__, __LINE__);
			return ret;
		}
		dvbs_init_flag = 1;
		demod_prvi->fw_downloaded = 1;
		/* update system */
		demod_prvi->previous_system = c->delivery_system;
	}

	if (b_diseqc_envelope_mode_enable)
		ret |= mdrv_dmd_dvbs_diseqc_set_envelope_mode(b_diseqc_envelope_mode_enable);

	c->sectone = tone;

	ret |= mdrv_dmd_dvbs_diseqc_set_22k(tone);
	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] mdrv_dmd_dvbs_diseqc_set_22k is error!\n",
			__func__, __LINE__);
	}
	return ret;
}

static int mtk_demod_diseqc_send_burst(struct dvb_frontend *fe,
				enum fe_sec_mini_cmd minicmd)
{
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	if (b_diseqc_envelope_mode_enable)
		ret |= mdrv_dmd_dvbs_diseqc_set_envelope_mode(b_diseqc_envelope_mode_enable);

	ret |= mdrv_dmd_dvbs_diseqc_set_tone(minicmd);
	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] mdrv_dmd_dvbs_diseqc_set_tone is error!\n",
			__func__, __LINE__);
	}
	return ret;
}

static int mtk_demod_diseqc_send_master_cmd(struct dvb_frontend *fe,
				struct dvb_diseqc_master_cmd *cmd)
{
	int ret = 0;
	bool Diseqc_flag = 0;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct mtk_demod_dev *demod_prvi = fe->demodulator_priv;
	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);
	if (demod_prvi->ip_version == 0x300) {
		if (demod_prvi->fw_downloaded == 0) {
/*
 *		// get DRAM addr from dts.
 *			if (demod_init_get_memory_flag != 1) {
 *				mtk_demod_get_memory_buffer(demod_prvi);
 *			pr_info("@@ dram_base_addr = 0x%lx !!!\n", demod_prvi->dram_base_addr);
 *			pr_info("@@ virt_base_addr = 0x%p !!!\n", demod_prvi->virt_dram_base_addr);
 *
 *			sdmd_isdbt_initdata.tdi_start_addr = (demod_prvi->dram_base_addr) / 16;
 *			sdmd_isdbt_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *			sDMD_DTMB_InitData.utdistartaddr32 = (demod_prvi->dram_base_addr) / 16;
 *			sDMD_DTMB_InitData.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *			sdmd_atsc_initdata.utdistartaddr32 = (demod_prvi->dram_base_addr);
 *			sdmd_atsc_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *			sdmd_vif_initdata.dram_base_addr = (demod_prvi->dram_base_addr) / 16;
 *			sdmd_vif_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *			demod_init_get_memory_flag = 1;
 *			}
 */
		Diseqc_flag = 1;
		/* init new system */
		switch (c->delivery_system) {
		case SYS_DVBT:
		case SYS_DVBT2:
			ret = dmd_drv_dvbt_t2_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case SYS_DVBS:
		case SYS_DVBS2:
			dvbs_init_flag = 1;
			ret = mdrv_dmd_dvbs_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] MDrv_DMD_DVBS_Init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;

		case SYS_DVBC_ANNEX_A:
			ret = dmd_drv_dvbc_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbc_init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case SYS_UNDEFINED:
		default:
			dvbs_init_flag = 1;
			ret = mdrv_dmd_dvbs_init(fe);
			if (ret) {
				pr_err("[mdbgerr_merak_demod_dd][%s][%d] SYS_UNDEFINED  MDrv_DMD_DVBS_Init fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		}
	}
	}

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	if (b_diseqc_envelope_mode_enable)
		ret |= mdrv_dmd_dvbs_diseqc_set_envelope_mode(b_diseqc_envelope_mode_enable);

	ret |= mdrv_dmd_dvbs_diseqc_send_cmd(cmd);
	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] mdrv_dmd_dvbs_diseqc_send_cmd is error!\n",
			__func__, __LINE__);
	}
	if (demod_prvi->ip_version == 0x300) {
		if (Diseqc_flag) {
			/* exit previous system */
			Diseqc_flag = 0;
			switch (demod_prvi->previous_system) {
			case SYS_DVBT:
			case SYS_DVBT2:
				ret = dmd_drv_dvbt_t2_exit(fe);
				if (ret) {
					pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_exit fail !\n",
						__func__, __LINE__);
					return ret;
				}
				break;
			case SYS_DVBS:
			case SYS_DVBS2:
				dvbs_init_flag = 0;
				ret = mdrv_dmd_dvbs_exit(fe);
				if (ret) {
					pr_err("[mdbgerr_merak_demod_dd][%s][%d] MDrv_DMD_DVBS_Exit fail !\n",
						__func__, __LINE__);
					return ret;
				}
				break;
			case SYS_DVBC_ANNEX_A:
				ret = dmd_drv_dvbc_exit(fe);
				if (ret) {
					pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbc_exit fail !\n",
						__func__, __LINE__);
					return ret;
				}
				break;
			case SYS_UNDEFINED:
			default:
				dvbs_init_flag = 0;
				ret = mdrv_dmd_dvbs_exit(fe);
				if (ret) {
					pr_err("[mdbgerr_merak_demod_dd ][%s][%d] SYS_UNDEFINED MDrv_DMD_DVBS_Exit fail !\n",
						__func__, __LINE__);
					return ret;
				}
				break;
			}
/*
 *			// release DRAM addr
 *			if (demod_init_get_memory_flag == 1) {
 *				mtk_demod_release_memory_buffer(demod_prvi);
 *				demod_init_get_memory_flag = 0;
 *			}
 */
		}
	}
	return ret;
}

static int mtk_demod_dump_debug_info(char *buffer, int buffer_size, int *eof,
				int offset, struct dvb_frontend *fe)
{
	int ret = 0;
	int char_size = 0;
	int curr_system = 0;
	struct dtv_frontend_properties *c = NULL;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	c = &fe->dtv_property_cache;

	*eof = 1; // fix to 1 first due to not exceed 512 size.

	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	switch (curr_system) {
	case DMDIP_DVBT:
		char_size = dvbt_t2_get_information(buffer);
		break;
	case DMDIP_DVBS:
		char_size = dvbs_get_information(buffer);
		break;
	case DMDIP_DVBC:
		char_size = dvbc_get_information(buffer);
		break;
	case DMDIP_J83B:
	case DMDIP_ATSC:
		char_size = atsc_get_information(buffer);
		break;
	case DMDIP_ISDBT:
		char_size = isdbt_get_information(buffer);
		break;
	case DMDIP_DTMB:
		char_size = dtmb_get_information(buffer);
		break;
	case DMDIP_VIF:
		char_size = drv_vif_fetch_information(buffer);
		break;
	default:
		ret = -EINVAL;
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
		__func__, __LINE__);
		break;
	}
	//Debug
	pr_info("[%s][%d] mt5896 demod dump!, size = %d, system = %d\n",
		__func__, __LINE__, char_size, c->delivery_system);
	pr_info("%s\n", buffer);

	return char_size;
}

static int mtk_demod_blindscan_start(struct dvb_frontend *fe,
				struct DMD_DVBS_BlindScan_Start_param *Start_param)
{
	struct dtv_frontend_properties *c = NULL;
	struct mtk_demod_dev *demod_prvi = NULL;
	struct cfm_tuner_pri_data *cfm_tuner_priv = NULL;

	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	c = &fe->dtv_property_cache;
	demod_prvi = fe->demodulator_priv;
	cfm_tuner_priv = fe->tuner_priv;

	demod_prvi->agc_polarity = cfm_tuner_priv->common_parameter.AGC_inversion;
	pr_info("delivery_system%d\n", c->delivery_system);
	pr_info("######## AGC Polarity = %d\n", demod_prvi->agc_polarity);

	bs_flag = 1;
	if (dvbs_init_flag == 0) {
		ret = mdrv_dmd_dvbs_init(fe);
		if (ret)
			pr_err("[%s][%d] mdrv_dmd_dvbs_init fail\n", __func__, __LINE__);
		dvbs_init_flag = 1;
		demod_prvi->fw_downloaded = 1;
	}
	demod_prvi->previous_system = c->delivery_system;
	ret = mdrv_dmd_dvbs_blindScan_start(Start_param);
	if (ret)
		pr_err("[%s][%d] mdrv_dmd_dvbs_blindScan_start fail\n", __func__, __LINE__);
	return ret;
}

static int mtk_demod_blindscan_get_tuner_freq(struct dvb_frontend *fe,
				struct DMD_DVBS_BlindScan_GetTunerFreq_param *GetTunerFreq_param)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	ret = mdrv_dmd_dvbs_blindScan_get_tuner_freq(GetTunerFreq_param);
	c->frequency = (GetTunerFreq_param->TunerCenterFreq)*transfer_unit;
	c->symbol_rate = GetTunerFreq_param->TunerCutOffFreq;
	return ret;
}

static int mtk_demod_blindscan_next_freq(struct dvb_frontend *fe,
				struct DMD_DVBS_BlindScan_NextFreq_param *NextFreq_param)
{
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	ret = mdrv_dmd_dvbs_blindScan_next_freq(NextFreq_param);
	return ret;
}

static int mtk_demod_blindscan_wait_curfreq_finished(struct dvb_frontend *fe,
		struct DMD_DVBS_BlindScan_WaitCurFreqFinished_param *WaitCurFreqFinished_param)
{
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	ret = mdrv_dmd_dvbs_blindScan_wait_finished(WaitCurFreqFinished_param);
	return ret;
}

static int mtk_demod_blindscan_get_channel(struct dvb_frontend *fe,
				struct DMD_DVBS_BlindScan_GetChannel_param *GetChannel_param)
{
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	ret = mdrv_dmd_dvbs_blindScan_get_channel(GetChannel_param);
	return ret;
}

static int mtk_demod_blindscan_end(struct dvb_frontend *fe)
{
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	bs_flag = 0;
	ret = mdrv_dmd_dvbs_blindScan_end();
	return ret;
}

static int mtk_demod_blindscan_cancel(struct dvb_frontend *fe)
{
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	bs_flag = 0;
	ret = mdrv_dmd_dvbs_blindScan_cancel();
	return ret;
}

static int mtk_demod_suspend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = NULL;
	struct mtk_demod_dev *demod_prvi = NULL;
	int curr_system = 0;
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	c = &fe->dtv_property_cache;
	demod_prvi = fe->demodulator_priv;

	if (!demod_prvi) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] demod_prvi is NULL !\n",
		__func__, __LINE__);
		return ret;
	}

	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	/* exit system */
	switch (curr_system) {
	case DMDIP_DVBT:
		ret = dmd_drv_dvbt_t2_exit(fe);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_dvbt_t2_exit fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_DVBS:
		ret = mdrv_dmd_dvbs_exit(fe);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d]MDrv_DMD_DVBS_Exit fail !\n",
				__func__, __LINE__);
			return ret;
		}
		dvbs_init_flag = 0;
		break;
	case DMDIP_DVBC:
		ret = dmd_drv_dvbc_exit(fe);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d]dmd_drv_dvbc_exit fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_J83B:
	case DMDIP_ATSC:
		ret = _mdrv_dmd_atsc_md_setpowerstate(0, E_POWER_SUSPEND);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] ATSC_suspend_fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_ATSC30:
		ret = _mdrv_dmd_atsc_md_setpowerstate(1, E_POWER_SUSPEND);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] ATSC3_suspend_fail !\n",
			__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_ISDBT:
		ret = _mdrv_dmd_isdbt_md_setpowerstate(0, E_POWER_SUSPEND);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d]ISDBT_suspend_fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_DTMB:
		ret = _mdrv_dmd_dtmb_md_setpowerstate(0, E_POWER_SUSPEND);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] DTMB_suspend_fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_VIF:
		ret = drv_vif_set_power_state(E_POWER_SUSPEND);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d]VIF_suspend_fail !\n",
				__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_UN:
	default:
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
			__func__, __LINE__);
		break;
	}

	demod_prvi->fw_downloaded = 0;
/*
 *	// release DRAM addr
 *	if (demod_init_get_memory_flag == 1) {
 *		mtk_demod_release_memory_buffer(demod_prvi);
 *		demod_init_get_memory_flag = 0;
 *	}
 */
	return ret;
}

static int mtk_demod_resume(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = NULL;
	struct mtk_demod_dev *demod_prvi = NULL;
	struct cfm_tuner_pri_data *cfm_tuner_priv = NULL;
	u32 u32IFFreq = 0;
	int curr_system = 0;
	int ret = 0;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	if (lock_ever_flag == 1) {
		cfm_prop_status[0].cmd = CFM_STREAMING_STATUS;
		cfm_prop_status[0].u.data = 0;
		cfm_props_status.num = 1;
		cfm_props_status.props = &cfm_prop_status[0];
		demod_notify(demod_unique_id,
			STREAMING_STATUS_CHANGED,
			&cfm_props_status);

		cfm_prop_configuration[0].cmd = CFM_STREAM_CLOCK_RATE;
		cfm_prop_configuration[0].u.data = 0;
		cfm_props_configuration.num = 1;
		cfm_props_configuration.props = &cfm_prop_configuration[0];
		demod_notify(demod_unique_id,
			STREAM_CONFIGURATION_CHANGED,
			&cfm_props_configuration);

		lock_ever_flag = 0;
		pr_info("[%s][%d] demod lost lock!\n", __func__, __LINE__);
	}
	c = &fe->dtv_property_cache;
	demod_prvi = fe->demodulator_priv;
	cfm_tuner_priv = fe->tuner_priv;

	/*fake para*/
	//struct analog_parameters a_para = {0};

	if (!demod_prvi) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] demod_prvi is NULL !\n",
		__func__, __LINE__);
		return ret;
	}
	u32IFFreq = (u32) cfm_tuner_priv->common_parameter.IF_frequency;
	demod_prvi->spectrum_inversion = cfm_tuner_priv->common_parameter.spectrum_inversion;
	demod_prvi->agc_polarity = cfm_tuner_priv->common_parameter.AGC_inversion;
	pr_info("######## IF Freq = %d KHz\n", u32IFFreq);
	pr_info("######## Spectrum Inversion = %d\n", demod_prvi->spectrum_inversion);
	pr_info("######## AGC Polarity = %d\n", demod_prvi->agc_polarity);
/*
 *	// get DRAM addr from dts.
 *	if (demod_init_get_memory_flag != 1) {
 *		mtk_demod_get_memory_buffer(demod_prvi);
 *		pr_info("@@@@@@@@@ dram_base_addr = 0x%lx !!!\n", demod_prvi->dram_base_addr);
 *		pr_info("@@@@@@@@@ virt_base_addr = 0x%p !!!\n", demod_prvi->virt_dram_base_addr);
 *
 *		if (demod_prvi->virt_dram_base_addr == NULL ||
 *			demod_prvi->dram_base_addr + MIU_BUS_ADDRESS_OFFSET == 0) {
 *			pr_err("[%s][%d] Get DMD memory buffer NULL !!!!\n", __func__, __LINE__);
 *			mtk_demod_release_memory_buffer(demod_prvi);
 *			demod_init_get_memory_flag = 0;
 *			return -ETIMEDOUT;
 *		}
 *
 *		sdmd_isdbt_initdata.tdi_start_addr = (demod_prvi->dram_base_addr) / 16;
 *		sdmd_isdbt_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *		sDMD_DTMB_InitData.utdistartaddr32 = (demod_prvi->dram_base_addr) / 16;
 *		sDMD_DTMB_InitData.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *		sdmd_atsc_initdata.utdistartaddr32 = (demod_prvi->dram_base_addr);
 *		sdmd_atsc_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *
 *		sdmd_vif_initdata.dram_base_addr = (demod_prvi->dram_base_addr) / 16;
 *		sdmd_vif_initdata.virt_dram_base_addr = demod_prvi->virt_dram_base_addr;
 *		demod_init_get_memory_flag = 1;
 *	}
 */
	curr_system = mtk_demod_ip_mapping(c->delivery_system);

	/* resume previous system */
	switch (curr_system) {
	case DMDIP_DVBT:
		ret = dmd_drv_dvbt_t2_init(fe);
		break;
	case DMDIP_DVBS:
		ret = mdrv_dmd_dvbs_init(fe);
		dvbs_init_flag = 1;
		break;
	case DMDIP_DVBC:
		ret = dmd_drv_dvbc_init(fe);
		break;
	case DMDIP_J83B:
	case DMDIP_ATSC:
		// Init Parameter
		sdmd_atsc_initdata.biqswap =
		cfm_tuner_priv->common_parameter.spectrum_inversion;
		sdmd_atsc_initdata.btunergaininvert =
		cfm_tuner_priv->common_parameter.AGC_inversion;
		sdmd_atsc_initdata.if_khz =
		cfm_tuner_priv->common_parameter.IF_frequency;
		sdmd_atsc_initdata.vsbagclockchecktime   = 100;
		sdmd_atsc_initdata.vsbprelockchecktime   = 300;
		sdmd_atsc_initdata.vsbfsynclockchecktime = 600;
		sdmd_atsc_initdata.vsbfeclockchecktime   = 4000;
		sdmd_atsc_initdata.qamagclockchecktime   = 60;
		sdmd_atsc_initdata.qamprelockchecktime   = 1000;
		sdmd_atsc_initdata.qammainlockchecktime  = 2000;
		sdmd_atsc_initdata.bisextdemod = FALSE;

		ret = !_mdrv_dmd_atsc_md_init(0, &sdmd_atsc_initdata,
			sizeof(sdmd_atsc_initdata));
		break;
	case DMDIP_ATSC30:
		// Init Parameter
		sdmd_atsc_initdata.biqswap = cfm_tuner_priv->common_parameter.spectrum_inversion;
		sdmd_atsc_initdata.btunergaininvert =
			cfm_tuner_priv->common_parameter.AGC_inversion;
		sdmd_atsc_initdata.if_khz = cfm_tuner_priv->common_parameter.IF_frequency;

		sdmd_atsc_initdata.bIsUseSspiLoadCode = false;
		sdmd_atsc_initdata.bis_qpad = false;
		sdmd_atsc_initdata.bIsSspiUseTsPin = false;

		sdmd_atsc_initdata.u8PLP_num = 1;
		sdmd_atsc_initdata.u8PLP_id[0] = 0xFF;
		sdmd_atsc_initdata.u8PLP_id[1] = 64;
		sdmd_atsc_initdata.u8PLP_id[2] = 64;
		sdmd_atsc_initdata.u8PLP_id[3] = 64;
		sdmd_atsc_initdata.u8PLP_layer[0] = 0;
		sdmd_atsc_initdata.u8PLP_layer[1] = 0;
		sdmd_atsc_initdata.u8PLP_layer[2] = 0;
		sdmd_atsc_initdata.u8PLP_layer[3] = 0;
		sdmd_atsc_initdata.u8Minor_Version = 0;
		sdmd_atsc_initdata.u8IsKRMode = 0;
		sdmd_atsc_initdata.bisextdemod = TRUE;
		ret = !_mdrv_dmd_atsc_md_init(1, &sdmd_atsc_initdata, sizeof(sdmd_atsc_initdata));
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_atsc_init fail !\n",
			__func__, __LINE__);
			return ret;
		}
		break;
	case DMDIP_ISDBT:
		sdmd_isdbt_initdata.is_ext_demod = 0;
		sdmd_isdbt_initdata.if_khz =
		cfm_tuner_priv->common_parameter.IF_frequency;
		sdmd_isdbt_initdata.agc_reference_value = 0x400;
		sdmd_isdbt_initdata.tuner_gain_invert =
		cfm_tuner_priv->common_parameter.AGC_inversion;
		ret = !_mdrv_dmd_isdbt_md_init(0, &sdmd_isdbt_initdata,
			sizeof(sdmd_isdbt_initdata));
		break;
	case DMDIP_DTMB:
		sDMD_DTMB_InitData.bIQSwap =
		cfm_tuner_priv->common_parameter.spectrum_inversion;
		sDMD_DTMB_InitData.bisextdemod = 0;
		sDMD_DTMB_InitData.uif_khz16 =
		cfm_tuner_priv->common_parameter.IF_frequency;
		sDMD_DTMB_InitData.btunergaininvert =
		cfm_tuner_priv->common_parameter.AGC_inversion;
		ret = !_mdrv_dmd_dtmb_md_init(0, &sDMD_DTMB_InitData,
			sizeof(sDMD_DTMB_InitData));
		break;
	case DMDIP_VIF:
		pr_info("vif_init is called\n");
		sdmd_vif_initdata.tuner_gain_inv =
		cfm_tuner_priv->common_parameter.AGC_inversion;
		sdmd_vif_initdata.spectrum_inv =
		cfm_tuner_priv->common_parameter.spectrum_inversion;
		sdmd_vif_initdata.if_khz =
		cfm_tuner_priv->common_parameter.IF_frequency;
		ret = !drv_vif_init(&sdmd_vif_initdata, sizeof(sdmd_vif_initdata));
		break;
	case DMDIP_UN:
	default:
	pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
		__func__, __LINE__);
		break;
	}

	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] drv_init fail for sys = %d !\n",
			__func__, __LINE__, c->delivery_system);
		return ret;
	}

	demod_prvi->fw_downloaded = 1;
	/* Program demod */
	switch (curr_system) {
	case DMDIP_DVBT:
		ret = dmd_drv_dvbt_t2_config(fe, u32IFFreq);
		break;
	case DMDIP_DVBS:
		ret = mdrv_dmd_dvbs_config(fe);
		break;
	case DMDIP_DVBC:
		ret = dmd_drv_dvbc_config(fe, u32IFFreq);
		break;
	case DMDIP_J83B:
		ret = !_mdrv_dmd_atsc_md_setconfig(0, DMD_ATSC_DEMOD_ATSC_256QAM, TRUE);
		break;
	case DMDIP_ATSC:
		ret = !_mdrv_dmd_atsc_md_setconfig(0, DMD_ATSC_DEMOD_ATSC_VSB, TRUE);
		break;
	case DMDIP_ATSC30:
		ret = !_mdrv_dmd_atsc_md_setconfig(1, DMD_ATSC_DEMOD_ATSC_OFDM, TRUE);
		break;
	case DMDIP_ISDBT:
		if (c->bandwidth_hz == 8000000)
			ret = !_mdrv_dmd_isdbt_md_advsetconfig(0, DMD_ISDBT_DEMOD_8M, TRUE);
		else
			ret = !_mdrv_dmd_isdbt_md_advsetconfig(0, DMD_ISDBT_DEMOD_6M, TRUE);
		break;
	case DMDIP_DTMB:
		ret = !_mdrv_dmd_dtmb_md_setconfig(0, DMD_DTMB_DEMOD_DTMB, TRUE);
		break;
	case DMDIP_VIF:
		pr_info("SYS_ANALOG set freqency to %d\n", (c->frequency));
		demod_prvi->start_time = mtk_demod_get_time();
		ret = mtk_demod_set_frontend_vif(fe, u32IFFreq);
		break;
	case DMDIP_UN:
	default:
		pr_warn("[mdbgerr_merak_demod_dd][%s][%d]SYS_UNDEFINED!\n",
			__func__, __LINE__);
		break;
	}

	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] dmd_drv_config fail for sys = %d !\n",
			__func__, __LINE__, c->delivery_system);
		return ret;
	}

	return ret;
}

static int mtk_demod_custom_file_open(struct platform_device *pdev)
{
	int ret = 0;
	char config_dat_path[DEMOD_FILE_PATH_LENGTH] = {0};
	const struct firmware *fw = NULL;

	struct device *dev = &(pdev->dev);

	pr_emerg("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!pdev) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] pdev is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	switch (sdmd_vif_initdata.region_model) {
	case CUSTOM_EQ_TYPE0:
		strncpy(config_dat_path, "custom_eq_0.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE0\n", __func__, __LINE__);
		break;
	case CUSTOM_EQ_TYPE1:
		strncpy(config_dat_path, "custom_eq_1.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE1\n", __func__, __LINE__);
		break;
	case CUSTOM_EQ_TYPE2:
		strncpy(config_dat_path, "custom_eq_2.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE2\n", __func__, __LINE__);
		break;
	case CUSTOM_EQ_TYPE3:
		strncpy(config_dat_path, "custom_eq_3.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE3\n", __func__, __LINE__);
		break;
	case CUSTOM_EQ_TYPE4:
		strncpy(config_dat_path, "custom_eq_4.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE4\n", __func__, __LINE__);
		break;
	case CUSTOM_EQ_TYPE5:
		strncpy(config_dat_path, "custom_eq_5.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE5\n", __func__, __LINE__);
		break;
	case CUSTOM_EQ_TYPE6:
		strncpy(config_dat_path, "custom_eq_6.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE6\n", __func__, __LINE__);
		break;
	case CUSTOM_EQ_TYPE7:
		strncpy(config_dat_path, "custom_eq_7.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE7\n", __func__, __LINE__);
		break;
	default:
		strncpy(config_dat_path, "custom_eq_0.dat", DEMOD_FILE_PATH_LENGTH);
		pr_emerg("[%s][%d] CUSTOM_EQ_TYPE0\n", __func__, __LINE__);
		break;
	}

	// open file
	ret = request_firmware_direct(&fw, config_dat_path, dev);

	if (ret) {
		pr_emerg("[%s][%d] custom file open fail !\n", __func__, __LINE__);

		sdmd_vif_initdata.custom_file_open = 0;

		release_firmware(fw);
	} else {
		sdmd_vif_initdata.custom_file_open = 1;

		drv_vif_coef_mapping(fw);

		release_firmware(fw);
	}

	return ret;
}

static int cfm_property_process_get(struct cfm_property *tvp)
{
	int ret = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	if (!tvp) {
		pr_err("[%s][%d] tvp is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	switch (tvp->cmd) {
	case CFM_STREAM_CLOCK_INV:
		tvp->u.data = 1;
		pr_info("[%s][%d]CFM_TS_CLOCK_INV!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_STREAM_EXT_SYNC:
		tvp->u.data = 1;
		pr_info("[%s][%d]CFM_TS_EXT_SYNC!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_STREAM_DATA_BITS_NUM:
		tvp->u.data = PARALLEL_BIT;
		pr_info("[%s][%d]CFM_TS_DATA_BITS_NUM!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_STREAM_BIT_SWAP:
		tvp->u.data = 0;
		pr_info("[%s][%d]CMF_TS_BIT_SWAP!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_TS_SOURCE_PACKET_SIZE:
		tvp->u.data = TS_PACKET_BIT;
		pr_info("[%s][%d]CMF_TS_SOURCE_PACKET_MODE!, value = %d\n",
			__func__, __LINE__, tvp->u.data);
		break;
	case CFM_DECOUPLE_DMD:
		if (delivery_sys_rec == DMDIP_ATSC30)
			decouple_demod = "atsc3_mtk";
		memcpy(tvp->u.buffer.data, decouple_demod, strlen(decouple_demod) + 1);
		pr_info("[%s][%d]CFM_DECOUPLE_DMD!, value = %s\n",
			__func__, __LINE__, tvp->u.buffer.data);
		break;
	case CFM_STREAM_PIN_MUX_INFO:
		if (delivery_sys_rec == DMDIP_ATSC30)
			pinctrl_ts = "ext1_ts_para_demod_pad_mux";

		memcpy(tvp->u.buffer.data, pinctrl_ts, strlen(pinctrl_ts) + 1);
		pr_info("[%s][%d]CFM_STREAM_PIN_MUX_INFO!, value = %s\n",
			__func__, __LINE__, tvp->u.buffer.data);
	break;
	default:
		pr_err("[%s][%d]FE property %d doesn't exist\n",
		 __func__, __LINE__, tvp->cmd);
		pr_info("[%s][%d]FE property %d doesn't exist\n",
			__func__, __LINE__, tvp->cmd);
		return -EINVAL;
	}
	return ret;
}

int mtk_demod_get_param(struct dvb_frontend *fe, struct cfm_properties *demodProps)
{
	int ret = 0;
	int i;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	if (!fe) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] fe is null !\n",
		__func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < demodProps->num; i++) {
		ret = cfm_property_process_get((demodProps->props)+i);
		if (ret < 0)
			goto out;
	}
out:
	return (ret > 0) ? 0 : ret;
}

int mtk_demod_set_notify(int device_unique_id,
	void (*dd_notify)(int, enum cfm_notify_event_type_t, struct cfm_properties *))
{
	demod_notify = dd_notify;

	return 0;
}

int mtk_demod_dvb_frontend_registered(int device_unique_id, struct dvb_frontend *fe)
{
	// save the mapping between device_unique_id and fe to handle multiple frontend cases
	return 0;
}

static const struct cfm_demod_ops mtk_demod_ops = {
	.chip_delsys.size = 10,
	.chip_delsys.element = { SYS_DTMB, SYS_DVBT, SYS_DVBT2, SYS_DVBC_ANNEX_A,
	SYS_ISDBT, SYS_ATSC, SYS_DVBC_ANNEX_B, SYS_ANALOG, SYS_DVBS, SYS_DVBS2},
	.info = {
		.name = "mtk_demod",
		.frequency_min_hz  =  42 * MHz,
		.frequency_max_hz  =  2150 * MHz,
		.caps = FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO |
			FE_CAN_HIERARCHY_AUTO |
			FE_CAN_MUTE_TS
	},

	.sleep = mtk_demod_sleep,
	.init = mtk_demod_init,
	.set_demod = mtk_demod_set_frontend,
	.get_demod = mtk_demod_get_frontend,
	.get_tune_settings = mtk_demod_get_tune_settings,
	.read_status = mtk_demod_read_status,
	.get_frontend_algo = mtk_demod_get_frontend_algo,
	.tune = mtk_demod_tune,
	.set_tone = mtk_demod_set_tone,
	.diseqc_send_burst = mtk_demod_diseqc_send_burst,
	.diseqc_send_master_cmd = mtk_demod_diseqc_send_master_cmd,
	.device_unique_id = 0,
	.dump_debug_info = mtk_demod_dump_debug_info,
	//.private_data_en = 0,
	.blindscan_start = mtk_demod_blindscan_start,
	.blindscan_get_tuner_freq = mtk_demod_blindscan_get_tuner_freq,
	.blindscan_next_freq = mtk_demod_blindscan_next_freq,
	.blindscan_wait_curfreq_finished = mtk_demod_blindscan_wait_curfreq_finished,
	.blindscan_get_channel = mtk_demod_blindscan_get_channel,
	.blindscan_end = mtk_demod_blindscan_end,
	.blindscan_cancel = mtk_demod_blindscan_cancel,
	.suspend = mtk_demod_suspend,
	.resume = mtk_demod_resume,
	.get_param = mtk_demod_get_param,
	.set_notify = mtk_demod_set_notify,
	.dvb_frontend_registered = mtk_demod_dvb_frontend_registered,
	.stop_tune = mtk_demod_stop_tune,
};

static int demod_probe(struct platform_device *pdev)
{
	struct mtk_demod_dev *demod_dev;
	struct resource *res;
	struct device_node *bank_node;
	struct device_node *clampgain_node;
	int u32tmp, ret = 0;
	unsigned int temp = 0;
	struct dvb_adapter *adapter;

	pr_info("[mdbgin_merak_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	demod_dev = devm_kzalloc(&pdev->dev, sizeof(struct mtk_demod_dev),
							GFP_KERNEL);

	if (!demod_dev) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to allocate demod device!\r\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	demod_dev->pdev = pdev;

	bank_node = of_find_node_by_name(pdev->dev.of_node, "banks");
	if (bank_node == NULL)
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get banks node\r\n",
			__func__, __LINE__);

	ret = of_property_read_u32(bank_node, "reset_reg", &u32tmp);
	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get reset_reg resource\r\n",
			__func__, __LINE__);
		return -ENONET;
	}
	pr_info("reset_bank = 0x%x\r\n", u32tmp);

	demod_dev->virt_reset_base_addr = ioremap(u32tmp, 1);

	ret = of_property_read_u32(pdev->dev.of_node, "ip-version", &u32tmp);
	if (ret) {
		pr_emerg("[mdbgerr_merak_demod_dd][%s][%d]Failed to get ip-version\r\n",
			__func__, __LINE__);
		return -ENONET;
	}
	pr_info("[mdbgerr_merak_demod_dd] ####### ip-version = 0x%x\r\n", u32tmp);
	demod_dev->ip_version = (u16)u32tmp;

	if (demod_dev->ip_version > 0x0100) {
		ret = of_property_read_u32(bank_node, "pmux_reg", &u32tmp);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get pmux_reg resource\r\n",
				__func__, __LINE__);
			return -ENONET;
		}
		pr_info("pmux_reg = 0x%x\r\n", u32tmp);
		demod_dev->virt_pmux_base_addr = ioremap(u32tmp, 1);

		ret = of_property_read_u32(bank_node, "vagc_reg", &u32tmp);
		if (ret) {
			pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get vagc_reg resource\r\n",
				__func__, __LINE__);
			return -ENONET;
		}
		pr_info("vagc_reg = 0x%x\r\n", u32tmp);
		demod_dev->virt_vagc_base_addr = ioremap(u32tmp, 2);

		ret = of_property_read_u32(bank_node, "mpll_reg", &u32tmp);
		// mark for CCN > 20 criteria
		//if (ret) {
		//pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get mpll_reg resource\r\n",
		//		__func__, __LINE__);
		//	return -ENONET;
		//}
		pr_info("mpll_reg = 0x%x\r\n", u32tmp);
		demod_dev->virt_mpll_base_addr = ioremap(u32tmp, 0x200);

		ret = of_property_read_u32(bank_node, "bw_ultra_reg", &u32tmp);
		pr_info("bw_ultra_reg = 0x%x\r\n", u32tmp);
		demod_dev->virt_bw_ultra_base_addr = ioremap(u32tmp, 0x200);

		ret = of_property_read_u32(bank_node, "efuse_reg", &u32tmp);
		pr_info("efuse_reg = 0x%x\r\n", u32tmp);
		demod_dev->virt_efuse_base_addr = ioremap(u32tmp, 0x200);

		ret = of_property_read_u32(bank_node, "demod_reg", &u32tmp);
		pr_info("demod_reg = 0x%x\r\n", u32tmp);
		demod_dev->virt_demod_base_addr = ioremap(u32tmp, 0x200);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "if_gain", &u32tmp);
	if (ret) {
		pr_emerg("[mdbgerr_merak_demod_dd][%s][%d]Failed to get if_gain\r\n",
			__func__, __LINE__);
		return -ENONET;
	}
	pr_info("[mdbgerr_merak_demod_dd] ####### if_gain = %d\r\n", u32tmp);
	demod_dev->if_gain = (u8)u32tmp;

	ret = of_property_read_u32(pdev->dev.of_node, "model_info", &u32tmp);
	pr_info("[mdbgerr_merak_demod_dd] ####### model_info = %d\r\n", u32tmp);
	demod_dev->model_info = (u8)u32tmp;

	ret = of_property_read_u32(bank_node, "ts_reg", &u32tmp);
	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get ts_reg resource\r\n",
			__func__, __LINE__);
		return -ENONET;
	}
	pr_info("ts_bank = 0x%x\r\n", u32tmp);

	demod_dev->virt_ts_base_addr = ioremap(u32tmp, 20);

	ret = of_property_read_u32(bank_node, "clk_reg", &u32tmp);
	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get clk_reg resource\r\n",
			__func__, __LINE__);
		return -ENONET;
	}
	pr_info("clkgen_reg = 0x%x\r\n", u32tmp);

	demod_dev->virt_clk_base_addr = ioremap(u32tmp, 0x4000);

	ret = of_property_read_u32(bank_node, "sram_reg", &u32tmp);
	if (ret) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get sram_reg resource\r\n",
			__func__, __LINE__);
		return -ENONET;
	}
	pr_info("sram_reg = 0x%x\r\n", u32tmp);

	demod_dev->virt_sram_base_addr = ioremap(u32tmp, 2);

	clampgain_node = of_find_node_by_name(pdev->dev.of_node, "clampgains");
	ret = of_property_read_u32(clampgain_node, "clampgain_b", &u32tmp);
	pr_info("clampgain_b = 0x%x\r\n", u32tmp);
	sdmd_vif_initdata.clamp_gain_b = ret ? 0 : (u16)u32tmp;

	ret = of_property_read_u32(clampgain_node, "clampgain_gh", &u32tmp);
	pr_info("clampgain_gh = 0x%x\r\n", u32tmp);
	sdmd_vif_initdata.clamp_gain_gh = ret ? 0 : (u16)u32tmp;

	ret = of_property_read_u32(clampgain_node, "clampgain_dk", &u32tmp);
	pr_info("clampgain_dk = 0x%x\r\n", u32tmp);
	sdmd_vif_initdata.clamp_gain_dk = ret ? 0 : (u16)u32tmp;

	ret = of_property_read_u32(clampgain_node, "clampgain_i", &u32tmp);
	pr_info("clampgain_i = 0x%x\r\n", u32tmp);
	sdmd_vif_initdata.clamp_gain_i = ret ? 0 : (u16)u32tmp;

	ret = of_property_read_u32(clampgain_node, "clampgain_l", &u32tmp);
	pr_info("clampgain_l = 0x%x\r\n", u32tmp);
	sdmd_vif_initdata.clamp_gain_l = ret ? 0 : (u16)u32tmp;

	ret = of_property_read_u32(clampgain_node, "clampgain_lp", &u32tmp);
	pr_info("clampgain_lp = 0x%x\r\n", u32tmp);
	sdmd_vif_initdata.clamp_gain_lp = ret ? 0 : (u16)u32tmp;

	ret = of_property_read_u32(clampgain_node, "clampgain_mn", &u32tmp);
	pr_info("clampgain_mn = 0x%x\r\n", u32tmp);
	sdmd_vif_initdata.clamp_gain_mn = ret ? 0 : (u16)u32tmp;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("[mdbgerr_merak_demod_dd][%s][%d]Failed to get reg resource\r\n",
			__func__, __LINE__);
		return -ENONET;
	}
	pr_info("[%s][%d] : start = 0x%zx, end = 0x%zx\r\n",
		__func__, __LINE__, res->start, res->end);

	demod_dev->virt_riu_base_addr = devm_ioremap_resource(&pdev->dev, res);
	pr_info("[%s][%d] : virtDMDBaseAddr = %p\r\n",
		__func__, __LINE__, demod_dev->virt_riu_base_addr);

	// Create Sysfs Attributes
	ret = mtk_dmd_pws_create_sysfs_attr(pdev);
	if (ret) {
		pr_err("[%s][%d]create sysfs attr failed\n",
			__func__, __LINE__);
	}

	// Open custom file
	sdmd_vif_initdata.region_model = demod_dev->model_info;
	ret = mtk_demod_custom_file_open(pdev);

	/* get DRAM addr from dts. */
	mtk_demod_get_memory_buffer(demod_dev);
	pr_info("@@@@@@@@@ dram_base_addr = 0x%x !!!\n", demod_dev->dram_base_addr);
	pr_info("@@@@@@@@@ virt_base_addr = 0x%lx !!!\n", demod_dev->virt_dram_base_addr);

	sdmd_atsc_initdata.virt_mppl_base_addr = demod_dev->virt_mpll_base_addr;
	sDMD_DTMB_InitData.virt_mpll_base_addr = demod_dev->virt_mpll_base_addr;
	sdmd_isdbt_initdata.virt_mpll_base_addr = demod_dev->virt_mpll_base_addr;

	sdmd_isdbt_initdata.tdi_start_addr = (demod_dev->dram_base_addr) / 16;
	sdmd_isdbt_initdata.virt_dram_base_addr = demod_dev->virt_dram_base_addr;
	sdmd_isdbt_initdata.virt_riu_base_addr = demod_dev->virt_riu_base_addr;
	sdmd_isdbt_initdata.virt_reset_base_addr = demod_dev->virt_reset_base_addr;
	sdmd_isdbt_initdata.virt_ts_base_addr = demod_dev->virt_ts_base_addr;
	sdmd_isdbt_initdata.virt_clk_base_addr = demod_dev->virt_clk_base_addr;
	sdmd_isdbt_initdata.virt_sram_base_addr = demod_dev->virt_sram_base_addr;
	sdmd_isdbt_initdata.ip_version = demod_dev->ip_version;
	sdmd_isdbt_initdata.virt_efuse_base_addr = demod_dev->virt_efuse_base_addr;
	sdmd_isdbt_initdata.virt_demod_base_addr = demod_dev->virt_demod_base_addr;

	if (demod_dev->ip_version > 0x0100) {
		sdmd_isdbt_initdata.virt_pmux_base_addr = demod_dev->virt_pmux_base_addr;
		sdmd_isdbt_initdata.virt_vagc_base_addr = demod_dev->virt_vagc_base_addr;
	}

	sDMD_DTMB_InitData.utdistartaddr32 = (demod_dev->dram_base_addr) / 16;
	sDMD_DTMB_InitData.virt_dram_base_addr = demod_dev->virt_dram_base_addr;
	sDMD_DTMB_InitData.virt_riu_base_addr = demod_dev->virt_riu_base_addr;
	sDMD_DTMB_InitData.virt_reset_base_addr = demod_dev->virt_reset_base_addr;
	sDMD_DTMB_InitData.virt_ts_base_addr = demod_dev->virt_ts_base_addr;
	sDMD_DTMB_InitData.virt_clk_base_addr = demod_dev->virt_clk_base_addr;
	sDMD_DTMB_InitData.virt_sram_base_addr = demod_dev->virt_sram_base_addr;
	sDMD_DTMB_InitData.m6_version = demod_dev->ip_version;
	sDMD_DTMB_InitData.virt_efuse_base_addr = demod_dev->virt_efuse_base_addr;
	sDMD_DTMB_InitData.virt_demod_base_addr = demod_dev->virt_demod_base_addr;
	if (demod_dev->ip_version > 0x0100) {
		sDMD_DTMB_InitData.virt_pmux_base_addr = demod_dev->virt_pmux_base_addr;
		sDMD_DTMB_InitData.virt_vagc_base_addr = demod_dev->virt_vagc_base_addr;
	}

	sdmd_atsc_initdata.utdistartaddr32 = (demod_dev->dram_base_addr);
	sdmd_atsc_initdata.virt_dram_base_addr = demod_dev->virt_dram_base_addr;
	sdmd_atsc_initdata.virt_riu_base_addr = demod_dev->virt_riu_base_addr;
	sdmd_atsc_initdata.virt_reset_base_addr = demod_dev->virt_reset_base_addr;
	sdmd_atsc_initdata.virt_ts_base_addr = demod_dev->virt_ts_base_addr;
	sdmd_atsc_initdata.virt_clk_base_addr = demod_dev->virt_clk_base_addr;
	sdmd_atsc_initdata.virt_sram_base_addr = demod_dev->virt_sram_base_addr;
	sdmd_atsc_initdata.m6_version = demod_dev->ip_version;
	sdmd_atsc_initdata.m6_version |= ((demod_dev->if_gain) << 4);
	sdmd_atsc_initdata.virt_efuse_base_addr = demod_dev->virt_efuse_base_addr;
	sdmd_atsc_initdata.virt_demod_base_addr = demod_dev->virt_demod_base_addr;

	if (demod_dev->ip_version > 0x0100) {
		sdmd_atsc_initdata.virt_pmux_base_addr = demod_dev->virt_pmux_base_addr;
		sdmd_atsc_initdata.virt_vagc_base_addr = demod_dev->virt_vagc_base_addr;
	}

	sdmd_vif_initdata.dram_base_addr = (demod_dev->dram_base_addr) / 16;
	sdmd_vif_initdata.virt_dram_base_addr = demod_dev->virt_dram_base_addr;
	sdmd_vif_initdata.virt_riu_base_addr = demod_dev->virt_riu_base_addr;
	sdmd_vif_initdata.virt_reset_base_addr = demod_dev->virt_reset_base_addr;
	sdmd_vif_initdata.virt_clk_base_addr = demod_dev->virt_clk_base_addr;
	sdmd_vif_initdata.virt_sram_base_addr = demod_dev->virt_sram_base_addr;
	sdmd_vif_initdata.virt_mpll_base_addr = demod_dev->virt_mpll_base_addr;
	sdmd_vif_initdata.ip_version = demod_dev->ip_version;
	sdmd_vif_initdata.virt_demod_base_addr = demod_dev->virt_demod_base_addr;
	if (demod_dev->ip_version > 0x0100) {
		sdmd_vif_initdata.virt_pmux_base_addr = demod_dev->virt_pmux_base_addr;
		sdmd_vif_initdata.virt_vagc_base_addr = demod_dev->virt_vagc_base_addr;
	}

	memcpy(&demod_dev->dmd_ops, &mtk_demod_ops,
		sizeof(struct cfm_demod_ops));
	pr_info("[%s][%d] AUSTINN = %d\n", __func__, __LINE__, p_austin_dev->austin_exist);

	if (p_austin_dev->austin_exist) {
		demod_dev->dmd_ops.chip_delsys.element[demod_dev->dmd_ops.chip_delsys.size] =
		(u8)(SYS_ATSC30);
		demod_dev->dmd_ops.chip_delsys.size = 11;
	}

	demod_dev->dmd_ops.demodulator_priv = demod_dev;

	ret = of_property_read_u32(pdev->dev.of_node, "unique_id", &temp);
	pr_info("[%s][%d], unique_id=%d\n", __func__, __LINE__, temp);
	if (ret == 0)
		demod_dev->dmd_ops.device_unique_id = (u16) temp;
	else
		pr_err("[mdbgerr_merak_demod_dd][%s][%d] Unable to get demod unique_id\n",
			__func__, __LINE__);

	demod_unique_id = demod_dev->dmd_ops.device_unique_id;

	ret = of_property_read_string(pdev->dev.of_node, "decouple-demod", &decouple_demod);
	pr_info("[%s][%d], decouple-demod=%s\n", __func__, __LINE__, decouple_demod);

	ret = of_property_read_string(pdev->dev.of_node, "pinctrl-ts", &pinctrl_ts);
	pr_info("[%s][%d], pinctrl-ts=%s\n", __func__, __LINE__, pinctrl_ts);

	ret = of_property_read_u32(pdev->dev.of_node, "envelope_tone_mode", &u32tmp);

	b_diseqc_envelope_mode_enable = FALSE;
	if ((ret == 0) && (u32tmp > 0))
		b_diseqc_envelope_mode_enable = TRUE;

	pr_info("[%s][%d], envelope_tone_mode = %d\n",
		__func__, __LINE__, b_diseqc_envelope_mode_enable);

	adapter = mdrv_get_dvb_adapter();
	if (!adapter) {
		pr_err("[%s][%d] get dvb adapter fail\n", __func__, __LINE__);
		return -ENODEV;
	}

	cfm_register_demod_device(adapter, &demod_dev->dmd_ops);
	demod_dev->previous_system = SYS_UNDEFINED;
	demod_dev->fw_downloaded = 0;
	platform_set_drvdata(pdev, demod_dev);
	mtk_demod_clk_base_addr_init(demod_dev);
	//demod_init_get_memory_flag = 0;

	return 0;
}

static int demod_remove(struct platform_device *pdev)
{
	struct mtk_demod_dev *demod_dev;
	int prev_system = 0;
	int ret = 0;

	pr_info("[%s] is called\n", __func__);

	demod_dev = platform_get_drvdata(pdev);

	if (!demod_dev) {
		pr_err("Failed to get demod_dev\n");
		return -EINVAL;
	}

	prev_system = mtk_demod_ip_mapping(demod_dev->previous_system);

	/* check if fw_loaded */
	if (demod_dev->fw_downloaded) {
		/* exit previous system */
		switch (prev_system) {
		case DMDIP_DVBT:
			ret = dmd_drv_dvbt_t2_exit(&demod_dev->fe);
			if (ret) {
				pr_err("[%s][%d] dmd_drv_dvbt_t2_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DVBS:
			ret = mdrv_dmd_dvbs_exit(&demod_dev->fe);
			if (ret) {
				pr_err("[%s][%d] MDrv_DMD_DVBS_Exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DVBC:
			ret = dmd_drv_dvbc_exit(&demod_dev->fe);
			if (ret) {
				pr_err("[%s][%d] dmd_drv_dvbc_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_J83B:
		case DMDIP_ATSC:
			ret = !_mdrv_dmd_atsc_md_exit(0);
			if (ret) {
				pr_err("[%s][%d] _mdrv_dmd_atsc_md_exit fail !\n",
				    __func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_ATSC30:
			ret = !_mdrv_dmd_atsc_md_exit(1);
			if (ret) {
				pr_err("[%s][%d] _mdrv_dmd_atsc_md_exit fail!\n",
				__func__, __LINE__);
				return ret;
			}
		break;
		case DMDIP_ISDBT:
			ret = !_mdrv_dmd_isdbt_md_exit(0);
			if (ret) {
				pr_err("[%s][%d] _MDrv_DMD_ISDBT_MD_Exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_DTMB:
			ret = !_mdrv_dmd_dtmb_md_exit(0);
			if (ret) {
				pr_err("[%s][%d] _mdrv_dmd_dtmb_md_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_VIF:
			pr_info("vif_exit (borrow dab) in remove is called\n");
			ret = !drv_vif_exit();
			if (ret) {
				pr_err("[%s][%d] drv_vif_exit fail !\n",
					__func__, __LINE__);
				return ret;
			}
			break;
		case DMDIP_UN:
		default:
			break;
		}
		demod_dev->fw_downloaded = 0;

		/* update system */
		demod_dev->previous_system = SYS_UNDEFINED;
	}

	// Remove Sysfs Attributes
	ret =  mtk_dmd_pws_remove_sysfs_attr(pdev);
	if (ret) {
		pr_err("[%s][%d] remove sysfs attr failed\n",
			__func__, __LINE__);
	}

//VT	dvb_unregister_frontend(&demod_dev->fe);
	vfree(pages_km);
	return ret;
}

static int demod_suspend(struct device *dev)
{
	/* @TODO: Not yet implement */
	return 0;
}

static int demod_resume(struct device *dev)
{
	/* @TODO: Not yet implement */
	return 0;
}

static const struct dev_pm_ops demod_pm_ops = {
	.suspend = demod_suspend,
	.resume = demod_resume,
};

static const struct of_device_id demod_dt_match[] = {
	{ .compatible = MERAK_DRV_NAME },
	{},
};

MODULE_DEVICE_TABLE(of, demod_dt_match);

static struct platform_driver demod_driver = {
	.probe	  = demod_probe,
	.remove	 = demod_remove,
	.driver	 = {
		.name   = MERAK_DRV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = demod_dt_match,
		.pm = &demod_pm_ops,
		.groups  = dmd_groups
	},
};

module_platform_driver(demod_driver);

MODULE_DESCRIPTION("Mediatek Demod Driver");
MODULE_AUTHOR("Mediatek Author");
MODULE_LICENSE("GPL");
