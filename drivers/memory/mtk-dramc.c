// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 */
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/time64.h>
#include <linux/pm_runtime.h>
#include <asm-generic/div64.h>
#include "mtk-dramc.h"
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/freezer.h>
#include <linux/wait.h>

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/iio/consumer.h>
#include <linux/of_platform.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>

#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/bug.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/iio/consumer.h>
#include <linux/of_platform.h>
#include <linux/ioport.h>

static unsigned int mtk_dramc_debug;
#define MTK_DRAMC_DBG(fmt, args...)	\
do {					\
	if (mtk_dramc_debug)		\
		pr_debug(fmt, ##args);	\
} while (0)
static struct DRAMC_RESOURCE_PRIVATE *pDramcRes;
static void __iomem *mailbox_dumreg_base;
#define MAILBOX_READ_REG(offset)    readl_relaxed(mailbox_dumreg_base + (offset))
#define MAILBOX_READW_REG(offset)    readw_relaxed(mailbox_dumreg_base + (offset))
#define MAILBOX_READB_REG(offset)    readb_relaxed(mailbox_dumreg_base + (offset))
#define DUMMY_REG_BASE 0x1c606a00
#define DUMMY_REG_LEN 0x1000
#define DUMMY_REG_A0 0xA0
#define DUMMY_REG_A1 0xA1
#define DUMMY_REG_A4 0xA4
#define DUMMY_REG_A5 0xA5
#define DUMMY_REG_A8 0xA8
#define DUMMY_REG_A9 0xA9
#define DUMMY_REG_38 0x38
#define DUMMY_REG_9D 0x9D
#define DUMMY_REG_AC 0xAC
#define DUMMY_REG_AD 0xAD
#define DUMMY_REG_B0 0xB0
#define ENABLE_DYNAMIC_REFRESH_FSM
//#define DUMMY_INFO_DEBUG
//#define DRAMC_SSC_DEBUG
#define ENABLE_SWITCH_SHUFFLE_DFS
#define ENABLE_MIFFY_SSC_ADJ

static u16 u16DramcChipID;
static int iChipIDindex;
// Chip ID ...
#define CHIP_ID_MACHLI  0x0101
#define CHIP_ID_MANKS   0x0109
#define CHIP_ID_MOKONA  0x010C
#define CHIP_ID_MIFFY   0x010D
#define CHIP_ID_UNKNOWN   0xFFFF
// pm_top
#define REG_PM_TOP_BASE        0x1c021000 // bank_108
#define REG_PM_TOP_SIZE        0x200
#define REG_PM_TOP_CHIP_ID     0x00
void __iomem *_pm_top_base;
#define PMTOP_READ_REG(offset)	                    readw(_pm_top_base + (offset))

#ifdef ENABLE_DYNAMIC_REFRESH_FSM
void __iomem *_ddrphy_ao_base;
void __iomem *_dramc_ao_base;
void __iomem *_dramc_nao_base;
static struct task_struct *stDramcThread;

#define DDRPHY_AO  0x1C380000
#define DRAMC_AO  0x1C38e000
#define DRAMC_NAO 0x1C3A0000
#define DDRPHY_READ_REG(offset)	                    readl_relaxed(_ddrphy_ao_base + (offset))
#define DDRPHY_WRITE_REG(val, offset)	        writel_relaxed((val), _ddrphy_ao_base + (offset))
#define DRAMC_READ_REG(offset)	                    readl_relaxed(_dramc_ao_base + (offset))
#define DRAMC_WRITE_REG(val, offset)	        writel_relaxed((val), _dramc_ao_base + (offset))
#define DRAMC_NAO_READ_REG(offset)	        readl_relaxed(_dramc_nao_base + (offset))
#define DRAMC_NAO_WRITE_REG(val, offset)    writel_relaxed((val), _dramc_nao_base + (offset))

#define DDR_REFRESH_RATE_INTERVAL_7900_NS 0x7900
#define DDR_REFRESH_RATE_INTERVAL_3900_NS 0x3900
#define DDR_REFRESH_RATE_INTERVAL_1950_NS 0x1950

#define DRAMC_DYNAMIC_REF_SUP_CHIP_NUM 3
//mokona    miffy   moka
static int u8I_High_Bound[DRAMC_DYNAMIC_REF_SUP_CHIP_NUM] = {102, 102, 102};
static int u8I_Low_Bound[DRAMC_DYNAMIC_REF_SUP_CHIP_NUM] = {92, 92, 92};
static int u8D_High_Bound[DRAMC_DYNAMIC_REF_SUP_CHIP_NUM] = {97, 97, 97};
static int u8D_Low_Bound[DRAMC_DYNAMIC_REF_SUP_CHIP_NUM] = {87, 87, 87};
static u32 u32CHIPIndex;
static u32 u32IHB;
static u32 u32ILB;
static u32 u32DHB;
static u32 u32DLB;

#define Refresh_Rate_Increase_High_Bound(x)   (u8I_High_Bound[x])
#define Refresh_Rate_Increase_Low_Bound(x)    (u8I_Low_Bound[x])
#define Refresh_Rate_Decrease_High_Bound(x)   (u8D_High_Bound[x])
#define Refresh_Rate_Decrease_Low_Bound(x)    (u8D_Low_Bound[x])
#define DramcChannelMax 4
#define DRAMC_POLLING_INTERVAL 1000

#define DDRPHY_AO_LEN 0x10000
#define DRAMC_AO_LEN 0x10000
#define DRAMC_NAO_LEN 0x6000
#define REG_DDR_REF_RATE 0x02F0
#define DDR_REF_RATE_1950 0x005d002e
#define DDR_REF_RATE_3900 0x00bb005d
#define DDR_REF_RATE_7900 0x017600bb
#define CH_OFFSET 0x2000

u16 u16Pre_interval = DDR_REFRESH_RATE_INTERVAL_1950_NS;
static int refresh_temp_pre;
static u32 u32DREFLog;
static u32 u32DREFEn = 1;
static u32 u32DREFSet;

#endif
#define MG_NUMBER_2 2
#define MG_NUMBER_8 8
#define MG_NUMBER_14 14
#define MG_NUMBER_15 15
#define MG_NUMBER_16 16
#define MG_NUMBER_1000 1000

#define BIT15_MASK 0x8000
#define BIT14_MASK 0x4000

#define DDRphyChannelMax 6

u16 g_u16VBE_Code_FT;
u16 g_u16TA_Code;

void __iomem *_efuse_base;
void __iomem *_t_sensor_base;

#define CPU_BASE                   0x1C000000
#define EFUSE_BASE                 (0x12900 << 1)
#define REG_EFUSE_TOGGLE           (0x28 << 2)
#define REG_VBE_CODE_FT            (0x2C << 2)
#define REG_TA_CODE                (0x2D << 2)
#define T_SENSOR_BASE              (0x10500 << 1)
#define REG_CPU_T_SENSOR           (0x4C << 2)
#define REG_XC_T_SENSOR            (0x4D << 2)
#define REG_HDMI_T_SENSOR          (0x4E << 2)
#define TSENSOR_BANK_UNIT           0x10000

#define EFUSE_READ_REG(offset)        readw(_efuse_base + (offset))
#define EFUSE_WRITE_REG(val, offset)  writew((val), _efuse_base + (offset))
#define T_SENSOR_READ_REG(offset)	    readw(_t_sensor_base + (offset))
#define T_SENSOR_WRITE_REG(val, offset)  writew((val), _t_sensor_base + (offset))

#define MIFFY_TSENSOR_EQUATION(VBE_FT, TA, VBE)   ((1270*(VBE_FT - VBE) + ((TA*100)+1500))/1000)
//1.27*(VBE_CODE_FT - VBE_CODE )+(TA_CODE/10+1.5)

#ifdef ENABLE_MIFFY_SSC_ADJ
void __iomem *_miu_ssc_ch0_base;
void __iomem *_miu_ssc_ch1_base;

#define    MIU_SSC_CH0                         0x1c2e7600    //0x2e7600 ~ 0x2e77ff
#define    MIU_SSC_CH1                         0x1c2e7800    //0x2e7800 ~ 0x2e79ff
#define    MIU_SSC_UNIT                        0x10000

#define MIU_SSC_CH0_READ_REG(offset)        readl_relaxed(_miu_ssc_ch0_base + (offset))
#define MIU_SSC_CH0_WRITE_REG(val, offset)  writel_relaxed((val), _miu_ssc_ch0_base + (offset))
#define MIU_SSC_CH1_READ_REG(offset)	    readl_relaxed(_miu_ssc_ch1_base + (offset))
#define MIU_SSC_CH1_WRITE_REG(val, offset)  writel_relaxed((val), _miu_ssc_ch1_base + (offset))

#define RG_DDRPLL_LOOP_DIV_FIRST 2
#define RG_DDRPLL_LOOP_DIV_SECOND 3
#define MIU_SSC_MCLK_LSB_OFFSET 0x60
#define MIU_SSC_MCLK_MSB_OFFSET 0x64
#define MIU_SSC_SSCRG_OFFSET 0x50

#define MIU_SSC_OFF		0
#define MIU_SSC_ON		1
#define SSC_FMODULATION_MAX 100
#define SSC_FMODULATION_MIN 20
#define SSC_Perc_MAX 10
#define SSC_Perc_MIN 2

#define MIU_SSC_STEP_OFFSET 0x50
#define MIU_SSC_SPAN_OFFSET 0x54

static u32 u32SSCCh;
static u32 u32SSCEn;
static u32 u32SSCKhzc;
static u32 u32SSCPerc;

int MIU_SSC_ioremap(void)
{
	_miu_ssc_ch0_base = ioremap(MIU_SSC_CH0, MIU_SSC_UNIT);
	_miu_ssc_ch1_base = ioremap(MIU_SSC_CH1, MIU_SSC_UNIT);

	if (!_miu_ssc_ch0_base || !_miu_ssc_ch1_base) {
		if (!_miu_ssc_ch0_base)
			pr_crit("[Dramc SSC] ioremap error for _miu_ssc_ch0_base!\n");
		else if (!_miu_ssc_ch1_base)
			pr_crit("[Dramc SSC] ioremap error for _miu_ssc_ch1_base!\n");
		return 0;
	}

	return 1;
}

int MIU_SSC_iounmap(void)
{
	iounmap(_miu_ssc_ch0_base);
	iounmap(_miu_ssc_ch1_base);
	return 1;
}
void MIU_SSC_debug(void)
{
	//u32 ch0_mclk = (MIU_SSC_CH0_READ_REG(MIU_SSC_MCLK_LSB_OFFSET))
	//		|(u32)(MIU_SSC_CH0_READ_REG(MIU_SSC_MCLK_MSB_OFFSET)<<MG_NUMBER_16);
	u32 ch0_step = MIU_SSC_CH0_READ_REG(MIU_SSC_STEP_OFFSET);
	u32 ch0_span = MIU_SSC_CH0_READ_REG(MIU_SSC_SPAN_OFFSET);
	//u32 ch1_mclk = (MIU_SSC_CH1_READ_REG(MIU_SSC_MCLK_LSB_OFFSET))
	//		|(u32)(MIU_SSC_CH1_READ_REG(MIU_SSC_MCLK_MSB_OFFSET)<<MG_NUMBER_16);
	u32 ch1_step = MIU_SSC_CH1_READ_REG(MIU_SSC_STEP_OFFSET);
	u32 ch1_span = MIU_SSC_CH1_READ_REG(MIU_SSC_SPAN_OFFSET);

	//u32 span_temp1;
	//u32 span_temp2;

	// bit0-9  : STEP
	// bit14   : SSC mode
	// bit15   : SSC enable
	// bit16-29: SPAN
	pr_crit("[Dramc SSC] MIU_SSC_INFO\n");

	//pr_crit("[Dramc SSC] CH0 mclk 0x%x\n", ch0_mclk);
	pr_crit("[Dramc SSC] CH0 step:0x%x\n", ch0_step);
	pr_crit("[Dramc SSC] CH0 span:0x%x\n", ch0_span);
	pr_crit("[Dramc SSC] CH0 enable:%x\n", (ch0_step>>MG_NUMBER_15)&0x1);
	//pr_crit("[Dramc SSC] CH0 mode:%x\n", (ch0_step>>MG_NUMBER_14)&0x1);

	//pr_crit("[Dramc SSC] CH1 mclk 0x%x\n", ch1_mclk);
	pr_crit("[Dramc SSC] CH1 step:0x%x\n", ch1_step);
	pr_crit("[Dramc SSC] CH1 span:0x%x\n", ch1_span);
	pr_crit("[Dramc SSC] CH1 enable:%x\n", (ch1_step>>MG_NUMBER_15)&0x1);
	//pr_crit("[Dramc SSC] CH1 mode:%x\n", (ch1_step>>MG_NUMBER_14)&0x1);

	//span_temp1 = (u32)((u64)(432*131072)*1000/(ch0_mclk*(40)));
	//span_temp2 = (u32)(((u64)(432*131072)*1000+((ch0_mclk*(40))/2))/(ch0_mclk*(40)));

}

int MIU_SSC_Read_Mclk(int ch)
{
	u32 mclk;

	if (ch == 0) {
		mclk = (MIU_SSC_CH0_READ_REG(MIU_SSC_MCLK_LSB_OFFSET))
				|(u32)(MIU_SSC_CH0_READ_REG(MIU_SSC_MCLK_MSB_OFFSET)<<MG_NUMBER_16);
	} else {
		mclk = (MIU_SSC_CH1_READ_REG(MIU_SSC_MCLK_LSB_OFFSET))
				|(u32)(MIU_SSC_CH1_READ_REG(MIU_SSC_MCLK_MSB_OFFSET)<<MG_NUMBER_16);
	}
	return mclk;
}

int MIU_SSC_Cal_Span(int ch, int KHZ)
{
	u32 span;
	u32 mclk = MIU_SSC_Read_Mclk(ch);

	span = (u32)(((u64)(432*131072)*1000+((mclk*(KHZ))/2))/(mclk*(KHZ)));

	return span;
}

int MIU_SSC_Cal_Step(int ch, int KHZ, int Percentage)
{
	u32 step;
	u32 span = MIU_SSC_Cal_Span(ch, KHZ);
	u32 mclk = MIU_SSC_Read_Mclk(ch);
	//multipy1000 for translating 0.2-1% to 2-10

step = ((mclk*MG_NUMBER_2*(Percentage))+(span*MG_NUMBER_1000))/(span*MG_NUMBER_2*MG_NUMBER_1000);

	return step;
}

void MIU_SSC_Set_Enable(int ch, int en)
{
	// bit0-9  : STEP
	// bit14   : SSC mode
	// bit15   : SSC enable
	// bit16-29: SPAN
	u32 STEP_RG_Temp;

	if (ch == 0) {
		STEP_RG_Temp = MIU_SSC_CH0_READ_REG(MIU_SSC_STEP_OFFSET);
		//0x173B_14[15]=1, no matter SSC on/off
		MIU_SSC_CH0_WRITE_REG(STEP_RG_Temp|BIT15_MASK, MIU_SSC_STEP_OFFSET);
		if (en)	//0x173B_14[14]=1 for ON
			MIU_SSC_CH0_WRITE_REG(STEP_RG_Temp|BIT14_MASK, MIU_SSC_STEP_OFFSET);
		else	//0x173B_14[14]=0 for OFF
			MIU_SSC_CH0_WRITE_REG(STEP_RG_Temp&(~BIT14_MASK), MIU_SSC_STEP_OFFSET);
	} else {
		STEP_RG_Temp = MIU_SSC_CH1_READ_REG(MIU_SSC_STEP_OFFSET);
		//0x173C_14[15]=1, no matter SSC on/off
		MIU_SSC_CH1_WRITE_REG(STEP_RG_Temp|BIT15_MASK, MIU_SSC_STEP_OFFSET);
		if (en) //0x173C_14[14]=1 for ON
			MIU_SSC_CH1_WRITE_REG(STEP_RG_Temp|BIT14_MASK, MIU_SSC_STEP_OFFSET);
		else	//0x173C_14[14]=0 for OFF
			MIU_SSC_CH1_WRITE_REG(STEP_RG_Temp&(~BIT14_MASK), MIU_SSC_STEP_OFFSET);
	}
}

void MIU_SSC_Set_Mode(int ch, int mode)
{
	u32 STEP_RG_Temp;

	if (ch == 1) {
		STEP_RG_Temp = MIU_SSC_CH1_READ_REG(MIU_SSC_STEP_OFFSET);

		if (mode)
			MIU_SSC_CH1_WRITE_REG(STEP_RG_Temp|BIT14_MASK, MIU_SSC_STEP_OFFSET);
		else
			MIU_SSC_CH1_WRITE_REG(STEP_RG_Temp&(~BIT14_MASK), MIU_SSC_STEP_OFFSET);
	} else {
		STEP_RG_Temp = MIU_SSC_CH0_READ_REG(MIU_SSC_STEP_OFFSET);

		if (mode)
			MIU_SSC_CH0_WRITE_REG(STEP_RG_Temp|BIT14_MASK, MIU_SSC_STEP_OFFSET);
		else
			MIU_SSC_CH0_WRITE_REG(STEP_RG_Temp&(~BIT14_MASK), MIU_SSC_STEP_OFFSET);
	}
}

static ssize_t dramc_ssc_show(struct device_driver *drv, char *buf)
{
	MIU_SSC_ioremap();
	MIU_SSC_debug();
	MIU_SSC_iounmap();

	return 0;
}

static ssize_t dramc_ssc_store(struct device_driver *drv, const char *buf, size_t count)
{
	size_t count_sscan;
	u32 STEP_RG_Temp;

	count_sscan = sscanf(buf, "%d %d %d %d", &u32SSCCh,
						&u32SSCEn, &u32SSCKhzc, &u32SSCPerc);
	pr_crit("count_sscan = %u\n",(unsigned int)(count_sscan));

	if (count_sscan == 4) {
		MIU_SSC_ioremap();
		pr_crit("\n[Dramc SSC] Tune SSC\n Ch:%d\n En:%d\n KHz:%d\n Perc:%d\n\n\n"
				, u32SSCCh, u32SSCEn, u32SSCKhzc, u32SSCPerc);
		MIU_SSC_Set_Enable(u32SSCCh, MIU_SSC_OFF);//no matter ON or OFF,turn off first
		if (!u32SSCEn) {
			pr_crit("[Dramc SSC] CH%d DDR SSC turn off\n", u32SSCCh*MG_NUMBER_2);

		} else {
			pr_crit("[Dramc SSC] MIU_SSC_Cal_Span = %x\n"
				, MIU_SSC_Cal_Span(u32SSCCh, u32SSCKhzc));

			pr_crit("[Dramc SSC] MIU_SSC_Cal_Step = %x\n"
				, MIU_SSC_Cal_Step(u32SSCCh, u32SSCKhzc, u32SSCPerc));

		if (u32SSCCh == 0) {
			STEP_RG_Temp = MIU_SSC_CH0_READ_REG(MIU_SSC_STEP_OFFSET);
			MIU_SSC_CH0_WRITE_REG(((MIU_SSC_Cal_Step(u32SSCCh, u32SSCKhzc, u32SSCPerc)
				&0x3FF)|(STEP_RG_Temp&0xC000)), MIU_SSC_STEP_OFFSET);

			MIU_SSC_CH0_WRITE_REG(MIU_SSC_Cal_Span(u32SSCCh, u32SSCKhzc)
								, MIU_SSC_SPAN_OFFSET);

		} else {
			STEP_RG_Temp = MIU_SSC_CH1_READ_REG(MIU_SSC_STEP_OFFSET);
			MIU_SSC_CH1_WRITE_REG(((MIU_SSC_Cal_Step(u32SSCCh, u32SSCKhzc, u32SSCPerc)
				&0x3FF)|(STEP_RG_Temp&0xC000)), MIU_SSC_STEP_OFFSET);

			MIU_SSC_CH1_WRITE_REG(MIU_SSC_Cal_Span(u32SSCCh, u32SSCKhzc)
								, MIU_SSC_SPAN_OFFSET);
		}

		MIU_SSC_Set_Enable(u32SSCCh, u32SSCEn);
		}
		MIU_SSC_iounmap();
	} else {
		pr_crit("[Dramc SSC] Wrong Argument!!!\n");
	}

	return count;
}

unsigned int   mtk_dramc_ssc_set(DRAM_CH_NUM eMiuCh,
			u32 u32SscFrequency, u32 u32SscRatio, u8 bEnable)
{
	u32 STEP_RG_Temp;

	if ((u32SscFrequency < SSC_FMODULATION_MIN) || (u32SscFrequency > SSC_FMODULATION_MAX) ||
		(u32SscRatio > SSC_Perc_MAX) || (u32SscRatio < SSC_Perc_MIN))
		return -1;

	MIU_SSC_ioremap();
	pr_crit("\n[MTK_DRAMC] Tune SSC\n Ch:%d\n En:%d\n KHz:%d\n Perc:%d\n\n\n"
			, eMiuCh, bEnable, u32SscFrequency, u32SscRatio);


	MIU_SSC_Set_Enable(eMiuCh, MIU_SSC_OFF);//no matter ON or OFF,turn off first
	if (!bEnable) {
		pr_crit("[MTK_DRAMC] CH%d DDR SSC turn off\n", eMiuCh*MG_NUMBER_2);
	} else {
		pr_crit("[MTK_DRAMC] MIU_SSC_Cal_Span = %x\n"
			, MIU_SSC_Cal_Span(eMiuCh, u32SscFrequency));

		pr_crit("[MTK_DRAMC] MIU_SSC_Cal_Step = %x\n"
			, MIU_SSC_Cal_Step(eMiuCh, u32SscFrequency, u32SscRatio));

		if (eMiuCh == 0) {
			STEP_RG_Temp = MIU_SSC_CH0_READ_REG(MIU_SSC_STEP_OFFSET);
			MIU_SSC_CH0_WRITE_REG(((MIU_SSC_Cal_Step(eMiuCh, u32SscFrequency,
				u32SscRatio)&0x3FF)|(STEP_RG_Temp&0xC000))
							, MIU_SSC_STEP_OFFSET);

			MIU_SSC_CH0_WRITE_REG(MIU_SSC_Cal_Span(eMiuCh, u32SscFrequency)
							, MIU_SSC_SPAN_OFFSET);
			//MIU_SSC_CH0_WRITE_REG((STEP_RG_Temp|0xC000),MIU_SSC_STEP_OFFSET);
		} else {
			STEP_RG_Temp = MIU_SSC_CH1_READ_REG(MIU_SSC_STEP_OFFSET);
			MIU_SSC_CH1_WRITE_REG(((MIU_SSC_Cal_Step(eMiuCh, u32SscFrequency,
				u32SscRatio)&0x3FF)|(STEP_RG_Temp&0xC000))
							, MIU_SSC_STEP_OFFSET);

			MIU_SSC_CH1_WRITE_REG(MIU_SSC_Cal_Span(eMiuCh, u32SscFrequency)
							, MIU_SSC_SPAN_OFFSET);
			//MIU_SSC_CH1_WRITE_REG((STEP_RG_Temp|0xC000),MIU_SSC_STEP_OFFSET);
		}

	MIU_SSC_Set_Enable(eMiuCh, bEnable);
	//MIU_SSC_Set_Mode(u32SSCCh, u32SSCMode);
	pr_crit("[MTK_DRAMC] mtk_dramc_ssc done!\n");

	}
	MIU_SSC_iounmap();
	return 0;
}
EXPORT_SYMBOL(mtk_dramc_ssc_set);
#endif

int pm_top_ioremap(void)
{
	_pm_top_base = ioremap(REG_PM_TOP_BASE, REG_PM_TOP_SIZE);

	if (!_pm_top_base) {
		pr_crit("[Dramc DRef] ioremap error for _pm_top_base!\n");
		return 0;
	}
	return 1;
}

int pm_top_iounmap(void)
{
	iounmap(_pm_top_base);
	return 1;
}

u16 pm_top_get_chip_id(void)
{
	u16 u16_chip_id;

	if (pm_top_ioremap() == 0)
		return CHIP_ID_UNKNOWN;

	u16_chip_id = PMTOP_READ_REG(REG_PM_TOP_CHIP_ID);
	pm_top_iounmap();
	return u16_chip_id;
}

int t_sensor_ioremap(void)
{
	_efuse_base = ioremap((CPU_BASE+EFUSE_BASE), TSENSOR_BANK_UNIT);
	_t_sensor_base = ioremap((CPU_BASE+T_SENSOR_BASE), TSENSOR_BANK_UNIT);
	if (!_efuse_base || !_t_sensor_base) {
		if (!_efuse_base)
			pr_crit("[Dramc t-sensor] ioremap error for _efuse_base!\n");
		else if (!_t_sensor_base)
			pr_crit("[Dramc t-sensor] ioremap error for _t_sensor_base!\n");
		return 0;
	}
	return 1;
}

int t_sensor_iounmap(void)
{
	iounmap(_efuse_base);
	iounmap(_t_sensor_base);
	return 1;
}

u16 Dramc_Thermal_VBE_Code_FT(void)
{
	u16 u16EfuseTogTemp = 0;
	u16 u16VBE_Code_FT = 0;

	u16EfuseTogTemp = EFUSE_READ_REG(REG_EFUSE_TOGGLE);
	EFUSE_WRITE_REG(0x206C, REG_EFUSE_TOGGLE); // switch efuse bank for read VBE/TA code

	msleep(20);
	u16VBE_Code_FT = EFUSE_READ_REG(REG_VBE_CODE_FT);
	EFUSE_WRITE_REG(u16EfuseTogTemp, REG_EFUSE_TOGGLE); // switch efuse bank to original

	return u16VBE_Code_FT;
}

u16 Dramc_Thermal_TA_Code(void)
{
	u16 u16EfuseTogTemp = 0;
	u16 u16TA_Code = 0;

	u16EfuseTogTemp = EFUSE_READ_REG(REG_EFUSE_TOGGLE);
	EFUSE_WRITE_REG(0x206C, REG_EFUSE_TOGGLE); // switch efuse bank for read VBE/TA code

	msleep(20);
	u16TA_Code = EFUSE_READ_REG(REG_TA_CODE);
	EFUSE_WRITE_REG(u16EfuseTogTemp, REG_EFUSE_TOGGLE); // switch efuse bank to original

	return u16TA_Code;
}

u16 Dramc_Thermal_CPU_Tsensor(void)
{
	u16 u6Thermal = 0;

	u6Thermal = MIFFY_TSENSOR_EQUATION(g_u16VBE_Code_FT,
				g_u16TA_Code,
				T_SENSOR_READ_REG(REG_CPU_T_SENSOR));

	return u6Thermal;
}

u16 Dramc_Thermal_XC_Tsensor(void)
{
	u16 u6Thermal = 0;

	u6Thermal = MIFFY_TSENSOR_EQUATION(g_u16VBE_Code_FT,
				g_u16TA_Code,
				T_SENSOR_READ_REG(REG_XC_T_SENSOR));

	return u6Thermal;
}

u16 Dramc_Thermal_HDMI_Tsensor(void)
{
	u16 u6Thermal = 0;

	u6Thermal = MIFFY_TSENSOR_EQUATION(g_u16VBE_Code_FT,
				g_u16TA_Code,
				T_SENSOR_READ_REG(REG_HDMI_T_SENSOR));

	return u6Thermal;
}

#ifdef ENABLE_DYNAMIC_REFRESH_FSM
int dramc_ioremap(void)
{
	_ddrphy_ao_base = ioremap(DDRPHY_AO, DDRPHY_AO_LEN);
	_dramc_ao_base = ioremap(DRAMC_AO, DRAMC_AO_LEN);
	_dramc_nao_base = ioremap(DRAMC_NAO, DRAMC_NAO_LEN);
	if (!_ddrphy_ao_base || !_dramc_ao_base || !_dramc_nao_base) {
		if (!_ddrphy_ao_base)
			pr_crit("[Dramc DRef] ioremap error for ddrphy_ao_base!\n");
		else if (!_dramc_ao_base)
			pr_crit("[Dramc DRef] ioremap error for dramc_ao_base!\n");
		else if (!_dramc_nao_base)
			pr_crit("[Dramc DRef] ioremap error for dramc_nao_base!\n");
		return 0;
	}
	return 1;
}

int dramc_iounmap(void)
{
	iounmap(_ddrphy_ao_base);
	iounmap(_dramc_ao_base);
	iounmap(_dramc_nao_base);
	return 1;
}

int dramc_remapping_chipid_to_index(u16 u16chipid)
{
	switch (u16chipid) {
	case CHIP_ID_MOKONA:
		return 0;
	case CHIP_ID_MIFFY:
		return 1;

	case CHIP_ID_MACHLI:
	case CHIP_ID_MANKS:
	case CHIP_ID_UNKNOWN:
	default:
		return 0xFF;
	}
}

static int Dramc_Cal_IC_Temperature_Avg(void)
{
	int t = 0;

	if (u16DramcChipID == CHIP_ID_MOKONA) {
		t = (get_cpu_temperature_bysar()
			+ get_cpu_temperature_bysar()
			+ get_cpu_temperature_bysar())/3;

		if (u32DREFLog == 1) {
			pr_crit("[Dramc_DRef] avg:%d cpu:%d gpu:%d pm:%d"
			, t
			, get_cpu_temperature_bysar()
			, get_cpu_temperature_bysar()
			, get_cpu_temperature_bysar());
			dramc_dynamic_ref_debug();
			pr_crit("\n\n");
		}
	} else if (u16DramcChipID == CHIP_ID_MIFFY) {
		t = (Dramc_Thermal_CPU_Tsensor()
			+ Dramc_Thermal_XC_Tsensor()
			+ Dramc_Thermal_HDMI_Tsensor())/3;

		if (u32DREFLog == 1) {
			pr_crit("[Dramc_DRef] avg:%d cpu:%d gpu:%d pm:%d"
			, t
			, Dramc_Thermal_CPU_Tsensor()
			, Dramc_Thermal_XC_Tsensor()
			, Dramc_Thermal_HDMI_Tsensor());
			dramc_dynamic_ref_debug();
			pr_crit("\n\n");
		}
	}

	return t;
}

int Dramc_Set_Ddr_Refresh_Rate_Setting_Compare(int interval)
{
		switch (interval) {
		case DDR_REFRESH_RATE_INTERVAL_1950_NS:
		{
			if (DRAMC_READ_REG(REG_DDR_REF_RATE) == DDR_REF_RATE_1950)
				return 1;
		}
		break;

		case DDR_REFRESH_RATE_INTERVAL_3900_NS:
		{
			if (DRAMC_READ_REG(REG_DDR_REF_RATE) == DDR_REF_RATE_3900)
				return 1;
		}
		break;

		case DDR_REFRESH_RATE_INTERVAL_7900_NS:
		{
			if (DRAMC_READ_REG(REG_DDR_REF_RATE) == DDR_REF_RATE_7900)
				return 1;
		}
		break;


		default:
			return 0;
		break;
	}

return 0;
}

void Dramc_Set_Ddr_Refresh_Rate(int interval)
{
	int Ch_Cnt = 0;

	switch (interval) {
	default:
	case DDR_REFRESH_RATE_INTERVAL_1950_NS:
	{
		for (Ch_Cnt = 0; Ch_Cnt < DramcChannelMax; Ch_Cnt++) {
			//Mokona XTAL = 24MHZ,
			//need modify REFCNT_FR_CLK_* for DDR4 KGD (tREF=1.9us)
			if (mtk_dramc_get_freq(Ch_Cnt) < 3200) { //1.95us is MPC only
				DRAMC_WRITE_REG(DDR_REF_RATE_1950,
				((Ch_Cnt*CH_OFFSET)+REG_DDR_REF_RATE));
			} else {
				DRAMC_WRITE_REG(DDR_REF_RATE_3900,
				((Ch_Cnt*CH_OFFSET)+REG_DDR_REF_RATE));
			}
		}
	}
	break;

	case DDR_REFRESH_RATE_INTERVAL_3900_NS:
	{
		for (Ch_Cnt = 0; Ch_Cnt < DramcChannelMax; Ch_Cnt++) {
			//Mokona XTAL = 24MHZ,
			//need modify REFCNT_FR_CLK_* for DDR4 KGD (tREF=3.9us)
			DRAMC_WRITE_REG(DDR_REF_RATE_3900, ((Ch_Cnt*CH_OFFSET)+REG_DDR_REF_RATE));
		}
	}
	break;

	case DDR_REFRESH_RATE_INTERVAL_7900_NS:
	{
		for (Ch_Cnt = 0; Ch_Cnt < DramcChannelMax; Ch_Cnt++) {
			//Mokona XTAL = 24MHZ,
			//need modify REFCNT_FR_CLK_* for DDR4 KGD (tREF=7.8us)
			DRAMC_WRITE_REG(DDR_REF_RATE_7900, ((Ch_Cnt*CH_OFFSET)+REG_DDR_REF_RATE));
			//DRAMC_WRITE_REG(0x02EC0176,((Ch_Cnt*0x2000)+0x02F0));    //15.6
		}
	}
	break;
	}
}

static int Dramc_IsDDR4(void)
{
	int IsDDR4 = 0;

	IsDDR4 = ((mtk_dramc_get_ddr_type() == 4) ? 1 : 0);

	return IsDDR4;
}

static void Dramc_Dynamic_Refresh_Rate_run(void)
{
	int temp = Dramc_Cal_IC_Temperature_Avg();

	if (temp <= Refresh_Rate_Decrease_Low_Bound(iChipIDindex)) {
		//set 7.8us
		if (!Dramc_Set_Ddr_Refresh_Rate_Setting_Compare(DDR_REFRESH_RATE_INTERVAL_7900_NS))
			Dramc_Set_Ddr_Refresh_Rate(DDR_REFRESH_RATE_INTERVAL_7900_NS);

	} else if (temp >= Refresh_Rate_Increase_High_Bound(iChipIDindex)) {
		//set 1.95us
		if (!Dramc_Set_Ddr_Refresh_Rate_Setting_Compare(DDR_REFRESH_RATE_INTERVAL_1950_NS))
			Dramc_Set_Ddr_Refresh_Rate(DDR_REFRESH_RATE_INTERVAL_1950_NS);

	} else if ((refresh_temp_pre > Refresh_Rate_Decrease_High_Bound(iChipIDindex))
				&& (temp <= Refresh_Rate_Decrease_High_Bound(iChipIDindex))) {
		//set 3.90
		if (!Dramc_Set_Ddr_Refresh_Rate_Setting_Compare(DDR_REFRESH_RATE_INTERVAL_3900_NS))
			Dramc_Set_Ddr_Refresh_Rate(DDR_REFRESH_RATE_INTERVAL_3900_NS);

	} else if ((refresh_temp_pre < Refresh_Rate_Increase_Low_Bound(iChipIDindex))
				&& (temp >= Refresh_Rate_Increase_Low_Bound(iChipIDindex)))	{
		//set 3.90
		if (!Dramc_Set_Ddr_Refresh_Rate_Setting_Compare(DDR_REFRESH_RATE_INTERVAL_3900_NS))
			Dramc_Set_Ddr_Refresh_Rate(DDR_REFRESH_RATE_INTERVAL_3900_NS);

	} else {
		// keep setting
	}

	refresh_temp_pre = temp;
}

static int Dramc_Dynamic_Refresh_Rate_Handler(void)
{
	if (Dramc_IsDDR4()) { // Check DDR4 here
		Dramc_Dynamic_Refresh_Rate_run();
	} else {
		return 0;
	}

	return 1;
}

static int Dynamic_Refresh_Rate_Task(__maybe_unused void *punused)
{
	if (pDramcRes != NULL) {
		while (pDramcRes->bDRAMCTaskProcFlag) {
			if ((u32DREFEn == 1) && (u16DramcChipID == CHIP_ID_MOKONA))
				Dramc_Dynamic_Refresh_Rate_Handler();

			//Do this process per 1 sec
			msleep(DRAMC_POLLING_INTERVAL);
		}
	}
	return 0;
}

static bool Dynamic_Refresh_Rate_Task_Init(void)
{
	bool bCreateSuccess = 1;

	if (pDramcRes->sDRAMCPollingTaskID < 0) {
		stDramcThread = kthread_create(Dynamic_Refresh_Rate_Task, pDramcRes, "Dramc task");

			if (IS_ERR(stDramcThread)) {
				bCreateSuccess = 0;
				stDramcThread = NULL;
				pr_err("Create DRAMC Thread Failed\r\n");
			} else {
				wake_up_process(stDramcThread);
				pDramcRes->sDRAMCPollingTaskID = stDramcThread->tgid;
				pDramcRes->bDRAMCTaskProcFlag = 1;
				pr_crit("Create DRAMC Thread OK\r\n");
			}
		}

		if (pDramcRes->sDRAMCPollingTaskID < 0) {
			pr_err("Create DRAMC Thread failed!!\n");
			bCreateSuccess = 0;
		}

	return bCreateSuccess;
}

void dramc_dynamic_ref_debug(void)
{
	if (Dramc_Set_Ddr_Refresh_Rate_Setting_Compare(DDR_REFRESH_RATE_INTERVAL_7900_NS))
		pr_crit("7800ns\n");
	else if (Dramc_Set_Ddr_Refresh_Rate_Setting_Compare(DDR_REFRESH_RATE_INTERVAL_3900_NS))
		pr_crit("3900ns\n");
	else if (Dramc_Set_Ddr_Refresh_Rate_Setting_Compare(DDR_REFRESH_RATE_INTERVAL_1950_NS))
		pr_crit("1950ns\n");
	else
		pr_crit("Wrong refresh rate setting\n");

}

static ssize_t dramc_dynamic_ref_show(struct device_driver *drv, char *buf)
{
	u32DREFLog = 0;

	pr_crit("\n[Dramc DRef] dynamic enable : %d\n", u32DREFEn);
	dramc_dynamic_ref_debug();

	pm_top_get_chip_id();
	pr_crit("\n");
	return 0;
}

static ssize_t dramc_dynamic_ref_store(struct device_driver *drv, const char *buf, size_t count)
{
	size_t count_sscan;

	count_sscan = sscanf(buf, "%d %d %d", &u32DREFEn,
						&u32DREFSet, &u32DREFLog);

	if (count_sscan == 3) {
		if (u32DREFEn == 0) {
			if (u32DREFSet == 1)
				Dramc_Set_Ddr_Refresh_Rate(DDR_REFRESH_RATE_INTERVAL_1950_NS);
			else if (u32DREFSet == 2)
				Dramc_Set_Ddr_Refresh_Rate(DDR_REFRESH_RATE_INTERVAL_3900_NS);
			else if (u32DREFSet == 3)
				Dramc_Set_Ddr_Refresh_Rate(DDR_REFRESH_RATE_INTERVAL_7900_NS);
		}

	} else {
		pr_crit("[Dramc DRef] Wrong Argument!!!\n");
	}

	return count;
}

static ssize_t dramc_dynamic_boundset_show(struct device_driver *drv, char *buf)
{
	int chipindex;

	pr_crit("[Dramc DRef] Dynamic refresh rate bound\n");

	for (chipindex = 0; chipindex < DRAMC_DYNAMIC_REF_SUP_CHIP_NUM; chipindex++) {
		pr_crit("[Dramc DRef] %d >> I_low:%d I_high:%d D_low:%d D_high:%d\n",
			chipindex,
			u8I_Low_Bound[chipindex],
			u8I_High_Bound[chipindex],
			u8D_Low_Bound[chipindex], u8D_High_Bound[chipindex]);
	}

	pr_crit("\n");
	return 0;
}

static ssize_t dramc_dynamic_boundset_store(struct device_driver *drv
							, const char *buf, size_t count)
{
	size_t count_sscan;

	count_sscan = sscanf(buf, "%d %d %d %d %d", &u32CHIPIndex,
						&u32ILB, &u32IHB, &u32DLB, &u32DHB);

	if (count_sscan == 5) {
		if (u32CHIPIndex < DRAMC_DYNAMIC_REF_SUP_CHIP_NUM) {
			u8I_Low_Bound[u32CHIPIndex] = u32ILB;
			u8I_High_Bound[u32CHIPIndex] = u32IHB;
			u8D_Low_Bound[u32CHIPIndex] = u32DLB;
			u8D_High_Bound[u32CHIPIndex] = u32DHB;

			pr_crit("[Dramc DRef] %d >> I_low:%d I_high:%d D_low:%d D_high:%d\n",
			u32CHIPIndex,
			u8I_Low_Bound[u32CHIPIndex],
			u8I_High_Bound[u32CHIPIndex],
			u8D_Low_Bound[u32CHIPIndex], u8D_High_Bound[u32CHIPIndex]);
		}

	} else {
		pr_crit("[Dramc DRef] Wrong Argument!!!\n");
	}

	return count;
}
#endif

#ifdef ENABLE_SWITCH_SHUFFLE_DFS
void __iomem *_dfs_ddrphy_ao_bs;
void __iomem *_dfs_dramc_ao_bs;
void __iomem *_dfs_dramc_nao_bs;
#define DFS_DDRPHY_AO		0x1C380000
#define DFS_DRAMC_AO		0x1C38e000
#define DFS_DRAMC_NAO		0x1C3A0000
#define DFS_DDRPHY_AO_LEN	0x10000
#define DFS_DRAMC_AO_LEN	0x10000
#define DFS_DRAMC_NAO_LEN	0x6000
#define DFS_DDRPHY_RD_REG(ofst)		readl_relaxed(_dfs_ddrphy_ao_bs + (ofst))
#define DFS_DDRPHY_WR_REG(val, ofst)	writel_relaxed((val), _dfs_ddrphy_ao_bs + (ofst))
#define DFS_DRAMC_RD_REG(ofst)		readl_relaxed(_dfs_dramc_ao_bs + (ofst))
#define DFS_DRAMC_WR_REG(val, ofst)	writel_relaxed((val), _dfs_dramc_ao_bs + (ofst))
#define DFS_DRAMC_NAO_RD_REG(ofst)		readl_relaxed(_dfs_dramc_nao_bs + (ofst))
#define DFS_DRAMC_NAO_WR_REG(val, ofst)	writel_relaxed((val), _dfs_dramc_nao_bs + (ofst))
#define DRAMC_CHANNEL_MAX	6
#define DRAMC_CHANNEL_F		5
#define DRAMC_OFFSET_DUMMY	0x000C

#define SHUFFLE_DFS_4266	4266
#define SHUFFLE_DFS_3200	3200
static unsigned int current_shuffle_dfs_freq = SHUFFLE_DFS_4266;
static int _mtk_dramc_ioremap_shuffle_dfs(void)
{
	_dfs_ddrphy_ao_bs = ioremap(DFS_DDRPHY_AO, DFS_DDRPHY_AO_LEN);
	_dfs_dramc_ao_bs = ioremap(DFS_DRAMC_AO, DFS_DRAMC_AO_LEN);
	_dfs_dramc_nao_bs = ioremap(DFS_DRAMC_NAO, DFS_DRAMC_NAO_LEN);
	if (!_dfs_ddrphy_ao_bs || !_dfs_dramc_ao_bs || !_dfs_dramc_nao_bs) {
		if (!_dfs_ddrphy_ao_bs)
			pr_crit("[%s] ioremap error for ddrphy_ao_base!\n", __func__);
		else if (!_dfs_dramc_ao_bs)
			pr_crit("[%s] ioremap error for dramc_ao_base!\n", __func__);
		else if (!_dfs_dramc_nao_bs)
			pr_crit("[%s] ioremap error for dramc_nao_base!\n", __func__);
		return 0;
	}
	return 1;
}

int _mtk_dramc_shuffle_dfs_iounmap(void)
{
	iounmap(_dfs_ddrphy_ao_bs);
	iounmap(_dfs_dramc_ao_bs);
	iounmap(_dfs_dramc_nao_bs);
	return 1;
}

static unsigned int _mtk_dramc_trans_addr_dramc(unsigned int u32addr, unsigned char u8channel)
{
	unsigned int u32addr_rslt_chx;

	//####################################
	// For channel base
	//####################################
	u32addr_rslt_chx = u32addr;
	if (u8channel >= DRAMC_CHANNEL_MAX)
		return u32addr_rslt_chx;
	//dramc addr
	u32addr_rslt_chx += u8channel*0x2000;
	return u32addr_rslt_chx;
}
static bool _mtk_dramc_set_shuffle_dfs_freq(unsigned int u32shuffledfsfreq)
{
	unsigned int u32addr_final = 0, u32addr_offset = DRAMC_OFFSET_DUMMY;
	unsigned char u8channel = DRAMC_CHANNEL_F;

	u32addr_final = _mtk_dramc_trans_addr_dramc(u32addr_offset, u8channel);
	DFS_DRAMC_WR_REG(u32shuffledfsfreq, u32addr_final);
	return 1;
}
#endif

void mtk_dramc_dummy_reg_ioremap(void)
{
	mailbox_dumreg_base = ioremap(DUMMY_REG_BASE, DUMMY_REG_LEN);

	if (!mailbox_dumreg_base)
		pr_err("ioremap error for mailbox_dumreg_base!\n");
}

unsigned int mtk_dramc_get_support_channel_num(void)
{
	static int support_channel_num;

	support_channel_num = MAILBOX_READ_REG(DUMMY_REG_38);
	return (support_channel_num>>MG_NUMBER_8)&0xF;
}
EXPORT_SYMBOL(mtk_dramc_get_support_channel_num);

unsigned int mtk_dramc_get_ddr_type(void)
{
	static int DDRTypeTemp;

	DDRTypeTemp = MAILBOX_READ_REG(DUMMY_REG_38);
	return DDRTypeTemp&0xF;
}
EXPORT_SYMBOL(mtk_dramc_get_ddr_type);


#ifdef ENABLE_SWITCH_SHUFFLE_DFS
unsigned int mtk_dramc_get_dfs_freq(void)
{
	return current_shuffle_dfs_freq;
}
EXPORT_SYMBOL(mtk_dramc_get_dfs_freq);

bool mtk_dramc_set_dfs_freq(unsigned int shuffle_dfs_freq)
{
	bool bSetDfsSuccess = 0;

	if ((shuffle_dfs_freq != SHUFFLE_DFS_4266) && (shuffle_dfs_freq != SHUFFLE_DFS_3200))
		return bSetDfsSuccess;

	_mtk_dramc_set_shuffle_dfs_freq(shuffle_dfs_freq);
	current_shuffle_dfs_freq = shuffle_dfs_freq;
	bSetDfsSuccess = 1;
	return bSetDfsSuccess;
}
EXPORT_SYMBOL(mtk_dramc_set_dfs_freq);
#endif

unsigned int mtk_dramc_get_channel_size(int channel)
{
	static int size_value;

	if (channel >= mtk_dramc_get_support_channel_num()) {
		pr_err("not support DDR channel num\n");
		return 0;
	}

	switch (channel) {
	case DRAM_CH_NUM_CH0:
		size_value = MAILBOX_READB_REG(DUMMY_REG_A0) & 0xf;
	break;

	case DRAM_CH_NUM_CH1:
		size_value = MAILBOX_READB_REG(DUMMY_REG_A1) & 0xf;
	break;

	case DRAM_CH_NUM_CH2:
		size_value = MAILBOX_READB_REG(DUMMY_REG_A4) & 0xf;
	break;

	case DRAM_CH_NUM_CH3:
		size_value = MAILBOX_READB_REG(DUMMY_REG_A5) & 0xf;
	break;

	case DRAM_CH_NUM_CH4:
		size_value = MAILBOX_READB_REG(DUMMY_REG_A8) & 0xf;
	break;

	case DRAM_CH_NUM_CH5:
		size_value = MAILBOX_READB_REG(DUMMY_REG_A9) & 0xf;
	break;

	default:
		pr_err("Not support\n");
	break;

	}

	return size_value;
}
EXPORT_SYMBOL(mtk_dramc_get_channel_size);

unsigned int mtk_dramc_get_channel_total_size(void)
{
	static int channel_num, ddr_total_size;

	for (channel_num = 0; channel_num < mtk_dramc_get_support_channel_num(); channel_num++)
		ddr_total_size += mtk_dramc_get_channel_size(channel_num);

	return ddr_total_size;
}
EXPORT_SYMBOL(mtk_dramc_get_channel_total_size);

unsigned int mtk_dramc_get_ddr_asymsize(void)
{
	//void __iomem *dm_reg_asymbound;
	signed int ddr_total_size = 0;
	signed int channel_idx;
	signed int phychannel_num = mtk_dramc_get_support_channel_num();
	unsigned int ddr_channel_size[DDRphyChannelMax];
	unsigned int ddr_min_size = 0xff;
	unsigned int ddr_asymbound;
	unsigned int ddr_asymsize = 0;

	if (phychannel_num > DDRphyChannelMax) {
		pr_err("not support DDR channel num\n");
		return 0xff;
	}
	//ddr_total_size = mtk_dramc_get_channel_total_size();
	for (channel_idx = 0; channel_idx < phychannel_num; channel_idx++) {
		ddr_channel_size[channel_idx] = mtk_dramc_get_channel_size(channel_idx);
		ddr_total_size += ddr_channel_size[channel_idx];
		ddr_min_size = (ddr_min_size < ddr_channel_size[channel_idx]) ?
			ddr_min_size : ddr_channel_size[channel_idx];
	}
	for (channel_idx = 0; channel_idx < phychannel_num; channel_idx++)
		ddr_asymsize = ddr_asymsize + (ddr_channel_size[channel_idx] - ddr_min_size);
	ddr_asymbound = ddr_total_size - ddr_asymsize;
	pr_crit("[MTK_DRAMC] ddr_asymbound = 0x%x\n", ddr_asymbound);
	pr_crit("[MTK_DRAMC] ddr_asymsize = 0x%x\n", ddr_asymsize);
	if (ddr_asymsize == 0)
		return 0;
	else
		return  ddr_asymbound;

}
EXPORT_SYMBOL(mtk_dramc_get_ddr_asymsize);

unsigned int mtk_dramc_get_ddrpkg(void)
{
	int ddrpkg;

	ddrpkg = MAILBOX_READB_REG(DUMMY_REG_9D) & 0xf;

	return ddrpkg;
}
EXPORT_SYMBOL(mtk_dramc_get_ddrpkg);

unsigned int mtk_dramc_get_freq(int phychannel)
{
	int PhyChannelFreq;

	if (phychannel >= mtk_dramc_get_support_channel_num()) {
		pr_err("not support DDR channel num\n");
		return 0;
	}
	if (mtk_dramc_get_support_channel_num() > 4)
		PhyChannelFreq = MAILBOX_READW_REG(DUMMY_REG_AC) & 0xffff;
	else {
		if (phychannel < 2)
			PhyChannelFreq = MAILBOX_READW_REG(DUMMY_REG_AC) & 0xffff;
		else
			PhyChannelFreq = MAILBOX_READW_REG(DUMMY_REG_B0) & 0xffff;
		}
	//pr_crit("[MTK_DRAMC]DDRPHY CH%d freq = %d\n", phychannel, PhyChannelFreq);
	return PhyChannelFreq;
}
EXPORT_SYMBOL(mtk_dramc_get_freq);

#define DFS_CMD_SIZE 10
static ssize_t dramc_dfstest_show(struct device_driver *drv, char *buf)
{
	int cx = 0;

	if (current_shuffle_dfs_freq ==	SHUFFLE_DFS_4266)
		cx  = snprintf(buf, DFS_CMD_SIZE, "%s\n", "4266");
	else if (current_shuffle_dfs_freq == SHUFFLE_DFS_3200)
		cx  = snprintf(buf, DFS_CMD_SIZE, "%s\n", "3200");
	else
		pr_crit("current freq error: %d!\n", current_shuffle_dfs_freq);

	if (!(cx > 0 && cx < DFS_CMD_SIZE))
		pr_crit("format is invalid!\n");

	return cx;
}

static ssize_t dramc_dfstest_store(struct device_driver *drv, const char *buf, size_t count)
{
	int cx;
	char dfsParabuf[DFS_CMD_SIZE] = "0000";

	cx  = snprintf(dfsParabuf, DFS_CMD_SIZE, "%s", buf);
	if (cx > 0 && cx < DFS_CMD_SIZE) {
		if (!strncmp(dfsParabuf, "4266", 4))
			mtk_dramc_set_dfs_freq(4266);
		else if (!strncmp(dfsParabuf, "3200", 4))
			mtk_dramc_set_dfs_freq(3200);
		else
			pr_crit("%s is invalid!\n", dfsParabuf);
	} else {
		pr_crit("format is invalid!\n");
	}
	return count;
}

//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx (SHMOO_XXX)
#define RUN_TIME_SHMOO_VERSION "[RUN_TIME_SHMOO] Release Version - 1.0 (20221206)"
#define RUN_TIME_SHMOO_RX
#define RUN_TIME_SHMOO_TX

void __iomem *_shm_ddrphy_ao_bs;
void __iomem *_shm_dramc_ao_bs;
void __iomem *_shm_dramc_nao_bs;

#define SHMOO_CPU_BASE	0x1C000000
#define TRANS_DDRPHY_AO_BASE   0x380000
#define TRANS_DRAMC_AO_BASE   0x38E000
#define TRANS_DRAMC_NAO_BASE   0x3A0000

#define SHMOO_DDRPHY_AO   (SHMOO_CPU_BASE+TRANS_DDRPHY_AO_BASE)
#define SHMOO_DRAMC_AO   (SHMOO_CPU_BASE+TRANS_DRAMC_AO_BASE)
#define SHMOO_DRAMC_NAO   (SHMOO_CPU_BASE+TRANS_DRAMC_NAO_BASE)
#define SHMOO_IOREMAP_SIZE   0x10000

#define SHM_DDRPHY_RD(offset)   readl_relaxed(_shm_ddrphy_ao_bs + (offset))
#define SHM_DDRPHY_WR(val, offset)   writel_relaxed((val), _shm_ddrphy_ao_bs + (offset))
#define SHM_DRAMC_RD(offset)   readl_relaxed(_shm_dramc_ao_bs + (offset))
#define SHM_DRAMC_WR(val, offset)   writel_relaxed((val), _shm_dramc_ao_bs + (offset))
#define SHM_DRAMC_NAO_RD(offset)   readl_relaxed(_shm_dramc_nao_bs + (offset))
#define SHM_DRAMC_NAO_WR(val, offset)   writel_relaxed((val), _shm_dramc_nao_bs + (offset))

#define CHANNEL_MAX   ((IsMachli() == true) ? 6 : 4)
#define RANK_MAX   2
#define BYTE_MAX   2

#define WIN_MAX   0xffffffff
#define CRB_MAX   0xff

#define SHIFT_L   0 //decrease rx phase
#define SHIFT_R   1 //increase rx phase
#define SHIFT_D   2 //decrease rx vref
#define SHIFT_U   3 //increase rx vref
#define SHIFT_L_RX_DQS   4 //decrease rx phase DQS
#define SHIFT_R_RX_DQS   5 //increase rx phase DQS

//================================================
#define TA2_BROADCAST_MODE0   ((IsMachli() == true) ? DISP_3_EMI_BORC_MODE0 : DISP_2_EMI_BORC_MODE0)
#define TA2_BROADCAST_MODE1   ((IsMachli() == true) ? DISP_3_EMI_BORC_MODE1 : DISP_2_EMI_BORC_MODE1)
#define TA2_BROADCAST_MODE0_M6L   DISP_2_EMI_BORC_MODE0
#define TA2_BROADCAST_MODE1_M6L   DISP_2_EMI_BORC_MODE1
//================================================

#define TRANS_ADDR_MSK   0x0000ffff
#define TRANS_AO_OFST   0x2000
#define TRANS_NAO_OFST   0x1000
#define TRANS_DDRPHY_CHMID   3

#define SHMOO_DELAY_1MS_MAX   1001
#define SHMOO_DELAY_1MS_MIN   999
#define SHMOO_DELAY_10MS_MAX   10001
#define SHMOO_DELAY_10MS_MIN   9999
#define SHMOO_MSLP_DELAY_MS   1000

//################################################
// SHMOO RG Offset: Shuffle/Channel/Rank/Byte
//################################################
//================================================
//M6
#define DRAMC_AO_SHU_OFST   0x700
#define DRAMC_AO_RK_OFST   0x200
#define DDRPHY_AO_SHU_OFST   0x700
#define DDRPHY_AO_RK_OFST   0x80
#define DDRPHY_AO_BT_OFST   0x180
#define DDRPHY_AO_RK_OFST_2   0x80
#define DDRPHY_AO_BT_OFST_2   0x180
//M6L
#define DRAMC_AO_SHU_OFST_M6L   0x700
#define DRAMC_AO_RK_OFST_M6L    0x100
#define DDRPHY_AO_SHU_OFST_M6L   0x700
#define DDRPHY_AO_RK_OFST_M6L   0x100
#define DDRPHY_AO_BT_OFST_M6L   0x50
#define DDRPHY_AO_RK_OFST_2_M6L 0x200
#define DDRPHY_AO_BT_OFST_2_M6L 0x80
//================================================

//################################################
// SHMOO TA2
//################################################
//================================================
//M6
// (((SHMOO_DRAMC_AO)))-TA2
#define DRAMC_REG_TEST2_A0_Ofst   0x0100 //DRAMC_REG_TEST2_0_Ofst
#define DRAMC_REG_RK_TEST2_A1_Ofst   0x0500 //DRAMC_REG_TEST2_1_Ofst
#define DRAMC_REG_TEST2_A2_Ofst   0x0104 //DRAMC_REG_TEST2_2_Ofst
#define DRAMC_REG_TEST2_A3_Ofst   0x0108 //DRAMC_REG_TEST2_3_Ofst
#define DRAMC_REG_TEST2_A4_Ofst   0x010C //DRAMC_REG_TEST2_4_Ofst
// (((SHMOO_DRAMC_NAO)))-TA2
#define DRAMC_REG_TESTRPT_Ofst   0x0120 //DRAMC_REG_TESTRPT_Ofst
#define DRAMC_REG_CMP_ERR_Ofst   0x0124 //DRAMC_REG_CMP_ERR_Ofst
//================================================
//M6L
// (((SHMOO_DRAMC_AO)))-TA2
#define DRAMC_REG_TEST2_A0_Ofst_M6L   0x0090 //DRAMC_REG_TEST2_0_Ofst
#define DRAMC_REG_RK_TEST2_A1_Ofst_M6L   0x0094 //DRAMC_REG_TEST2_1_Ofst
#define DRAMC_REG_TEST2_A2_Ofst_M6L   0x0098 //DRAMC_REG_TEST2_2_Ofst
#define DRAMC_REG_TEST2_A3_Ofst_M6L   0x009C //DRAMC_REG_TEST2_3_Ofst
#define DRAMC_REG_TEST2_A4_Ofst_M6L   0x00A0 //DRAMC_REG_TEST2_4_Ofst
// (((SHMOO_DRAMC_NAO)))-TA2
#define DRAMC_REG_TESTRPT_Ofst_M6L   0x0120 //DRAMC_REG_TESTRPT_Ofst
#define DRAMC_REG_CMP_ERR_Ofst_M6L   0x0124 //DRAMC_REG_CMP_ERR_Ofst
//================================================

//################################################
// RUN_TIME_SHMOO_RX
//################################################
//================================================
//M6
// (((Ddrphy_AO)))-RX Vref
#define DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst   0x0790 //DDRPHY_SHU1_B0_DQ5_Ofst
// (((Ddrphy_AO)))-RX Phase DQ
#define DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst   0x0774 //DDRPHY_SHU1_R0_B0_DQ2_Ofst (8 bit)
#define DDRPHY_REG_SHU_R0_B0_RXDLY1_Ofst   0x0778 //DDRPHY_SHU1_R0_B0_DQ3_Ofst (8 bit)
#define DDRPHY_REG_SHU_R0_B0_RXDLY2_Ofst   0x077C //DDRPHY_SHU1_R0_B0_DQ4_Ofst (8 bit)
#define DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst   0x0780 //DDRPHY_SHU1_R0_B0_DQ5_Ofst (8 bit)
// (((Ddrphy_AO)))-RX Phase DQS
#define DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst   0x0788 //DDRPHY_SHU1_R0_B0_DQ6_Ofst
// (((Ddrphy_AO)))-RX Phase DQS CTRL
#define DDRPHY_REG_B0_RXDVS0_Ofst   0x0164 //DDRPHY_B0_RXDVS0_Ofst
#define DDRPHY_REG_RK_B0_RXDVS2_Ofst   0x0068 //DDRPHY_R0_B0_RXDVS2_Ofst
//================================================
//M6L
// (((Ddrphy_AO)))-RX Vref
#define DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst_M6L   0x0C14 //DDRPHY_SHU1_B0_DQ5_Ofst
// (((Ddrphy_AO)))-RX Phase DQ
#define DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst_M6L   0x0E08 //DDRPHY_SHU1_R0_B0_DQ2_Ofst (6 bit)
#define DDRPHY_REG_SHU_R0_B0_RXDLY1_Ofst_M6L   0x0E0C //DDRPHY_SHU1_R0_B0_DQ3_Ofst (6 bit)
#define DDRPHY_REG_SHU_R0_B0_RXDLY2_Ofst_M6L   0x0E10 //DDRPHY_SHU1_R0_B0_DQ4_Ofst (6 bit)
#define DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst_M6L   0x0E14 //DDRPHY_SHU1_R0_B0_DQ5_Ofst (6 bit)
// (((Ddrphy_AO)))-RX Phase DQS
#define DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst_M6L   0x0E18 //DDRPHY_SHU1_R0_B0_DQ6_Ofst
// (((Ddrphy_AO)))-RX Phase DQS CTRL
#define DDRPHY_REG_B0_RXDVS0_Ofst_M6L   0x05F0 //DDRPHY_B0_RXDVS0_Ofst
#define DDRPHY_REG_RK_B0_RXDVS2_Ofst_M6L   0x0608 //DDRPHY_R0_B0_RXDVS2_Ofst
//================================================

//################################################
// RUN_TIME_SHMOO_TX
//################################################
//================================================
//M6
// (((SHMOO_DRAMC_AO)))-TX Vref
#define DRAMC_REG_CKECTRL_Ofst   0x0204 //DRAMC_REG_CKECTRL_Ofst
#define DRAMC_REG_SWCMD_CTRL0_Ofst   0x0128 //DRAMC_REG_MRS_Ofst
#define DRAMC_REG_SWCMD_EN_Ofst   0x0124 //DRAMC_REG_SPCMD_Ofst
// (((SHMOO_DRAMC_NAO)))-TX Vref
#define DRAMC_REG_SPCMDRESP_Ofst   0x0088 //DRAMC_REG_SPCMDRESP_Ofst
#define DRAMC_REG_MRR_STATUS_Ofst   0x008C //DRAMC_REG_MRR_STATUS
// (((SHMOO_DDRPHY_AO)))-TX Phase CMD/CS
#define DDRPHY_REG_SHU_R0_CA_CMD0_Ofst   0x0A94 //DDRPHY_SHU1_R0_CA_CMD9_Ofst
// (((SHMOO_DRAMC_AO)))-TX Phase DQM/DQ ...
#define DRAMC_REG_SHURK_PI_Ofst   0x1224 //DRAMC_REG_SHU1RK0_PI_Ofst
// (((SHMOO_DDRPHY_AO)))-TX Phase DQS
#define DDRPHY_REG_SHU_R0_B0_DQ0_Ofst   0x0794 //DDRPHY_SHU1_R0_B0_DQ7_Ofst
// (((SHMOO_DDRPHY_AO)))-TX Phase CTRL
#define DDRPHY_REG_MISC_CG_CTRL5_Ofst   0x0500 //DDRPHY_MISC_CG_CTRL5_Ofst
// (((SHMOO_DRAMC_AO)))-TX Phase CTRL
#define DRAMC_REG_DQSOSCR_Ofst   0x0248 //DRAMC_REG_DQSOSCR_Ofst
// (((SHMOO_DRAMC_NAO)))-TX Phase CTRL
#define DRAMC_REG_TCMDO1LAT_Ofst   0x00C0 //DRAMC_REG_TCMDO1LAT_Ofst
#define DRAMC_REG_RK0_PI_DQ_CAL_Ofst   0x0660
#define DRAMC_REG_RK0_PI_DQM_CAL_Ofst   0x0668
// (((SHMOO_DDRPHY_AO)))-Shuffle Level [16]
#define DDRPHY_REG_MISC_RG_DFS_CTRL_Ofst   0x0538
#define SHU_LEVEL   BIT(16)
//================================================
//M6L
// (((SHMOO_DRAMC_AO)))-TX Vref
#define DRAMC_REG_CKECTRL_Ofst_M6L   0x0024 //DRAMC_REG_CKECTRL_Ofst
#define DRAMC_REG_SWCMD_CTRL0_Ofst_M6L   0x005C //DRAMC_REG_MRS_Ofst
#define DRAMC_REG_SWCMD_EN_Ofst_M6L   0x0060 //DRAMC_REG_SPCMD_Ofst
// (((SHMOO_DRAMC_NAO)))-TX Vref
#define DRAMC_REG_SPCMDRESP_Ofst_M6L   0x0088 //DRAMC_REG_SPCMDRESP_Ofst
#define DRAMC_REG_MRR_STATUS_Ofst_M6L   0x008C //DRAMC_REG_MRR_STATUS
// (((SHMOO_DDRPHY_AO)))-TX Phase CMD/CS
#define DDRPHY_REG_SHU_R0_CA_CMD0_Ofst_M6L   0x0EC4 //DDRPHY_SHU1_R0_CA_CMD9_Ofst
// (((SHMOO_DDRPHY_AO)))-TX Phase DQM/DQ ...
#define DRAMC_REG_SHURK_PI_Ofst_M6L   0x0A0C //DRAMC_REG_SHU1RK0_PI_Ofst
// (((SHMOO_DDRPHY_AO)))-TX Phase DQS
#define DDRPHY_REG_SHU_R0_B0_DQ0_Ofst_M6L   0x0E1C //DDRPHY_SHU1_R0_B0_DQ7_Ofst
// (((SHMOO_DDRPHY_AO)))-TX Phase CTRL
#define DDRPHY_REG_MISC_CG_CTRL5_Ofst_M6L   0x0298 //DDRPHY_MISC_CG_CTRL5_Ofst
// (((SHMOO_DRAMC_AO)))-TX Phase CTRL
#define DRAMC_REG_DQSOSCR_Ofst_M6L   0x00C8 //DRAMC_REG_DQSOSCR_Ofst
// (((SHMOO_DRAMC_NAO)))-TX Phase CTRL
#define DRAMC_REG_TCMDO1LAT_Ofst_M6L   0x00C0 //DRAMC_REG_TCMDO1LAT_Ofst
#define DRAMC_REG_RK0_PI_DQ_CAL_Ofst_M6L   0x0660
#define DRAMC_REG_RK0_PI_DQM_CAL_Ofst_M6L   0x0668

//================================================

//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx (SHMOO_COMN)
static bool IsMachli(void)
{
	return (u16DramcChipID == CHIP_ID_MACHLI) ? true : false;
}

static bool IsManks(void)
{
	return (u16DramcChipID == CHIP_ID_MANKS) ? true : false;
}

static bool IsShuffleRG1(void)
{
	u32 u32addr_shuLvl = 0, u32val_shuLvl = 0;

	if (IsMachli() == true) {
		// Read CH_A will be OK
		u32addr_shuLvl = DDRPHY_REG_MISC_RG_DFS_CTRL_Ofst;
		u32val_shuLvl = DDRPHY_READ_REG(u32addr_shuLvl);
		return ((u32val_shuLvl & SHU_LEVEL) != 0) ? true : false;
	}
	return false;
}

static u32 MapAddr_ddrphy_m6(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_chk = 0, u32addr_rslt_rkxbx = 0, u32addr_rslt_chx = 0;
	u32 u32addr_rslt_mapcrb = 0;

	u32addr_chk = u32addr & TRANS_ADDR_MSK;
	u32addr_rslt_rkxbx  = u32addr & TRANS_ADDR_MSK;
	//####################################
	// For rank/byte base
	//####################################
	//##################
	// SHMOO_RX
	//##################
	//rxdqvrefsel_bytex (((RX Vref)))
	if (u32addr_chk == DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst)
		u32addr_rslt_rkxbx += DDRPHY_AO_BT_OFST_2*u8byte;
	//rxdvs0_bytex
	else if (u32addr_chk == DDRPHY_REG_B0_RXDVS0_Ofst)
		u32addr_rslt_rkxbx += DDRPHY_AO_BT_OFST_2*u8byte;
	//rxdvs2_rkx_bytex
	else if (u32addr_chk == DDRPHY_REG_RK_B0_RXDVS2_Ofst)
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST_2*u8rank
			+ DDRPHY_AO_BT_OFST_2*u8byte;
	//rxdqs_dly_rkx_bytex (((RX Phase: DQS/DQM)))
	else if (u32addr_chk == DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst)
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST*u8rank
			+ DDRPHY_AO_BT_OFST*u8byte;
	//rxdq_dly_rkx_bytex (((RX Phase: DQ)))
	else if ((u32addr_chk >= DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst)
		&& (u32addr_chk <= DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst))
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST*u8rank
									+ DDRPHY_AO_BT_OFST*u8byte;
	//##################
	// SHMOO_TX
	//##################
	//txcmd_rkx
	else if (u32addr_chk == DDRPHY_REG_SHU_R0_CA_CMD0_Ofst)
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST*u8rank;
	//txdq_dly_rkx_bytex
	else if (u32addr_chk == DDRPHY_REG_SHU_R0_B0_DQ0_Ofst)
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST*u8rank
			+ DDRPHY_AO_BT_OFST*u8byte;
	//####################################
	// For channel base
	//####################################
	//ddrphy addr
	u32addr_rslt_chx += u8channel*TRANS_AO_OFST +
		((u8channel >= TRANS_DDRPHY_CHMID) ? TRANS_AO_OFST : 0);
	//####################################
	// map address result
	//####################################
	u32addr_rslt_mapcrb = u32addr_rslt_chx + u32addr_rslt_rkxbx;
	return u32addr_rslt_mapcrb;
}

static u32 MapAddr_ddrphy_m6l(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_chk = 0, u32addr_rslt_rkxbx = 0, u32addr_rslt_chx = 0;
	u32 u32addr_rslt_mapcrb = 0;

	u32addr_chk = u32addr & TRANS_ADDR_MSK;
	u32addr_rslt_rkxbx  = u32addr & TRANS_ADDR_MSK;
	//####################################
	// For rank/byte base
	//####################################
	//##################
	// SHMOO_RX
	//##################
	//rxdqvrefsel_bytex (((RX Vref)))
	if (u32addr_chk == DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst_M6L)
		u32addr_rslt_rkxbx += DDRPHY_AO_BT_OFST_2_M6L*u8byte;
	//rxdvs0_bytex
	else if (u32addr_chk == DDRPHY_REG_B0_RXDVS0_Ofst_M6L)
		u32addr_rslt_rkxbx += DDRPHY_AO_BT_OFST_2_M6L*u8byte;
	//rxdvs2_rkx_bytex
	else if (u32addr_chk == DDRPHY_REG_RK_B0_RXDVS2_Ofst_M6L)
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST_2_M6L*u8rank
			+ DDRPHY_AO_BT_OFST_2_M6L*u8byte;
	//rxdqs_dly_rkx_bytex (((RX Phase: DQS/DQM)))
	else if (u32addr_chk == DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst_M6L)
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST_M6L*u8rank
			+ DDRPHY_AO_BT_OFST_M6L*u8byte;
	//rxdq_dly_rkx_bytex (((RX Phase: DQ)))
	else if ((u32addr_chk >= DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst_M6L)
		&& (u32addr_chk <= DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst_M6L))
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST_M6L*u8rank
			+ DDRPHY_AO_BT_OFST_M6L*u8byte;
	//##################
	// SHMOO_TX
	//##################
	//txcmd_rkx
	else if (u32addr_chk == DDRPHY_REG_SHU_R0_CA_CMD0_Ofst_M6L)
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST_M6L*u8rank;
	//txdq_dly_rkx_bytex
	else if (u32addr_chk == DDRPHY_REG_SHU_R0_B0_DQ0_Ofst_M6L)
		u32addr_rslt_rkxbx += DDRPHY_AO_RK_OFST_M6L*u8rank
			+ DDRPHY_AO_BT_OFST_M6L*u8byte;
	//####################################
	// For channel base
	//####################################
	//ddrphy addr
	u32addr_rslt_chx += u8channel*TRANS_AO_OFST +
		((u8channel >= TRANS_DDRPHY_CHMID) ? TRANS_AO_OFST : 0);
	//####################################
	// map address result
	//####################################
	u32addr_rslt_mapcrb = u32addr_rslt_chx + u32addr_rslt_rkxbx;
	return u32addr_rslt_mapcrb;
}

static u32 MapAddr_ddrphy_chip(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rslt_chip = u32addr;
	u32 u32addr_rslt_mapcrb = 0;

	switch (u32addr) {
	case DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst_M6L;
		break;
	case DDRPHY_REG_B0_RXDVS0_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_B0_RXDVS0_Ofst_M6L;
		break;
	case DDRPHY_REG_RK_B0_RXDVS2_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_RK_B0_RXDVS2_Ofst_M6L;
		break;
	case DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst_M6L;
		break;
	case DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst_M6L;
		break;
	case DDRPHY_REG_SHU_R0_B0_RXDLY1_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_SHU_R0_B0_RXDLY1_Ofst_M6L;
		break;
	case DDRPHY_REG_SHU_R0_B0_RXDLY2_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_SHU_R0_B0_RXDLY2_Ofst_M6L;
		break;
	case DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst_M6L;
		break;
	case DDRPHY_REG_SHU_R0_CA_CMD0_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_SHU_R0_CA_CMD0_Ofst_M6L;
		break;
	case DDRPHY_REG_SHU_R0_B0_DQ0_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_SHU_R0_B0_DQ0_Ofst_M6L;
		break;
	case DDRPHY_REG_MISC_CG_CTRL5_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DDRPHY_REG_MISC_CG_CTRL5_Ofst_M6L;
		break;
	default:
		u32addr_rslt_chip = u32addr;
		break;
	}

	if (IsMachli() == true)
		u32addr_rslt_mapcrb = MapAddr_ddrphy_m6(
			u32addr_rslt_chip, u8channel, u8rank, u8byte);
	else
		u32addr_rslt_mapcrb = MapAddr_ddrphy_m6l(
			u32addr_rslt_chip, u8channel, u8rank, u8byte);

	return u32addr_rslt_mapcrb;
}

static u32 TransAddr_ddrphy(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rslt = 0, u32addr_rslt_mapcrb = 0;
	u32 u32TransBase = TRANS_DDRPHY_AO_BASE;
	u32 u32addr_rslt_shu = 0;

	pr_err("[%s] u32addr=0x%08x, u8channel=%d, u8rank=%d, u8byte=%d\n",
		__func__, u32addr, u8channel, u8rank, u8byte);
	u32addr_rslt = u32addr;
	if ((u8channel >= CHANNEL_MAX) || (u8rank >= RANK_MAX) || (u8byte >= BYTE_MAX))
		return u32addr_rslt;
	//####################################
	// For shuffle base
	//####################################
	u32addr_rslt_shu = (IsShuffleRG1() == true) ? DDRPHY_AO_SHU_OFST : 0;
	//####################################
	// For final address
	//####################################
	u32addr_rslt_mapcrb = MapAddr_ddrphy_chip(u32addr, u8channel, u8rank, u8byte);
	u32addr_rslt = u32addr_rslt_mapcrb;
	u32addr_rslt += u32addr_rslt_shu;
	pr_err("[%s] TRANS_ADDR = 0x%08x\n", __func__, (u32)(u32addr_rslt+u32TransBase));

	if (u32addr_rslt > SHMOO_IOREMAP_SIZE)
		pr_crit("[%s] offset(0x%08x) larger than ddrphy ioremap size(0x10000)!!!",
			__func__, u32addr_rslt);

	return u32addr_rslt;
}

static u32 MapAddr_dramc_m6(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_chk = 0, u32addr_rslt_rkxbx = 0, u32addr_rslt_chx = 0;
	u32 u32addr_rslt_mapcrb = 0;

	u32addr_chk = u32addr & TRANS_ADDR_MSK;
	u32addr_rslt_rkxbx  = u32addr & TRANS_ADDR_MSK;
	//####################################
	// For rank/byte base
	//####################################
	//rkx_bytex
	if (u32addr_chk == DRAMC_REG_SHURK_PI_Ofst)
		u32addr_rslt_rkxbx += DRAMC_AO_RK_OFST*u8rank;
	//####################################
	// For channel base
	//####################################
	//dramc addr
	u32addr_rslt_chx += u8channel*TRANS_AO_OFST;
	//####################################
	// map address result
	//####################################
	u32addr_rslt_mapcrb = u32addr_rslt_chx + u32addr_rslt_rkxbx;
	return u32addr_rslt_mapcrb;
}

static u32 MapAddr_dramc_m6l(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_chk = 0, u32addr_rslt_rkxbx = 0, u32addr_rslt_chx = 0;
	u32 u32addr_rslt_mapcrb = 0;

	u32addr_chk = u32addr & TRANS_ADDR_MSK;
	u32addr_rslt_rkxbx  = u32addr & TRANS_ADDR_MSK;
	//####################################
	// For rank/byte base
	//####################################
	//rkx_bytex
	if (u32addr_chk == DRAMC_REG_SHURK_PI_Ofst_M6L)
		u32addr_rslt_rkxbx += DRAMC_AO_RK_OFST_M6L*u8rank;
	//####################################
	// For channel base
	//####################################
	//dramc addr
	u32addr_rslt_chx += u8channel*TRANS_AO_OFST;
	//####################################
	// map address result
	//####################################
	u32addr_rslt_mapcrb = u32addr_rslt_chx + u32addr_rslt_rkxbx;
	return u32addr_rslt_mapcrb;
}

static u32 MapAddr_dramc_chip(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rslt_chip = u32addr;
	u32 u32addr_rslt_mapcrb = 0;

	switch (u32addr) {
	case DRAMC_REG_TEST2_A0_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_TEST2_A0_Ofst_M6L;
		break;
	case DRAMC_REG_RK_TEST2_A1_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_RK_TEST2_A1_Ofst_M6L;
		break;
	case DRAMC_REG_TEST2_A2_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_TEST2_A2_Ofst_M6L;
		break;
	case DRAMC_REG_TEST2_A3_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_TEST2_A3_Ofst_M6L;
		break;
	case DRAMC_REG_TEST2_A4_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_TEST2_A4_Ofst_M6L;
		break;
	case DRAMC_REG_CKECTRL_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_CKECTRL_Ofst_M6L;
		break;
	case DRAMC_REG_SWCMD_CTRL0_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_SWCMD_CTRL0_Ofst_M6L;
		break;
	case DRAMC_REG_SWCMD_EN_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_SWCMD_EN_Ofst_M6L;
		break;
	case DRAMC_REG_SHURK_PI_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_SHURK_PI_Ofst_M6L;
		break;
	case DRAMC_REG_DQSOSCR_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_DQSOSCR_Ofst_M6L;
		break;
	default:
		u32addr_rslt_chip = u32addr;
		break;
	}

	if (IsMachli() == true)
		u32addr_rslt_mapcrb = MapAddr_dramc_m6(
			u32addr_rslt_chip, u8channel, u8rank, u8byte);
	else
		u32addr_rslt_mapcrb = MapAddr_dramc_m6l(
			u32addr_rslt_chip, u8channel, u8rank, u8byte);

	return u32addr_rslt_mapcrb;
}

static u32 TransAddr_dramc(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rslt = 0, u32addr_rslt_mapcrb = 0;
	u32 u32TransBase = TRANS_DRAMC_AO_BASE;
	u32 u32addr_rslt_shu = 0;

	pr_err("[%s] u32addr=0x%08x, u8channel=%d, u8rank=%d, u8byte=%d\n",
		__func__, u32addr, u8channel, u8rank, u8byte);
	u32addr_rslt = u32addr;
	if ((u8channel >= CHANNEL_MAX) || (u8rank >= RANK_MAX) || (u8byte >= BYTE_MAX))
		return u32addr_rslt;
	//####################################
	// For shuffle base
	//####################################
	u32addr_rslt_shu = (IsShuffleRG1() == true) ? DRAMC_AO_SHU_OFST : 0;
	//####################################
	// For final address
	//####################################
	u32addr_rslt_mapcrb = MapAddr_dramc_chip(u32addr, u8channel, u8rank, u8byte);
	u32addr_rslt = u32addr_rslt_mapcrb;
	u32addr_rslt += u32addr_rslt_shu;
	pr_err("[%s] TRANS_ADDR = 0x%08x\n", __func__, (u32)(u32addr_rslt+u32TransBase));

	if (u32addr_rslt > SHMOO_IOREMAP_SIZE)
		pr_crit("[%s] offset(0x%08x) larger than dramc ioremap size(0x10000)!!!",
			__func__, u32addr_rslt);

	return u32addr_rslt;
}

static u32 MapAddr_dramc_nao_m6(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_chk = 0, u32addr_rslt_rkxbx = 0, u32addr_rslt_chx = 0;
	u32 u32addr_rslt_mapcrb = 0;

	u32addr_chk = u32addr & TRANS_ADDR_MSK;
	u32addr_rslt_rkxbx  = u32addr & TRANS_ADDR_MSK;
	//####################################
	// For rank/byte base
	//####################################
	//rkx_bytex
	//if ((u32addr_chk == DRAMC_REG_RK0_PI_DQ_CAL_Ofst)
	//|| (u32addr_chk == DRAMC_REG_RK0_PI_DQM_CAL_Ofst))
	//	u32addr_rslt_rkxbx += 0x200*u8rank;
	//####################################
	// For channel base
	//####################################
	//dramc addr
	u32addr_rslt_chx += u8channel*TRANS_NAO_OFST;
	//####################################
	// map address result
	//####################################
	u32addr_rslt_mapcrb = u32addr_rslt_chx + u32addr_rslt_rkxbx;
	return u32addr_rslt_mapcrb;
}

static u32 MapAddr_dramc_nao_m6l(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_chk = 0, u32addr_rslt_rkxbx = 0, u32addr_rslt_chx = 0;
	u32 u32addr_rslt_mapcrb = 0;

	u32addr_chk = u32addr & TRANS_ADDR_MSK;
	u32addr_rslt_rkxbx  = u32addr & TRANS_ADDR_MSK;
	//####################################
	// For rank/byte base
	//####################################
	//rkx_bytex
	//if ((u32addr_chk == DRAMC_REG_RK0_PI_DQ_CAL_Ofst_M6L)
	//|| (u32addr_chk == DRAMC_REG_RK0_PI_DQM_CAL_Ofst_M6L))
	//	u32addr_rslt_rkxbx += 0x200*u8rank;
	//####################################
	// For channel base
	//####################################
	//dramc addr
	u32addr_rslt_chx += u8channel*TRANS_NAO_OFST;
	//####################################
	// map address result
	//####################################
	u32addr_rslt_mapcrb = u32addr_rslt_chx + u32addr_rslt_rkxbx;
	return u32addr_rslt_mapcrb;
}

static u32 MapAddr_dramc_nao_chip(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rslt_chip = u32addr;
	u32 u32addr_rslt_mapcrb = 0;

	switch (u32addr) {
	case DRAMC_REG_TESTRPT_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_TESTRPT_Ofst_M6L;
		break;
	case DRAMC_REG_CMP_ERR_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_CMP_ERR_Ofst_M6L;
		break;
	case DRAMC_REG_SPCMDRESP_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_SPCMDRESP_Ofst_M6L;
		break;
	case DRAMC_REG_MRR_STATUS_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_MRR_STATUS_Ofst_M6L;
		break;
	case DRAMC_REG_TCMDO1LAT_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_TCMDO1LAT_Ofst_M6L;
		break;
	case DRAMC_REG_RK0_PI_DQ_CAL_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_RK0_PI_DQ_CAL_Ofst_M6L;
		break;
	case DRAMC_REG_RK0_PI_DQM_CAL_Ofst:
		if (IsManks() == true)
			u32addr_rslt_chip = DRAMC_REG_RK0_PI_DQM_CAL_Ofst_M6L;
		break;
	default:
		u32addr_rslt_chip = u32addr;
		break;
	}

	if (IsMachli() == true)
		u32addr_rslt_mapcrb = MapAddr_dramc_nao_m6(
			u32addr_rslt_chip, u8channel, u8rank, u8byte);
	else
		u32addr_rslt_mapcrb = MapAddr_dramc_nao_m6l(
			u32addr_rslt_chip, u8channel, u8rank, u8byte);

	return u32addr_rslt_mapcrb;
}

static u32 TransAddr_dramc_nao(u32 u32addr, u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rslt = 0, u32addr_rslt_mapcrb = 0;
	u32 u32TransBase = TRANS_DRAMC_NAO_BASE;

	pr_err("[%s] u32addr=0x%08x, u8channel=%d, u8rank=%d, u8byte=%d\n",
		__func__, u32addr, u8channel, u8rank, u8byte);
	u32addr_rslt = u32addr;
	if ((u8channel >= CHANNEL_MAX) || (u8rank >= RANK_MAX) || (u8byte >= BYTE_MAX))
		return u32addr_rslt;
	//####################################
	// For final address
	//####################################
	u32addr_rslt_mapcrb = MapAddr_dramc_nao_chip(u32addr, u8channel, u8rank, u8byte);
	u32addr_rslt = u32addr_rslt_mapcrb;
	pr_err("[%s] TRANS_ADDR = 0x%08x\n", __func__, (u32)(u32addr_rslt+u32TransBase));

	if (u32addr_rslt > SHMOO_IOREMAP_SIZE)
		pr_crit("[%s] offset(0x%08x) larger than dramc_nao ioremap size(0x10000)!!!",
			__func__, u32addr_rslt);

	return u32addr_rslt;
}

static int runtime_shmoo_ta2_broadcast(bool enable)
{
	#define EMI_DISPATCH_BASE   0x1C37E600
	#define EMI_IOREMAP_SIZE   0x1000
	void __iomem *base = 0;

	base = ioremap(EMI_DISPATCH_BASE, EMI_IOREMAP_SIZE);
	if (!base) {
		pr_err("[%s] Can't find EMI_DISPATCH device base!!\n", __func__);
		return -1;
	}
	if (enable) {
		pr_err("[%s] ON\n", __func__);
		WRITE_REG(TA2_BROADCAST_MODE0, BROC_MODE0);
		WRITE_REG(TA2_BROADCAST_MODE1, BROC_MODE1);
	} else	{
		pr_err("[%s] OFF\n", __func__);
		WRITE_REG(0x0000, BROC_MODE0);
		WRITE_REG(0x0000, BROC_MODE1);
	}
	iounmap(base);

	wmb(); /* make sure settings are written */
	return 0;

}

static int runtime_shmoo_ta2(u8 u8Rank, u32 u32ta2looptimes)
{
	#define TA2_TST_STP   0x12000880
	#define TA2_TST_WR   0x92000880 //[31]W+R mode [30] read only mode
	#define TA2_TST_MOD0   0x0080510D
	#define TA2_TST_MOD1	 0x0180510D
	#define TA2_TST_PTRN0   0x000055AA
	#define TA2_TST_ADRST   0x00000000
	#define TA2_TST_ADRLN   0x00020000
	#define TA2_TST_STTS_FNSH   BIT(0)
	#define TA2_TST_STTS_FAIL   BIT(4)
	#define TA2_TST_CHKCNT 10000

	u32 u32Ta2TstAddr = 0, u32Ta2TstStts = 0, u32ErrBitStts = 0;
	u32 u32TransBase = TRANS_DRAMC_AO_BASE;
	u32 u32addr = 0, u32val = 0;
	int count = 0, ch = 0;
	u32 u32TA2Count = 0;

	if (u32ta2looptimes == 0) {
		//pr_crit("[%s] Parameter Error: u32ta2looptimes: 0\n", __func__);
		//pr_crit("[%s] Error: TEST_PARAM\n", __func__);
		return 1;
	}

	pr_crit("Run TA2 Bist Test: Start\n");

	for (u32TA2Count = 0; u32TA2Count < u32ta2looptimes; u32TA2Count++) {
		runtime_shmoo_ta2_broadcast(true);
		// Stop TA2 test
		u32addr = TransAddr_dramc(DRAMC_REG_TEST2_A3_Ofst, 0, 0, 0);
		u32val = TA2_TST_STP;
		SHM_DRAMC_WR(u32val, u32addr);
		pr_err("[%s] DRAMC_WRITE[0x%08x]=0x%08x\n", __func__, u32addr+u32TransBase, u32val);
		// Configure
		u32addr = TransAddr_dramc(DRAMC_REG_TEST2_A0_Ofst, 0, 0, 0);
		u32val = TA2_TST_PTRN0;
		SHM_DRAMC_WR(u32val, u32addr);
		pr_err("[%s] DRAMC_WRITE[0x%08x]=0x%08x\n", __func__, u32addr+u32TransBase, u32val);
		u32addr = TransAddr_dramc(DRAMC_REG_TEST2_A4_Ofst, 0, 0, 0);
		u32val = (u8Rank == 0) ? TA2_TST_MOD0 : TA2_TST_MOD1;
		SHM_DRAMC_WR(u32val, u32addr);
		pr_err("[%s] DRAMC_WRITE[0x%08x]=0x%08x\n", __func__, u32addr+u32TransBase, u32val);
		u32addr = TransAddr_dramc(DRAMC_REG_RK_TEST2_A1_Ofst, 0, 0, 0);
		u32val = TA2_TST_ADRST;
		SHM_DRAMC_WR(u32val, u32addr);
		pr_err("[%s] DRAMC_WRITE[0x%08x]=0x%08x\n", __func__, u32addr+u32TransBase, u32val);
		u32addr = TransAddr_dramc(DRAMC_REG_TEST2_A2_Ofst, 0, 0, 0);
		u32val = TA2_TST_ADRLN;
		SHM_DRAMC_WR(u32val, u32addr);
		pr_err("[%s] DRAMC_WRITE[0x%08x]=0x%08x\n", __func__, u32addr+u32TransBase, u32val);
		// Trigger TA2 test
		u32addr = TransAddr_dramc(DRAMC_REG_TEST2_A3_Ofst, 0, 0, 0);
		u32val = TA2_TST_WR;
		SHM_DRAMC_WR(u32val, u32addr);
		pr_err("[%s] DRAMC_WRITE[0x%08x]=0x%08x\n", __func__, u32addr+u32TransBase, u32val);

		runtime_shmoo_ta2_broadcast(false);
		for (ch = 0; ch < CHANNEL_MAX; ch++) {
			for (count = 0 ; count < TA2_TST_CHKCNT; count++) {
				usleep_range(SHMOO_DELAY_1MS_MIN, SHMOO_DELAY_1MS_MAX);
				u32Ta2TstAddr = TransAddr_dramc_nao(
					DRAMC_REG_TESTRPT_Ofst, ch, 0, 0);
				u32Ta2TstStts = SHM_DRAMC_NAO_RD(u32Ta2TstAddr);
				if (u32Ta2TstStts & TA2_TST_STTS_FNSH) {
					if (u32Ta2TstStts & TA2_TST_STTS_FAIL) {
						pr_crit("[%s] ch %d TA2 fail\n", __func__, ch);
						u32Ta2TstAddr = TransAddr_dramc_nao(
							DRAMC_REG_CMP_ERR_Ofst, ch, 0, 0);
						u32ErrBitStts = SHM_DRAMC_NAO_RD(u32Ta2TstAddr);
						pr_crit("[%s] ch %d Error Bit Status = 0x%08x\n",
							__func__, ch, u32ErrBitStts);
						pr_crit("[%s] Error: TEST_FAIL\n", __func__);
						return 0;
					}
					break;
				}
			}

			if (count == TA2_TST_CHKCNT) {
				pr_crit("[%s] ch %d timeout, value 0x%08x\n",
					__func__, ch, u32Ta2TstStts);
				pr_crit("[%s] Error: TEST_TIMEOUT\n", __func__);
				return 0;
			}

		}
		// Stop TA2 test
		runtime_shmoo_ta2_broadcast(true);
		u32addr = DRAMC_REG_TEST2_A3_Ofst;
		u32val = TA2_TST_STP;
		SHM_DRAMC_WR(u32val, u32addr);
		pr_err("[%s] DRAMC_WRITE[0x%08x]=0x%08x\n", __func__, u32addr+u32TransBase, u32val);
		runtime_shmoo_ta2_broadcast(false);
		usleep_range(SHMOO_DELAY_10MS_MIN, SHMOO_DELAY_10MS_MAX);
		pr_err("[%s] TA2 Loop = %d\n", __func__, u32TA2Count);
	}

	pr_crit("Run TA2 Bist Test: Done\n");
	return 1;
}
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx (SHMOO_COMN)

#ifdef RUN_TIME_SHMOO_RX
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
// RX Window Center Assignment
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
static u32 u32RxWinChannel = CRB_MAX;
static u32 u32RxWinRank = CRB_MAX;
static u32 u32RxWinByte = CRB_MAX;
static u32 u32RxWinDirection;
static u32 u32RxWinChangeDelta = WIN_MAX;
static u32 u32RxWinTA2LoopTimes = 1;

//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
// RX Shmoo Vref+Phase Scan
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
static u32 u32RxShmChannel;
static u32 u32RxShmRank;
static u32 u32RxShmByte;
static u32 u32RxShmDirection;
static u32 u32RxShmDelaysec;
static u32 u32RxShmTA2LoopTimes = 1;

#define RX_VREF_MSK_L   ((IsMachli() == true) ? 0x0000007f : 0x0000003f)
#define RX_VREF_MSK_H   ((IsMachli() == true) ? 0x00007f00 : 0x00000000)
#define RX_VREF_SFT   8

#define RX_VREF_MAX   RX_VREF_MSK_L
#define RX_PHASE_DQS_MSK   ((IsMachli() == true) ? 0x000001ff : 0x007f0000)
#define RX_PHASE_DQS_MAX   ((IsMachli() == true) ? 0x000001ff : 0x0000007f)
#define RX_PHASE_DQS_SFT   ((IsMachli() == true) ? 0 : 16)
#define RX_DVS0_SW_UP   BIT(8)
#define RX_DVS2_MOD_MSK   (BIT(31) | BIT(30))
#define RX_DVS2_MOD_SW   BIT(30)
#define RX_DVS2_MOD_HW   BIT(31)
#define RX_DVS2_MOD_RG   0x00000000

#define RX_PHASE_DQ_MSK   ((IsMachli() == true) ? 0x000000ff : 0x0000003f)
#define RX_PHASE_DQ_MAX   RX_PHASE_DQ_MSK
#define RX_PHASE_DQ_GRP   4
#define RX_PHASE_DQ_SFT   8

static u32 dramc_rx_vref_read(u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rxdqvref = 0, u32val_rxdqvref = 0;

	// read rg value
	pr_err("[%s] u8channel(%d), u8rank(%d), u8byte(%d)\n", __func__, u8channel, u8rank, u8byte);
	u32addr_rxdqvref = TransAddr_ddrphy(
		DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst, u8channel, u8rank, u8byte);
	u32val_rxdqvref = SHM_DDRPHY_RD(u32addr_rxdqvref);
	pr_crit("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n", __func__, u32addr_rxdqvref, u32val_rxdqvref);
	return u32val_rxdqvref;
}

static u8 dramc_rx_vref_write(u8 u8channel, u8 u8rank, u8 u8byte, u32 u32vref_target)
{
	u32 u32addr_rxdqvref = 0, u32val_rxdqvref = 0;

	// read rg value
	u32addr_rxdqvref = TransAddr_ddrphy(
		DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst, u8channel, u8rank, u8byte);
	u32val_rxdqvref = SHM_DDRPHY_RD(u32addr_rxdqvref);
	// write rg value
	u32val_rxdqvref &= (~(RX_VREF_MSK_H | RX_VREF_MSK_L));
	u32val_rxdqvref |= (((u32vref_target << RX_VREF_SFT) & RX_VREF_MSK_H)
		| (u32vref_target & RX_VREF_MSK_L));
	SHM_DDRPHY_WR(u32val_rxdqvref, u32addr_rxdqvref);
	pr_crit("[%s] DDRPHY_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_rxdqvref, u32val_rxdqvref);
	return 1;
}

static u32 dramc_rx_phase_read_DQS(u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rxdqsdly = 0, u32val_rxdqsdly = 0;

	// read rg value
	pr_err("[%s] u8channel(%d), u8rank(%d), u8byte(%d)\n", __func__, u8channel, u8rank, u8byte);
	u32addr_rxdqsdly = TransAddr_ddrphy(
		DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst, u8channel, u8rank, u8byte);
	u32val_rxdqsdly = SHM_DDRPHY_RD(u32addr_rxdqsdly);
	pr_crit("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n", __func__, u32addr_rxdqsdly, u32val_rxdqsdly);
	return u32val_rxdqsdly;
}

static u8 dramc_rx_phase_write_DQS(u8 u8channel, u8 u8rank, u8 u8byte, u32 u32phase_target)
{

	u32 u32addr_rxdvs2 = 0, u32val_rxdvs2 = 0;
	u32 u32addr_rxdqsdly = 0, u32val_rxdqsdly = 0;
	u32 u32addr_rxdvs0 = 0, u32val_rxdvs0 = 0;

	// read rg value
	pr_crit("[%s]  u8channel(%d), u8rank(%d), u8byte(%d)\n",
		__func__, u8channel, u8rank, u8byte);
	u32addr_rxdvs2 = TransAddr_ddrphy(
		DDRPHY_REG_RK_B0_RXDVS2_Ofst, u8channel, u8rank, u8byte);
	u32val_rxdvs2 = DDRPHY_READ_REG(u32addr_rxdvs2);
	pr_err("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n",
		__func__, u32addr_rxdvs2, u32val_rxdvs2);
	u32addr_rxdqsdly = TransAddr_ddrphy(
		DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst, u8channel, u8rank, u8byte);
	u32val_rxdqsdly = DDRPHY_READ_REG(u32addr_rxdqsdly);
	pr_err("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n",
		__func__, u32addr_rxdqsdly, u32val_rxdqsdly);
	u32addr_rxdvs0 = TransAddr_ddrphy(
		DDRPHY_REG_B0_RXDVS0_Ofst, u8channel, u8rank, u8byte);
	u32val_rxdvs0 = DDRPHY_READ_REG(u32addr_rxdvs0);
	pr_err("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n",
		__func__, u32addr_rxdvs0, u32val_rxdvs0);

	// write rg value
	//(1) set rxdvs2
	u32val_rxdvs2 = (u32val_rxdvs2 & (~RX_DVS2_MOD_MSK)) | RX_DVS2_MOD_SW;

	DDRPHY_WRITE_REG(u32val_rxdvs2, u32addr_rxdvs2);
	pr_err("[%s] DDRPHY_WRITE_REG[0x%08x] = 0x%08x\n",
		__func__, u32addr_rxdvs2, u32val_rxdvs2);
	//(2) set rxdqs_dly
	u32val_rxdqsdly =  (u32val_rxdqsdly & (~RX_PHASE_DQS_MSK))
		| (u32phase_target << RX_PHASE_DQS_SFT);
	DDRPHY_WRITE_REG(u32val_rxdqsdly, u32addr_rxdqsdly);
	pr_crit("[%s] DDRPHY_WRITE_REG[0x%08x] = 0x%08x\n",
		__func__, u32addr_rxdqsdly, u32val_rxdqsdly);
	//(3) toggle rxdvs0
	u32val_rxdvs0 &= (~RX_DVS0_SW_UP); //bit8=0
	DDRPHY_WRITE_REG(u32val_rxdvs0, u32addr_rxdvs0);
	pr_err("[%s] DDRPHY_WRITE_REG[0x%08x] = 0x%08x (Toggle 0)\n",
		__func__, u32addr_rxdvs0, u32val_rxdvs0);
	u32val_rxdvs0 |= RX_DVS0_SW_UP; //bit8=1
	DDRPHY_WRITE_REG(u32val_rxdvs0, u32addr_rxdvs0);
	pr_err("[%s] DDRPHY_WRITE_REG[0x%08x] = 0x%08x (Toggle 1)\n",
		__func__, u32addr_rxdvs0, u32val_rxdvs0);
	return 1;
}

static u32 dramc_rx_phase_read(u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_rxdqdly = 0, u32val_rxdqdly = 0;
	u32 u32addrofst[RX_PHASE_DQ_GRP] = {0};
	u8 u8dqrgIdx = 0;

	pr_err("[%s] u8channel(%d), u8rank(%d), u8byte(%d)\n", __func__, u8channel, u8rank, u8byte);
	//configure address offset
	u32addrofst[u8dqrgIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst;
	u32addrofst[u8dqrgIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY1_Ofst;
	u32addrofst[u8dqrgIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY2_Ofst;
	u32addrofst[u8dqrgIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst;
	//operate rg
	for (u8dqrgIdx = 0; u8dqrgIdx < RX_PHASE_DQ_GRP; u8dqrgIdx++) {
		// read rg value
		u32addr_rxdqdly = TransAddr_ddrphy(
			u32addrofst[u8dqrgIdx], u8channel, u8rank, u8byte);
		u32val_rxdqdly = SHM_DDRPHY_RD(u32addr_rxdqdly);
		pr_crit("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_rxdqdly, u32val_rxdqdly);
	}
	return 1;
}

static u8 dramc_rx_phase_write(u8 u8channel, u8 u8rank, u8 u8byte, u32 u32val_rxdqdly_target[])
{
	u32 u32addr_rxdqdly = 0, u32val_rxdqdly = 0;
	u32 u32addrofst[RX_PHASE_DQ_GRP] = {0};
	u8 u8dqrgIdx = 0;

	pr_crit("[%s] u8channel(%d), u8rank(%d), u8byte(%d)\n",
		__func__, u8channel, u8rank, u8byte);

	//configure address offset
	u32addrofst[u8dqrgIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst;
	u32addrofst[u8dqrgIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY1_Ofst;
	u32addrofst[u8dqrgIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY2_Ofst;
	u32addrofst[u8dqrgIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst;
	//operate rg
	for (u8dqrgIdx = 0; u8dqrgIdx < RX_PHASE_DQ_GRP; u8dqrgIdx++) {
		// read rg value
		u32addr_rxdqdly = TransAddr_ddrphy(
			u32addrofst[u8dqrgIdx], u8channel, u8rank, u8byte);
		u32val_rxdqdly = SHM_DDRPHY_RD(u32addr_rxdqdly);
		pr_err("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_rxdqdly, u32val_rxdqdly);
		// write rg value
		u32val_rxdqdly =  u32val_rxdqdly_target[u8dqrgIdx];
		SHM_DDRPHY_WR(u32val_rxdqdly, u32addr_rxdqdly);
		pr_crit("[%s] DDRPHY_WRITE[0x%08x] = 0x%08x\n",
			__func__, u32addr_rxdqdly, u32val_rxdqdly);
	}
	return 1;
}

//##################################################################(Fix CCN)
static u8 dramc_rxwincenter_run_vref(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8dir, u8 u8changedelta, u32 u32ta2looptimes)
{
	//vref dq
	u32 u32addr_rxdqvref = 0, u32val_rxdqvref = 0;
	u32 u32val_rxdqvref_target = 0;

	pr_crit("\n\n########################\n");
	pr_crit("Run dramc_rxwincenter for Vref\n");
	pr_crit("########################\n\n");

	// read rx vref value
	if ((u8dir == SHIFT_D) || (u8dir == SHIFT_U)) {
		u32addr_rxdqvref = TransAddr_ddrphy(
			DDRPHY_REG_SHU_B0_PHY_VREF_SEL_Ofst, u8channel, u8rank, u8byte);
		u32val_rxdqvref = SHM_DDRPHY_RD(u32addr_rxdqvref);
		//get upper bound value only
		u32val_rxdqvref_target = u32val_rxdqvref & RX_VREF_MSK_L;
		pr_crit("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_rxdqvref, u32val_rxdqvref);
	}

	//process vref
	switch (u8dir) {
	case SHIFT_D: //decrease vref
		if (u32val_rxdqvref_target < (u32)u8changedelta) {
			pr_crit("[%s][vref] RX Vref touch MIN value 0x00 !!!\n", __func__);
			u32val_rxdqvref_target = 0;
		} else {
			u32val_rxdqvref_target -= (u32)u8changedelta;
		}
		dramc_rx_vref_write(u8channel, u8rank, u8byte, u32val_rxdqvref_target);
		break;
	case SHIFT_U: //increase vref
		if ((u32val_rxdqvref_target+(u32)u8changedelta) > RX_VREF_MAX) {
			pr_crit("[%s][vref] RX Vref touch MAX value 0x%08x !!!\n",
				__func__, RX_VREF_MAX);
			u32val_rxdqvref_target = RX_VREF_MAX;
		} else {
			u32val_rxdqvref_target += (u32)u8changedelta;
		}
		dramc_rx_vref_write(u8channel, u8rank, u8byte, u32val_rxdqvref_target);
		break;
	default:
		pr_crit("[%s] RX Vref Direction Error: do nothing\n", __func__);
		return 0;
	}

	return 1;
}

static u8 dramc_rxwincenter_run_phase(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8dir, u8 u8changedelta, u32 u32ta2looptimes)
{
	//phase dqs
	u32 u32addr_rxdqsdly = 0, u32val_rxdqsdly = 0;
	u32 u32val_rxdqsdly_target = 0;
	//phase dq
	u32 u32addr_rxdqdly = 0, u32val_rxdqdly = 0;
	u32 u32val_rxdqdly_target[RX_PHASE_DQ_GRP] = {0};
	u8 u8val_dqtgt[RX_PHASE_DQ_GRP] = {0};
	u32 u32addrofst[RX_PHASE_DQ_GRP] = {0};
	u8 u8dqrgIdx, i;
	u8 u8dqGrpIdx = 0;

	pr_crit("\n\n########################\n");
	pr_crit("Run dramc_rxwincenter for Phase\n");
	pr_crit("########################\n\n");

	// read rx phase value for dq
	if ((u8dir == SHIFT_L) || (u8dir == SHIFT_R)) {
		//configure address offset
		u32addrofst[u8dqGrpIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY0_Ofst;
		u32addrofst[u8dqGrpIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY1_Ofst;
		u32addrofst[u8dqGrpIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY2_Ofst;
		u32addrofst[u8dqGrpIdx++] = DDRPHY_REG_SHU_R0_B0_RXDLY3_Ofst;
		for (u8dqrgIdx = 0; u8dqrgIdx < RX_PHASE_DQ_GRP; u8dqrgIdx++) {
			u32addr_rxdqdly = TransAddr_ddrphy(
				u32addrofst[u8dqrgIdx], u8channel, u8rank, u8byte);
			u32val_rxdqdly = SHM_DDRPHY_RD(u32addr_rxdqdly);
			u32val_rxdqdly_target[u8dqrgIdx] = u32val_rxdqdly;
			pr_crit("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n",
				__func__, u32addr_rxdqdly, u32val_rxdqdly);
		}
	}

	// read rx phase value for dqs
	if ((u8dir == SHIFT_L_RX_DQS) || (u8dir == SHIFT_R_RX_DQS)) {
		u32addr_rxdqsdly = TransAddr_ddrphy(
			DDRPHY_REG_SHU_R0_B0_RXDLY5_Ofst, u8channel, u8rank, u8byte);
		u32val_rxdqsdly = SHM_DDRPHY_RD(u32addr_rxdqsdly);
		u32val_rxdqsdly_target = (u32val_rxdqsdly & RX_PHASE_DQS_MSK) >> RX_PHASE_DQS_SFT;
		pr_crit("[%s] DDRPHY_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_rxdqsdly, u32val_rxdqsdly);
	}

	//process phase
	switch (u8dir) {
	case SHIFT_L: //decrease phase
		for (u8dqrgIdx = 0; u8dqrgIdx < RX_PHASE_DQ_GRP; u8dqrgIdx++) {
			for (i = 0 ; i < RX_PHASE_DQ_GRP; i++) {
				u8val_dqtgt[i] = (u8)((u32val_rxdqdly_target[u8dqrgIdx]
					>> (RX_PHASE_DQ_SFT*i)) & RX_PHASE_DQ_MSK);
				if (u8val_dqtgt[i] < (u8)u8changedelta) {
					pr_crit("[%s][phase] DQ[%d] RX Phase touch MIN value 0x00 !!!\n",
						__func__, (i>>1));
					u8val_dqtgt[i] = 0;
				} else {
					u8val_dqtgt[i] -= (u8)u8changedelta;
				}
				//Assigned odd value
				//u8val_dqtgt[i] += ((u8val_dqtgt[i]%2) == 0) ? 1 : 0;
			}
			u32val_rxdqdly_target[u8dqrgIdx] = 0;
			for (i = 0; i < RX_PHASE_DQ_GRP; i++)
				u32val_rxdqdly_target[u8dqrgIdx]
					|= (u32)(u8val_dqtgt[i] << (RX_PHASE_DQ_SFT*i));
		}
		dramc_rx_phase_write(u8channel, u8rank, u8byte, u32val_rxdqdly_target);
		break;
	case SHIFT_R: //increase phase
		for (u8dqrgIdx = 0; u8dqrgIdx < RX_PHASE_DQ_GRP; u8dqrgIdx++) {
			for (i = 0; i < RX_PHASE_DQ_GRP; i++) {
				u8val_dqtgt[i] = (u8)((u32val_rxdqdly_target[u8dqrgIdx]
					>> (RX_PHASE_DQ_SFT*i)) & RX_PHASE_DQ_MSK);
				if (((u32)u8changedelta+(u32)u8val_dqtgt[i]) > RX_PHASE_DQ_MAX) {
					pr_crit("[%s][phase] DQ[%d] RX Phase touch MAX value 0x%08x !!!\n",
						__func__, (i>>1), RX_PHASE_DQ_MAX);
					u8val_dqtgt[i] = RX_PHASE_DQ_MAX;
				} else {
					u8val_dqtgt[i] += (u8)u8changedelta;
				}
				//Assigned odd value
				//u8val_dqtgt[i] -= ((u8val_dqtgt[i]%2) == 0) ? 1 : 0;
			}
			u32val_rxdqdly_target[u8dqrgIdx] = 0;
			for (i = 0; i < RX_PHASE_DQ_GRP; i++)
				u32val_rxdqdly_target[u8dqrgIdx]
					|= (u32)(u8val_dqtgt[i] << (RX_PHASE_DQ_SFT*i));
		}
		dramc_rx_phase_write(u8channel, u8rank, u8byte, u32val_rxdqdly_target);
		break;
	case SHIFT_L_RX_DQS: //decrease phase DQS
		if (u32val_rxdqsdly_target < (u32)u8changedelta) {
			pr_crit("[%s][phase] RX Phase touch MIN value 0x00 !!!\n", __func__);
			u32val_rxdqsdly_target = 0;
		} else {
			u32val_rxdqsdly_target -= (u32)u8changedelta;
		}
		dramc_rx_phase_write_DQS(u8channel, u8rank, u8byte, u32val_rxdqsdly_target);
		break;
	case SHIFT_R_RX_DQS: //increase phase DQS
		if (((u32)u8changedelta+u32val_rxdqsdly_target) > RX_PHASE_DQS_MAX) {
			pr_crit("[%s][phase] RX Phase touch MAX value 0x%08x !!!\n",
				__func__, RX_PHASE_DQS_MAX);
			u32val_rxdqsdly_target = RX_PHASE_DQS_MAX;
		} else {
			u32val_rxdqsdly_target += (u32)u8changedelta;
		}
		dramc_rx_phase_write_DQS(u8channel, u8rank, u8byte, u32val_rxdqsdly_target);
		break;
	default:
		pr_crit("[%s] RX Phase Direction Error: do nothing\n", __func__);
		return 0;
	}

	return 1;
}

static u8 dramc_rxwincenter_run(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8dir, u8 u8changedelta, u32 u32ta2looptimes)
{
	u8 u8Result = 0;

	pr_crit("\n\n########################\n");
	pr_crit("Run dramc_rxwincenter\n");
	pr_crit("########################\n\n");

	// check parameters
	if ((u8channel >= CHANNEL_MAX) || (u8rank >= RANK_MAX) || (u8byte >= BYTE_MAX)) {
		pr_crit("\n[%s] ====================================\n", __func__);
		if (u8channel >= CHANNEL_MAX)
			pr_crit("[%s] ERROR: Only support channel (0 ~ %d)\n",
				__func__, (CHANNEL_MAX-1));
		if (u8rank >= RANK_MAX)
			pr_crit("[%s] ERROR: Only support rank (0 ~ %d)\n",
				__func__, (RANK_MAX-1));
		if (u8byte >= BYTE_MAX)
			pr_crit("[%s] ERROR: Only support byte (0 ~ %d)\n",
				__func__, (BYTE_MAX-1));
		pr_crit("[%s] ====================================\n\n", __func__);
		return 0;
	}

	// check if rx vref
	if ((u8dir == SHIFT_D) || (u8dir == SHIFT_U))
		u8Result = dramc_rxwincenter_run_vref(u8channel, u8rank, u8byte,
			u8dir, u8changedelta, u32ta2looptimes);
	else //rx phase
		u8Result = dramc_rxwincenter_run_phase(u8channel, u8rank, u8byte,
			u8dir, u8changedelta, u32ta2looptimes);

	if (u8Result == 0)
		return 0;

	if (runtime_shmoo_ta2(u8rank, u32ta2looptimes) == 0) {
		pr_crit("\n[%s] ====================================\n", __func__);
		pr_crit("[%s] TA2 ERROR: STOP\n", __func__);
		pr_crit("[%s] ====================================\n\n", __func__);
		return 0;
	}

	return 1;
}
//##################################################################(Fix CCN)

static u8 dramc_rxwincenter_disp(void)
{
	u8 u8channel = 0, u8rank = 0, u8byte = 0;

	pr_crit("\n\n########################\n");
	pr_crit("Disp dramc_rxwincenter\n");
	pr_crit("########################\n\n");
	for (u8channel = 0; u8channel < CHANNEL_MAX; u8channel++) {
		for (u8rank = 0; u8rank < RANK_MAX; u8rank++) {
			for (u8byte = 0; u8byte < BYTE_MAX; u8byte++) {
				pr_crit("\n[%s] u8channel(%d), u8rank(%d), u8byte(%d)\n",
					__func__, u8channel, u8rank, u8byte);
				dramc_rx_vref_read(u8channel, u8rank, u8byte);
				dramc_rx_phase_read_DQS(u8channel, u8rank, u8byte);
				dramc_rx_phase_read(u8channel, u8rank, u8byte);
			}
		}
	}
	pr_crit("\n\n");
	return 1;
}

static ssize_t dramc_rxwincenter_show(struct device_driver *drv, char *buf)
{
	if ((IsMachli() == false) && (IsManks() == false))
		return 0;

	if (1) {
		_shm_ddrphy_ao_bs = ioremap(SHMOO_DDRPHY_AO, SHMOO_IOREMAP_SIZE);
		_shm_dramc_ao_bs = ioremap(SHMOO_DRAMC_AO, SHMOO_IOREMAP_SIZE);
		_shm_dramc_nao_bs = ioremap(SHMOO_DRAMC_NAO, SHMOO_IOREMAP_SIZE);
		if (!_shm_ddrphy_ao_bs || !_shm_dramc_ao_bs || !_shm_dramc_nao_bs) {
			if (!_shm_ddrphy_ao_bs)
				pr_crit("[%s] ioremap error for ddrphy_ao!\n", __func__);
			else if (!_shm_dramc_ao_bs)
				pr_crit("[%s] ioremap error for dramc_ao!\n", __func__);
			else if (!_shm_dramc_nao_bs)
				pr_crit("[%s] ioremap error for dramc_nao!\n", __func__);
			return 0;
		}
		pr_err("[%s] RX Window Center Display Start\n", __func__);
		dramc_rxwincenter_disp();
		iounmap(_shm_ddrphy_ao_bs);
		iounmap(_shm_dramc_ao_bs);
		iounmap(_shm_dramc_nao_bs);
		pr_err("[%s] RX Window Center Display Done\n", __func__);
	} else {
		pr_crit("[%s] Please echo the parameters firstly!\n", __func__);
	}
	return 0;
}

static ssize_t dramc_rxwincenter_store(struct device_driver *drv, const char *buf, size_t count)
{
	char *str_dir = 0;
	size_t count_sscan = 0;

	if ((IsMachli() == false) && (IsManks() == false))
		return 0;

	count_sscan = sscanf(buf, "%d %d %d %d %d %d", &u32RxWinChannel, &u32RxWinRank,
		&u32RxWinByte, &u32RxWinDirection, &u32RxWinChangeDelta, &u32RxWinTA2LoopTimes);
	if (count_sscan == 6) {
		if (u32RxWinChangeDelta != WIN_MAX) {
			_shm_ddrphy_ao_bs = ioremap(SHMOO_DDRPHY_AO, SHMOO_IOREMAP_SIZE);
			_shm_dramc_ao_bs = ioremap(SHMOO_DRAMC_AO, SHMOO_IOREMAP_SIZE);
			_shm_dramc_nao_bs = ioremap(SHMOO_DRAMC_NAO, SHMOO_IOREMAP_SIZE);
			if (!_shm_ddrphy_ao_bs || !_shm_dramc_ao_bs || !_shm_dramc_nao_bs) {
				if (!_shm_ddrphy_ao_bs)
					pr_crit("[%s] ioremap error for ddrphy_ao!\n", __func__);
				else if (!_shm_dramc_ao_bs)
					pr_crit("[%s] ioremap error for dramc_ao!\n", __func__);
				else if (!_shm_dramc_nao_bs)
					pr_crit("[%s] ioremap error for dramc_nao!\n", __func__);
				return 0;
			}
			switch (u32RxWinDirection) {
			case SHIFT_L:
				str_dir = "[L]: decrease rx dq phase"; break;
			case SHIFT_R:
				str_dir = "[R]: increase rx dq phase"; break;
			case SHIFT_D:
				str_dir = "[D]: decrease rx vref"; break;
			case SHIFT_U:
				str_dir = "[U]: increase rx vref"; break;
			case SHIFT_L_RX_DQS:
				str_dir = "[L]: decrease rx dqs phase"; break;
			case SHIFT_R_RX_DQS:
				str_dir = "[R]: increase rx dqs phase"; break;
			default:
				u32RxWinDirection = SHIFT_R;
				str_dir = "[R]: increase rx phase"; break;
			}

			pr_crit("\n[%s] RX Center Parameters\n	Channel = %d\n	Rank = %d\n	Byte = %d\n	",
				__func__, u32RxWinChannel, u32RxWinRank, u32RxWinByte);
			pr_crit("Direction = %d (%s)\n	Delta = %d\n	TA2LoopTimes = %d\n\n",
				u32RxWinDirection, str_dir,
				u32RxWinChangeDelta, u32RxWinTA2LoopTimes);
			pr_err("[%s] RX Center Run Start\n", __func__);
			dramc_rxwincenter_run((u8)u32RxWinChannel, (u8)u32RxWinRank,
				(u8)u32RxWinByte,	(u8)u32RxWinDirection,
				(u8)u32RxWinChangeDelta, (u32)u32RxWinTA2LoopTimes);
			iounmap(_shm_ddrphy_ao_bs);
			iounmap(_shm_dramc_ao_bs);
			iounmap(_shm_dramc_nao_bs);
			pr_err("[%s] RX Center Run Done\n", __func__);
		} else {
			pr_crit("[%s] RX Center Args must be assigned values!!!\n", __func__);
		}
	} else {
		pr_crit("[%s] RX Center Wrong Args (count=%d) (count_sscan=%d)!!!\n",
			__func__, (int)count, (int)count_sscan);
	}

	return count;
}

static u8 dramc_rxshmoo_run_vref(u8 u8channel, u8 u8rank, u8 u8byte, u8 u8dir)
{
	u8 u8changedelta, u8Ta2Result = 0;
	u32 u32ta2looptimes;

	pr_crit("\n[%s] RX Shmoo Running Vref\n", __func__);
	u8changedelta = 1; // decrease & increase: change delta 1
	u32ta2looptimes = u32RxShmTA2LoopTimes;
	while (1) {
		u8Ta2Result = dramc_rxwincenter_run(
			u8channel, u8rank, u8byte, u8dir, u8changedelta, u32ta2looptimes);
		if (u8Ta2Result == 0)
			return 0;
		msleep(u32RxShmDelaysec*SHMOO_MSLP_DELAY_MS);
	}
	return 1;
}

static u8 dramc_rxshmoo_run_phase(u8 u8channel, u8 u8rank, u8 u8byte, u8 u8dir)
{
	u8 u8changedelta = 0, u8Ta2Result = 0;
	u32 u32ta2looptimes = 0;

	pr_crit("\n[%s] RX Shmoo Running Phase\n", __func__);
	u8changedelta = 1; // decrease & increase: change delta 1
	u32ta2looptimes = u32RxShmTA2LoopTimes;
	while (1) {
		u8Ta2Result = dramc_rxwincenter_run(
			u8channel, u8rank, u8byte, u8dir, u8changedelta, u32ta2looptimes);
		if (u8Ta2Result == 0)
			return 0;
		msleep(u32RxShmDelaysec*SHMOO_MSLP_DELAY_MS);
	}
	return 1;
}

static u8 dramc_rxshmoo_run(u8 u8channel, u8 u8rank, u8 u8byte, u8 u8dir)
{
	switch (u32RxShmDirection) {
	case SHIFT_L:
	case SHIFT_R:
	case SHIFT_L_RX_DQS:
	case SHIFT_R_RX_DQS:
		dramc_rxshmoo_run_phase(u8channel, u8rank, u8byte, u8dir);
		break;
	case SHIFT_D:
	case SHIFT_U:
		dramc_rxshmoo_run_vref(u8channel, u8rank, u8byte, u8dir);
		break;
	default:
		pr_crit("[%s] do nothing\n", __func__);
		return 0;
	}
	return 1;
}

static ssize_t dramc_rxshmoo_show(struct device_driver *drv, char *buf)
{
	//u32 reg_val=0;
	//#define SHMOO_DDRPHY_AO  0x1C380000
	//#define SHMOO_DRAMC_AO  0x1C38e000

	if ((IsMachli() == false) && (IsManks() == false))
		return 0;

	if (u32RxShmDelaysec) {
		_shm_ddrphy_ao_bs = ioremap(SHMOO_DDRPHY_AO, SHMOO_IOREMAP_SIZE);
		_shm_dramc_ao_bs = ioremap(SHMOO_DRAMC_AO, SHMOO_IOREMAP_SIZE);
		_shm_dramc_nao_bs = ioremap(SHMOO_DRAMC_NAO, SHMOO_IOREMAP_SIZE);
		if (!_shm_ddrphy_ao_bs || !_shm_dramc_ao_bs || !_shm_dramc_nao_bs) {
			if (!_shm_ddrphy_ao_bs)
				pr_crit("[%s] ioremap error for ddrphy_ao!\n", __func__);
			if (!_shm_dramc_ao_bs)
				pr_crit("[%s] ioremap error for dramc_ao!\n", __func__);
			if (!_shm_dramc_nao_bs)
				pr_crit("[%s] ioremap error for dramc_nao!\n", __func__);
			iounmap(_shm_ddrphy_ao_bs);
			iounmap(_shm_dramc_ao_bs);
			iounmap(_shm_dramc_nao_bs);
			return 0;
		}

		pr_err("[%s] RX Scan Start\n", __func__);
		dramc_rxshmoo_run((u8)u32RxShmChannel, (u8)u32RxShmRank, (u8)u32RxShmByte,
			(u8)u32RxShmDirection);
		iounmap(_shm_ddrphy_ao_bs);
		iounmap(_shm_dramc_ao_bs);
		iounmap(_shm_dramc_nao_bs);
		pr_err("[%s] RX Scan Done\n", __func__);

	} else {
		pr_crit("[%s] Please echo the parameters firstly!\n", __func__);
	}
	return 0;
}

static ssize_t dramc_rxshmoo_store(struct device_driver *drv, const char *buf, size_t count)
{
	char *str_dir = 0;
	size_t count_sscan = 0;

	if ((IsMachli() == false) && (IsManks() == false))
		return 0;

	count_sscan = sscanf(buf, "%d %d %d %d %d %d", &u32RxShmChannel, &u32RxShmRank,
		&u32RxShmByte, &u32RxShmDirection, &u32RxShmDelaysec, &u32RxShmTA2LoopTimes);
	if (count_sscan == 6) {
		switch (u32RxShmDirection) {
		case SHIFT_L:
			str_dir = "[L]: decrease rx dq phase"; break;
		case SHIFT_R:
			str_dir = "[R]: increase rx dq phase"; break;
		case SHIFT_D:
			str_dir = "[D]: decrease rx vref"; break;
		case SHIFT_U:
			str_dir = "[U]: increase rx vref"; break;
		case SHIFT_L_RX_DQS:
			str_dir = "[L]: decrease rx dqs phase"; break;
		case SHIFT_R_RX_DQS:
			str_dir = "[R]: increase rx dqs phase"; break;
		default:
			u32RxShmDirection = SHIFT_R;
			str_dir = "[R]: increase rx phase"; break;
		}

		pr_crit("\n[%s] RX Shmoo Parameters\n	Channel = %d\n	Rank = %d\n	Byte = %d\n	",
			__func__, u32RxShmChannel, u32RxShmRank, u32RxShmByte);
		pr_crit("Direction = %d (%s)\n	DelayTime(sec) = %d\n	TA2LoopTimes = %d\n\n",
			u32RxShmDirection, str_dir, u32RxShmDelaysec, u32RxShmTA2LoopTimes);

	} else {
		pr_crit("[%s] RX Shmoo Wrong Args (count=%d) (count_sscan=%d)!!!\n",
			__func__, (int)count, (int)count_sscan);
	}

	return count;
}
#endif//RUN_TIME_SHMOO_RX

#ifdef RUN_TIME_SHMOO_TX
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
// TX Window Center Assignment
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
static u32 u32TxWinChannel = CRB_MAX;
static u32 u32TxWinRank = CRB_MAX;
static u32 u32TxWinByte = CRB_MAX;
static u32 u32TxWinDirection;
static u32 u32TxWinMode;
static u32 u32TxWinChangeDelta = WIN_MAX;
static u32 u32TxWinTA2LoopTimes = 1;

//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
// TX Shmoo Vref+Phase Scan
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
static u32 u32TxShmChannel;
static u32 u32TxShmRank;
static u32 u32TxShmByte;
static u32 u32TxShmDirection;
static u32 u32TxShmDelaysec;
static u32 u32TxShmMode;
static u32 u32TxShmTA2LoopTimes = 1;

#define MR14_IDXMSK   0x001FFF00
#define MR14_RKMSK   0x03000000
#define MR14_VRFMSK   0x0000003F
#define MR14_RNGMSK   0x00000040
#define MR14_RNGSFT   6

#define MR14_VALMSK   (MR14_RNGMSK | MR14_VRFMSK)
#define MR14_INDEX   14 //MR14
#define MR14_RNK_SFT   24
#define MR14_IDX_SFT   8
#define MR14_RNG0_MAX   0x1E //overlapped max
#define MR14_RNG0_MIN   0
#define MR14_RNG1_MAX   0x32
#define MR14_RNG1_MIN   0

#define MR14_SSMSK   0x007F0000 //[6]:Range, [5:0]:Value
#define MR14_SSSFT   16

#define MR14_RD   ((IsMachli() == true) ? BIT(12) : BIT(1))
#define MR14_WR  ((IsMachli() == true) ? BIT(11) : BIT(0))
#define MR14_RD_RSPNS   BIT(1)
#define MR14_WR_RSPNS   BIT(0)
#define CKE_OFFON_MSK   (BIT(7) | BIT(6))
#define CKE_OFFON_01   (0 | BIT(6))
#define CKE1_OFFON_MSK   (BIT(5) | BIT(4))
#define CKE1_OFFON_01   (0 | BIT(4))

#define TX_PHASE_CMD_SFT   ((IsMachli() == true) ? 16 : 8)
#define TX_PHASE_CS_SFT   ((IsMachli() == true) ? 8 : 0)
#define TX_PHASE_DQM_B0_SFT   24
#define TX_PHASE_DQM_B1_SFT   16
#define TX_PHASE_DQ_B0_SFT   8
#define TX_PHASE_DQ_B1_SFT   0
#define TX_PHASE_DQS_SFT   24

#define TX_PHASE_XRPI_MSK   0x0000003F
#define TX_PHASE_XRPI_MAX   0x3F
#define MSCCTRL_MSK   BIT(22)
#define SETTXUPD_MSK   BIT(5)
#define LDTXUPD_MSK   BIT(6)
#define TCMDOTXUPD_MSK   (BIT(7)|BIT(6))

#define XRPI_CMD   0
#define XRPI_CS	  1
#define XRPI_DQM   2
#define XRPI_DQ	  3
#define XRPI_DQS   4
#define XRPI_MAX   5

#define MAX_LOOPS   1000

static u32 dramc_tx_vref_read(u8 u8channel, u8 u8rank, u8 u8byte)
{
	u32 u32addr_txdqvref = 0, u32val_txdqvref = 0, u32val_txdqvref_target = 0;
	u32 u32Loops = 0;

	pr_crit("[%s] ==========================================\n", __func__);
	pr_crit("[%s] u8channel(%d), u8rank(%d), u8byte(%d)\n",
		__func__, u8channel, u8rank, u8byte);
	pr_crit("[%s] ==========================================\n", __func__);

	// read rg value
	//MRIdx=14
	u32addr_txdqvref = TransAddr_dramc(
		DRAMC_REG_SWCMD_CTRL0_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_RD(u32addr_txdqvref);
	pr_err("[%s][1_1] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref = u32val_txdqvref & (~MR14_RKMSK) & (~MR14_IDXMSK);
	pr_err("[%s][1_2] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref |= ((u8rank << MR14_RNK_SFT) | (MR14_INDEX << MR14_IDX_SFT));
	pr_err("[%s][1_3] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	SHM_DRAMC_WR(u32val_txdqvref, u32addr_txdqvref);//Set MR14
	pr_err("[%s][1_4] DRAMC_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	//MRR trigger start
	u32addr_txdqvref = TransAddr_dramc(
		DRAMC_REG_SWCMD_EN_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_RD(u32addr_txdqvref);
	pr_err("[%s][2_1] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref |= MR14_RD;
	pr_err("[%s][2_2] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	SHM_DRAMC_WR(u32val_txdqvref, u32addr_txdqvref);
	pr_err("[%s][2_3] DRAMC_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	//MRR check response
	u32addr_txdqvref = TransAddr_dramc_nao(
		DRAMC_REG_SPCMDRESP_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_NAO_RD(u32addr_txdqvref);
	pr_err("[%s][nao] DRAMC_NAO_READ[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	while (u32Loops++ < MAX_LOOPS) {
		if (u32val_txdqvref & MR14_RD_RSPNS) { //read response
			u32addr_txdqvref = TransAddr_dramc_nao(
				DRAMC_REG_MRR_STATUS_Ofst, u8channel, u8rank, u8byte);
			u32val_txdqvref = SHM_DRAMC_NAO_RD(u32addr_txdqvref);
			pr_err("[%s][nao] DRAMC_NAO_READ[0x%08x] = 0x%08x\n",
				__func__, u32addr_txdqvref, u32val_txdqvref);
			u32val_txdqvref_target = (u32val_txdqvref & MR14_SSMSK) >> MR14_SSSFT;
			pr_crit("[%s][out] u32val_txdqvref_target = 0x%08x\n",
				__func__, u32val_txdqvref_target);
			break;
		}
		usleep_range(SHMOO_DELAY_1MS_MIN, SHMOO_DELAY_1MS_MAX);
	}
	//MRR trigger done
	u32addr_txdqvref = TransAddr_dramc(
		DRAMC_REG_SWCMD_EN_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_RD(u32addr_txdqvref);
	pr_err("[%s][3_1] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref &= (~MR14_RD);
	pr_err("[%s][3_2] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	SHM_DRAMC_WR(u32val_txdqvref, u32addr_txdqvref);
	pr_err("[%s][3_3] DRAMC_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	pr_crit("[%s] done\n\n", __func__);
	return u32val_txdqvref_target;
}

static u8 dramc_tx_vref_write(u8 u8channel, u8 u8rank, u8 u8byte, u32 u32vref_target)
{
	u32 u32addr_txdqvref = 0, u32val_txdqvref = 0;
	u32 u32Loops = 0;

	pr_crit("[%s] ==========================================\n", __func__);
	pr_crit("[%s] u8channel(%d), u8rank(%d), u8byte(%d), u32vref_target = 0x%08x\n",
		__func__, u8channel, u8rank, u8byte, u32vref_target);
	pr_crit("[%s] ==========================================\n", __func__);

	// read rg value
	//cke fix_off=0, fix_on=1
	u32addr_txdqvref = TransAddr_dramc(
		DRAMC_REG_CKECTRL_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_RD(u32addr_txdqvref);
	pr_err("[%s][1_1] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref = (u32val_txdqvref & ((u8rank == 0) ? (~CKE_OFFON_MSK) : (~CKE1_OFFON_MSK)))
		| ((u8rank == 0) ? CKE_OFFON_01 : CKE1_OFFON_01);
	pr_err("[%s][1_2] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	SHM_DRAMC_WR(u32val_txdqvref, u32addr_txdqvref);
	pr_err("[%s][1_3] DRAMC_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	//MRIdx=14 + MR rank + MR Ref val
	u32addr_txdqvref = TransAddr_dramc(
		DRAMC_REG_SWCMD_CTRL0_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_RD(u32addr_txdqvref);
	pr_err("[%s][2_1] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref = u32val_txdqvref & (~MR14_RKMSK) & (~MR14_IDXMSK) & (~MR14_VALMSK);
	pr_err("[%s][2_2] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref = u32val_txdqvref | (u8rank << MR14_RNK_SFT) | (MR14_INDEX << MR14_IDX_SFT)
		| (u32vref_target & MR14_VALMSK);
	pr_err("[%s][2_3] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	SHM_DRAMC_WR(u32val_txdqvref, u32addr_txdqvref);//Set MR14
	pr_crit("[%s][2_4] DRAMC_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	//MRW trigger start
	u32addr_txdqvref = TransAddr_dramc(
		DRAMC_REG_SWCMD_EN_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_RD(u32addr_txdqvref);
	pr_err("[%s][3_1] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref |= MR14_WR;
	pr_err("[%s][3_2] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	SHM_DRAMC_WR(u32val_txdqvref, u32addr_txdqvref);
	pr_err("[%s][3_3] DRAMC_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	//MRW check response
	u32addr_txdqvref = TransAddr_dramc_nao(
		DRAMC_REG_SPCMDRESP_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_NAO_RD(u32addr_txdqvref);
	pr_err("[%s][nao] DRAMC_NAO_READ[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	while (u32Loops++ < MAX_LOOPS) {
		if (u32val_txdqvref & MR14_WR_RSPNS)//write response
			break;
		usleep_range(SHMOO_DELAY_1MS_MIN, SHMOO_DELAY_1MS_MAX);
	}
	//MRW trigger done
	u32addr_txdqvref = TransAddr_dramc(
		DRAMC_REG_SWCMD_EN_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_RD(u32addr_txdqvref);
	pr_err("[%s][4_1] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref &= (~MR14_WR);
	pr_err("[%s][4_2] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	SHM_DRAMC_WR(u32val_txdqvref, u32addr_txdqvref);
	pr_err("[%s][4_3] DRAMC_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	//cke fix_off=0, fix_on=0
	u32addr_txdqvref = TransAddr_dramc(
		DRAMC_REG_CKECTRL_Ofst, u8channel, u8rank, u8byte);
	u32val_txdqvref = SHM_DRAMC_RD(u32addr_txdqvref);
	pr_err("[%s][5_1] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	u32val_txdqvref &= ((u8rank == 0) ? (~CKE_OFFON_MSK) : (~CKE1_OFFON_MSK));
	pr_err("[%s][5_2] u32val_txdqvref = 0x%08x\n", __func__, u32val_txdqvref);
	SHM_DRAMC_WR(u32val_txdqvref, u32addr_txdqvref);
	pr_err("[%s][5_3] DRAMC_WRITE[0x%08x] = 0x%08x\n",
		__func__, u32addr_txdqvref, u32val_txdqvref);
	//pr_crit("[%s] done\n\n", __func__);
	return 1;
}

static u32 dramc_tx_phase_read(u8 u8channel, u8 u8rank, u8 u8byte, u8 u8mode)
{
	u32 u32addr_cmdcs = 0, u32addr_dqmdq = 0, u32addr_dqs = 0;
	u32 u32val_cmdcs = 0, u32val_dqmdq = 0, u32val_dqs = 0;
	u32 u32addr_target = 0, u32val_target = 0, u32sftnm = 0;
	u8 *u8str = 0;

	pr_crit("[%s] ==========================================\n", __func__);
	pr_crit("[%s] u8channel(%d), u8rank(%d), u8byte(%d), u8mode(%d)\n", __func__,
		u8channel, u8rank, u8byte, u8mode);
	pr_crit("[%s] ==========================================\n\n", __func__);

	//########################
	// read rg value
	//########################
	if ((u8mode == XRPI_CMD) || (u8mode == XRPI_CS)) {
		u32addr_cmdcs = TransAddr_ddrphy(
			DDRPHY_REG_SHU_R0_CA_CMD0_Ofst, u8channel, u8rank, u8byte);
		u32val_cmdcs = SHM_DDRPHY_RD(u32addr_cmdcs);
		pr_err("[%s][CMD_CS] DDRPHY_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_cmdcs, u32val_cmdcs);
	}
	if ((u8mode == XRPI_DQM) || (u8mode == XRPI_DQ)) {
		u32addr_dqmdq = TransAddr_dramc(
			DRAMC_REG_SHURK_PI_Ofst, u8channel, u8rank, u8byte);
		u32val_dqmdq = SHM_DRAMC_RD(u32addr_dqmdq);
		pr_err("[%s][DQM_DQ] DRAMC_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_dqmdq, u32val_dqmdq);
	}
	if (u8mode == XRPI_DQS) {
		u32addr_dqs = TransAddr_ddrphy(
			DDRPHY_REG_SHU_R0_B0_DQ0_Ofst, u8channel, u8rank, u8byte);
		u32val_dqs = SHM_DDRPHY_RD(u32addr_dqs);
		pr_err("[%s][DQS] DDRPHY_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_dqs, u32val_dqs);
	}

	//########################
	// get phase value
	//########################
	switch (u8mode) {
	case XRPI_CMD:
		u32sftnm = TX_PHASE_CMD_SFT;
		u32val_target = u32val_cmdcs;
		u32addr_target = u32addr_cmdcs;
		u8str = "CMD";
		break;
	case XRPI_CS:
		u32sftnm = TX_PHASE_CS_SFT;
		u32val_target = u32val_cmdcs;
		u32addr_target = u32addr_cmdcs;
		u8str = "CS";
		break;
	case XRPI_DQM:
		u32sftnm = (u8byte == 0) ? TX_PHASE_DQM_B0_SFT : TX_PHASE_DQM_B1_SFT;
		u32val_target = u32val_dqmdq;
		u32addr_target = u32addr_dqmdq;
		u8str = (u8byte == 0) ? "DQM_B0" : "DQM_B1";
		break;
	case XRPI_DQ:
		u32sftnm = (u8byte == 0) ? TX_PHASE_DQ_B0_SFT : TX_PHASE_DQ_B1_SFT;
		u32val_target = u32val_dqmdq;
		u32addr_target = u32addr_dqmdq;
		u8str = (u8byte == 0) ? "DQ_B0" : "DQ_B1";
		break;
	case XRPI_DQS:
		u32sftnm = TX_PHASE_DQS_SFT;
		u32val_target = u32val_dqs;
		u32addr_target = u32addr_dqs;
		u8str = (u8byte == 0) ? "DQS_B0" : "DQS_B1";
		break;
	default:
		pr_crit("[%s] ERROR: mode(%d) cannot be handled\n\n", __func__, u8mode);
		return 0;
	}

	u32val_target = (u32val_target & (TX_PHASE_XRPI_MSK << u32sftnm)) >> u32sftnm;
	pr_crit("[%s][%s] REG[0x%08x] = 0x%08x\n\n",
		__func__, u8str, u32addr_target, u32val_target);
	return u32val_target;
}

//##################################################################(Fix CCN)
static u8 dramc_tx_phase_write_cmdcs(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8mode, u32 u32phase_target)
{
	u32 u32addr_cmdcs = 0, u32val_cmdcs = 0;
	u32 u32addr_target = 0, u32val_target = 0, u32sftnm = 0;
	u8 *u8str = 0;
	//==== Machli Only ====
	u32 u32addr_ctrl = 0;
	u32 u32val_ctrl = 0;
	//==== Machli Only ====

	pr_crit("[%s] ==========================================\n", __func__);
	pr_crit("[%s] u8channel(%d), u8rank(%d), u8byte(%d), u8mode(%d), u32phase_target = 0x%08x\n",
		__func__, u8channel, u8rank, u8byte, u8mode, u32phase_target);
	pr_crit("[%s] ==========================================\n\n", __func__);

	//########################
	// read rg value
	//########################
	if ((u8mode == XRPI_CMD) || (u8mode == XRPI_CS)) {
		u32addr_cmdcs = TransAddr_ddrphy(
			DDRPHY_REG_SHU_R0_CA_CMD0_Ofst, u8channel, u8rank, u8byte);
		u32val_cmdcs = SHM_DDRPHY_RD(u32addr_cmdcs);
		pr_err("[%s][CMD_CS] DDRPHY_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_cmdcs, u32val_cmdcs);
	}

	//########################
	// write phase pre-setting
	//########################
	//==== Machli Only ====
	if (IsMachli() == true) {
		if ((u8mode == XRPI_CMD) || (u8mode == XRPI_CS)) {
			//disable CA PI DCM
			u32addr_ctrl = TransAddr_ddrphy(
				DDRPHY_REG_MISC_CG_CTRL5_Ofst, u8channel, u8rank, u8byte);
			u32val_ctrl = SHM_DDRPHY_RD(u32addr_ctrl);
			pr_err("[%s][MSC_CTRL] DDRPHY_READ[0x%08x] = 0x%08x\n",
				__func__, u32addr_ctrl, u32val_ctrl);
			u32val_ctrl &= (~MSCCTRL_MSK);
			SHM_DDRPHY_WR(u32val_ctrl, u32addr_ctrl);
			pr_err("[%s][MSC_CTRL] DDRPHY_WRITE[0x%08x] = 0x%08x\n\n",
				__func__, u32addr_ctrl, u32val_ctrl);
		}
	}
	//==== Machli Only ====

	//########################
	// get phase value
	//########################
	switch (u8mode) {
	case XRPI_CMD:
		u32sftnm = TX_PHASE_CMD_SFT;
		u32val_target = u32val_cmdcs;
		u32addr_target = u32addr_cmdcs;
		u8str = "CMD";
		break;
	case XRPI_CS:
		u32sftnm = TX_PHASE_CS_SFT;
		u32val_target = u32val_cmdcs;
		u32addr_target = u32addr_cmdcs;
		u8str = "CS";
		break;
	default:
		pr_crit("[%s] ERROR: mode(%d) cannot be handled\n\n", __func__, u8mode);
		return 0;
	}

	pr_err("[%s][T0][%s] u32val_target = 0x%08x\n\n", __func__, u8str, u32val_target);
	u32val_target &=  (~(TX_PHASE_XRPI_MSK << u32sftnm));
	pr_err("[%s][T1][%s] u32val_target = 0x%08x\n\n", __func__, u8str, u32val_target);
	u32val_target |= (u32phase_target << u32sftnm);
	pr_err("[%s][S2][%s] u32val_target = 0x%08x\n\n", __func__, u8str, u32val_target);

	//########################
	// write phase value
	//########################
	if ((u8mode == XRPI_CMD) || (u8mode == XRPI_CS)) {
		SHM_DDRPHY_WR(u32val_target, u32addr_target);
		pr_crit("[%s][%s] DDRPHY_WRITE[0x%08x] = 0x%08x\n\n",
			__func__, u8str, u32addr_target, u32val_target);
	}

	//########################
	// write phase post-setting
	//########################
	//==== Machli Only ====
	if (IsMachli() == true) {
		if ((u8mode == XRPI_CMD) || (u8mode == XRPI_CS)) {
			//enable CA PI DCM
			u32addr_ctrl = TransAddr_ddrphy(
				DDRPHY_REG_MISC_CG_CTRL5_Ofst, u8channel, u8rank, u8byte);
			u32val_ctrl = SHM_DDRPHY_RD(u32addr_ctrl);
			pr_err("[%s][MSC_CTRL] DDRPHY_READ[0x%08x] = 0x%08x\n",
				__func__, u32addr_ctrl, u32val_ctrl);
			u32val_ctrl |= MSCCTRL_MSK;
			SHM_DDRPHY_WR(u32val_ctrl, u32addr_ctrl);
			pr_err("[%s][MSC_CTRL] DDRPHY_WRITE[0x%08x] = 0x%08x\n\n",
				__func__, u32addr_ctrl, u32val_ctrl);
		}
	}
	//==== Machli Only ====

	return 1;
}


static u8 dramc_tx_phase_write_dqsdqmdq(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8mode, u32 u32phase_target)
{
	u32 u32addr_dqmdq = 0, u32addr_dqs = 0;
	u32 u32val_dqmdq = 0, u32val_dqs = 0;
	u32 u32addr_target = 0, u32val_target = 0, u32sftnm = 0;
	u8 *u8str = 0;
	//==== Machli Only ====
	u32 u32addr_dqsoscr = 0, u32addr_tcmdo = 0;
	u32 u32val_dqsoscr = 0, u32val_tcmdo = 0;
	u32 u32Loops = 0;
	//==== Machli Only ====

	pr_crit("[%s] ==========================================\n", __func__);
	pr_crit("[%s] u8channel(%d), u8rank(%d), u8byte(%d), u8mode(%d), u32phase_target = 0x%08x\n",
		__func__, u8channel, u8rank, u8byte, u8mode, u32phase_target);
	pr_crit("[%s] ==========================================\n\n", __func__);

	//########################
	// read rg value
	//########################
	if ((u8mode == XRPI_DQM) || (u8mode == XRPI_DQ)) {
		u32addr_dqmdq = TransAddr_dramc(
			DRAMC_REG_SHURK_PI_Ofst, u8channel, u8rank, u8byte);
		u32val_dqmdq = SHM_DRAMC_RD(u32addr_dqmdq);
		pr_err("[%s][DQM_DQ] DRAMC_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_dqmdq, u32val_dqmdq);
	}
	if (u8mode == XRPI_DQS) {
		u32addr_dqs = TransAddr_ddrphy(
			DDRPHY_REG_SHU_R0_B0_DQ0_Ofst, u8channel, u8rank, u8byte);
		u32val_dqs = SHM_DDRPHY_RD(u32addr_dqs);
		pr_err("[%s][DQS] DDRPHY_READ[0x%08x] = 0x%08x\n",
			__func__, u32addr_dqs, u32val_dqs);
	}

	//########################
	// write phase pre-setting
	//########################
	//==== Machli Only ====
	//if (IsMachli() == true) {
	//	if ((u8mode == XRPI_DQM) || (u8mode == XRPI_DQ) || (u8mode == XRPI_DQS)) {
	//		// Enable TX PI manual mode
	//		u32addr_dqsoscr = TransAddr_dramc(
	//			DRAMC_REG_DQSOSCR_Ofst, u8channel, u8rank, u8byte);
	//		u32val_dqsoscr = SHM_DRAMC_RD(u32addr_dqsoscr);
	//		pr_err("[%s][DQSOSCR] DRAMC_READ[0x%08x] = 0x%08x\n",
	//			__func__, u32addr_dqsoscr, u32val_dqsoscr);
	//		u32val_dqsoscr |= SETTXUPD_MSK;
	//		SHM_DRAMC_WR(u32val_dqsoscr, u32addr_dqsoscr);
	//		pr_err("[%s][DQSOSCR] DRAMC_WRITE[0x%08x] = 0x%08x\n\n",
	//			__func__, u32addr_dqsoscr, u32val_dqsoscr);
	//	}
	//}
	//==== Machli Only ====

	//########################
	// get phase value
	//########################
	switch (u8mode) {
	case XRPI_DQM:
		u32sftnm = (u8byte == 0) ? TX_PHASE_DQM_B0_SFT : TX_PHASE_DQM_B1_SFT;
		u32val_target = u32val_dqmdq;
		u32addr_target = u32addr_dqmdq;
		u8str = (u8byte == 0) ? "DQM_B0" : "DQM_B1";
		break;
	case XRPI_DQ:
		u32sftnm = (u8byte == 0) ? TX_PHASE_DQ_B0_SFT : TX_PHASE_DQ_B1_SFT;
		u32val_target = u32val_dqmdq;
		u32addr_target = u32addr_dqmdq;
		u8str = (u8byte == 0) ? "DQ_B0" : "DQ_B1";
		break;
	case XRPI_DQS:
		u32sftnm = TX_PHASE_DQS_SFT;
		u32val_target = u32val_dqs;
		u32addr_target = u32addr_dqs;
		u8str = (u8byte == 0) ? "DQS_B0" : "DQS_B1";
		break;
	default:
		pr_crit("[%s] ERROR: mode(%d) cannot be handled\n\n", __func__, u8mode);
		return 0;
	}

	pr_err("[%s][T0][%s] u32val_target = 0x%08x\n\n", __func__, u8str, u32val_target);
	u32val_target &=  (~(TX_PHASE_XRPI_MSK << u32sftnm));
	pr_err("[%s][T1][%s] u32val_target = 0x%08x\n\n", __func__, u8str, u32val_target);
	u32val_target |= (u32phase_target << u32sftnm);
	pr_err("[%s][S2][%s] u32val_target = 0x%08x\n\n", __func__, u8str, u32val_target);

	//########################
	// write phase value
	//########################
	if ((u8mode == XRPI_DQM) || (u8mode == XRPI_DQ)) {
		SHM_DRAMC_WR(u32val_target, u32addr_target);
		pr_crit("[%s][%s] DRAMC_WRITE[0x%08x] = 0x%08x\n\n",
			__func__, u8str, u32addr_target, u32val_target);
	} else {
		SHM_DDRPHY_WR(u32val_target, u32addr_target);
		pr_crit("[%s][%s] DDRPHY_WRITE[0x%08x] = 0x%08x\n\n",
			__func__, u8str, u32addr_target, u32val_target);
	}

	//########################
	// write phase post-setting
	//########################
	//==== Machli Only ====
	if (IsMachli() == true) {
		if (u8mode == XRPI_DQS) {
			// Load new TX SW value
			u32addr_dqsoscr = TransAddr_dramc(
				DRAMC_REG_DQSOSCR_Ofst, u8channel, u8rank, u8byte);
			u32val_dqsoscr = SHM_DRAMC_RD(u32addr_dqsoscr);
			pr_err("[%s][DQSOSCR] DRAMC_READ[0x%08x] = 0x%08x\n",
				__func__, u32addr_dqsoscr, u32val_dqsoscr);
			u32val_dqsoscr |= LDTXUPD_MSK;
			SHM_DRAMC_WR(u32val_dqsoscr, u32addr_dqsoscr);
			pr_err("[%s][DQSOSCR] DRAMC_WRITE[0x%08x] = 0x%08x\n\n",
				__func__, u32addr_dqsoscr, u32val_dqsoscr);
			//wait response for finishing Load new TX SW value
			u32addr_tcmdo = TransAddr_dramc_nao(
				DRAMC_REG_TCMDO1LAT_Ofst, u8channel, u8rank, u8byte);
			u32val_tcmdo = SHM_DRAMC_NAO_RD(u32addr_tcmdo);
			pr_err("[%s][nao] DRAMC_NAO_READ[0x%08x] = 0x%08x\n",
				__func__, u32addr_tcmdo, u32val_tcmdo);
			while (u32Loops++ < MAX_LOOPS) {
				//write response
				if ((u32val_tcmdo & TCMDOTXUPD_MSK) == TCMDOTXUPD_MSK)
					break;
				usleep_range(SHMOO_DELAY_1MS_MIN, SHMOO_DELAY_1MS_MAX);
			}
			// Rollback Setting
			u32addr_dqsoscr = TransAddr_dramc(
				DRAMC_REG_DQSOSCR_Ofst, u8channel, u8rank, u8byte);
			u32val_dqsoscr = SHM_DRAMC_RD(u32addr_dqsoscr);
			pr_err("[%s][DQSOSCR] DRAMC_READ[0x%08x] = 0x%08x\n",
				__func__, u32addr_dqsoscr, u32val_dqsoscr);
			u32val_dqsoscr = u32val_dqsoscr & (~LDTXUPD_MSK) & (~SETTXUPD_MSK);
			SHM_DRAMC_WR(u32val_dqsoscr, u32addr_dqsoscr);
			pr_err("[%s][DQSOSCR] DRAMC_WRITE[0x%08x] = 0x%08x\n\n",
				__func__, u32addr_dqsoscr, u32val_dqsoscr);
		}
	}
	//==== Machli Only ====

	return 1;
}

static u8 dramc_tx_phase_write(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8mode, u32 u32phase_target)
{
	u8 u8Result = 0;

	pr_crit("[%s] ==========================================\n", __func__);
	pr_crit("[%s] u8channel(%d), u8rank(%d), u8byte(%d), u8mode(%d), u32phase_target = 0x%08x\n",
		__func__, u8channel, u8rank, u8byte, u8mode, u32phase_target);
	pr_crit("[%s] ==========================================\n\n", __func__);

	//########################
	// read rg value
	//########################
	// check if tx phase cmd/cs
	if ((u8mode == XRPI_CMD) || (u8mode == XRPI_CS))
		u8Result = dramc_tx_phase_write_cmdcs(u8channel, u8rank, u8byte,
			u8mode, u32phase_target);
	else
		u8Result = dramc_tx_phase_write_dqsdqmdq(u8channel, u8rank, u8byte,
			u8mode, u32phase_target);

	return u8Result;
}
//##################################################################(Fix CCN)

static u8 dramc_txwincenter_disp(void)
{
	u8 u8channel = 0, u8rank = 0, u8byte = 0, u8mode = 0;

	pr_crit("\n\n########################\n");
	pr_crit("Disp dramc_txwincenter\n");
	pr_crit("########################\n\n");
	for (u8channel = 0; u8channel < CHANNEL_MAX; u8channel++) {
		for (u8rank = 0; u8rank < RANK_MAX; u8rank++) {
			dramc_tx_vref_read(u8channel, u8rank, u8byte);
			for (u8byte = 0; u8byte < BYTE_MAX; u8byte++) {
				for (u8mode = 0; u8mode < XRPI_MAX; u8mode++)
					dramc_tx_phase_read(u8channel, u8rank, u8byte, u8mode);
			}
		}
	}
	pr_crit("\n\n");
	return 1;
}

static u8 dramc_txwincenter_run_vref(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8dir, u8 u8mode, u8 u8changedelta, u32 u32ta2looptimes)
{
	u32 u32val_txvref_target = 0;
	s32 s32val_txvref_range = 0, s32val_txvref_vref = 0;
	u8 u8Result = 1;

	pr_crit("\n\n########################\n");
	pr_crit("Run dramc_txwincenter for Vref\n");
	pr_crit("########################\n\n");

	// read tx vref value
	if ((u8dir == SHIFT_D) || (u8dir == SHIFT_U)) {
		u32val_txvref_target = dramc_tx_vref_read(u8channel, u8rank, u8byte);
		s32val_txvref_range = (s32)((u32val_txvref_target & MR14_RNGMSK) >> MR14_RNGSFT);
		s32val_txvref_vref = (s32)(u32val_txvref_target & MR14_VRFMSK);
		pr_crit("[%s] TX Vref Value = Range[%d], Vref[0x%02x]\n",
			__func__, s32val_txvref_range, s32val_txvref_vref);
	}

	switch (u8dir) {
	case SHIFT_D: //decrease vref
		s32val_txvref_vref -= (s32)u8changedelta;
		//check if reaches range 1 (32.9%)
		if ((s32val_txvref_range == 1) && (s32val_txvref_vref < MR14_RNG1_MIN)) {
			pr_crit("[%s] TX Vref Change to range 0 (32.9%)\n", __func__);
			s32val_txvref_range = 0;
			s32val_txvref_vref += MR14_RNG0_MAX;
			u8Result = 0;
		}
		//check if reaches range 0 (15.0%)
		if ((s32val_txvref_range == 0) && (s32val_txvref_vref < MR14_RNG0_MIN)) {
			pr_crit("[%s] TX Vref Keep as range 0 (15.0%)\n", __func__);
			s32val_txvref_vref = MR14_RNG0_MIN;
		}
		if (s32val_txvref_range == 1)
			s32val_txvref_vref |= MR14_RNGMSK;

		u32val_txvref_target = s32val_txvref_vref;
		pr_crit("[%s] Set TX Vref = 0x%08x\n", __func__, u32val_txvref_target);
		dramc_tx_vref_write(u8channel, u8rank, u8byte, u32val_txvref_target);
		break;
	case SHIFT_U: //increase vref
		s32val_txvref_vref += (s32)u8changedelta;
		//check if reaches range 0 (32.9%)
		if ((s32val_txvref_range == 0) && (s32val_txvref_vref >= MR14_RNG0_MAX)) {
			pr_crit("[%s] TX Vref Change to range 1 (32.9%)\n", __func__);
			s32val_txvref_range = 1;
			s32val_txvref_vref -= MR14_RNG0_MAX;
		}
		//check if reaches range 1 (62.9%)
		if ((s32val_txvref_range == 1) && (s32val_txvref_vref >= MR14_RNG1_MAX)) {
			pr_crit("[%s] TX Vref Keep as range 1 (62.9%)\n", __func__);
			s32val_txvref_vref = MR14_RNG1_MAX;
			u8Result = 0;
		}
		if (s32val_txvref_range == 1)
			s32val_txvref_vref |= MR14_RNGMSK;

		u32val_txvref_target = s32val_txvref_vref;
		pr_crit("[%s] Set TX Vref = 0x%08x\n", __func__, u32val_txvref_target);
		dramc_tx_vref_write(u8channel, u8rank, u8byte, u32val_txvref_target);
		break;
	default:
		pr_crit("[%s] TX Direction Error: do nothing\n", __func__);
		return 0;
	}

	return u8Result;
}

static u8 dramc_txwincenter_run_phase(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8dir, u8 u8mode, u8 u8changedelta, u32 u32ta2looptimes)
{
	u32 u32val_txphase_target = 0;
	s32 s32val_txphase_update = 0, s32val_txphase_maximum = 0;
	u8 u8Result = 1;

	pr_crit("\n\n########################\n");
	pr_crit("Run dramc_txwincenter for Phase\n");
	pr_crit("########################\n\n");

	// read tx phase value by mode
	if ((u8dir == SHIFT_L) || (u8dir == SHIFT_R)) {
		s32val_txphase_update = dramc_tx_phase_read(u8channel, u8rank, u8byte, u8mode);
		pr_crit("[%s] TX Phase[%d] Value = 0x%02x\n",
			__func__, u8mode, s32val_txphase_update);
	}

	switch (u8dir) {
	case SHIFT_L: //decrease phase
		s32val_txphase_update -= (s32)u8changedelta;
		if (s32val_txphase_update < 0) {
			pr_crit("[%s] TX Phase(%d) touch MIN(0) !!!\n", __func__, u8mode);
			s32val_txphase_update = 0;
			u8Result = 0;
		}
		u32val_txphase_target = s32val_txphase_update;
		pr_crit("[%s] Set TX Phase = 0x%08x\n", __func__, u32val_txphase_target);
		dramc_tx_phase_write(u8channel, u8rank, u8byte, u8mode, u32val_txphase_target);
		break;
	case SHIFT_R: //increase phase
		s32val_txphase_update += (s32)u8changedelta;
		s32val_txphase_maximum = TX_PHASE_XRPI_MAX;
		if (s32val_txphase_update > s32val_txphase_maximum) {
			pr_crit("[%s] TX Phase(%d) touch MAX(0x%02X) !!!\n",
				__func__, u8mode, s32val_txphase_maximum);
			s32val_txphase_update = s32val_txphase_maximum;
			u8Result = 0;
		}
		u32val_txphase_target = s32val_txphase_update;
		pr_crit("[%s] Set TX Phase = 0x%08x\n", __func__, u32val_txphase_target);
		dramc_tx_phase_write(u8channel, u8rank, u8byte, u8mode, u32val_txphase_target);
		break;
	default:
		pr_crit("[%s] TX Direction Error: do nothing\n", __func__);
		return 0;
	}

	return u8Result;
}

static u8 dramc_txwincenter_run(u8 u8channel, u8 u8rank, u8 u8byte,
	u8 u8dir, u8 u8mode, u8 u8changedelta, u32 u32ta2looptimes)
{
	u8 u8Result = 0;

	pr_crit("\n\n########################\n");
	pr_crit("Run dramc_txwincenter\n");
	pr_crit("########################\n\n");

	// check parameters
	if ((u8channel >= CHANNEL_MAX) || (u8rank >= RANK_MAX) || (u8byte >= BYTE_MAX)) {
		pr_crit("\n[%s] ====================================\n", __func__);
		if (u8channel >= CHANNEL_MAX)
			pr_crit("[%s] ERROR: Only support channel (0 ~ %d)\n",
				__func__, (CHANNEL_MAX-1));
		if (u8rank >= RANK_MAX)
			pr_crit("[%s] ERROR: Only support rank (0 ~ %d)\n",
				__func__, (RANK_MAX-1));
		if (u8byte >= BYTE_MAX)
			pr_crit("[%s] ERROR: Only support byte (0 ~ %d)\n",
				__func__, (BYTE_MAX-1));
		pr_crit("[%s] ====================================\n\n", __func__);
		return 0;
	}

	// check if tx vref
	if ((u8dir == SHIFT_D) || (u8dir == SHIFT_U))
		u8Result = dramc_txwincenter_run_vref(u8channel, u8rank, u8byte,
			u8dir, u8mode, u8changedelta, u32ta2looptimes);
	else // tx phase
		u8Result = dramc_txwincenter_run_phase(u8channel, u8rank, u8byte,
			u8dir, u8mode, u8changedelta, u32ta2looptimes);

	if (u8Result == 0)
		return 0;

	if (runtime_shmoo_ta2(u8rank, u32ta2looptimes) == 0) {
		pr_crit("\n[%s] ====================================\n", __func__);
		pr_crit("[%s] TA2 ERROR: STOP\n", __func__);
		pr_crit("[%s] ====================================\n\n", __func__);
		u8Result = 0;
	}

	return u8Result;
}

static ssize_t dramc_txwincenter_show(struct device_driver *drv, char *buf)
{
	if ((IsMachli() == false) && (IsManks() == false))
		return 0;

	if (1) { //if((u8TxWinChannel!=CRB_MAX) && (u8TxWinRank!=CRB_MAX) && (u8TxWinByte!=CRB_MAX))
		_shm_ddrphy_ao_bs = ioremap(SHMOO_DDRPHY_AO, SHMOO_IOREMAP_SIZE);
		_shm_dramc_ao_bs = ioremap(SHMOO_DRAMC_AO, SHMOO_IOREMAP_SIZE);
		_shm_dramc_nao_bs = ioremap(SHMOO_DRAMC_NAO, SHMOO_IOREMAP_SIZE);
		if (!_shm_ddrphy_ao_bs || !_shm_dramc_ao_bs || !_shm_dramc_nao_bs) {
			if (!_shm_ddrphy_ao_bs)
				pr_crit("[%s] ioremap error for ddrphy_ao!\n", __func__);
			else if (!_shm_dramc_ao_bs)
				pr_crit("[%s] ioremap error for dramc_ao!\n", __func__);
			else if (!_shm_dramc_nao_bs)
				pr_crit("[%s] ioremap error for dramc_nao!\n", __func__);
			return 0;
		}

		pr_crit("\n\n############################################################\n");
		pr_crit("%s\n", RUN_TIME_SHMOO_VERSION);
		pr_crit("############################################################\n\n");
		pr_err("TX Center Display Start\n");
		dramc_txwincenter_disp();
		iounmap(_shm_ddrphy_ao_bs);
		iounmap(_shm_dramc_ao_bs);
		iounmap(_shm_dramc_nao_bs);
		pr_err("TX Center Display Done\n");
	} else {
		pr_crit("Please echo the parameters firstly!\n");
	}
	return 0;
}

static ssize_t dramc_txwincenter_store(struct device_driver *drv, const char *buf, size_t count)
{
	char *str_dir = 0;
	size_t count_sscan = 0;

	if ((IsMachli() == false) && (IsManks() == false))
		return 0;

	count_sscan = sscanf(buf, "%d %d %d %d %d %d %d",
		&u32TxWinChannel, &u32TxWinRank, &u32TxWinByte,
		&u32TxWinDirection, &u32TxWinChangeDelta,
		&u32TxWinTA2LoopTimes, &u32TxWinMode);
	if (count_sscan == 7) {
		if (u32TxWinChangeDelta != WIN_MAX) {
			_shm_ddrphy_ao_bs = ioremap(SHMOO_DDRPHY_AO, SHMOO_IOREMAP_SIZE);
			_shm_dramc_ao_bs = ioremap(SHMOO_DRAMC_AO, SHMOO_IOREMAP_SIZE);
			_shm_dramc_nao_bs = ioremap(SHMOO_DRAMC_NAO, SHMOO_IOREMAP_SIZE);
			if (!_shm_ddrphy_ao_bs || !_shm_dramc_ao_bs || !_shm_dramc_nao_bs) {
				if (!_shm_ddrphy_ao_bs)
					pr_crit("[%s] ioremap error for ddrphy_ao!\n", __func__);
				else if (!_shm_dramc_ao_bs)
					pr_crit("[%s] ioremap error for dramc_ao!\n", __func__);
				else if (!_shm_dramc_nao_bs)
					pr_crit("[%s] ioremap error for dramc_nao!\n", __func__);
				return 0;
			}
			switch (u32TxWinDirection) {
			case SHIFT_L:
				str_dir = "[L]: decrease tx phase"; break;
			case SHIFT_R:
				str_dir = "[R]: increase tx phase"; break;
			case SHIFT_D:
				str_dir = "[D]: decrease tx vref"; break;
			case SHIFT_U:
				str_dir = "[U]: increase tx vref"; break;
			default:
				u32TxWinDirection = SHIFT_R;
				str_dir = "[R]: increase tx phase"; break;
			}

			pr_crit("\n\n############################################################\n");
			pr_crit("%s\n", RUN_TIME_SHMOO_VERSION);
			pr_crit("############################################################\n\n");
			pr_crit("\n[%s] TX Center Parameters\n	Channel = %d\n	Rank = %d\n	Byte = %d\n	",
				__func__, u32TxWinChannel, u32TxWinRank, u32TxWinByte);
			pr_crit("\nDirection = %d (%s)\n	Delta = %d\n	TA2LoopTimes = %d\n	Mode = %d\n\n",
				u32TxWinDirection, str_dir, u32TxWinChangeDelta,
				u32TxWinTA2LoopTimes, u32TxWinMode);
			pr_err("[%s] TX Center Run Start\n", __func__);
			dramc_txwincenter_run((u8)u32TxWinChannel, (u8)u32TxWinRank,
				(u8)u32TxWinByte, (u8)u32TxWinDirection, (u8)u32TxWinMode,
				(u8)u32TxWinChangeDelta, u32TxWinTA2LoopTimes);
			iounmap(_shm_ddrphy_ao_bs);
			iounmap(_shm_dramc_ao_bs);
			iounmap(_shm_dramc_nao_bs);
			pr_err("[%s] TX Center Run Done\n", __func__);
		} else {
			pr_crit("[%s] TX Center Args must be assigned values!!!\n", __func__);
		}
	} else {
		pr_crit("[%s] TX Center Wrong Args (count=%d) (count_sscan=%d)!!!\n",
			__func__, (int)count, (int)count_sscan);
	}

	return count;
}

static u8 dramc_txshmoo_run_vref(u8 u8channel, u8 u8rank, u8 u8byte, u8 u8dir, u8 u8mode)
{
	u8 u8changedelta = 0, u8Ta2Result = 0;
	u32 u32ta2looptimes = 0;

	pr_crit("\n[%s] TX Shmoo Running Vref\n", __func__);
	u8changedelta = 1; // decrease & increase: change delta 1
	u32ta2looptimes = u32TxShmTA2LoopTimes;
	while (1) {
		u8Ta2Result = dramc_txwincenter_run(u8channel, u8rank, u8byte,
			u8dir, u8mode, u8changedelta, u32ta2looptimes);
		if (u8Ta2Result == 0)
			return 0;
		msleep(u32TxShmDelaysec*SHMOO_MSLP_DELAY_MS);
	}
	return 1;
}

static u8 dramc_txshmoo_run_phase(u8 u8channel, u8 u8rank, u8 u8byte, u8 u8dir, u8 u8mode)
{
	u8 u8changedelta = 0, u8Ta2Result = 0;
	u32 u32ta2looptimes = 0;

	pr_crit("\n[%s] TX Shmoo Running Phase\n", __func__);
	u8changedelta = 1; // decrease & increase: change delta 1
	u32ta2looptimes = u32TxShmTA2LoopTimes;
	while (1) {
		u8Ta2Result = dramc_txwincenter_run(u8channel, u8rank, u8byte,
			u8dir, u8mode, u8changedelta, u32ta2looptimes);
		if (u8Ta2Result == 0)
			return 0;
		msleep(u32TxShmDelaysec*SHMOO_MSLP_DELAY_MS);
	}
	return 1;
}

static u8 dramc_txshmoo_run(u8 u8channel, u8 u8rank, u8 u8byte, u8 u8dir, u8 u8mode)
{
	switch (u32TxShmDirection) {
	case SHIFT_L:
	case SHIFT_R:
		dramc_txshmoo_run_phase(u8channel, u8rank, u8byte, u8dir, u8mode);
		break;
	case SHIFT_D:
	case SHIFT_U:
		dramc_txshmoo_run_vref(u8channel, u8rank, u8byte, u8dir, u8mode);
		break;
	default:
		pr_crit("[%s] do nothing\n", __func__);
		return 0;
	}
	return 1;
}

static ssize_t dramc_txshmoo_show(struct device_driver *drv, char *buf)
{
	//u32 reg_val=0;

	if ((IsMachli() == false) && (IsManks() == false))
		return 0;

	if (u32TxShmDelaysec) {
		_shm_ddrphy_ao_bs = ioremap(SHMOO_DDRPHY_AO, SHMOO_IOREMAP_SIZE);
		_shm_dramc_ao_bs = ioremap(SHMOO_DRAMC_AO, SHMOO_IOREMAP_SIZE);
		_shm_dramc_nao_bs = ioremap(SHMOO_DRAMC_NAO, SHMOO_IOREMAP_SIZE);
		if (!_shm_ddrphy_ao_bs || !_shm_dramc_ao_bs || !_shm_dramc_nao_bs) {
			if (!_shm_ddrphy_ao_bs)
				pr_crit("[%s] ioremap error for ddrphy_ao!\n", __func__);
			if (!_shm_dramc_ao_bs)
				pr_crit("[%s] ioremap error for dramc_ao!\n", __func__);
			if (!_shm_dramc_nao_bs)
				pr_crit("[%s] ioremap error for dramc_nao!\n", __func__);
			iounmap(_shm_ddrphy_ao_bs);
			iounmap(_shm_dramc_ao_bs);
			iounmap(_shm_dramc_nao_bs);
			return 0;
		}

		pr_crit("\n\n############################################################\n");
		pr_crit("%s\n", RUN_TIME_SHMOO_VERSION);
		pr_crit("############################################################\n\n");
		pr_err("[%s] TX Shmoo Start\n", __func__);
		dramc_txshmoo_run((u8)u32TxShmChannel, (u8)u32TxShmRank, (u8)u32TxShmByte,
			(u8)u32TxShmDirection, (u8)u32TxShmMode);
		iounmap(_shm_ddrphy_ao_bs);
		iounmap(_shm_dramc_ao_bs);
		iounmap(_shm_dramc_nao_bs);
		pr_err("[%s] TX Shmoo Done\n", __func__);
	} else {
		pr_crit("[%s] Please echo the parameters firstly!\n", __func__);
	}
	return 0;
}

static ssize_t dramc_txshmoo_store(struct device_driver *drv, const char *buf, size_t count)
{
	char *str_dir = 0;
	size_t count_sscan = 0;

	if ((IsMachli() == false) && (IsManks() == false))
		return 0;

	count_sscan = sscanf(buf, "%d %d %d %d %d %d %d", &u32TxShmChannel, &u32TxShmRank,
		&u32TxShmByte, &u32TxShmDirection, &u32TxShmDelaysec, &u32TxShmTA2LoopTimes,
		&u32TxShmMode);
	if (count_sscan == 7) {
		switch (u32TxShmDirection) {
		case SHIFT_L:
			str_dir = "[L]: decrease tx phase"; break;
		case SHIFT_R:
			str_dir = "[R]: increase tx phase"; break;
		case SHIFT_D:
			str_dir = "[D]: decrease tx vref"; break;
		case SHIFT_U:
			str_dir = "[U]: increase tx vref"; break;
		default:
			u32TxShmDirection = SHIFT_R;
			str_dir = "[R]: increase tx phase"; break;
		}

		pr_crit("\n\n############################################################\n");
		pr_crit("%s\n", RUN_TIME_SHMOO_VERSION);
		pr_crit("############################################################\n\n");
		pr_crit("\n[%s] TX Shmoo Parameters\n	Channel = %d\n	Rank = %d\n	Byte = %d\n	",
			__func__, u32TxShmChannel, u32TxShmRank, u32TxShmByte);
		pr_crit("\nDirection = %d (%s)\n	DelayTime(sec) = %d\n	TA2LoopTimes = %d\n	Mode = %d\n\n",
			u32TxShmDirection, str_dir, u32TxShmDelaysec,
			u32TxShmTA2LoopTimes, u32TxShmMode);
	} else {
		pr_crit("[%s] TX Shmoo Wrong Args (count=%d) (count_sscan=%d)!!!\n",
			__func__, (int)count, (int)count_sscan);
	}

	return count;
}
#endif//RUN_TIME_SHMOO_TX
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx (SHMOO_XXX)

#ifdef RUN_TIME_SHMOO_RX
static DRIVER_ATTR_RW(dramc_rxshmoo);
static DRIVER_ATTR_RW(dramc_rxwincenter);
#endif//RUN_TIME_SHMOO_RX
#ifdef RUN_TIME_SHMOO_TX
static DRIVER_ATTR_RW(dramc_txshmoo);
static DRIVER_ATTR_RW(dramc_txwincenter);
#endif//RUN_TIME_SHMOO_TX

static DRIVER_ATTR_RW(dramc_dfstest);
static DRIVER_ATTR_RW(dramc_ssc);
static DRIVER_ATTR_RW(dramc_dynamic_ref);
static DRIVER_ATTR_RW(dramc_dynamic_boundset);
static int create_dramc_related_node(struct device_driver *drv)
{
	int err = 0;

	err = driver_create_file(drv, &driver_attr_dramc_dfstest);
	if (err) {
		pr_notice("[MTK_DRAMC]driver_attr_dramc_dfstest create attribute error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_dramc_ssc);
	if (err) {
		pr_notice("[MTK_DRAMC]driver_attr_dramc_ssc create attribute error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_dramc_dynamic_ref);
	if (err) {
		pr_notice("[MTK_DRAMC]driver_attr_dramc_dynamic_ref create attribute error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_dramc_dynamic_boundset);
	if (err) {
		pr_notice("[MTK_DRAMC]driver_attr_dramc_dynamic_boundset create attribute error\n");
		return err;
	}
	#ifdef RUN_TIME_SHMOO_RX
	err = driver_create_file(drv, &driver_attr_dramc_rxshmoo);
	if (err) {
		pr_notice("[MTK_BW]driver_attr_dramc_rxshmoo create attribute error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_dramc_rxwincenter);
	if (err) {
		pr_notice("[MTK_BW]driver_attr_dramc_rxwincenter create attribute error\n");
		return err;
	}
	#endif//RUN_TIME_SHMOO_RX
	#ifdef RUN_TIME_SHMOO_TX
	err = driver_create_file(drv, &driver_attr_dramc_txshmoo);
	if (err) {
		pr_notice("[MTK_BW]driver_attr_dramc_txshmoo create attribute error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_dramc_txwincenter);
	if (err) {
		pr_notice("[MTK_BW]driver_attr_dramc_txwincenter create attribute error\n");
		return err;
	}
	#endif//RUN_TIME_SHMOO_TX
	return err;
}

static int create_sysfs_node(void)
{
	struct device_driver *drv;
	int err = 0;

	drv = driver_find("mtk-dramc", &platform_bus_type);

	err = create_dramc_related_node(drv);
	if (err) {
		pr_err("[MTK_DRAMC]create_dramc_related_node fail\n");
		return err;
	}

	return err;
}

static int mtk_dramc_suspend(struct device *dev)
{
	pr_crit("[MTK_DRAMC] MTK DRAMC Suspend\n");

	return 0;
}

static int mtk_dramc_resume(struct device *dev)
{
	pr_crit("[MTK_DRAMC] MTK DRAMC Resume\n");

	return 0;
}

static const struct dev_pm_ops dramc_pm_ops = {
	.suspend = mtk_dramc_suspend,
	.resume = mtk_dramc_resume,
};

static int mtk_dramc_probe(struct platform_device *pdev)
{
	//struct device *dev = &pdev->dev;
	return 0;
}
static int mtk_dramc_remove(struct platform_device *pdev)
{

	return 0;
}
static const struct of_device_id mtk_dramc_id_table[] = {
	{ .compatible = "mtk-dramc" },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_dramc_id_table);
static struct platform_driver mtk_dramc_driver = {
	.probe = mtk_dramc_probe,
	.remove = mtk_dramc_remove,
	.driver = {
		.name = "mtk-dramc",
		.of_match_table = mtk_dramc_id_table,
		.pm = &dramc_pm_ops,
	},
};
static int __init mtk_dramc_init(void)
{
	int ret;
#ifdef DUMMY_INFO_DEBUG
	static int channel_num;
#endif
	mtk_dramc_dummy_reg_ioremap();
	u16DramcChipID = pm_top_get_chip_id();
	iChipIDindex = dramc_remapping_chipid_to_index(u16DramcChipID);

	pr_crit("[Dramc DRef] u16DramcChipID:%x iChipIDindex:%d\r\n", u16DramcChipID, iChipIDindex);

#ifdef DRAMC_SSC_DEBUG
	MIU_SSC_ioremap();
	MIU_SSC_debug();
	MIU_SSC_iounmap();
#endif
#ifdef DUMMY_INFO_DEBUG
	pr_err("ch%x type%x\n", mtk_dramc_get_support_channel_num(), mtk_dramc_get_ddr_type());

	for (channel_num = 0; channel_num < mtk_dramc_get_support_channel_num(); channel_num++)
		pr_err("id%d size%x\n", channel_num, mtk_dramc_get_channel_size(channel_num));
#endif

	ret = platform_driver_register(&mtk_dramc_driver);
	if (ret != 0) {
		pr_err("[MTK_DRAMC]Failed to register mtk_dramc_driver\n");
		return ret;
	}

	if (pDramcRes == NULL)
		pDramcRes = kmalloc(sizeof(struct DRAMC_RESOURCE_PRIVATE), GFP_KERNEL);
	if (pDramcRes == NULL)
		return -ENOMEM;

	//if (pDramcRes != NULL) {
	pDramcRes->bSelfCreateTaskFlag = 0;
	pDramcRes->bDRAMCTaskProcFlag = 0;
	pDramcRes->sDRAMCPollingTaskID = -1;
	pDramcRes->bShuffleDFSTaskFlag = 0;

	#ifdef ENABLE_DYNAMIC_REFRESH_FSM
		if (!pDramcRes->bSelfCreateTaskFlag) {
			dramc_ioremap();

			if (Dynamic_Refresh_Rate_Task_Init())
				pDramcRes->bSelfCreateTaskFlag = 1;

		}
	#endif
	//}



	ret = create_sysfs_node();
	if (ret != 0) {
		pr_err("[MTK_DRAMC]Failed to create_sysfs_node\n");
		return ret;
	}

#ifdef ENABLE_SWITCH_SHUFFLE_DFS
	//if (pDramcRes != NULL) {
		if (!pDramcRes->bShuffleDFSTaskFlag) {
			_mtk_dramc_ioremap_shuffle_dfs();
			pDramcRes->bShuffleDFSTaskFlag = 1;
		}
	//}
#endif

	t_sensor_ioremap();
	g_u16VBE_Code_FT = (Dramc_Thermal_VBE_Code_FT()>>6)&0x3FF;
	g_u16TA_Code = (Dramc_Thermal_TA_Code()&0x1FF);

	pr_crit("Dramc Initial Setting Done\n");

	return ret;
}
static void __exit mtk_dramc_exit(void)
{
	if (pDramcRes != NULL) {
		kfree(pDramcRes);
		pDramcRes = NULL;
	}
	platform_driver_unregister(&mtk_dramc_driver);
	dramc_iounmap();
	_mtk_dramc_shuffle_dfs_iounmap();
	t_sensor_iounmap();
}

module_init(mtk_dramc_init);
module_exit(mtk_dramc_exit);
module_param(mtk_dramc_debug, uint, 0644);
MODULE_PARM_DESC(mtk_dramc_debug, "Debug flag for debug message display");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("kenneth.yang@mediatek.com");
MODULE_DESCRIPTION("MTK Dramc driver");
