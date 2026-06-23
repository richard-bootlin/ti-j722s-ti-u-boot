/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * K3: LPM Architecture common definitions
 *
 * Copyright (C) 2026 Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2026 Bootlin
 */

#ifndef _LPM_COMMON_H_
#define _LPM_COMMON_H_

void __noreturn do_resume(void);
void lpm_process(void);
void k3_deassert_ddr_ret(const char *pmic_name, unsigned int ddr_ret_val,
			 unsigned int ddr_ret_clk, bool toggle);

#endif
