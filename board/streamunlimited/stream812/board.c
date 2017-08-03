/*
 * Copyright (C) 2015-2016 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx7-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hab.h>
#include <asm/gpio.h>
#include <asm/imx-common/iomux-v3.h>
#include <asm/imx-common/boot_mode.h>
#include <asm/imx-common/regs-gpmi.h>
#include <asm/io.h>
#include <linux/sizes.h>
#include <linux/mtd/mtd.h>
#include <common.h>
#include <miiphy.h>
#include <netdev.h>
#include <i2c.h>
#include <asm/imx-common/mxc_i2c.h>
#include <axp152.h>
#include <asm/arch/imx-regs.h>
#include <const_env_common.h>
#include <fsl_esdhc.h>

#include "../common/fwupdate.h"
#include "../common/partitions.h"
#include "../common/device_interface.h"
#include "../common/flags_mx7.h"

static struct sue_device_info __attribute__((section (".data"))) current_device;

DECLARE_GLOBAL_DATA_PTR;

#define WIFI_PDN_GPIO		IMX_GPIO_NR(5, 2)

#define PHY_NRESET_GPIO_S810	IMX_GPIO_NR(4, 22)
#define PHY_NRESET_GPIO_S812	IMX_GPIO_NR(5, 0)
#define USDHC3_PWR_GPIO		IMX_GPIO_NR(6, 11)

#define UART_PAD_CTRL		(PAD_CTL_DSE_3P3V_49OHM | PAD_CTL_PUS_PU100KOHM | PAD_CTL_HYS)
#define ENET_PAD_CTRL		(PAD_CTL_PUS_PU100KOHM | PAD_CTL_DSE_3P3V_49OHM)
#define ENET_RX_PAD_CTRL	(PAD_CTL_PUS_PU100KOHM | PAD_CTL_DSE_3P3V_49OHM)
#define I2C_PAD_CTRL		(PAD_CTL_DSE_3P3V_32OHM | PAD_CTL_SRE_SLOW | \
				 PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PUS_PU100KOHM)
#define USDHC_PAD_CTRL		(PAD_CTL_DSE_3P3V_32OHM | PAD_CTL_SRE_SLOW | \
				 PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PUS_PU47KOHM)

#define SNVS_HPCOMR		0x04

#ifdef CONFIG_SYS_I2C_MXC
/* I2C4 */
struct i2c_pads_info i2c_pad_info4 = {
	.scl = {
		.i2c_mode = MX7D_PAD_ENET1_RGMII_TD2__I2C4_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX7D_PAD_ENET1_RGMII_TD2__GPIO7_IO8 | MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(7, 8),
	},
	.sda = {
		.i2c_mode = MX7D_PAD_ENET1_RGMII_TD3__I2C4_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX7D_PAD_ENET1_RGMII_TD3__GPIO7_IO9 | MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(7, 9),
	},
};
#endif

#ifdef CONFIG_SYS_USE_MMC
static iomux_v3_cfg_t const usdhc3_emmc_pads[] = {
	MX7D_PAD_SD3_CLK__SD3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_CMD__SD3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_DATA0__SD3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_DATA1__SD3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_DATA2__SD3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_DATA3__SD3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_DATA4__SD3_DATA4 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_DATA5__SD3_DATA5 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_DATA6__SD3_DATA6 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_DATA7__SD3_DATA7 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX7D_PAD_SD3_STROBE__SD3_STROBE	 | MUX_PAD_CTRL(USDHC_PAD_CTRL),

	MX7D_PAD_SD3_RESET_B__GPIO6_IO11 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static struct fsl_esdhc_cfg usdhc_cfg[1] = {
	{ USDHC3_BASE_ADDR },
};

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;

	switch (cfg->esdhc_base) {
	case USDHC3_BASE_ADDR:
		ret = 1; /* Assume uSDHC3 emmc is always present */
		break;
	}

	return ret;
}

int board_mmc_init(bd_t *bis)
{
	int ret;

	imx_iomux_v3_setup_multiple_pads( usdhc3_emmc_pads, ARRAY_SIZE(usdhc3_emmc_pads));
	gpio_request(USDHC3_PWR_GPIO, "usdhc3_pwr");
	gpio_direction_output(USDHC3_PWR_GPIO, 0);
	udelay(500);
	gpio_direction_output(USDHC3_PWR_GPIO, 1);

	usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);

	ret = fsl_esdhc_initialize(bis, &usdhc_cfg[0]);
	if (ret)
		return ret;

	return 0;
}
#endif

static const iomux_v3_cfg_t const wifi_pads[] = {
	/* WLAN_PDN */
	MX7D_PAD_SD1_RESET_B__GPIO5_IO2 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int dram_init(void)
{
#ifdef CONFIG_ARMV7_TEE
	gd->ram_size = PHYS_SDRAM_SIZE - CONFIG_TEE_RAM_SIZE;
#else
	gd->ram_size = PHYS_SDRAM_SIZE;
#endif

	return 0;
}

static iomux_v3_cfg_t const uart1_pads[] = {
	MX7D_PAD_UART1_TX_DATA__UART1_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX7D_PAD_UART1_RX_DATA__UART1_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart1_pads, ARRAY_SIZE(uart1_pads));
}

#ifdef CONFIG_FEC_MXC
static const iomux_v3_cfg_t fec2_pads_s810[] = {
	MX7D_PAD_GPIO1_IO14__ENET2_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX7D_PAD_GPIO1_IO15__ENET2_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX7D_PAD_EPDC_GDRL__ENET2_RGMII_TX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX7D_PAD_EPDC_SDCE2__ENET2_RGMII_TD0 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX7D_PAD_EPDC_SDCE3__ENET2_RGMII_TD1 | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX7D_PAD_EPDC_SDCE0__ENET2_RGMII_RX_CTL | MUX_PAD_CTRL(ENET_RX_PAD_CTRL),
	MX7D_PAD_EPDC_SDCLK__ENET2_RGMII_RD0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL),
	MX7D_PAD_EPDC_SDLE__ENET2_RGMII_RD1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL),
	MX7D_PAD_EPDC_SDCE1__ENET2_RX_ER | MUX_PAD_CTRL(ENET_RX_PAD_CTRL),

	MX7D_PAD_EPDC_BDR0__CCM_ENET_REF_CLK2 | MUX_PAD_CTRL(ENET_PAD_CTRL) | MUX_MODE_SION,
};

static const iomux_v3_cfg_t fec2_nreset_pads_s810[] = {
	/* NRES_ETH pin */
	MX7D_PAD_ECSPI2_MISO__GPIO4_IO22 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static const iomux_v3_cfg_t fec2_nreset_pads_s812[] = {
	/* NRES_ETH pin */
	MX7D_PAD_SD1_CD_B__GPIO5_IO0 | MUX_PAD_CTRL(NO_PAD_CTRL),
};


static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads(fec2_pads_s810, ARRAY_SIZE(fec2_pads_s810));

	if (current_device.module == SUE_MODULE_S812_EXTENDED) {
		imx_iomux_v3_setup_multiple_pads(fec2_pads_s810, ARRAY_SIZE(fec2_nreset_pads_s812));
	} else {
		imx_iomux_v3_setup_multiple_pads(fec2_pads_s810, ARRAY_SIZE(fec2_nreset_pads_s810));
	}

}

int board_eth_init(bd_t *bis)
{
	int i;
	int ret = 0;

	setup_iomux_fec();

	/* Find the first phy on the mdio bus and assume it's connected to the FEC2 */
	for (i = 0; i < 8; i++) {
		ret = fecmxc_initialize_multi(bis, CONFIG_FEC_ENET_DEV, i, IMX_FEC_BASE);

		if (ret == 0) {
			printf("found PHY on address %d\n", i);
			current_device.fec2_phy_addr = i;
			break;
		}
	}

	return ret;
}

int board_phy_config(struct phy_device *phydev)
{
	int gpio;

	if (current_device.module == SUE_MODULE_S812_EXTENDED) {
		gpio = PHY_NRESET_GPIO_S812;
	} else {
		gpio = PHY_NRESET_GPIO_S810;
	}

	/* Reset the PHY */
	gpio_direction_output(gpio, 0);
	mdelay(5);
	gpio_set_value(gpio, 1);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

static int setup_fec(int fec_id)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs = (struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;
	int ret;

	if (fec_id == 0) {
		/* FEC1 */
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
				(IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_MASK | IOMUXC_GPR_GPR1_GPR_ENET1_CLK_DIR_MASK),
				(IOMUXC_GPR_GPR1_GPR_ENET1_CLK_DIR_MASK));
	} else if (fec_id == 1) {
		/* FEC2 */
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
				(IOMUXC_GPR_GPR1_GPR_ENET2_TX_CLK_SEL_MASK | IOMUXC_GPR_GPR1_GPR_ENET2_CLK_DIR_MASK),
				(IOMUXC_GPR_GPR1_GPR_ENET2_CLK_DIR_MASK));
	} else {
		return -EINVAL;
	}

	ret = set_clk_enet(ENET_50MHz);
	if (ret) {
		printf("%s: set_clk_enet() failed\n", __func__);
		return ret;
	}

	return 0;
}
#endif

#define FDT_BUFSIZE 32

int ft_board_setup(void *blob, bd_t *bd)
{
#ifdef CONFIG_FEC_MXC
	/* Patch fec2 phy address to first found phy */
	if (current_device.fec2_phy_addr != -1) {
		u32 new_phy_addr = current_device.fec2_phy_addr;
		int node, ret;

		printf("Patching fec2 PHY address in the device tree to %d\n", new_phy_addr);
		node = fdt_path_offset(blob, "/soc/aips-bus@30800000/ethernet@30bf0000/mdio/ethernet-phy@0");
		if (node < 0) {
			printf("WARN: Could not find the ethernet-phy node %d\n", node);
		} else {
			ret = fdt_setprop_u32(blob, node, "reg", new_phy_addr);
			if (ret < 0) {
				printf("WARN: Could not set reg property of ethernet-phy node %d\n", ret);
			}
		}
	}
#endif

	if (current_device.carrier == SUE_CARRIER_FACTORY_TESTER) {
		int node, ret;

		printf("INFO: Applying factory carrier specific fixups!\n");

		if (current_device.fec2_phy_addr == -1) {
			/* No ethernet phy found, disable fec2 */
			printf("INFO: fec2 will be disabled\n");

			node = fdt_path_offset(blob, "/soc/aips-bus@30800000/ethernet@30bf0000");
			if (node < 0) {
				printf("WARN: Could not find the ethernet node %d\n", node);
			} else {
				ret = fdt_setprop_string(blob, node, "status", "disabled");
				if (ret < 0) {
					printf("WARN: Could not set status property of ethernet node %d\n", ret);
				}
			}
		}
	}

	/*
	 * Enable additional operating points if desired.
	 *
	 * This allows us to specify `additional-operating-points` in the devicetree,
	 * which will be enabled when the `enable_additional_operating_points` variable
	 * in the const partition is set.
	 *
	 * We use this mechanism to allow customers to enable or disable 1.2GHz
	 * operation of the CPU.
	 *
	 * Essentially we are just taking the data from the `additional-operating-points`
	 * property and append it to the current `operating-points` before booting.
	 *
	 * NOTE: since in the current implementation this depends on `getconst_yesno()`
	 * we can only use it if CONFIG_CONST_ENV_COMMON is enabled.
	 */
#ifdef CONFIG_CONST_ENV_COMMON
	if (getconst_yesno("enable_additional_operating_points") == 1) {
		int ret, node, addproplen;
		const void *addprop;


		printf("INFO: Patching devicetree to enable additional operating points\n");
		node = fdt_path_offset(blob, "/cpus/cpu@0");
		if (node < 0) {
			printf("WARN: Could not find the cpu node %d\n", node);
		} else {
			addprop = fdt_getprop(blob, node, "additional-operating-points", &addproplen);
			if (addprop == NULL) {
				/* When fdt_getprop() returns NULL, addproplen will contain a negative error code */
				printf("WARN: Could not get `additional-operating-points` for cpu@0 %d\n", addproplen);
			} else if (addproplen > FDT_BUFSIZE) {
				/*
				 * If the `additional-operating-points` property has too many entries, it will be too
				 * large for our buffer, thus we print out a warning.
				 */
				printf("WARN: Too many additional operating points\n");
			} else {
				u8 buffer[FDT_BUFSIZE];

				/* Otherwise just copy the data of the additional-operating-points */
				memcpy(buffer, addprop, addproplen);

				/* And append it to our current operating-points */
				ret = fdt_appendprop(blob, node, "operating-points", buffer, addproplen);
				if (ret < 0) {
					printf("WARN: Failed to append `additional-operating-points` %d\n", ret);
				}
			}
		}
	}
#endif

	return 0;
}



int board_early_init_f(void)
{
	sue_device_detect(&current_device);

	setup_iomux_uart();

#ifdef CONFIG_SYS_I2C_MXC
	setup_i2c(3, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info4);
#endif
	return 0;
}

static enum sue_reset_cause get_reset_cause(void)
{
	u8 reg, flag;

	i2c_read(AXP152_ADDR, AXP152_GPIO0, 1, &reg, 1);
	if (reg & (1 << 2)) {
		/*
		 * If the bit is set we have a cold boot since, the
		 * bit is set per default on the AXP152. So we clear the bit
		 * and return a cold boot
		 */
		reg &= ~(1 << 2);
		i2c_write(AXP152_ADDR, AXP152_GPIO0, 1, &reg, 1);
		return SUE_RESET_CAUSE_POR;
	} else {
		/*
		 * Otherwise it was either a watchdog reset or a software reset.
		 * If it was a software reset, then before the actual reset, the
		 * kernel or U-Boot has set the `FWUP_FLAG_SWRESET_REQ` flag,
		 * so we can differentiate between those two.
		 */
		flag_read(FWUP_FLAG_SWRESET_REQ, &flag);

		if (flag) {
			flag_write(FWUP_FLAG_SWRESET_REQ, 0);
			return SUE_RESET_CAUSE_SOFTWARE;
		} else {
			return SUE_RESET_CAUSE_WDOG;
		}
	}
	return SUE_RESET_CAUSE_UNKNOWN;
}

/*
 * Before we do the reset we need to set a flag in the SNVS registers
 * so we can differentiate between a reset requested by the user/system
 * or a reset done by the HW watchdog.
 */
void reset_misc(void)
{
	flag_write(FWUP_FLAG_SWRESET_REQ, 1);
}

/*
 * This function prints the reset cause an does some thing like
 * clearing all flags if the reset cause was POR.
 */
static int handle_reset_cause(enum sue_reset_cause reset_cause)
{
	printf("Reset cause: ");
	switch(current_device.reset_cause) {
		case SUE_RESET_CAUSE_POR:
			flags_clear();
			bootcnt_write(0);
			printf("POR\n");
			break;
		case SUE_RESET_CAUSE_SOFTWARE:
			printf("SOFTWARE\n");
			break;
		case SUE_RESET_CAUSE_WDOG:
			printf("WDOG\n");
			break;
		default:
			printf("unknown\n");
			return -EINVAL;
			break;
	};

	return 0;
}

int board_init(void)
{
	u32 reg;

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	/*
	 * Set "Non-Privileged Software Access Enable" bit
	 * This is required for normal-world SW to write to SRTC ('hwclock -w' hangs) and GP (sfuflags doesn't work) registers
	 */
	reg = readl(SNVS_BASE_ADDR + SNVS_HPCOMR);
	reg |= (1 << 31);
	writel(reg, SNVS_BASE_ADDR + SNVS_HPCOMR);

	/*
	 * Disable SNVS zeroization, otherwise the flags in the SNVS_LPGPR register will not be written
	 * because we are probably getting some tamper events.
	 */
	reg = readl(SNVS_BASE_ADDR + SNVS_LPCR);
	reg |= (1 << 24);
	writel(reg, SNVS_BASE_ADDR + SNVS_LPCR);

	current_device.reset_cause = get_reset_cause();
	handle_reset_cause(current_device.reset_cause);

#ifdef CONFIG_FEC_MXC
	setup_fec(CONFIG_FEC_ENET_DEV);
#endif

	/* We want to keep the WiFi in power down until the kernel puts it out of it. */
	imx_iomux_v3_setup_multiple_pads(wifi_pads, ARRAY_SIZE(wifi_pads));
	gpio_direction_output(WIFI_PDN_GPIO, 0);

	sue_carrier_ops_init(&current_device);
	sue_carrier_init(&current_device);

	return 0;
}

#ifdef CONFIG_POWER
int power_init_board(void)
{
	i2c_set_bus_num(3);

	axp152_init();
	/* Disable LDO0 and ALDO2 */
	axp152_disable_ldo0();
	axp152_set_power_output(0xFB);
	mdelay(20);

	axp152_set_ldo0(AXP152_LDO0_3V3, AXP152_LDO0_CURR_1500MA);
	axp152_set_aldo2(AXP152_ALDO_3V3);

	axp152_set_dcdc3(1350);
	axp152_set_power_output(0xFF);

	return 0;
}
#endif

static const iomux_v3_cfg_t wdog_pads[] = {
	MX7D_PAD_GPIO1_IO00__WDOG1_WDOG_B | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int board_late_init(void)
{
	char buffer[64];

	if (fwupdate_init(&current_device) < 0) {
		printf("ERROR: fwupdate_init() call failed!\n");
	}

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));
	set_wdog_reset((struct wdog_regs *)WDOG1_BASE_ADDR);

	if (current_device.carrier_flags & SUE_CARRIER_FLAGS_HAS_DAUGHTER) {
		snprintf(buffer, sizeof(buffer), "%s_%s_%s",
				sue_device_get_canonical_module_name(&current_device),
				sue_device_get_canonical_carrier_name(&current_device),
				sue_device_get_canonical_daughter_name(&current_device));
	} else {
		snprintf(buffer, sizeof(buffer), "%s_%s", sue_device_get_canonical_module_name(&current_device), sue_device_get_canonical_carrier_name(&current_device));
	}
	printf("Setting fit_config: %s\n", buffer);
	setenv("fit_config", buffer);

	// pass board 'secure' state (ie, locked secure fuses/..) to env
	snprintf(buffer, sizeof(buffer), "%d", is_hab_enabled() ? 1 : 0);
	printf("Setting secure_board: %s\n", buffer);
	setenv("secure_board", buffer);

	sue_carrier_late_init(&current_device);

	return 0;
}

int checkboard(void)
{
	sue_print_device_info(&current_device);
	return 0;
}

#ifdef CONFIG_USB_EHCI_MX7
iomux_v3_cfg_t const usb_otg1_pads[] = {
	MX7D_PAD_GPIO1_IO05__USB_OTG1_PWR | MUX_PAD_CTRL(NO_PAD_CTRL),
};

iomux_v3_cfg_t const usb_otg2_pads[] = {
	MX7D_PAD_GPIO1_IO07__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int board_ehci_hcd_init(int port)
{
	switch (port) {
	case 0:
		imx_iomux_v3_setup_multiple_pads(usb_otg1_pads,
						 ARRAY_SIZE(usb_otg1_pads));
		break;
	case 1:
		imx_iomux_v3_setup_multiple_pads(usb_otg2_pads,
						 ARRAY_SIZE(usb_otg2_pads));
		break;
	default:
		printf("MXC USB port %d not yet supported\n", port);
		return 1;
	}
	return 0;
}
#endif
