/*
 * flags_mx7.c
 *
 * Copyright (C) 2016, StreamUnlimited Engineering GmbH, http://www.streamunlimited.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>

/* We use the SNVS_LPGPR0_33 register to keep flags and
 * bootcounts across a reset. We use bits [0:7] to store boolean
 * flags, these flags are accessed with flag_write/flag_read.
 * Bits [8:15] store the bootcount which is accessed using
 * the bootcnt_write/bootcnt_read.
 */
#define SNVS_REG_ADDR (SNVS_BASE_ADDR + SNVS_LPGPR0_33)

int flag_write(u8 index, u8 data)
{
	u32 reg;

	if (index > 7 || data > 1)
		return -EINVAL;

	reg = readl(SNVS_REG_ADDR);
	reg &= ~(1 << index);
	reg |= (data << index);
	writel(reg, SNVS_REG_ADDR);

	return 0;
}

int flag_read(u8 index, u8 *data)
{
	u32 reg;

	if (index > 7)
		return -EINVAL;

	reg = readl(SNVS_REG_ADDR);
	*data = ((reg & (1 << index)) != 0) ? 1 : 0;

	return 0;
}

int flags_clear(void)
{
	u32 reg;

	reg = readl(SNVS_REG_ADDR);
	reg &= ~(0xFF);
	writel(reg, SNVS_REG_ADDR);

	return 0;
}

int bootcnt_write(u8 data)
{
	u32 reg;

	reg = readl(SNVS_REG_ADDR);
	reg &= ~(0xFF << 8);
	reg |= (data << 8);
	writel(reg, SNVS_REG_ADDR);

	return 0;
}

int bootcnt_read(u8 *data)
{
	u32 reg;

	reg = readl(SNVS_REG_ADDR);
	*data = (reg >> 8) & 0xFF;

	return 0;
}
