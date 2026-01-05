// SPDX-License-Identifier: GPL-2.0+
/*
 * K3 DDRSS LPM specific functions
 *
 * Copyright (C) 2025 Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2025 BayLibre, SAS
 */

#include <asm/io.h>
#include <linux/compiler_types.h>
#include <linux/types.h>
#include <time.h>
#include <vsprintf.h>
#include <wait_bit.h>

#include "k3-ddrss-lpm.h"

#define AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL				0x430080d0
#define AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL_DATA_RET_LD		BIT(31)
#define AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL_DATA_RETENTION_MASK	GENMASK(3, 0)

#define AM62XX_WKUP_CTRL_MMR_CANUART_WAKE_STAT1				0x4301830c
#define AM62XX_WKUP_CTRL_MMR_CANUART_WAKE_STAT1_CANUART_IO_MODE		BIT(0)

#define AM62XX_WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT			0x43018318
#define AM62XX_WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT_MW		0x555555

#define K3_DDRSS_CFG_DENALI_CTL_20				0x0050
#define K3_DDRSS_CFG_DENALI_CTL_20_PHY_INDEP_TRAIN_MODE		BIT(24)
#define K3_DDRSS_CFG_DENALI_CTL_21				0x0054
#define K3_DDRSS_CFG_DENALI_CTL_21_PHY_INDEP_INIT_MODE		BIT(8)
#define K3_DDRSS_CFG_DENALI_CTL_106				0x01a8
#define K3_DDRSS_CFG_DENALI_CTL_106_PWRUP_SREFRESH_EXIT		BIT(16)
#define K3_DDRSS_CFG_DENALI_CTL_160				0x0280
#define K3_DDRSS_CFG_DENALI_CTL_160_LP_CMD_MASK			GENMASK(14, 8)
#define K3_DDRSS_CFG_DENALI_CTL_160_LP_CMD_ENTRY		BIT(9)
#define K3_DDRSS_CFG_DENALI_CTL_169				0x02a4
#define K3_DDRSS_CFG_DENALI_CTL_169_LP_AUTO_EXIT_EN_MASK	GENMASK(27, 24)
#define K3_DDRSS_CFG_DENALI_CTL_169_LP_AUTO_ENTRY_EN_MASK	GENMASK(19, 16)
#define K3_DDRSS_CFG_DENALI_CTL_169_LP_STATE_MASK		GENMASK(14, 8)
#define K3_DDRSS_CFG_DENALI_CTL_169_LP_STATE_SHIFT		8
#define K3_DDRSS_CFG_DENALI_CTL_345				0x0564
#define K3_DDRSS_CFG_DENALI_CTL_345_INT_STATUS_LOWPOWER_SHIFT	16
#define K3_DDRSS_CFG_DENALI_CTL_353				0x0584
#define K3_DDRSS_CFG_DENALI_CTL_353_INT_ACK_LOWPOWER_SHIFT	16
#define K3_DDRSS_CFG_DENALI_PI_6				0x2018
#define K3_DDRSS_CFG_DENALI_PI_6_PI_DFI_PHYMSTR_STATE_SEL_R	BIT(8)
#define K3_DDRSS_CFG_DENALI_PI_146				0x2248
#define K3_DDRSS_CFG_DENALI_PI_150				0x2258
#define K3_DDRSS_CFG_DENALI_PI_150_PI_DRAM_INIT_EN		BIT(8)
#define K3_DDRSS_CFG_DENALI_PHY_1820				0x5C70
#define K3_DDRSS_CFG_DENALI_PHY_1820_SET_DFI_INPUT_2_SHIFT	16

#define AM62XX_WKUP_CTRL_DDRSS_RETENTION_TIMEOUT_MS	5000
#define K3_DDRSS_LPM_TIMEOUT_MS				5000

static void k3_ddrss_reg_update_bits(void __iomem *addr, u32 offset, u32 mask, u32 set)
{
	u32 val = readl(addr + offset);

	val &= ~mask;
	val |= set;
	writel(val, addr + offset);
}

void k3_ddrss_self_refresh_exit(void __iomem *ddrss_ctl_cfg)
{
	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_CTL_169,
				 K3_DDRSS_CFG_DENALI_CTL_169_LP_AUTO_EXIT_EN_MASK |
				 K3_DDRSS_CFG_DENALI_CTL_169_LP_AUTO_ENTRY_EN_MASK,
				 0x0);
	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_PHY_1820,
				 0,
				 BIT(2) << K3_DDRSS_CFG_DENALI_PHY_1820_SET_DFI_INPUT_2_SHIFT);
	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_CTL_106,
				 0,
				 K3_DDRSS_CFG_DENALI_CTL_106_PWRUP_SREFRESH_EXIT);
	writel(0, ddrss_ctl_cfg + K3_DDRSS_CFG_DENALI_PI_146);
	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_PI_150,
				 K3_DDRSS_CFG_DENALI_PI_150_PI_DRAM_INIT_EN,
				 0x0);
	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_PI_6,
				 0,
				 K3_DDRSS_CFG_DENALI_PI_6_PI_DFI_PHYMSTR_STATE_SEL_R);
	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_CTL_21,
				 K3_DDRSS_CFG_DENALI_CTL_21_PHY_INDEP_INIT_MODE,
				 0);
	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_CTL_20,
				 0,
				 K3_DDRSS_CFG_DENALI_CTL_20_PHY_INDEP_TRAIN_MODE);
}

void k3_ddrss_lpm_resume(void __iomem *ddrss_ctl_cfg)
{
	int ret;
	unsigned long timeout_start;

	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_CTL_160,
				 K3_DDRSS_CFG_DENALI_CTL_160_LP_CMD_MASK,
				 K3_DDRSS_CFG_DENALI_CTL_160_LP_CMD_ENTRY);
	ret = wait_for_bit_32(ddrss_ctl_cfg + K3_DDRSS_CFG_DENALI_CTL_345,
			      (1 << K3_DDRSS_CFG_DENALI_CTL_345_INT_STATUS_LOWPOWER_SHIFT),
			      true, 5000, false);
	if (ret)
		panic("Failed waiting for low power command %d\n", ret);

	k3_ddrss_reg_update_bits(ddrss_ctl_cfg,
				 K3_DDRSS_CFG_DENALI_CTL_353,
				 0,
				 1 << K3_DDRSS_CFG_DENALI_CTL_353_INT_ACK_LOWPOWER_SHIFT);

	timeout_start = get_timer(0);
	while (1) {
		if (get_timer(timeout_start) > K3_DDRSS_LPM_TIMEOUT_MS)
			panic("Failed waiting for low power state\n");

		if ((readl(ddrss_ctl_cfg + K3_DDRSS_CFG_DENALI_CTL_169) &
		     K3_DDRSS_CFG_DENALI_CTL_169_LP_STATE_MASK) ==
		    0x40 << K3_DDRSS_CFG_DENALI_CTL_169_LP_STATE_SHIFT)
			break;
	}
}

void am62xx_ddrss_deassert_retention(void)
{
	int ret;

	k3_ddrss_reg_update_bits((void *)AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL,
				 0x0,
				 AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL_DATA_RET_LD |
				 AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL_DATA_RETENTION_MASK,
				 0);
	k3_ddrss_reg_update_bits((void *)AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL,
				 0x0,
				 AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL_DATA_RET_LD,
				 AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL_DATA_RET_LD);

	ret = wait_for_bit_32((void *)AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL,
			      AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL_DATA_RET_LD,
			      true, AM62XX_WKUP_CTRL_DDRSS_RETENTION_TIMEOUT_MS, false);
	if (ret)
		panic("Failed waiting for latching of retention %d\n", ret);

	k3_ddrss_reg_update_bits((void *)AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL,
				 0x0,
				 AM62XX_WKUP_CTRL_MMR0_DDR16SS_PMCTRL_DATA_RET_LD,
				 0);
}

static bool am62xx_wkup_conf_canuart_wakeup_active(void)
{
	u32 active;

	active = readl((void *)AM62XX_WKUP_CTRL_MMR_CANUART_WAKE_STAT1);

	return !!(active & AM62XX_WKUP_CTRL_MMR_CANUART_WAKE_STAT1_CANUART_IO_MODE);
}

static bool am62xx_wkup_conf_canuart_magic_word_set(void)
{
	u32 magic_word;

	magic_word = readl((void *)AM62XX_WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT);

	return magic_word == AM62XX_WKUP_CTRL_MMR_CANUART_WAKE_OFF_MODE_STAT_MW;
}

bool am62xx_wkup_conf_boot_is_resume(void)
{
	return IS_ENABLED(CONFIG_K3_IODDR) &&
		am62xx_wkup_conf_canuart_wakeup_active() &&
		am62xx_wkup_conf_canuart_magic_word_set();
}
