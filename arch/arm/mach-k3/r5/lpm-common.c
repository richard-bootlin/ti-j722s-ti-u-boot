// SPDX-License-Identifier: GPL-2.0+
/*
 * K3: R5 Common LPM Architecture initialization
 *
 * Copyright (C) 2023-2026 Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2026 Bootlin
 */

#include <clk.h>
#include <dm/read.h>
#include <elf.h>
#include <linux/printk.h>
#include <linux/soc/ti/ti_sci_protocol.h>
#include <power-domain.h>
#include <remoteproc.h>
#include <mach/security.h>

#include "../common.h"
#include "../lpm-common.h"

#define FW_IMAGE_SIZE 0x80000

struct lpm_addr_info {
	unsigned long context_save_addr;
	unsigned long atf_cert_addr;
	unsigned long optee_cert_addr;
	unsigned long dm_save_addr;
	u32 size;
};

__weak bool j7xx_board_is_resuming(void)
{
	return false;
}

static int extract_lpm_region(struct lpm_addr_info *mem_addr_lpm)
{
	ofnode node;
	fdt_addr_t lpm_reg_addr;
	fdt_size_t lpm_reg_size;

	node = ofnode_path("/reserved-memory/lpm-memory");
	if (!ofnode_valid(node)) {
		printf("lpm will not be functional\n");
		return -ENODEV;
	}

	lpm_reg_addr = ofnode_get_addr(node);
	if (lpm_reg_addr == FDT_ADDR_T_NONE) {
		printf("Can't find a valid reserved node!\n");
		return -ENODEV;
	}

	lpm_reg_size = ofnode_get_size(node);
	if (lpm_reg_size == FDT_ADDR_T_NONE) {
		printf("Can't find a valid reserved node!\n");
		return -ENODEV;
	}

	mem_addr_lpm->context_save_addr = lpm_reg_addr;
	mem_addr_lpm->atf_cert_addr = mem_addr_lpm->context_save_addr + FW_IMAGE_SIZE;
	mem_addr_lpm->optee_cert_addr = mem_addr_lpm->atf_cert_addr + FW_IMAGE_SIZE;
	mem_addr_lpm->dm_save_addr = mem_addr_lpm->optee_cert_addr + (2 * FW_IMAGE_SIZE);
	mem_addr_lpm->size = lpm_reg_size;

	return 0;
}

static int save_certificate(struct lpm_addr_info *mem_addr_lpm)
{
	int ret;

	if (!fit_image_info[IMAGE_ID_ATF].image_start ||
	    !fit_image_info[IMAGE_ID_OPTEE].image_start ||
	    !fit_image_info[IMAGE_ID_DM_FW].image_start) {
		pr_err("Invalid images to save\n");
		return -EINVAL;
	}

	ret = extract_lpm_region(mem_addr_lpm);
	if (ret) {
		pr_err("Cannot find valid LPM address range..\n");
		return -ENOMEM;
	}

	memcpy((void *)mem_addr_lpm->atf_cert_addr,
	       (void *)fit_image_info[IMAGE_ID_ATF].image_start,
	       fit_image_info[IMAGE_ID_ATF].image_len);

	memcpy((void *)mem_addr_lpm->optee_cert_addr,
	       (void *)fit_image_info[IMAGE_ID_OPTEE].image_start,
	       fit_image_info[IMAGE_ID_OPTEE].image_len);

	memcpy((void *)mem_addr_lpm->dm_save_addr,
	       (void *)fit_image_info[IMAGE_ID_DM_FW].image_start,
	       fit_image_info[IMAGE_ID_DM_FW].image_len);

	return 0;
}

void lpm_process(void)
{
	int ret = 0;
	struct lpm_addr_info mem_addr_lpm;
	struct ti_sci_handle *ti_sci = get_ti_sci_handle();

	ret = save_certificate(&mem_addr_lpm);
	if (ret)
		return;
	/*
	 * As there is no function to check TIFS capabilities, we can't really
	 * know if the call failed because it's not supported by TIFS or for
	 * another reason.
	 */
	ret = ti_sci->ops.lpm_ops.lpm_save_addr(ti_sci,
						mem_addr_lpm.context_save_addr,
						mem_addr_lpm.size);
	if (ret)
		pr_err("TIFS lpm save addr fails (message not supported?)\n");
}

static unsigned long resume_to_dm_f(const struct lpm_addr_info *mem_addr_lpm)
{
	struct ti_sci_handle *ti_sci = get_ti_sci_handle();
	unsigned long loadaddr;
	int ret = 0;

	loadaddr = mem_addr_lpm->dm_save_addr;
	if (!valid_elf_image(loadaddr))
		panic("%s: DM-Firmware image is not valid, it cannot be loaded\n",
		      __func__);

	loadaddr = load_elf_image_phdr(loadaddr);
	ret = ti_sci->ops.lpm_ops.lpm_save_addr(ti_sci,
						mem_addr_lpm->context_save_addr,
						mem_addr_lpm->size);
	if (ret)
		panic("TIFS lpm save addr fail : %x\n", ret);

	/*
	 * TIFS minimal context restore
	 * This restores also the firewall
	 */
	ret = ti_sci->ops.lpm_ops.min_context_restore(ti_sci, 0);
	if (ret)
		panic("TIFS restore_context failed (%d)\n", ret);

	/*
	 * Restore TFA in msmc memory
	 */
	ret = ti_sci->ops.lpm_ops.decrypt_tfa(ti_sci,
					      CONFIG_K3_ATF_LOAD_ADDR);
	if (ret)
		panic("%s: TIFS failed to decrytp TFA : %x\n", __func__, ret);

	/* restore TFA resume vector address in main core */
	ret = ti_sci->ops.lpm_ops.core_resume(ti_sci);
	if (ret)
		panic("ATF failed to resume (%d)\n", ret);

	return loadaddr;
}

static void resume_rproc_f(void)
{
	struct power_domain rproc_pwrdmn;
	unsigned long gtc_rate;
	struct udevice *dev;
	struct clk gtc_clk;
	void *gtc_base;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_REMOTEPROC, 1, &dev);
	if (ret)
		panic("Unknown remote processor 1 (%d)\n", ret);

	ret = power_domain_get_by_index(dev, &rproc_pwrdmn, 1);
	if (ret)
		panic("power_domain_get_rproc() failed: %d\n", ret);

	ret = clk_get_by_index(dev, 0, &gtc_clk);
	if (ret)
		panic("clk_get failed: %d\n", ret);

	gtc_base = dev_read_addr_ptr(dev);
	if (!gtc_base)
		panic("Get GTC address failed\n");

	gtc_rate = clk_get_rate(&gtc_clk);

#define GTC_CNTCR_REG	0x0
#define GTC_CNTFID0_REG	0x20
#define GTC_CNTR_EN	0x3
	/* TFA expect the Global Timebase Counter to be set-up */
	writel((u32)gtc_rate, gtc_base + GTC_CNTFID0_REG);
	writel(GTC_CNTR_EN, gtc_base + GTC_CNTCR_REG);

	ret = power_domain_on(&rproc_pwrdmn);
	if (ret)
		panic("power_domain_on failed: %d\n", ret);
}

typedef void __noreturn (*image_entry_noargs_t)(void);

void __noreturn do_resume(void)
{
	struct lpm_addr_info mem_addr_lpm;
	image_entry_noargs_t image_entry;
	size_t sz = FW_IMAGE_SIZE;
	unsigned long loadaddr;
	void *image_addr;
	int ret;

	ret = extract_lpm_region(&mem_addr_lpm);
	if (ret)
		panic("Cannot find valid LPM address range... LPM resume failed\n");

	ret = rproc_load(1, mem_addr_lpm.atf_cert_addr, 0x200);
	if (ret)
		panic("rproc failed to be initialized (%d)\n", ret);

	image_addr = (void *)mem_addr_lpm.atf_cert_addr;
	ti_secure_image_auth_apply_fwls(&image_addr, sz);

	image_addr = (void *)mem_addr_lpm.optee_cert_addr;
	ti_secure_image_auth_apply_fwls(&image_addr, sz);

	loadaddr = resume_to_dm_f(&mem_addr_lpm);
	printf("Starting ATF on ARM64 core...\n\n");
	resume_rproc_f();

	image_entry = (image_entry_noargs_t)loadaddr;
	image_entry();
}
