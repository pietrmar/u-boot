/*
 * Copyright (C) 2015-2016 Freescale Semiconductor, Inc.
 *
 * Configuration settings for the Freescale i.MX7D SABRESD board.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __MX7D_SABRESD_CONFIG_H
#define __MX7D_SABRESD_CONFIG_H

#include <asm/arch/imx-regs.h>
#include <linux/sizes.h>
#include "mx7_common.h"
#include <asm/imx-common/gpio.h>
#include <configs/sue_fwupdate_common.h>

#define CONFIG_MX7
#define CONFIG_ROM_UNIFIED_SECTIONS
#define CONFIG_SYS_GENERIC_BOARD
#define CONFIG_DISPLAY_CPUINFO
#define CONFIG_DISPLAY_BOARDINFO

#define PHYS_SDRAM_SIZE CONFIG_PHYS_SDRAM_SIZE

#ifdef CONFIG_ARMV7_TEE
#define CONFIG_SMP_PEN_ADDR 		0x3039007c
#define CONFIG_TIMER_CLK_FREQ		CONFIG_SC_TIMER_CLK
#define CONFIG_ARMV7_SECURE_BASE	(IRAM_BASE_ADDR + SZ_32K)
#endif

#define CONFIG_FIT
#define CONFIG_FIT_SIGNATURE
#define CONFIG_RSA
#define CONFIG_RSA_SOFTWARE_EXP

#define CONFIG_DBG_MONITOR

/* uncomment for SECURE mode support */
#define CONFIG_SECURE_BOOT

#ifdef CONFIG_SECURE_BOOT
#ifdef CONFIG_USE_PLUGIN
#define CONFIG_CSF_SIZE 0x2000
#else
#define CONFIG_CSF_SIZE 0x4000
#endif /* CONFIG_USE_PLUGIN */
#endif /* CONFIG_SECURE_BOOT */

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG
#define CONFIG_REVISION_TAG

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(32 * SZ_1M)

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_CMD_GPIO
#define CONFIG_MXC_GPIO
#define CONFIG_CMD_BMODE

#define CONFIG_MXC_UART
#define CONFIG_MXC_UART_BASE		UART1_IPS_BASE_ADDR

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_CONS_INDEX		1
#define CONFIG_BAUDRATE			115200

/* enable 'nulldev' input device */
#define CONFIG_SYS_DEVICE_NULLDEV

#define CONFIG_CMD_FUSE
#define CONFIG_MXC_OCOTP

#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_MII
#define CONFIG_CMD_NET
#define CONFIG_MII

#define CONFIG_FEC_MXC
#define CONFIG_FEC_XCV_TYPE             RMII
#define CONFIG_ETHPRIME                 "FEC"
#define CONFIG_FEC_MXC_PHYADDR          0x0
#define CONFIG_PHYLIB
#define CONFIG_PHY_SMSC
#define CONFIG_FEC_DMA_MINALIGN		64
#define CONFIG_FEC_ENET_DEV		1
#define IMX_FEC_BASE			ENET2_IPS_BASE_ADDR
#define CONFIG_FEC_MXC_MDIO_BASE	ENET2_IPS_BASE_ADDR

/* PMIC */
#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_AXP152_POWER

#undef CONFIG_BOOTM_NETBSD
#undef CONFIG_BOOTM_PLAN9
#undef CONFIG_BOOTM_RTEMS

#undef CONFIG_CMD_EXPORTENV
#undef CONFIG_CMD_IMPORTENV

/* I2C configs */
#define CONFIG_CMD_I2C
#define CONFIG_SYS_I2C
#define CONFIG_SYS_I2C_MXC
#define CONFIG_SYS_I2C_SPEED		100000
#define CONFIG_SYS_SPD_BUS_NUM		3

/* Command definition */
#include <config_cmd_default.h>

#define CONFIG_CMD_SFU_PARSER
#define CONFIG_SFU_RAM

#undef CONFIG_CMD_IMLS

#define CONFIG_BOOTDELAY		1

#define CONFIG_LOADADDR			0x80800000
#define CONFIG_SYS_TEXT_BASE		0x87800000

#define CONFIG_SYS_USE_MMC
#ifdef CONFIG_SYS_USE_MMC
/* MMC Configs */
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR	0

#define CONFIG_MMC
#define CONFIG_CMD_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_CMD_FAT
#define CONFIG_DOS_PARTITION
#define CONFIG_SUPPORT_EMMC_BOOT /* eMMC specific */
#endif

#define CONFIG_MMCROOT			"/dev/mmcblk0p2"  /* USDHC1 */


#define CONFIG_CMD_SETEXPR

#define CONFIG_SYS_ADC_MXC

#define CONFIG_FIT

#define CONFIG_BOOTCOUNT_LIMIT

#define CONFIG_RBTREE
#define CONFIG_LZO

#define CONFIG_MFG_ENV_SETTINGS \
	"mfg_args=setenv bootargs console=${console},${baudrate} " \
		"root=/dev/ram0 rdinit=/linuxrc\0" \
	"bootcmd_mfg=run mfg_args; setenv loadaddr ${fdt_addr}; bootm ${loadaddr}#factory@1;\0"

#if 0
		"dtbfile=imx7d-sue-s812-factory-0-0.dtb\0" \
		"rootpath=/srv/nfs/rootfs/stream810\0" \
		"serverip=10.1.14.80\0" \
		"bootfile=zImage\0" \
		"nfsopts=nolock\0" \
		"fdt_addr=0x83000000\0" \
		"console=ttymxc0,115200\0" \
		"optargs=debug\0" \
		"bootargs_defaults=setenv bootargs console=${console} ${optargs} ${mtdparts}\0" \
		"net_args=run bootargs_defaults; setenv bootargs ${bootargs} root=/dev/nfs nfsroot=${serverip}:${rootpath},${nfsopts} rw ip=dhcp; echo ${bootargs}\0" \
		"net_run=fdt addr ${fdt_addr};fdt resize; bootz ${loadaddr} - ${fdt_addr}\0" \
		"net_boot=setenv bootm_boot_mode sec; setenv autoload no; dhcp; setenv loadaddr 0x80800000; tftp ${loadaddr} ${bootfile};tftp ${fdt_addr} ${dtbfile};run net_args; run net_run\0" \
		"tftp_boot=setenv bootm_boot_mode sec; setenv autoload no; dhcp; setenv loadaddr 0x80800000; tftp ${loadaddr} ${bootfile};tftp ${fdt_addr} ${dtbfile};run nandargs; run net_run\0" \
		"bootcmd=setenv ethaddr 00:01:02:03:04:05; run net_boot\0"
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
		CONFIG_MFG_ENV_SETTINGS \
		"fdt_addr=0x83000000\0" \
		"fdt_high=0xffffffff\0" \
		"console=ttymxc0,115200\0" \
		"bootcmd=echo boot not implemented!\0"

#define CONFIG_BOOTCOMMAND 		SUE_FWUPDATE_BOOTCOMMAND

/* Miscellaneous configurable options */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT		"=> "
#define CONFIG_AUTO_COMPLETE
#define CONFIG_SYS_CBSIZE		2048

/* Print Buffer Size */
#define CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_MAXARGS		256
#define CONFIG_SYS_BARGSIZE CONFIG_SYS_CBSIZE

#ifdef CONFIG_CMD_MEMTEST
#define CONFIG_SYS_MEMTEST_START	0x80000000
/* TODO: Check the size here */
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + SZ_128M)
#define CONFIG_SYS_ALT_MEMTEST
#endif

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_HZ			1000

#define CONFIG_CMDLINE_EDITING
#define CONFIG_STACKSIZE		SZ_128K

/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM
#define CONFIG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE	IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* FLASH and environment organization */
#define CONFIG_SYS_NO_FLASH

#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_ENV_SIZE			SZ_256K
#define CONFIG_ENV_SECT_SIZE		CONFIG_ENV_SIZE

#define CONFIG_OF_LIBFDT
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_CMD_BOOTZ

#define CONFIG_CMD_CACHE

/* USB Configs */
#define CONFIG_CMD_USB
#define CONFIG_USB_EHCI
#define CONFIG_USB_EHCI_MX7
#define CONFIG_USB_STORAGE
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET
#define CONFIG_MXC_USB_PORTSC  (PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_MXC_USB_FLAGS   0
#define CONFIG_USB_MAX_CONTROLLER_COUNT 2

#define CONFIG_DOS_PARTITION
#define CONFIG_EFI_PARTITION
#define CONFIG_PARTITION_UUIDS
#define CONFIG_CMD_FAT
#define CONFIG_CMD_GPT

#define CONFIG_USB_HOST_ETHER
#define CONFIG_USB_ETHER_ASIX
#define CONFIG_USB_ETHER_ASIX88179
#define CONFIG_USB_ETHER_MCS7830
#define CONFIG_USB_ETHER_SMSC95XX

#define CONFIG_IMX_THERMAL

#endif	/* __CONFIG_H */
