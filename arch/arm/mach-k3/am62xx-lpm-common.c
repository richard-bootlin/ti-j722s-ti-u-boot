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
#include <wait_bit.h>

#include "am62xx-lpm-common.h"

/*
 * Shared WKUP_CTRL_MMR0 definitions used to remove IO isolation
 */
#define WKUP_CTRL_MMR_PMCTRL_IO_0				0x18084
#define WKUP_CTRL_MMR_PMCTRL_IO_0_ISOCLK_OVRD_0			BIT(0)
#define WKUP_CTRL_MMR_PMCTRL_IO_0_ISOOVR_EXTEND_0		BIT(4)
#define WKUP_CTRL_MMR_PMCTRL_IO_0_ISO_BYPASS_OVR_0		BIT(6)
#define WKUP_CTRL_MMR_PMCTRL_IO_0_WUCLK_CTRL_0			BIT(8)
#define WKUP_CTRL_MMR_PMCTRL_IO_0_GLOBAL_WUEN_0			BIT(16)
#define WKUP_CTRL_MMR_PMCTRL_IO_0_IO_ISO_CTRL_0			BIT(24)
#define WKUP_CTRL_MMR_PMCTRL_IO_0_WRITE_MASK ( \
		WKUP_CTRL_MMR_PMCTRL_IO_0_ISOCLK_OVRD_0 |	\
		WKUP_CTRL_MMR_PMCTRL_IO_0_ISOOVR_EXTEND_0 |	\
		WKUP_CTRL_MMR_PMCTRL_IO_0_ISO_BYPASS_OVR_0 |	\
		WKUP_CTRL_MMR_PMCTRL_IO_0_WUCLK_CTRL_0 |	\
		WKUP_CTRL_MMR_PMCTRL_IO_0_GLOBAL_WUEN_0 |	\
		WKUP_CTRL_MMR_PMCTRL_IO_0_IO_ISO_CTRL_0)

#define WKUP_CTRL_MMR_PMCTRL_IO_GLB				0x1809c
#define WKUP_CTRL_MMR_DEEPSLEEP_CTRL				0x18160

#define WKUP_CTRL_MMR_CANUART_WAKE_CTRL				0x18300
#define WKUP_CTRL_MMR_CANUART_WAKE_CTRL_MW			0x2aaaaaaa
#define WKUP_CTRL_MMR_CANUART_WAKE_CTRL_MW_SHIFT		1
#define WKUP_CTRL_MMR_CANUART_WAKE_CTRL_MW_LOAD_EN		BIT(0)

#define WKUP_CTRL_MMR_CANUART_WAKE_STAT1			0x1830c
#define WKUP_CTRL_MMR_CANUART_WAKE_STAT1_CANUART_IO_MODE	BIT(0)

#define WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT		0x18318
#define WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT_MW		0x555555

#define CLKSTOP_TRANSITION_TIMEOUT_MS	10

static int wkup_ctrl_remove_can_io_isolation(void)
{
	const void *wait_reg = (const void *)(WKUP_CTRL_MMR0_BASE +
					      WKUP_CTRL_MMR_CANUART_WAKE_STAT1);
	int ret;
	u32 reg = 0;

	/* Program magic word */
	reg = readl(WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_CANUART_WAKE_CTRL);
	reg |= WKUP_CTRL_MMR_CANUART_WAKE_CTRL_MW << WKUP_CTRL_MMR_CANUART_WAKE_CTRL_MW_SHIFT;
	writel(reg, WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_CANUART_WAKE_CTRL);

	/* Set enable bit. */
	reg |= WKUP_CTRL_MMR_CANUART_WAKE_CTRL_MW_LOAD_EN;
	writel(reg, WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_CANUART_WAKE_CTRL);

	/* Clear enable bit. */
	reg &= ~WKUP_CTRL_MMR_CANUART_WAKE_CTRL_MW_LOAD_EN;
	writel(reg, WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_CANUART_WAKE_CTRL);

	/* wait for CAN_ONLY_IO signal to be 0 */
	ret = wait_for_bit_32(wait_reg,
			      WKUP_CTRL_MMR_CANUART_WAKE_STAT1_CANUART_IO_MODE,
			      false,
			      CLKSTOP_TRANSITION_TIMEOUT_MS,
			      false);
	if (ret < 0)
		return ret;

	/* Reset magic word */
	writel(0, WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_CANUART_WAKE_CTRL);

	/* Remove WKUP IO isolation */
	reg = readl(WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_PMCTRL_IO_0);
	reg = reg & WKUP_CTRL_MMR_PMCTRL_IO_0_WRITE_MASK & ~WKUP_CTRL_MMR_PMCTRL_IO_0_GLOBAL_WUEN_0;
	writel(reg, WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_PMCTRL_IO_0);

	/* clear global IO isolation */
	reg = readl(WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_PMCTRL_IO_0);
	reg = reg & WKUP_CTRL_MMR_PMCTRL_IO_0_WRITE_MASK & ~WKUP_CTRL_MMR_PMCTRL_IO_0_IO_ISO_CTRL_0;
	writel(reg, WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_PMCTRL_IO_0);

	/* Release all IOs from deepsleep mode and clear IO daisy chain control */
	writel(0, WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_DEEPSLEEP_CTRL);
	writel(0, WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR_PMCTRL_IO_GLB);

	return 0;
}

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

int __maybe_unused wkup_ctrl_remove_can_io_isolation_if_set(void)
{
	if (wkup_ctrl_canuart_wakeup_active() && !wkup_ctrl_canuart_magic_word_set())
		return wkup_ctrl_remove_can_io_isolation();
	return 0;
}
