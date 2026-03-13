/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2026, Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2026, Bootlin
 */

#ifndef __K3_LPM_H
#define __K3_LPM_H

/* Magic value in PMIC register to indicate the suspend state (SOC_OFF) */
#define K3_LPM_MAGIC_SUSPEND 0xba

/* PMIC register where the magic value resides */
#define K3_LPM_SCRATCH_PAD_REG 0xcb

struct lpm_scratch_space {
	u16 wake_src;
	u16 reserved;
} __packed;

#endif /* __K3_LPM_H */

