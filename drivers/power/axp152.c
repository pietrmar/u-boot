/*
 * (C) Copyright 2012
 * Henrik Nordstrom <henrik@henriknordstrom.net>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <i2c.h>
#include <axp152.h>
#include <errno.h>

static int axp152_write(enum axp152_reg reg, u8 val)
{
	return i2c_write(AXP152_ADDR, reg, 1, &val, 1);
}

static int axp152_read(enum axp152_reg reg, u8 *val)
{
	return i2c_read(AXP152_ADDR, reg, 1, val, 1);
}

static u8 axp152_mvolt_to_target(int mvolt, int min, int max, int div)
{
	if (mvolt < min)
		mvolt = min;
	else if (mvolt > max)
		mvolt = max;

	return (mvolt - min) / div;
}

int axp152_set_dcdc1(enum axp152_dcdc1_voltages volt)
{
	if (volt < AXP152_DCDC1_1V7 || volt > AXP152_DCDC1_3V5)
		return -EINVAL;

	return axp152_write(AXP152_DCDC1_VOLTAGE, volt);
}

int axp152_set_dcdc2(int mvolt)
{
	int rc;
	u8 current, target;

	target = axp152_mvolt_to_target(mvolt, 700, 2275, 25);

	/* Do we really need to be this gentle? It has built-in voltage slope */
	while ((rc = axp152_read(AXP152_DCDC2_VOLTAGE, &current)) == 0 &&
	       current != target) {
		if (current < target)
			current++;
		else
			current--;
		rc = axp152_write(AXP152_DCDC2_VOLTAGE, current);
		if (rc)
			break;
	}
	return rc;
}

int axp152_set_dcdc3(int mvolt)
{
	u8 target = axp152_mvolt_to_target(mvolt, 700, 3500, 50);

	return axp152_write(AXP152_DCDC3_VOLTAGE, target);
}

int axp152_set_dcdc4(int mvolt)
{
	u8 target = axp152_mvolt_to_target(mvolt, 700, 3500, 25);

	return axp152_write(AXP152_DCDC4_VOLTAGE, target);
}

int axp152_set_ldo0(enum axp152_ldo0_volts volt, enum axp152_ldo0_curr_limit curr_limit)
{
	u8 target = curr_limit | (volt << 4) | (1 << 7);

	return axp152_write(AXP152_LDO0_VOLTAGE, target);
}

int axp152_disable_ldo0(void)
{
	int ret;
	u8 target;

	ret = axp152_read(AXP152_LDO0_VOLTAGE, &target);
	if (ret)
		return ret;

	target &= ~(1 << 7);

	return axp152_write(AXP152_LDO0_VOLTAGE, target);
}

int axp152_set_ldo1(int mvolt)
{
	u8 target = axp152_mvolt_to_target(mvolt, 700, 3500, 100);

	return axp152_write(AXP152_LDO1_VOLTAGE, target);
}

int axp152_set_ldo2(int mvolt)
{
	u8 target = axp152_mvolt_to_target(mvolt, 700, 3500, 100);

	return axp152_write(AXP152_LDO2_VOLTAGE, target);
}

int axp152_set_aldo1(enum axp152_aldo_voltages volt)
{
	u8 val;
	int ret;

	ret = axp152_read(AXP152_ALDO1_ALDO2_VOLTAGE, &val);
	if (ret)
		return ret;

	val |= (volt << 4);
	return axp152_write(AXP152_ALDO1_ALDO2_VOLTAGE, val);
}

int axp152_set_aldo2(enum axp152_aldo_voltages volt)
{
	u8 val;
	int ret;

	ret = axp152_read(AXP152_ALDO1_ALDO2_VOLTAGE, &val);
	if (ret)
		return ret;

	val |= volt;
	return axp152_write(AXP152_ALDO1_ALDO2_VOLTAGE, val);
}

int axp152_set_power_output(int val)
{
	return axp152_write(AXP152_POWER_CONTROL, val);
}

int axp152_init(void)
{
	u8 reg;
	int ret;

	ret = axp152_read(AXP152_CHIP_VERSION, &reg);
	if (ret)
		return ret;

	if (reg != 0x05)
		return -1;


	/* Set the power off sequence to `reverse of power on sequence` */
	ret = axp152_read(AXP152_SHUTDOWN, &reg);
	if (ret)
		return ret;
	reg |= AXP152_POWEROFF_SEQ;
	ret = axp152_write(AXP152_SHUTDOWN, reg);

	return ret;
}
