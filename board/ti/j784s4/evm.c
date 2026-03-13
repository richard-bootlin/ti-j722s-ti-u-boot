// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Board specific initialization for J784S4 EVM
 *
 * Copyright (C) 2023-2024 Texas Instruments Incorporated - https://www.ti.com/
 *	Hari Nagalla <hnagalla@ti.com>
 *
 */

#include <dm.h>
#include <efi_loader.h>
#include <init.h>
#include <spl.h>
#include <asm/arch/hardware.h>
#include <asm/arch/k3-ddr.h>
#include <wait_bit.h>
#include "../common/fdt_ops.h"

struct efi_fw_image fw_images[] = {
	{
		.image_type_id = AM69_SK_TIBOOT3_IMAGE_GUID,
		.fw_name = u"AM69_SK_TIBOOT3",
		.image_index = 1,
	},
	{
		.image_type_id = AM69_SK_SPL_IMAGE_GUID,
		.fw_name = u"AM69_SK_SPL",
		.image_index = 2,
	},
	{
		.image_type_id = AM69_SK_UBOOT_IMAGE_GUID,
		.fw_name = u"AM69_SK_UBOOT",
		.image_index = 3,
	}
};

struct efi_capsule_update_info update_info = {
	.dfu_string = "sf 0:0=tiboot3.bin raw 0 80000;"
	"tispl.bin raw 80000 200000;u-boot.img raw 280000 400000",
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};

#if defined(CONFIG_XPL_BUILD)
void spl_perform_board_fixups(struct spl_image_info *spl_image)
{
	if (IS_ENABLED(CONFIG_K3_DDRSS)) {
		if (IS_ENABLED(CONFIG_K3_INLINE_ECC))
			fixup_ddr_driver_for_ecc(spl_image);
	} else {
		fixup_memory_node(spl_image);
	}
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	ti_set_fdt_env(NULL, NULL);
	return 0;
}
#endif

#if (IS_ENABLED(CONFIG_SPL_BUILD) && IS_ENABLED(CONFIG_TARGET_J784S4_R5_EVM))
void clear_isolation(void)
{
	int ret;
	const void *wait_reg = (const void *)(WKUP_CTRL_MMR0_BASE + MCU_GEN_WAKE_STAT1);

	/* un-set the magic word for MCU_GENERAL IOs */
	writel(IO_ISO_MAGIC_VAL, WKUP_CTRL_MMR0_BASE + MCU_GEN_WAKE_CTRL);
	writel((IO_ISO_MAGIC_VAL + 0x1), WKUP_CTRL_MMR0_BASE + MCU_GEN_WAKE_CTRL);
	writel(IO_ISO_MAGIC_VAL, WKUP_CTRL_MMR0_BASE + MCU_GEN_WAKE_CTRL);

	/* wait for MCU_GEN_IO_MODE bit to be cleared */
	ret = wait_for_bit_32(wait_reg,
			      MCU_GEN_WAKE_STAT1_MCU_GEN_IO_MODE,
			      false,
			      DEISOLATION_TIMEOUT_MS,
			      false);
	if (ret < 0)
		pr_err("Deisolation timeout");
}
#endif /* CONFIG_SPL_BUILD && CONFIG_TARGET_J784s4_R5_EVM */

void spl_board_init(void)
{
	struct udevice *dev;
	int ret;

	if (IS_ENABLED(CONFIG_ESM_K3)) {
		const char * const esms[] = {"esm@700000", "esm@40800000", "esm@42080000"};

		for (int i = 0; i < ARRAY_SIZE(esms); ++i) {
			ret = uclass_get_device_by_name(UCLASS_MISC, esms[i],
							&dev);
			if (ret) {
				printf("MISC init for %s failed: %d\n", esms[i], ret);
				break;
			}
		}
	}

	if (IS_ENABLED(CONFIG_ESM_PMIC) && ret == 0) {
		ret = uclass_get_device_by_driver(UCLASS_MISC,
						  DM_DRIVER_GET(pmic_esm),
						  &dev);
		if (ret)
			printf("ESM PMIC init failed: %d\n", ret);
	}
}
