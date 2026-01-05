// SPDX-License-Identifier: GPL-2.0+
/*
 * am62xx common LPM functions
 *
 * Copyright (C) 2025 Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2025 BayLibre, SAS
 */

#include <config.h>
#include <asm/arch/hardware.h>
#include <asm/io.h>

#include "am62xx-lpm-common.h"

#define WKUP_CTRL_MMR_CANUART_WAKE_STAT1			0x1830c
#define WKUP_CTRL_MMR_CANUART_WAKE_STAT1_CANUART_IO_MODE	BIT(0)

#define WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT		0x18318
#define WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT_MW		0x555555

static bool wkup_ctrl_canuart_wakeup_active(void)
{
	return !!(readl(WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_CANUART_WAKE_STAT1) &
		WKUP_CTRL_MMR_CANUART_WAKE_STAT1_CANUART_IO_MODE);
}

static bool wkup_ctrl_canuart_magic_word_set(void)
{
	return readl(WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT) ==
		WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT_MW;
}

bool wkup_ctrl_is_lpm_exit(void)
{
	return IS_ENABLED(CONFIG_K3_IODDR) &&
		wkup_ctrl_canuart_wakeup_active() &&
		wkup_ctrl_canuart_magic_word_set();
}
