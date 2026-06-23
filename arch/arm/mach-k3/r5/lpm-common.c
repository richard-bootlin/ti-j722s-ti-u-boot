// SPDX-License-Identifier: GPL-2.0+
/*
 * K3: R5 Common LPM Architecture initialization
 *
 * Copyright (C) 2023-2026 Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2026 Bootlin
 */

#include <asm/arch/hardware.h>
#include <asm/global_data.h>
#include <clk.h>
#include <dm/device.h>
#include <dm/read.h>
#include <elf.h>
#include <i2c.h>
#include <linux/printk.h>
#include <linux/soc/ti/ti_sci_protocol.h>
#include <power-domain.h>
#include <power/pmic.h>
#include <remoteproc.h>
#include <mach/security.h>

#include "../common.h"
#include "../lpm-common.h"

/* Magic value in PMIC register to indicate the suspend state (SOC_OFF) */
#define K3_LPM_MAGIC_SUSPEND 0xba

/* PMIC register where the magic value resides */
#define K3_LPM_SCRATCH_PAD_REG_3 0xcb

/* PMIC NSLEEP register */
#define K3_LPM_NSLEEP_TRIGGER_REG 0x86

/* Wake-up source IDs */
#define K3_LPM_WAKE_SOURCE_MAIN_IO 0x80
#define K3_LPM_WAKE_SOURCE_MCU_IO 0x81
#define K3_LPM_WAKE_SOURCE_PMIC_GPIO 0xB0

#define IO_ISO_STATUS BIT(25)
#define FW_IMAGE_SIZE 0x80000

struct lpm_addr_info {
	unsigned long context_save_addr;
	unsigned long atf_cert_addr;
	unsigned long optee_cert_addr;
	unsigned long dm_save_addr;
	u32 size;
};

struct lpm_scratch_space {
	u16 wake_src;
	u16 reserved;
} __packed;

__weak void clear_isolation(void) { }

/* This is used by J722s */
__weak void ctrl_mmr_unlock(void) { }

#define GPIO_OUT_1 0x3D
void k3_deassert_ddr_ret(const char *pmic_name, unsigned int ddr_ret_val,
			 unsigned int ddr_ret_clk, bool toggle)
{
	struct udevice *pmic;
	int regval;
	int err;

	err = uclass_get_device_by_name(UCLASS_PMIC, pmic_name, &pmic);
	if (err) {
		printf("Getting %s init failed: %d\n", pmic_name, err);
		return;
	}

	/* Set DDR_RET Signal Low on PMIC */
	regval = pmic_reg_read(pmic, GPIO_OUT_1) & ~ddr_ret_val;
	pmic_reg_write(pmic, GPIO_OUT_1, regval);

	if (toggle) {
		/* Now toggle the CLK of the latch for DDR retention */
		pmic_reg_write(pmic, GPIO_OUT_1, regval | ddr_ret_clk);
		pmic_reg_write(pmic, GPIO_OUT_1, regval & ~ddr_ret_clk);
		pmic_reg_write(pmic, GPIO_OUT_1, regval | ddr_ret_clk);
		pmic_reg_write(pmic, GPIO_OUT_1, regval & ~ddr_ret_clk);
	}
}

/* in board_init_f(), there's no BSS, so we can't use global/static variables */
bool j7xx_board_is_resuming(void)
{
	struct lpm_scratch_space *lpm_scratch;
	struct udevice *pmic, *i2c;
	u32 pmctrl_val = 0;
	int ret, magic;

	if (gd_k3_resuming() != K3_RESUME_STATE_UNKNOWN)
		goto end;

#ifdef PMCTRL_IO_LPM
	pmctrl_val = readl(PMCTRL_IO_LPM);
#endif
	lpm_scratch = (struct lpm_scratch_space *)CONFIG_SYS_K3_SCRATCH_LPM_ADDR;
	if (lpm_scratch) {
		lpm_scratch->wake_src = 0;
		lpm_scratch->reserved = 0;
	}
	if ((pmctrl_val & IO_ISO_STATUS) == IO_ISO_STATUS) {
		if (lpm_scratch) {
			if (IS_ENABLED(CONFIG_SOC_K3_J784S4))
				lpm_scratch->wake_src = K3_LPM_WAKE_SOURCE_MCU_IO;
			else
				lpm_scratch->wake_src = K3_LPM_WAKE_SOURCE_MAIN_IO;
		}
		clear_isolation();
		gd_set_k3_resuming(K3_RESUME_STATE_RESUMING);
		debug("board is resuming from IO_DDR mode\n");
		goto end;
	}

	if (IS_ENABLED(CONFIG_SOC_K3_J722S)) {
		/*
		 * On J722S devices, i2c access fails unless MMR
		 * registers are unlocked.
		 * Moreover, it fails also if we use PMIC API instead of I2C API.
		 */
		ctrl_mmr_unlock();
		ret = uclass_get_device_by_name(UCLASS_I2C,
						"i2c@2b200000", &i2c);
		if (ret) {
			printf("Getting I2C failed: %d\n", ret);
			goto end;
		}
		ret = dm_i2c_probe(i2c, 0x48, 0, &pmic);
		if (ret) {
			printf("Getting PMIC failed: %d\n", ret);
			goto end;
		}
	} else {
		ret = uclass_get_device_by_name(UCLASS_PMIC,
						"pmic@48", &pmic);
		if (ret) {
			printf("Getting PMIC init failed: %d\n", ret);
			goto end;
		}
	}
	debug("%s: PMIC is detected (%s)\n", __func__, pmic->name);

	if (IS_ENABLED(CONFIG_SOC_K3_J722S))
		magic = dm_i2c_reg_read(pmic, K3_LPM_SCRATCH_PAD_REG_3);
	else
		magic = pmic_reg_read(pmic, K3_LPM_SCRATCH_PAD_REG_3);

	if (magic == K3_LPM_MAGIC_SUSPEND) {
		debug("%s: board is resuming\n", __func__);
		if (lpm_scratch)
			lpm_scratch->wake_src = K3_LPM_WAKE_SOURCE_PMIC_GPIO;
		gd_set_k3_resuming(K3_RESUME_STATE_RESUMING);

		/* clean magic suspend */
		if (IS_ENABLED(CONFIG_SOC_K3_J722S))
			ret = dm_i2c_reg_write(pmic, K3_LPM_SCRATCH_PAD_REG_3, 0);
		else
			ret = pmic_reg_write(pmic, K3_LPM_SCRATCH_PAD_REG_3, 0);

		if (ret)
			printf("Failed to clean magic value for suspend detection in PMIC\n");

		/*
		 * Ensure that NSLEEP triggers bits are set to prevent a
		 * suspend/resume transition when accessing PMIC interrupt
		 * registers.
		 */
		if (IS_ENABLED(CONFIG_SOC_K3_J722S)) {
			ret = dm_i2c_reg_write(pmic, K3_LPM_NSLEEP_TRIGGER_REG, 0x3);
		} else {
			struct udevice *pmic_b;

			/* Some boards may have more than one pmic */
			ret = uclass_get_device_by_name(UCLASS_PMIC, "pmic@4c", &pmic_b);
			if (ret == 0) {
				ret = pmic_reg_write(pmic_b, K3_LPM_NSLEEP_TRIGGER_REG, 0x03);
				if (ret)
					printf("Failed to set NSLEEP triggers bits on PMIC-B\n");
			}

			ret = pmic_reg_write(pmic, K3_LPM_NSLEEP_TRIGGER_REG, 0x3);
		}
		if (ret)
			printf("Failed to set PMIC NSLEEP triggers. Spurious suspend/resume may happen.\n");

		/*
		 * For robustness, the DM should also clean the magic value at
		 * startup.
		 */
	} else {
		debug("%s: board is booting (no resume detected)\n", __func__);
		gd_set_k3_resuming(K3_RESUME_STATE_BOOTING);
	}
end:
	return gd_k3_resuming() == K3_RESUME_STATE_RESUMING;
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
	struct udevice *dev;
	void *gtc_base;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_REMOTEPROC, 1, &dev);
	if (ret)
		panic("Unknown remote processor 1 (%d)\n", ret);

	ret = power_domain_get_by_index(dev, &rproc_pwrdmn, 1);
	if (ret)
		panic("power_domain_get_rproc() failed: %d\n", ret);

	gtc_base = dev_read_addr_ptr(dev);
	if (!gtc_base)
		panic("Get GTC address failed\n");

#define GTC_CNTCR_REG	0x0
#define GTC_CNTR_EN	0x3

	/* GTC counter values have been restored by TIFS, so enable the counter */
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
