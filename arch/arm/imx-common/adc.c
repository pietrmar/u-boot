/*
 * adc.c
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
#include <asm/arch/clock.h>

#define BIT(_x_)	(1 << (_x_))

/* ADC register */
#define IMX7D_REG_ADC_CH_A_CFG1			0x00
#define IMX7D_REG_ADC_CH_A_CFG2			0x10
#define IMX7D_REG_ADC_CH_B_CFG1			0x20
#define IMX7D_REG_ADC_CH_B_CFG2			0x30
#define IMX7D_REG_ADC_CH_C_CFG1			0x40
#define IMX7D_REG_ADC_CH_C_CFG2			0x50
#define IMX7D_REG_ADC_CH_D_CFG1			0x60
#define IMX7D_REG_ADC_CH_D_CFG2			0x70
#define IMX7D_REG_ADC_CH_SW_CFG			0x80
#define IMX7D_REG_ADC_TIMER_UNIT		0x90
#define IMX7D_REG_ADC_DMA_FIFO			0xa0
#define IMX7D_REG_ADC_FIFO_STATUS		0xb0
#define IMX7D_REG_ADC_INT_SIG_EN		0xc0
#define IMX7D_REG_ADC_INT_EN			0xd0
#define IMX7D_REG_ADC_INT_STATUS		0xe0
#define IMX7D_REG_ADC_CHA_B_CNV_RSLT		0xf0
#define IMX7D_REG_ADC_CHC_D_CNV_RSLT		0x100
#define IMX7D_REG_ADC_CH_SW_CNV_RSLT		0x110
#define IMX7D_REG_ADC_DMA_FIFO_DAT		0x120
#define IMX7D_REG_ADC_ADC_CFG			0x130

#define IMX7D_REG_ADC_CHANNEL_CFG2_BASE		0x10
#define IMX7D_EACH_CHANNEL_REG_OFFSET		0x20

#define IMX7D_REG_ADC_CH_CFG1_CHANNEL_EN			(0x1 << 31)
#define IMX7D_REG_ADC_CH_CFG1_CHANNEL_SINGLE			BIT(30)
#define IMX7D_REG_ADC_CH_CFG1_CHANNEL_AVG_EN			BIT(29)
#define IMX7D_REG_ADC_CH_CFG1_CHANNEL_SEL(x)			((x) << 24)

#define IMX7D_REG_ADC_CH_CFG2_AVG_NUM_4				(0x0 << 12)
#define IMX7D_REG_ADC_CH_CFG2_AVG_NUM_8				(0x1 << 12)
#define IMX7D_REG_ADC_CH_CFG2_AVG_NUM_16			(0x2 << 12)
#define IMX7D_REG_ADC_CH_CFG2_AVG_NUM_32			(0x3 << 12)

#define IMX7D_REG_ADC_TIMER_UNIT_PRE_DIV_4			(0x0 << 29)
#define IMX7D_REG_ADC_TIMER_UNIT_PRE_DIV_8			(0x1 << 29)
#define IMX7D_REG_ADC_TIMER_UNIT_PRE_DIV_16			(0x2 << 29)
#define IMX7D_REG_ADC_TIMER_UNIT_PRE_DIV_32			(0x3 << 29)
#define IMX7D_REG_ADC_TIMER_UNIT_PRE_DIV_64			(0x4 << 29)
#define IMX7D_REG_ADC_TIMER_UNIT_PRE_DIV_128			(0x5 << 29)

#define IMX7D_REG_ADC_ADC_CFG_ADC_CLK_DOWN			BIT(31)
#define IMX7D_REG_ADC_ADC_CFG_ADC_POWER_DOWN			BIT(1)
#define IMX7D_REG_ADC_ADC_CFG_ADC_EN				BIT(0)

#define IMX7D_REG_ADC_INT_CHA_COV_INT_EN			BIT(8)
#define IMX7D_REG_ADC_INT_CHB_COV_INT_EN			BIT(9)
#define IMX7D_REG_ADC_INT_CHC_COV_INT_EN			BIT(10)
#define IMX7D_REG_ADC_INT_CHD_COV_INT_EN			BIT(11)
#define IMX7D_REG_ADC_INT_CHANNEL_INT_EN			(IMX7D_REG_ADC_INT_CHA_COV_INT_EN | \
								 IMX7D_REG_ADC_INT_CHB_COV_INT_EN | \
								 IMX7D_REG_ADC_INT_CHC_COV_INT_EN | \
								 IMX7D_REG_ADC_INT_CHD_COV_INT_EN)
#define IMX7D_REG_ADC_INT_STATUS_CHANNEL_INT_STATUS		0xf00
#define IMX7D_REG_ADC_INT_STATUS_CHANNEL_CONV_TIME_OUT		0xf0000

#define IMX7D_ADC1_BASE						0x30610000
#define IMX7D_ADC2_BASE						0x30620000

static const u32 adc_bases[] = {
	IMX7D_ADC1_BASE,
	IMX7D_ADC2_BASE,
};

#define valid_adc_num(__x) ((__x) > 0 && (__x) <= ARRAY_SIZE(adc_bases))

static u32 __attribute__((section (".data"))) active_adc_list;

int init_adc(int adc_num)
{
	u32 reg, base;

	if (!valid_adc_num(adc_num))
		return -EINVAL;
	base = adc_bases[adc_num - 1];

	if (active_adc_list == 0)
		enable_adc_clk(1);

	/* power up and enable adc analogue core */
	reg = readl(base + IMX7D_REG_ADC_ADC_CFG);
	reg &= ~(IMX7D_REG_ADC_ADC_CFG_ADC_CLK_DOWN | IMX7D_REG_ADC_ADC_CFG_ADC_POWER_DOWN);
	reg |= IMX7D_REG_ADC_ADC_CFG_ADC_EN;
	writel(reg, base + IMX7D_REG_ADC_ADC_CFG);

	active_adc_list |= (1 << (adc_num - 1));

	return 0;
}

int shutdown_adc(int adc_num)
{
	u32 reg, base;

	if (!valid_adc_num(adc_num))
		return -EINVAL;
	base = adc_bases[adc_num - 1];

	reg = readl(base + IMX7D_REG_ADC_ADC_CFG);
	reg |= IMX7D_REG_ADC_ADC_CFG_ADC_CLK_DOWN | IMX7D_REG_ADC_ADC_CFG_ADC_POWER_DOWN;
	reg &= ~IMX7D_REG_ADC_ADC_CFG_ADC_EN;
	writel(reg, base + IMX7D_REG_ADC_ADC_CFG);

	active_adc_list &= ~(1 << (adc_num - 1));

	if (active_adc_list == 0)
		enable_adc_clk(0);

	return 0;
}

int read_adc_channel(int adc_num, int channel)
{
	u32 reg, base;

	if (!valid_adc_num(adc_num))
		return -EINVAL;
	base = adc_bases[adc_num - 1];

	if (!(active_adc_list & (1 << (adc_num - 1))))
		return -EIO;

	reg = IMX7D_REG_ADC_CH_CFG2_AVG_NUM_32;
	writel(reg, base + IMX7D_REG_ADC_CH_A_CFG2);

	/* Disable all interrupts */
	writel(0, base + IMX7D_REG_ADC_INT_EN);

	reg = IMX7D_REG_ADC_CH_CFG1_CHANNEL_SINGLE | IMX7D_REG_ADC_CH_CFG1_CHANNEL_AVG_EN | IMX7D_REG_ADC_CH_CFG1_CHANNEL_EN | IMX7D_REG_ADC_CH_CFG1_CHANNEL_SEL(channel & 0xF);
	writel(reg, base + IMX7D_REG_ADC_CH_A_CFG1);

	/* TODO: Maybe implement timeout */
	do {
		reg = readl(base + IMX7D_REG_ADC_INT_STATUS);
	} while (!(reg & IMX7D_REG_ADC_INT_CHA_COV_INT_EN));

	/* Apparently we need to clear the conversion bit manually */
	reg &= ~(IMX7D_REG_ADC_INT_CHA_COV_INT_EN);
	writel(reg, base + IMX7D_REG_ADC_INT_STATUS);

	reg = readl(base + IMX7D_REG_ADC_CHA_B_CNV_RSLT);

	return (reg & 0xFFF);
}
