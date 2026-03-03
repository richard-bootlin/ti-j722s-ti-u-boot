// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for J722S platforms
 *
 * Copyright (C) 2024 Texas Instruments Incorporated - https://www.ti.com/
 *
 */

#include <asm/arch/hardware.h>
#include <asm/io.h>
#include <cpu_func.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <i2c.h>
#include <env.h>
#include <fdt_support.h>
#include <spl.h>
#include <asm/arch/k3-ddr.h>
#include "../common/fdt_ops.h"

#if IS_ENABLED(CONFIG_SPL_BUILD)
void spl_board_init(void)
{
	enable_caches();
}
#endif

#if (IS_ENABLED(CONFIG_SPL_BUILD) && IS_ENABLED(CONFIG_TARGET_J722S_R5_EVM))

extern void ctrl_mmr_unlock(void);

#define SCRATCH_PAD_REG_3 0xCB
#define MAGIC_SUSPEND 0xBA

/* in board_init_f(), there's no BSS, so we can't use global/static variables */
int board_is_resuming(void)
{
	struct udevice *pmic, *i2c;
	int err;

	if (gd_k3_resuming() >= 0)
		goto end;

	/*
	 * On HS-SE devices, i2c access fails unless MMR registers are unlocked.
	 * Moreover, it fails also if we use PMIC API instead of I2C API.
	 */
	ctrl_mmr_unlock();
	err = uclass_get_device_by_name(UCLASS_I2C,
					"i2c@2b200000", &i2c);
	if (err) {
		printf("Getting I2C failed: %d\n", err);
		goto end;
	}
	err = dm_i2c_probe(i2c, 0x48, 0, &pmic);
	if (err) {
		printf("Getting PMIC failed: %d\n", err);
		goto end;
	}

	debug("%s: PMIC is detected (%s)\n", __func__, pmic->name);

	if (dm_i2c_reg_read(pmic, SCRATCH_PAD_REG_3) == MAGIC_SUSPEND) {
		debug("%s: board is resuming\n", __func__);
		gd_set_k3_resuming(1);

		/* clean magic suspend */
		if (dm_i2c_reg_write(pmic, SCRATCH_PAD_REG_3, 0))
			printf("Failed to clean magic value for suspend detection in PMIC\n");
	} else {
		debug("%s: board is booting (no resume detected)\n", __func__);
		gd_set_k3_resuming(0);
	}
end:
	return gd_k3_resuming();
}
#else
int board_is_resuming(void)
{
	return 0;
}
#endif /* CONFIG_SPL_BUILD && CONFIG_TARGET_J722S_R5_EVM */

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

#if IS_ENABLED(CONFIG_BOARD_LATE_INIT)
int board_late_init(void)
{
	ti_set_fdt_env(NULL, NULL);
	return 0;
}
#endif
