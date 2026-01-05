/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * am62xx common LPM functions
 *
 * Copyright (C) 2025 Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2025 BayLibre, SAS
 */

#ifndef _AM62XX_LPM_COMMON_H_
#define _AM62XX_LPM_COMMON_H_

#include <asm/io.h>

bool wkup_ctrl_is_lpm_exit(void);
int wkup_ctrl_remove_can_io_isolation_if_set(void);
int wkup_r5f_am62_lpm_meta_data_addr(u64 *meta_data_addr);
void lpm_resume_from_ddr(u64 meta_data_addr);

#endif  /* _AM62XX_LPM_COMMON_H_ */
