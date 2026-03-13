/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2026 Texas Instruments Incorporated - https://www.ti.com/
 */

#ifndef LPDDR4_K3_REG
#define LPDDR4_K3_REG

#define _lpddr4_k3_readreg(_shift, _fnct, _ddrss, _reg, _p) do {	\
	u16 offset = 0U;						\
	u32 result = 0U;						\
	TH_OFFSET_FROM_REG(_reg, _shift, offset);			\
	result = (_fnct)(&(_ddrss)->pd, _p, (u16 *)(&offset), 1);	\
	if (result > 0U) {						\
		printf("%s: Failed to read %s\n", __func__, xstr(_reg));\
		hang();							\
	}								\
} while (0)

#define _lpddr4_k3_writereg(_shift, _fnct, _ddrss, _reg, _val)  do {	\
	u16 offset = 0U;						\
	u32 result = 0U;						\
	u32 writeval = _val;						\
	TH_OFFSET_FROM_REG(_reg, _shift, offset);			\
	result = (_fnct)(&(_ddrss)->pd,	&writeval, (u16 *)(&offset), 1);\
	if (result > 0U) {						\
		printf("%s: Failed to write %s\n", __func__, xstr(_reg));\
		hang();							\
	}								\
} while (0)

#define lpddr4_k3_readreg_ctl(_ddrss, _reg, _pt)		\
	_lpddr4_k3_readreg(CTL_SHIFT,				\
			   (_ddrss)->driverdt->readctlconfig,	\
			   _ddrss, _reg, _pt)

#define lpddr4_k3_readreg_pi(_ddrss, _reg, _pt)				\
	_lpddr4_k3_readreg(PI_SHIFT,					\
			   (_ddrss)->driverdt->readphyindepconfig,	\
			   _ddrss, _reg, _pt)

#define lpddr4_k3_readreg_phy(_ddrss, _reg, _pt)		\
	_lpddr4_k3_readreg(PHY_SHIFT,				\
			   (_ddrss)->driverdt->readphyconfig,	\
			   _ddrss, _reg, _pt)

#define lpddr4_k3_writereg_ctl(_ddrss, _reg, _val)		\
	_lpddr4_k3_writereg(CTL_SHIFT,				\
			    (_ddrss)->driverdt->writectlconfig,	\
			    _ddrss, _reg, _val)

#define lpddr4_k3_writereg_pi(_ddrss, _reg, _val)			\
	_lpddr4_k3_writereg(PI_SHIFT,					\
			    (_ddrss)->driverdt->writephyindepconfig,	\
			    _ddrss, _reg, _val)

#define lpddr4_k3_writereg_phy(_ddrss, _reg, _val)		\
	_lpddr4_k3_writereg(PHY_SHIFT,				\
			    (_ddrss)->driverdt->writephyconfig,	\
			    _ddrss, _reg, _val)

#define _lpddr4_k3_set(_type, _ddrss, _reg, _mask) do {	\
	u32 _val;					\
	lpddr4_k3_readreg_##_type(_ddrss, _reg, &_val);	\
	_val |= _mask;					\
	lpddr4_k3_writereg_##_type(_ddrss, _reg, _val);	\
} while (0)

#define _lpddr4_k3_clr(_type, _ddrss, _reg, _mask) do {	\
	u32 _val;					\
	lpddr4_k3_readreg_##_type(_ddrss, _reg, &_val);	\
	_val &= ~(_mask);				\
	lpddr4_k3_writereg_##_type(_ddrss, _reg, _val);	\
} while (0)

#define lpddr4_k3_set_ctl(_ddrss, _reg, _mask)	\
	_lpddr4_k3_set(ctl, _ddrss, _reg, _mask)

#define lpddr4_k3_clr_ctl(_ddrss, _reg, _mask) \
	_lpddr4_k3_clr(ctl, _ddrss, _reg, _mask)

#define lpddr4_k3_set_pi(_ddrss, _reg, _mask) \
	_lpddr4_k3_set(pi, _ddrss, _reg, _mask)

#define lpddr4_k3_clr_pi(_ddrss, _reg, _mask) \
	_lpddr4_k3_clr(pi, _ddrss, _reg, _mask)

#define lpddr4_k3_set_phy(_ddrss, _reg, _mask) \
	_lpddr4_k3_set(phy, _ddrss, _reg, _mask)

#define lpddr4_k3_clr_phy(_ddrss, _reg, _mask) \
	_lpddr4_k3_clr(phy, _ddrss, _reg, _mask)

#endif  /* LPDDR4_K3_REG */
