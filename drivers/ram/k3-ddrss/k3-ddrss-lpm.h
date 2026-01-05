/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * K3 DDRSS LPM specific functions
 *
 * Copyright (C) 2025 Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2025 BayLibre, SAS
 */

#ifndef _K3_DDRSS_LPM_H_
#define _K3_DDRSS_LPM_H_

#include <linux/compiler_types.h>
#include <linux/types.h>

void k3_ddrss_self_refresh_exit(void __iomem *ddrss_ctl_cfg);
void k3_ddrss_lpm_resume(void __iomem *ddrss_ctl_cfg);
void am62xx_ddrss_deassert_retention(void);
bool am62xx_wkup_conf_boot_is_resume(void);

#endif  /* _K3_DDRSS_LPM_H_ */
