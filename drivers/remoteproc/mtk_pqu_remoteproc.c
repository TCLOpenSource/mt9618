// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <asm/barrier.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/devcoredump.h>

#include "mtk-reserved-memory.h"
#include "remoteproc_internal.h"

#include "mtk_pqu_fdt.h"
#include "mtk_pqu_internal.h"

#define REG_IMPLEMENTATION_0    0x38
#define REG_IMPLEMENTATION_1    0x3c
#define REG_I0_ADR_OFFSET_0     0x48
#define REG_I0_ADR_OFFSET_1     0x4c
#define REG_D0_ADR_OFFSET_0     0x60
#define REG_D0_ADR_OFFSET_1     0x64
#define REG_L2_CONF_BASE_0      0x78
#define REG_L2_CONF_BASE_1      0x7c

#define REG_NUM                 32
#define STACK_DUMP_SIZE         1024
#define STACK_POINTER_IDX       2
#define STACK_DUMP_BYTES        16
#define STACK_DUMP_LINE         (STACK_DUMP_SIZE / STACK_DUMP_BYTES)
#define DUMP_DATA_SIZE          4096
#define REGISTER_SECTION        "[register]\n"
#define MEMORY_SECTION          "[memory]\n"
#define REGISTER_BASE_IDX       2

#define BYTE_0                  0
#define BYTE_1                  1
#define BYTE_2                  2
#define BYTE_3                  3
#define BYTE_4                  4
#define BYTE_5                  5
#define BYTE_6                  6
#define BYTE_7                  7
#define BYTE_8                  8
#define BYTE_9                  9
#define BYTE_10                 10
#define BYTE_11                 11
#define BYTE_12                 12
#define BYTE_13                 13
#define BYTE_14                 14
#define BYTE_15                 15

#define SHMCTRL_CMA_ID 2

struct mtk_pqu {
	bool support_suspend;
	int irq;
	int recovery_irq;
	struct device *dev;
	struct rproc *rproc;
	struct clk *clk;
	void *fw_base, *intg_base, *frcpll;
	u32 ipi_offset, ipi_bit, emi_base;
	u64 phy_base;
	void *register_base;
	struct regmap *ipi_regmap, *ckgen_regmap;
	struct list_head clk_init_seq, pll_init_seq;
};

struct pqu_clk {
	struct list_head list;
	u32 offset, mask, val;
};

#define OFFSET_INDEX	0
#define MASK_INDEX		1
#define VAL_INDEX		2

// @TODO: move the following codes to device tree
#define PLL_BYTE_MASK			0xFF
#define PLL_BIT_SHIFT			8
#define PLL_CONFIG_0			0x0000
#define PLL_CONFIG_1			0x0001
#define PLL_CONFIG_2			0x1110
#define PLL_CONFIG_3			0x1100
#define PLL_DELAY				100
#define PLL_DELAY_500			500
#define REG_0000_RV55FRCPLL		0x0
#define REG_0004_RV55FRCPLL		0x4
#define REG_0008_RV55FRCPLL		0x8
#define REG_000C_RV55FRCPLL		0xc
#define REG_0010_RV55FRCPLL		0x10
#define REG_0014_RV55FRCPLL		0x14
#define REG_0018_RV55FRCPLL		0x18
#define REG_001C_RV55FRCPLL		0x1c
#define REG_0020_RV55FRCPLL		0x20
#define REG_0028_RV55FRCPLL		0x28
#define REG_0038_RV55FRCPLL		0x38

#define UART_SEL		0x1c601008
#define UART_SEL_VAL	0xf2
#define SZ_UART_SEL		0x10

#if !defined(LOWORD)
#define LOWORD(d)		((unsigned short)(d))
#endif
#if !defined(HIWORD)
#define HIWORD(d)		((unsigned short)((((unsigned long)(d)) >> 16) & 0xffff))
#endif

#define ADDR_MSB(d)		((unsigned short)((((unsigned long long)(d)) >> 32) & 0x0003))

#define MRV55E035		1
#define L2_BASE_ADDR	0xa8000000

#define FREQ_996MHZ		996
#define FREQ_1200MHZ	1200
#define DIV_2			0x00
#define DIV_BYPASS		0x04
#define CLK_12MHZ		12
#define CLK_24MHZ		24


static bool auto_boot;
module_param(auto_boot, bool, 0444);
MODULE_PARM_DESC(auto_boot, "Enable auto boot");

static int def_freq = FREQ_996MHZ;
module_param(def_freq, int, 0444);
MODULE_PARM_DESC(def_freq, "Set default frequency");

/* Lock protecting enter/exit critical region. */
static struct mutex pqu_state_mutex;
static bool is_mutex_inited;
static int common_clk_init_count = -1;
static int output_div;
static int loop_div;

static void check_def_freq(void)
{
	if (def_freq < FREQ_1200MHZ) {
		output_div = DIV_2;
		loop_div = def_freq / CLK_12MHZ;
		def_freq = loop_div * CLK_12MHZ;
	} else {
		output_div = DIV_BYPASS;
		loop_div = def_freq / CLK_24MHZ;
		def_freq = loop_div * CLK_24MHZ;
	}
}

static int setup_init_seq(struct device *dev, struct mtk_pqu *pqu,
		struct device_node *np)
{
	int count, i, rc;
	struct of_phandle_args clkspec;

	count = of_count_phandle_with_args(np, "pqu-clk", "#pqu-clk-cells");
	if (count < 0) {
		dev_warn(dev, "pqu-clk not found\n");
		return count;
	}

	dev_dbg(dev, "%d clock to configure\n", count);

	for (i = 0; i < count; i++) {
		struct pqu_clk *clk;

		rc = of_parse_phandle_with_args(np, "pqu-clk",
				"#pqu-clk-cells", i, &clkspec);
		if (rc < 0) {
			dev_err(dev, "pqu-clk parse fail with err %d\n", rc);
			return rc;
		}

		dev_dbg(dev, "clk\t%d:\t%x\t%x\t%x\n", i, clkspec.args[OFFSET_INDEX],
			clkspec.args[MASK_INDEX], clkspec.args[VAL_INDEX]);

		clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
		if (!clk)
			return -ENOMEM;

		clk->offset = clkspec.args[OFFSET_INDEX];
		clk->mask = clkspec.args[MASK_INDEX];
		clk->val = clkspec.args[VAL_INDEX];

		list_add_tail(&clk->list, &pqu->clk_init_seq);
	}

	return 0;
}

static void pqu_clk_init(struct device *dev, struct mtk_pqu *pqu)
{
	struct pqu_clk *clk;

	if (list_empty(&pqu->clk_init_seq)) {
		dev_warn(dev, "clk_init_seq is not set\n");
		return;
	}

	list_for_each_entry(clk, &pqu->clk_init_seq, list) {
		dev_dbg(dev, "clk.offset = %x, clk.mask = %x, clk.val = %x\n",
			 clk->offset, clk->mask, clk->val);
		regmap_update_bits_base(pqu->ckgen_regmap, clk->offset, clk->mask,
					clk->val, NULL, false, true);
	}
}

void pqu_pll_init(struct mtk_pqu *pqu)
{
	//base_addr: 0x0200c00, offset(byte): 0x00010
	//base_addr: 0x0200c00, offset(byte): 0x00011
	writew(PLL_CONFIG_0, pqu->frcpll + REG_0010_RV55FRCPLL);

	//base_addr: 0x0200c00, offset(byte): 0x00018
	//base_addr: 0x0200c00, offset(byte): 0x00019
	writew(((PLL_BYTE_MASK & output_div) << PLL_BIT_SHIFT) | PLL_CONFIG_0,
			pqu->frcpll + REG_0018_RV55FRCPLL);

	//base_addr: 0x0200c00, offset(byte): 0x0001c
	//base_addr: 0x0200c00, offset(byte): 0x0001d
	writew(PLL_CONFIG_0, pqu->frcpll + REG_001C_RV55FRCPLL);

	//base_addr: 0x0200c00, offset(byte): 0x00020
	//base_addr: 0x0200c00, offset(byte): 0x00021
	writew(PLL_CONFIG_0 | (PLL_BYTE_MASK & loop_div), pqu->frcpll + REG_0020_RV55FRCPLL);

	//base_addr: 0x0200c00, offset(byte): 0x00008
	//base_addr: 0x0200c00, offset(byte): 0x00009
	writew(PLL_CONFIG_0, pqu->frcpll + REG_0008_RV55FRCPLL);

	//base_addr: 0x0200c00, offset(byte): 0x00004
	//base_addr: 0x0200c00, offset(byte): 0x00005
	writew(PLL_CONFIG_1, pqu->frcpll + REG_0004_RV55FRCPLL);

	//base_addr: 0x0200c00, offset(byte): 0x00000
	//base_addr: 0x0200c00, offset(byte): 0x00001
	writew(PLL_CONFIG_2, pqu->frcpll + REG_0000_RV55FRCPLL);

	udelay(PLL_DELAY_500);

	//base_addr: 0x0200c00, offset(byte): 0x00000
	//base_addr: 0x0200c00, offset(byte): 0x00001
	writew(PLL_CONFIG_3, pqu->frcpll + REG_0000_RV55FRCPLL);
}

void pqu_uart_init(struct mtk_pqu *pqu)
{
	unsigned int tmp;
	void __iomem *reg;

	reg = ioremap(UART_SEL, SZ_UART_SEL);
	tmp = readw(reg);
	tmp = UART_SEL_VAL;	//reg_uart_sel4[3:0] = 2
	writew(tmp, reg);
	iounmap(reg);
}

#ifdef CONFIG_DMABUF_HEAPS
static int mtk_pqu_dma_heaps_init(struct device *dev)
{
	int ret;

	/* shmctrl cma SHMCTRL_CMA_ID */
	ret = of_reserved_mem_device_init_by_idx(dev, dev->of_node, SHMCTRL_CMA_ID);
	if (ret && ret != -ENODEV)
		return ret;

	/* create dma heaps */
	ret = mtk_pqu_dma_heaps_create(dev);
	if (ret)
		dev_warn(dev, "dma_heap create failed (%d)\n", ret);

	return 0;
}
#endif

static int pqu_start(struct rproc *rproc)
{
	struct mtk_pqu *pqu = rproc->priv;
	u64 emi_offset;
	unsigned long val;

	emi_offset = pqu->phy_base - pqu->emi_base;

	dev_info(pqu->dev, "fw mem base = %llx, emi_offset = %llx\n", pqu->phy_base, emi_offset);

	writew(LOWORD(emi_offset) + ADDR_MSB(emi_offset), pqu->intg_base + REG_I0_ADR_OFFSET_0);
	writew(HIWORD(emi_offset), pqu->intg_base + REG_I0_ADR_OFFSET_1);
	writew(LOWORD(emi_offset) + ADDR_MSB(emi_offset), pqu->intg_base + REG_D0_ADR_OFFSET_0);
	writew(HIWORD(emi_offset), pqu->intg_base + REG_D0_ADR_OFFSET_1);
	val = readw(pqu->intg_base + REG_IMPLEMENTATION_0);
	if (val & MRV55E035) {
		writew(LOWORD(L2_BASE_ADDR), pqu->intg_base + REG_L2_CONF_BASE_0);
		writew(HIWORD(L2_BASE_ADDR), pqu->intg_base + REG_L2_CONF_BASE_1);
	}

	writew(1, pqu->intg_base);
	return 0;
}

struct _dma_mem {
	void		*virt_base;
	dma_addr_t	device_base;
	unsigned long	pfn_base;
	int		size;
	unsigned long	*bitmap;
	spinlock_t	spinlock;
	bool		use_dev_dma_pfn_offset;
};

static void _dma_release_coherent_memory(struct _dma_mem *mem)
{
	if (!mem)
		return;

	memunmap(mem->virt_base);
	kfree(mem->bitmap);
	kfree(mem);
}


static int pqu_stop(struct rproc *rproc)
{
	struct mtk_pqu *pqu = rproc->priv;
	struct rproc_subdev *subdev;
	struct rproc_vdev *rvdev;

	/* pll disable */
	writew(0, pqu->intg_base);

	list_for_each_entry_reverse(subdev, &rproc->subdevs, node) {
		rvdev = container_of(subdev, struct rproc_vdev, subdev);
		if (rvdev->dev.dma_mem) {
			_dma_release_coherent_memory((struct _dma_mem *)(rvdev->dev.dma_mem));
			rvdev->dev.dma_mem = NULL;
		}
	}

	return 0;
}

static void *pqu_da_to_va(struct rproc *rproc, u64 da, int len)
{
	struct mtk_pqu *pqu = rproc->priv;

	return pqu->fw_base + da;
}

/* kick a virtqueue */
static void pqu_kick(struct rproc *rproc, int vqid)
{
	struct mtk_pqu *pqu = rproc->priv;

	regmap_update_bits_base(pqu->ipi_regmap, pqu->ipi_offset, BIT(pqu->ipi_bit),
				BIT(pqu->ipi_bit), NULL, false, true);
	regmap_update_bits_base(pqu->ipi_regmap, pqu->ipi_offset, BIT(pqu->ipi_bit),
				0, NULL, false, true);
}

static int _overlay_dtb(struct mtk_pqu *pqu, u32 memsz, void *ptr)
{
	int ret = 0;
	const char *dtb = NULL;
	const struct firmware *firmware_p;
	int cov_sz;   // for coverity check

	of_property_read_string_index(pqu->dev->of_node, "dtb-name", 0, &dtb);

	dev_info(pqu->dev, "dtb-name = %s\n", dtb);

	if (dtb && strlen(dtb)) {
		ret = request_firmware(&firmware_p, dtb, pqu->dev);
		if (ret != 0) {
			dev_err(pqu->dev, "dtb request_firmware failed: %d\n", ret);
		} else {
			dev_info(pqu->dev, "dtb filesz = %zu\n", firmware_p->size);

			if (firmware_p->size <= memsz) {
				dev_info(pqu->dev, "replace dtb\n");

				memcpy(ptr, firmware_p->data, firmware_p->size);
			}

			cov_sz = (int)memsz - firmware_p->size;
			cov_sz = ((cov_sz < 0) || (cov_sz > (INT_MAX - 1))) ? 0 : cov_sz;
			//if ((memsz - firmware_p->size) > 0)
			//	memset(ptr + firmware_p->size, 0,
			//			memsz - firmware_p->size);
			if ((memsz - firmware_p->size) > 0)
				memset(ptr + firmware_p->size, 0, cov_sz);
			release_firmware(firmware_p);
		}
	}

	return ret;
}

static int _overlay_dtbo(struct mtk_pqu *pqu, struct elf32_phdr *phdr,
		const u8 *elf_data, u32 memsz, void *ptr)
{
	int ret = 0;
	const char *dtbo;
	const char *dtbo_name;
	const struct firmware *firmware_p;
	int cov_sz;   // for coverity check
	int i, j, prop_count, count;
	void *expand_buf = NULL;

	cov_sz = (int)memsz;
	cov_sz = ((cov_sz < 0) || (cov_sz > (INT_MAX - 1))) ? 0 : cov_sz;
	if (cov_sz == 0)
		return -EINVAL;
	expand_buf = vmalloc(/*memsz*/cov_sz);
	if (!expand_buf) {
		dev_err(pqu->dev, "unable to allocate expand_buf\n");
		return -ENOMEM;
	}

	ret = fdt_open_into((const void *)
			(elf_data + phdr->p_offset), expand_buf,
			memsz);
	dev_info(pqu->dev, "expanded result = %d\n", ret);
	if (ret != 0) {
		dev_err(pqu->dev, "fdt_open_into failed: %d\n", ret);
		goto _overlay_dtbo_end;
	}

	prop_count = of_property_count_strings(pqu->dev->of_node, "dtbo-prop");
	dev_info(pqu->dev, "dtbo-prop count = %d\n", prop_count);

	for (i = 0; i < prop_count; i++) {
		of_property_read_string_index(pqu->dev->of_node, "dtbo-prop", i, &dtbo_name);
		dev_info(pqu->dev, "dtbo-prop[%d] = %s\n", i, dtbo_name);

		count = of_property_count_strings(pqu->dev->of_node, dtbo_name);
		for (j = 0; j < count; j++) {
			of_property_read_string_index(pqu->dev->of_node, dtbo_name, j, &dtbo);
			dev_info(pqu->dev, "dtbo-name[%d] = %s\n", j, dtbo);

			if (dtbo && strlen(dtbo)) {
				ret = request_firmware(&firmware_p, dtbo, pqu->dev);
				if (ret != 0) {
					dev_err(pqu->dev, "dtbo request_firmware fail: %d\n", ret);
					continue;
				}

				dev_info(pqu->dev, "dtbo filesz = %zu\n", firmware_p->size);
				dev_info(pqu->dev, "start to do dtbo overlay...\n");

				ret = fdt_overlay_apply(expand_buf,
						(void *)firmware_p->data);
				dev_info(pqu->dev, "overlay result = %d\n", ret);
				if (ret != 0)
					dev_err(pqu->dev, "fdt_overlay_apply fail: %d\n", ret);

				release_firmware(firmware_p);
			}
		}
	}

	memcpy(ptr, expand_buf, memsz);

_overlay_dtbo_end:
	vfree(expand_buf);

	return ret;
}

static int pqu_elf_load_segments(struct rproc *rproc, const struct firmware *fw)
{
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int i, ret = 0;
	const u8 *elf_data = fw->data;
	struct mtk_pqu *pqu = (struct mtk_pqu *)rproc->priv;
	int cov_num;   // for coverity check
	int cov_sz;   // for coverity check
	void *cov_ptr; // for coverity check

	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	/* go through the available ELF segments */
	cov_num = (int)ehdr->e_phnum;
	cov_num = ((cov_num < 0) || (cov_num > (INT_MAX - 1))) ? 0 : cov_num;
	for (i = 0; i < /*ehdr->e_phnum*/cov_num; i++, phdr++) {
		u32 da = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		u32 offset = phdr->p_offset;
		void *ptr;

		if (phdr->p_type != PT_LOAD)
			continue;

		dev_info(pqu->dev, "phdr(%d): type %d da 0x%x memsz 0x%x filesz 0x%x offset 0x%x\n",
			i, phdr->p_type, da, memsz, filesz, offset);

		if (filesz > memsz) {
			dev_err(pqu->dev, "bad phdr filesz 0x%x memsz 0x%x\n",
				filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > fw->size) {
			dev_err(pqu->dev, "truncated fw: need 0x%x avail 0x%zx\n",
				offset + filesz, fw->size);
			ret = -EINVAL;
			break;
		}

		/* grab the kernel address for this device address */
		ptr = rproc_da_to_va(rproc, da, memsz);
		if (!ptr) {
			dev_err(pqu->dev, "bad phdr da 0x%x mem 0x%x\n", da, memsz);
			ret = -EINVAL;
			break;
		}

		/* put the segment where the remote processor expects it */
		if (phdr->p_filesz)
			memcpy(ptr, elf_data + phdr->p_offset, filesz);

		/*
		 * Zero out remaining memory for this segment.
		 *
		 * This isn't strictly required since dma_alloc_coherent already
		 * did this for us. albeit harmless, we may consider removing
		 * this.
		 */
		cov_sz = (int)memsz - filesz;
		cov_sz = ((cov_sz < 0) || (cov_sz > (INT_MAX - 1))) ? 0 : cov_sz;
		if (memsz > filesz)
			memset(ptr + filesz, 0, /*memsz - filesz*/cov_sz);

		//if (fdt_magic((elf_data + phdr->p_offset)) == FDT_MAGIC) {
		cov_ptr = (void *)(elf_data + phdr->p_offset);
		if (cov_ptr && (fdt_magic(cov_ptr) == FDT_MAGIC)) {
			dev_info(pqu->dev,
					"DTB section is found, phdr(%d): filesz 0x%x offset 0x%x\n",
					i, filesz, offset);

			ret = _overlay_dtb(pqu, memsz, ptr);
			if (ret == 0) {
				cov_sz = (int)memsz;
				cov_sz = ((cov_sz < 0) || (cov_sz > (INT_MAX - 1))) ? 0 : cov_sz;
				ret = _overlay_dtbo(pqu, phdr, elf_data, /*memsz*/cov_sz, ptr);
			}
			if (ret != 0)
				break;
		}
	}

	return ret;
}

static const struct rproc_ops pqu_ops = {
	.start		= pqu_start,
	.stop		= pqu_stop,
	.kick		= pqu_kick,
	.da_to_va	= pqu_da_to_va,
};

/**
 * handle_event() - inbound virtqueue message workqueue function
 *
 * This function is registered as a kernel thread and is scheduled by the
 * kernel handler.
 */
static irqreturn_t handle_event(int irq, void *p)
{
	struct rproc *rproc = (struct rproc *)p;

	/* Process incoming buffers on all our vrings */
	rproc_vq_interrupt(rproc, 0);
	rproc_vq_interrupt(rproc, 1);

	return IRQ_HANDLED;
}

static int mtk_pqu_custom_coredump(struct rproc *rproc)
{
	int ret;
	struct mtk_pqu *pqu;
	uint32_t *register_dump;
	uint32_t register_addr;
	void *data = NULL;
	char *stackframe;
	size_t data_size;
	size_t offset;
	uint8_t i;

	if (!rproc) {
		pr_err("rproc is not present\n");
		ret = -EINVAL;
		goto dump_end;
	}

	pqu = (struct mtk_pqu *)rproc->priv;
	if (!pqu) {
		dev_err(&rproc->dev, "pqu is not present\n");
		ret = -EINVAL;
		goto dump_end;
	}

	if (pqu->register_base == 0) {
		dev_err(&rproc->dev, "register base is not present\n");
		ret = -EINVAL;
		goto dump_end;
	}

	register_dump = (uint32_t *)pqu->register_base;

	if (register_dump[STACK_POINTER_IDX] == 0) {
		dev_err(&rproc->dev, "stack physical address is NULL\n");
		ret = -EINVAL;
		goto dump_end;
	}
	dev_info(&rproc->dev, "Stack frame start = 0x%08x\n", register_dump[STACK_POINTER_IDX]);
	stackframe = (char *)rproc_da_to_va(rproc,
		register_dump[STACK_POINTER_IDX], STACK_DUMP_SIZE);
	if (stackframe == NULL) {
		dev_err(&rproc->dev, "stack virtual address is NULL\n");
		ret = -EINVAL;
		goto dump_end;
	}

	data_size = DUMP_DATA_SIZE;
	data = vmalloc(data_size);
	if (data == NULL) {
		ret = -ENOMEM;
		goto dump_end;
	}

	memcpy(data, (void *)MEMORY_SECTION, strlen(MEMORY_SECTION));
	offset = strlen(MEMORY_SECTION);
	register_addr = register_dump[STACK_POINTER_IDX];
	for (i = 0; i < STACK_DUMP_LINE; i++) {
		offset += snprintf(data + offset, data_size - offset,
			"0x%08x=[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]\n",
			register_addr,
			stackframe[BYTE_0],  stackframe[BYTE_1],
			stackframe[BYTE_2],  stackframe[BYTE_3],
			stackframe[BYTE_4],  stackframe[BYTE_5],
			stackframe[BYTE_6],  stackframe[BYTE_7],
			stackframe[BYTE_8],  stackframe[BYTE_9],
			stackframe[BYTE_10], stackframe[BYTE_11],
			stackframe[BYTE_12], stackframe[BYTE_13],
			stackframe[BYTE_14], stackframe[BYTE_15]);
		stackframe += STACK_DUMP_BYTES;
		register_addr += STACK_DUMP_BYTES;
	}

	memcpy(data + offset, (void *)REGISTER_SECTION, strlen(REGISTER_SECTION));
	offset += strlen(REGISTER_SECTION);
	for (i = 0; i < REG_NUM; i++) {
		dev_info(&rproc->dev, "register[%d] = 0x%08x\n", i, register_dump[i]);
		offset += snprintf(data + offset, data_size - offset,
			"x%d=0x%08x\n", i, register_dump[i]);
	}

	dev_coredumpv(&rproc->dev, data, ((offset > data_size)?data_size:offset), GFP_KERNEL);

	ret = 0;
dump_end:
	return ret;
}

static irqreturn_t handle_recovery_event(int irq, void *p)
{
	struct rproc *rproc = (struct rproc *)p;

	mtk_pqu_custom_coredump(rproc);

	rproc_report_crash(rproc, RPROC_WATCHDOG);

	return IRQ_HANDLED;
}

static int mtk_pqu_check_mem_region(struct device *dev, struct device_node *np)
{
	int ret;
	struct of_mmap_info_data dma, fwbuf;

	/* dma mem */
	ret = of_mtk_get_reserved_memory_info_by_idx(np, 0, &dma);
	if (ret)
		return ret;

	/* firmware buffer */
	ret = of_mtk_get_reserved_memory_info_by_idx(np, 1, &fwbuf);
	if (ret)
		return ret;

	if (dma.start_bus_address < fwbuf.start_bus_address) {
		dev_err(dev, "invalid memory region 0:%llx 1:%llx\n", dma.start_bus_address,
				fwbuf.start_bus_address);
		return -EINVAL;
	}

	return 0;
}

static int mtk_pqu_parse_dt(struct platform_device *pdev)
{
	const char *key;
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node, *syscon_np, *mmi_node;
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_pqu *pqu = rproc->priv;
	struct resource *res;
	struct of_mmap_info_data mmap_info;
	u64 register_dump_pa = 0;

	pqu->irq = platform_get_irq(pdev, 0);
	if (pqu->irq < 0) {
		dev_err(dev, "%s no irq found\n", dev->of_node->name);
		return -ENXIO;
	}

	pqu->recovery_irq = platform_get_irq(pdev, 1);
	if (pqu->recovery_irq < 0)
		dev_err(dev, "%s no recovery irq found\n", dev->of_node->name);


	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "intg");
	pqu->intg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pqu->intg_base)) {
		dev_err(dev, "no intg base\n");
		return PTR_ERR(pqu->intg_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "frcpll");
	// FIXME: frcpll is shared by mulitple mrv
	// pqu->frcpll = devm_ioremap_resource(dev, res);
	pqu->frcpll = ioremap(res->start, resource_size(res));
	if (IS_ERR(pqu->frcpll)) {
		dev_err(dev, "no frcpll\n");
		return PTR_ERR(pqu->frcpll);
	}

	syscon_np = of_parse_phandle(np, "mtk,ipi", 0);
	if (!syscon_np) {
		dev_err(dev, "no mtk,ipi node\n");
		return -ENODEV;
	}

	pqu->ipi_regmap = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(pqu->ipi_regmap))
		return PTR_ERR(pqu->ipi_regmap);

	/* TODO: Remove this after memcu clk init seq ready */
	pqu->support_suspend = of_property_read_bool(np, "mediatek,pqu-support-suspend");
	if (pqu->support_suspend) {
		syscon_np = of_parse_phandle(np, "pqu-clk-bank", 0);
		if (!syscon_np) {
			dev_err(dev, "no mtk,ckgen node\n");
			return -ENODEV;
		}

		pqu->ckgen_regmap = syscon_node_to_regmap(syscon_np);
		if (IS_ERR(pqu->ckgen_regmap))
			return PTR_ERR(pqu->ckgen_regmap);

		ret = setup_init_seq(dev, pqu, np);
		if (ret)
			return ret;
	}

	key = "mtk,ipi";
	ret = of_property_read_u32_index(np, key, 1, &pqu->ipi_offset);
	if (ret < 0) {
		dev_err(dev, "no offset in %s\n", key);
		return -EINVAL;
	}

	ret = of_property_read_u32_index(np, key, 2, &pqu->ipi_bit);
	if (ret < 0) {
		dev_err(dev, "no bit in %s\n", key);
		return -EINVAL;
	}

	/* check memory region, share dma region should be after fw mem */
	ret = mtk_pqu_check_mem_region(dev, np);
	if (ret)
		return ret;

	ret = of_reserved_mem_device_init(dev);
	if (ret)
		return ret;

	ret = of_mtk_get_reserved_memory_info_by_idx(np, 1, &mmap_info);
	if (ret)
		return ret;

	dev_info(dev, "fwmem start = %llx, size = %llx\n", mmap_info.start_bus_address,
			mmap_info.buffer_size);
	pqu->fw_base = devm_ioremap_wc(dev, mmap_info.start_bus_address, mmap_info.buffer_size);

	if (IS_ERR(pqu->fw_base)) {
		of_reserved_mem_device_release(dev);
		return PTR_ERR(pqu->fw_base);
	}

	pqu->phy_base = mmap_info.start_bus_address;

	register_dump_pa = pqu->phy_base + mmap_info.buffer_size - REG_NUM * sizeof(u32);
	dev_info(dev, "register dump physical address = 0x%llx\n", register_dump_pa);
	pqu->register_base =
		devm_ioremap_wc(dev, register_dump_pa, (REG_NUM * sizeof(u32)));

	mmi_node = of_find_node_by_name(NULL, "memory_info");
	if (!mmi_node)
		return -ENODEV;
	ret = of_property_read_u32(mmi_node, "cpu_emi0_base", &pqu->emi_base);
	dev_info(dev, "emi_base = 0x%x\n", pqu->emi_base);
	if (ret)
		return ret;

	return 0;
}

static int __maybe_unused mtk_pqu_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_pqu *pqu = rproc->priv;

	mutex_lock(&pqu_state_mutex);
	if (common_clk_init_count > 0)
		common_clk_init_count--;
	mutex_unlock(&pqu_state_mutex);

	if (!pqu->support_suspend) {
		dev_warn(dev, "device doesn't support suspend\n");
		return 0;
	}

	/* do not handle pm ops if !auto_boot*/
	if (!rproc->auto_boot)
		return 0;

	dev_info(dev, "shutdown pqu\n");
	rproc_shutdown(rproc);
	return 0;
}

static int __maybe_unused mtk_pqu_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_pqu *pqu = rproc->priv;

	if (!pqu->support_suspend) {
		dev_warn(dev, "device doesn't support suspend\n");
		return 0;
	}

	/* do not handle pm ops if !auto_boot*/
	if (!rproc->auto_boot)
		return 0;

	mutex_lock(&pqu_state_mutex);
	if (common_clk_init_count == 0) {
		dev_info(dev, "reinit pqu clk\n");
		pqu_pll_init(pqu);
		pqu_clk_init(dev, pqu);
		udelay(PLL_DELAY);
		pqu_uart_init(pqu);
		udelay(PLL_DELAY);
	}
	common_clk_init_count++;
	mutex_unlock(&pqu_state_mutex);

	dev_info(dev, "boot pqu\n");
	return rproc_boot(rproc);
}

static const struct dev_pm_ops mtk_pqu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_pqu_suspend, mtk_pqu_resume)
};

static int mtk_pqu_rproc_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct mtk_pqu *pqu;
	struct rproc *rproc;
	const char *firmware;

	if (!is_mutex_inited) {
		is_mutex_inited = true;
		mutex_init(&pqu_state_mutex);
		common_clk_init_count = 0;
	}

	ret = of_property_read_string(pdev->dev.of_node, "firmware-name",
				      &firmware);
	if (ret) {
		dev_err(dev, "prop: firmware-name not found\n");
		return ret;
	}

	rproc = rproc_alloc(dev, np->name, &pqu_ops,
				firmware, sizeof(*pqu));
	if (!rproc) {
		dev_err(dev, "unable to allocate remoteproc\n");
		return -ENOMEM;
	}

	rproc->ops->load = pqu_elf_load_segments;

	/*
	 * At first we lookup the device tree setting.
	 * If there's no auto-boot prop, then check the module param.
	 */
	rproc->auto_boot = of_property_read_bool(np, "mediatek,auto-boot");
	if (!rproc->auto_boot)
		rproc->auto_boot = auto_boot;

	rproc->has_iommu = false;

	pqu = (struct mtk_pqu *)rproc->priv;
	pqu->rproc = rproc;
	pqu->dev = dev;
	INIT_LIST_HEAD(&pqu->clk_init_seq);
	INIT_LIST_HEAD(&pqu->pll_init_seq);

	platform_set_drvdata(pdev, rproc);
	ret = mtk_pqu_parse_dt(pdev);
	if (ret) {
		dev_err(dev, "parse dt fail\n");
		return ret;
	}

#ifdef CONFIG_DMABUF_HEAPS
	ret = mtk_pqu_dma_heaps_init(dev);
	if (ret) {
		dev_err(dev, "dma heap init fail\n");
		return ret;
	}
#endif

	mutex_lock(&pqu_state_mutex);
	if (common_clk_init_count == 0) {
		check_def_freq();
		dev_info(dev, "init pqu clk @ %d MHz\n", def_freq);
		pqu_pll_init(pqu);
		pqu_clk_init(dev, pqu);
		udelay(PLL_DELAY);
		pqu_uart_init(pqu);
		udelay(PLL_DELAY);
	}
	common_clk_init_count++;
	mutex_unlock(&pqu_state_mutex);

	dev->dma_pfn_offset = PFN_DOWN(pqu->phy_base);
	device_enable_async_suspend(dev);
	ret = devm_request_threaded_irq(dev, pqu->irq, NULL,
				handle_event, IRQF_ONESHOT,
				"mtk-pqu", rproc);
	if (ret) {
		dev_err(dev, "request irq @%d failed\n", pqu->irq);
		return ret;
	}

	if (pqu->recovery_irq > 0) {
		ret = devm_request_threaded_irq(dev, pqu->recovery_irq, NULL,
					handle_recovery_event, IRQF_ONESHOT | IRQF_SHARED,
					"mtk-pqu", rproc);
		if (ret) {
			dev_err(dev, "request irq @%d failed\n", pqu->recovery_irq);
			return ret;
		}
	}

	dev_info(dev, "request irq @%d for kick pqu\n",  pqu->irq);
	ret = rproc_add(rproc);
	if (ret) {
		dev_err(dev, "rproc_add failed! ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_pqu_rproc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rproc *rproc = platform_get_drvdata(pdev);

	of_reserved_mem_device_release(dev);
	rproc_del(rproc);
	rproc_free(rproc);
	return 0;
}

static const struct of_device_id mtk_pqu_of_match[] = {
	{ .compatible = "mediatek,pqu"},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pqu_of_match);

static struct platform_driver mtk_pqu_rproc = {
	.probe = mtk_pqu_rproc_probe,
	.remove = mtk_pqu_rproc_remove,
	.driver = {
		.name = "mtk-pqu",
		.pm = &mtk_pqu_pm_ops,
		.of_match_table = of_match_ptr(mtk_pqu_of_match),
	},
};

module_platform_driver(mtk_pqu_rproc);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Mark-PK Tsai <mark-pk.tsai@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PQU Remoteproce driver");
