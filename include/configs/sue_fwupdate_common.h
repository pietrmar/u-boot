/*
 * sue_fwupdate_common.h
 *
 * This file contains the common U-Boot script for the firmware upgrade process.
 */

#ifndef __SUE_FWUPDATE_COMMON_H
#define __SUE_FWUPDATE_COMMON_H

/*
 * UPDATE PROCESS is fully described at: https://extern.streamunlimited.com:8443/display/Stream800/Firmware+Update+Architecture
 *
 * Update methods:
 *
 * 	USB RECOVERY:
 *
 * 		Following conditions has to be fulfilled to start USB recovery in u-boot:
 * 			board specific tests on USB recovery passes
 * 			=> this test usually check press of button connected to GPIO1_9
 * 			USB stick is plugged in during boot
 * 			USB stick contains valid file called sfupdate on first partition, which has to be fat32
 *
 * 		Following steps will be performed if conditions are fulfilled:
 * 			U-boot copy sfupdate to download partition
 * 			U-boot set FAIL flag and reset the board
 * 			Restarted U-boot loads sfupdate from download partition into RAM
 * 			U-boot verifies checksums and versions of all parts of sfupdate inside RAM
 * 			U-boot flashes MLO, u-boot and u-boot-env if version changed  In this case it also reset the board
 * 				If reset was performed than, u-boot recognize that FAIL flag is set and continues usb recovery
 * 				loads sfupdate from download partition into RAM again
 * 				verifies the checksums and versions and it will find out that MLO, u-boot and u-boot-env are up to date
 * 			FAIL flag is still set so u-boot erase the fit parition
 * 			U-boot automatically flashes rootfs without version check.
 * 			U-boot automatically flashes the fit image.
 * 			U-boot boots the fit image.
 * 			Userspace after correct boot should clear FAIL flag.
 *
 * 	NETWORK UPDATE:
 *
 * 		Steps happening in the userspace:
 * 			update image is downloaded and flashed to download partition
 * 			board is restarted with UPDATE flag set
 *
 * 		Following conditions has to be fullfilled to start net update in u-boot:
 * 			UPDATE flag is set
 *
 * 		Following steps will be performed if conditions are fulfilled:
 * 			Restarted U-boot loads sfupdate from download partition into RAM
 * 			U-boot verifies checksums and versions of all parts of sfupdate inside RAM
 * 			U-boot flashes MLO, u-boot and u-boot-env if version changed. In this case it also reset the board
 * 				If reset was performed than, u-boot recognize that UPDATE flag is set and continues with network update
 * 				loads sfupdate from download partition into RAM again
 * 				verifies the checksums and versions and it will find out that MLO, u-boot and u-boot-env are up to date
 * 			If rootfs or fit image version changed, fit partition is erased
 * 			If rootfs version changed, rootfs is erased and flashed
 * 			If rootfs or fit image version changed, fit image is flashed
 * 			U-boot clear UPDATE flag
 * 			U-boot set FAIL flag
 * 			U-boot boots the fit image
 * 			Userspace after correct boot should clear FAIL flag.
 *
 * METHODS:
 *		readUimage                      - loads uImage to RAM
 *		handle_encm                     - decrypts chunk configured by last "sfu chnkhdr", if it was encrypted
 *		update_download_from_usb        - loads sfupdate file from usb and saves it into download partition
 * 		check_bootloaders_need_flashing - check MLO, u-boot, u-boot-env partition in image, set enviroments to signalize that flashing them is needed
 * 		flash_bootloaders_as_needed     - flash MLO, u-boot, u-boot-env partition as needed, decision is based on enviroment set by check_bootloaders_need_flashing
 * 		check_system_need_flashing      - rootfs, fit partition in image, set enviroments to signalize that flashing them is needed
 * 		flash_system_as_needed          - rootfs, fit partition in image as needed, decision is based on enviroment set by check_system_need_flashing
 *		sfu_boot                        - main method
 *
 * VARIABLES:
 * 		bootcount                       - actual count of uncorrect reboots
 * 		bootlimit                       - limit of uncorrcet reboots, if reached, board does not boot anymore
 * 		sfu_load_addr                   - base RAM address for u-boot operations
 * 		uboot_vers                      - version of u-boot stored in NAND
 * 		ubootenv_vers                   - version of u-bootenv stored in NAND
 * 		rootfs_vers                     - version of rootfs stored in NAND
 * 		cramfsdir                       - directory in cramfs image where dtb file for this board should be located
 * 		board_name                      - name of the board, used for locating DTB file in cramfs image
 * 		board_rev                       - rev of the board, used for locating DTB file in cramfs image
 * 		cramfsdir                       - directory in cramfs image where dtb file for this board should be located
 * 		SFU_CHNK_ENCM                   - actual chunk encryption flag in sfupdate file, set by call sfu chnkdr command
 * 		SFU_CHNK_DATA                   - actual chunk data start address, set by call sfu chnkdr command
 * 		SFU_CHNK_SIZE                   - actual chunk data size, set by call sfu chnkdr command
 * 		SFU_CHNK_VERS                   - actual chunk version in sfupdate file, set by call sfu chnkdr command
 * 		SFU_DECRYPT_MLO_CHNK_SIZE       - size of MLO chunk after decryption, set by call sfu decrypt command
 * 		SFU_DECRYPT_UBOOT_CHNK_SIZE     - size of u-boot chunk after decryption, set by call sfu decrypt command
 * 		SFU_DECRYPT_UBOOTENV_CHNK_SIZE  - size of u-boot chunk after decryption, set by call sfu decrypt command
 * 		SFU_DECRYPT_ROOTFS_CHNK_SIZE    - size rootfs chunk after decryption, set by call sfu decrypt command
 * 		SFU_DECRYPT_FIT_CHNK_SIZE       - size of fit chunk after decryption, set by call sfu decrypt command
 * 		UBOOT_NEEDS_FLASHING            - flag for later u-boot flashing
 * 		UBOOTENV_NEEDS_FLASHING         - flag for later u-boot-env flashing
 * 		ROOTFS_NEEDS_FLASHING           - flag for later rootfs flashing
 * 		FIT_NEEDS_FLASHING              - flag for later fit flashing
 */

#define SUE_FWUPDATE_EXTRA_ENV_SETTINGS \
    "fdt_addr=0x83000000\0" \
    "fdt_high=0xffffffff\0" \
    "console=ttymxc0,115200\0" \
    "bootfile=uImage\0" \
    "optargs=mwifiex.driver_mode=0x3\0" \
    "sfu_load_addr=0x80800000\0" \
    "wdtargs=imx2_wdt.timeout=120 imx2_wdt.early_disable=0\0" \
    "bootargs_defaults=setenv bootargs console=${console}\0" \
    "eraseSettings=" \
        "if nand erase.part settings; " \
            "then " \
            "echo \"INFO: nand erase successful\"; " \
        "else " \
            "echo \"ERROR: nand erase failed\"; " \
            "sfu errstate; " \
        "fi; \0" \
    "mtdparts=\0" \
    "readuImage=" \
        "if nboot ${fdt_addr} fit; " \
            "then " \
            "echo \"INFO: fit partition load successful\"; " \
        "else " \
            "echo \"ERROR: cannot load fit image from nand\"; " \
            "reset; " \
        "fi;\0" \
\
\
    "nandroot=ubi0:stream800-rootfs rw\0" \
    "nandrootfstype=ubifs rootwait=1\0" \
    "nandargs=const toenv eth_int_addr; " \
        "setenv bootargs console=${console} " \
        "ubi.mtd=5 root=${nandroot} noinitrd ${wdtargs} " \
        "rootfstype=${nandrootfstype} fec.macaddr=${eth_int_addr} " \
        "${mtdparts} ${optargs}\0" \
    "nand_boot=echo \"Booting from nand ...\"; " \
        "run nandargs; " \
        "echo \"INFO: loading fit image into RAM...\"; " \
        "bstate booting; " \
        "run readuImage; " \
        "echo \"INFO: booting fit image...\"; " \
        "bootm ${fdt_addr}; " \
        "echo \"INFO: fit boot failed...\"; " \
        "echo \"INFO: resetting...\"; " \
        "reset;\0" \
    "panicargs=panic=10 mem=127M\0" \
    "usbmount=usb reset\0" \
 \
 \
    "handle_encm=" \
        "if test ${SFU_CHNK_ENCM} = 00000001; " \
            "then " \
            "if sfu decrypt ${SFU_CHNK_DATA} ${SFU_CHNK_SIZE}; " \
                "then " \
                "echo \"INFO: chunk decrypt successful\"; " \
            "else " \
                "echo \"ERROR: chunk decrypt failed\"; " \
                "sfu errstate; " \
            "fi; " \
        "elif test ${SFU_CHNK_ENCM} = 00000000; " \
            "then " \
             "echo \"INFO: chunk not encrypted\"; " \
        "else " \
            "echo \"ERROR: encryption method invalid (${SFU_CHNK_ENCM})\"; " \
            "sfu errstate; " \
        "fi;\0" \
 \
 \
    "verify_sfu_image="\
	"if sfu valid ${sfu_load_addr}; " \
           "then " \
           "echo \"INFO: SFU image valid\"; " \
           "echo \"INFO: download partition being erased\"; " \
           "nand erase.part download; " \
           "echo \"INFO: writing sfupdate image to flash...\"; " \
           "if nand write ${sfu_load_addr} download ${filesize}; " \
               "then " \
               "echo \"INFO: nand write successful\"; " \
           "else " \
               "echo \"ERROR: nand write failed\"; " \
               "sfu errstate; " \
           "fi; " \
           "echo \"INFO: setting fail flag...\"; " \
           "fwup set fail; " \
           "SFU_IMAGE_LOAD_VALID=yes;" \
           "echo \"INFO: SFU image load and valid\"; " \
        "else echo \"INFO: SFU image invalid\"; " \
        "sfu errstate; " \
        "fi;\0" \
 \
 \
    "check_factory_state=" \
	"echo \"INFO: Checking settings,fit,rootfs,download partitions if are empty.\"; " \
	"setenv target_addr ${sfu_load_addr}; " \
	"setenv factory 1; " \
	"mw ${sfu_load_addr} 0xffffffff; " \
	"setexpr target_addr ${target_addr} + 4; " \
	"for part in settings fit rootfs download; " \
	    "do; " \
	    "nand read ${target_addr} $part 4; " \
	    "cmp.l ${sfu_load_addr} ${target_addr} 1; " \
		"if test $? -eq 1; " \
		    "then; " \
		    "setenv factory 0; " \
		    "echo \"INFO: partition $part is not empty.\"; " \
		"fi; " \
	"done; " \
	"if test ${factory} -eq 0; " \
	    "then " \
	    "echo \"Board is NOT in factory state.\"; " \
	"else " \
	    "echo \"Board is in factory state.\"; " \
	"fi;\0" \
 \
 \
    "update_download_from_usb=" \
        "echo \"INFO: usb thumbdrive sfupdate check...\"; " \
        "if fatload usb ${usbdev} ${sfu_load_addr} sfupdate; " \
            "then " \
            "echo \"INFO: SFU update image found on usb thumbdrive\"; " \
	    "run verify_sfu_image;" \
        "else echo \"INFO: SFU image USB load failed\"; " \
        "fi; " \
        "echo \"INFO: usb thumbdrive sfupdate check complete\";\0" \
 \
 \
    "check_bootloaders_need_flashing=" \
        "echo \"INFO: Start check_bootloaders_need_flashing...\"; " \
        "if sfu chnkhdr ${sfu_load_addr} u-boot; " \
            "then " \
            "echo \"INFO: uboot in download partition SFU update image\"; " \
            "if test -z \\\\'${uboot_vers}\\\\' || test ${uboot_vers} != ${SFU_CHNK_VERS}; " \
                "then " \
                "if test -z \\\\'${uboot_vers}\\\\'; then echo \"INFO: uboot_vers is missing\"; fi; " \
                "if test ${uboot_vers} != ${SFU_CHNK_VERS}; then echo \"INFO: uboot_vers(${uboot_vers}) not equal SFU_CHNK_VERS(${SFU_CHNK_VERS})\"; fi; " \
                "echo \"INFO: u-boot needs flashing...\"; " \
                "run handle_encm; " \
                "if test ${SFU_CHNK_ENCM} = 00000001; " \
                    "then " \
                    "SFU_DECRYPT_UBOOT_CHNK_SIZE=${SFU_CHNK_SIZE}; " \
                "fi; " \
                "UBOOT_NEEDS_FLASHING=yes; " \
            "fi; " \
        "else " \
            "echo \"INFO: u-boot not in download partition SFU update image\"; " \
        "fi; " \
        "if sfu chnkhdr ${sfu_load_addr} u-boot-env; " \
            "then " \
            "echo \"INFO: uboot environment in download partition SFU update image\"; " \
            "if test -z \\\\'${ubootenv_vers}\\\\' || test ${ubootenv_vers} != ${SFU_CHNK_VERS}; " \
                "then " \
                "if test -z \\\\'${ubootenv_vers}\\\\'; then echo \"INFO: ubootenv_vers is missing\"; fi; " \
                "if test ${ubootenv_vers} != ${SFU_CHNK_VERS}; then echo \"INFO: ubootenv_vers(${ubootenv_vers}) not equal SFU_CHNK_VERS(${SFU_CHNK_VERS})\"; fi; " \
                "echo \"INFO: u-boot-env needs flashing...\"; " \
                "run handle_encm; " \
                "if test ${SFU_CHNK_ENCM} = 00000001; " \
                    "then " \
                    "SFU_DECRYPT_UBOOTENV_CHNK_SIZE=${SFU_CHNK_SIZE}; " \
                "fi; " \
                "UBOOTENV_NEEDS_FLASHING=yes; " \
            "fi; " \
        "else " \
            "echo \"INFO: u-boot-env not in download partition SFU update image\"; " \
        "fi; " \
        "echo \"INFO: Completed check_bootloaders_need_flashing\";\0" \
 \
 \
    "flash_bootloaders_as_needed=" \
        "echo \"INFO: Starting flash_bootloaders_as_needed..\"; " \
        "TMP_UBOOT_VERS=${uboot_vers}; " \
        "TMP_UBOOTENV_VERS=${ubootenv_vers}; " \
        "TMP_ROOTFS_VERS=${rootfs_vers}; " \
        "TMP_FIT_VERS=${fit_vers}; " \
        "if test ${UBOOT_NEEDS_FLASHING} = yes; " \
            "then " \
            "if sfu chnkhdr ${sfu_load_addr} u-boot; " \
                "then " \
                "echo \"INFO: uboot in download partition SFU update image\"; " \
                "bstate dontunplug; " \
                "echo \"INFO: u-boot partition being erased\"; " \
                "nand erase.part ${SFU_CHNK_DEST}; " \
                "if test ${SFU_CHNK_ENCM} = 00000001; " \
                    "then " \
                    "SFU_CHNK_SIZE=${SFU_DECRYPT_UBOOT_CHNK_SIZE}; " \
                "fi; " \
                "echo \"INFO: writing uboot to flash...\"; " \
		"if kobs init ${SFU_CHNK_DATA} ${SFU_CHNK_SIZE}; " \
                    "then " \
                    "echo \"INFO: kobs init successful\"; " \
                "else " \
                    "echo \"ERROR: kobs init failed\"; " \
                    "sfu errstate; " \
                "fi; " \
                "TMP_UBOOT_VERS=${SFU_CHNK_VERS}; " \
                "UBOOT_RESET=yes; " \
            "else " \
                "echo \"ERROR: u-boot not in download partition SFU update image\"; " \
                "sfu errstate; " \
            "fi; " \
        "fi; " \
        "if test ${UBOOTENV_NEEDS_FLASHING} = yes; " \
            "then " \
            "if sfu chnkhdr ${sfu_load_addr} u-boot-env; " \
                "then " \
                "echo \"INFO: uboot environment in download partition SFU update image\"; " \
                "bstate dontunplug; " \
                "echo \"INFO: u-boot-env partition being erased\"; " \
                "nand erase.part ${SFU_CHNK_DEST}; " \
                "if test ${SFU_CHNK_ENCM} = 00000001; " \
                    "then " \
                    "SFU_CHNK_SIZE=${SFU_DECRYPT_UBOOTENV_CHNK_SIZE}; " \
                "fi; " \
                "echo \"INFO: writing uboot-env to flash...\"; " \
                "if nand write ${SFU_CHNK_DATA} ${SFU_CHNK_DEST} ${SFU_CHNK_SIZE}; " \
                    "then " \
                    "echo \"INFO: nand write successful\"; " \
                "else " \
                    "echo \"ERROR: nand write failed\"; " \
                    "sfu errstate; " \
                "fi; " \
                "echo \"INFO: reloading uboot-env from flash to internal memory so new environment is used...\"; " \
                "env reload;" \
                "TMP_UBOOTENV_VERS=${SFU_CHNK_VERS};" \
                "UBOOT_RESET=yes; " \
            "else " \
                "echo \"ERROR: u-boot-env not in download partition SFU update image\"; " \
                "sfu errstate; " \
            "fi; " \
        "fi; " \
        "if test ${UBOOT_NEEDS_FLASHING} = yes; " \
            "then " \
            "echo \"INFO: setting uboot_vers in environment...\"; " \
            "setenv uboot_vers ${TMP_UBOOT_VERS};" \
            "saveenv;" \
        "fi; " \
        "if test ${UBOOTENV_NEEDS_FLASHING} = yes; " \
            "then " \
            "echo \"INFO: setting uboot_vers in environment...\"; " \
            "setenv uboot_vers ${TMP_UBOOT_VERS};" \
            "echo \"INFO: setting ubootenv_vers in environment...\"; " \
            "setenv ubootenv_vers ${TMP_UBOOTENV_VERS};" \
            "echo \"INFO: restoring rootfs_vers in environment...\"; " \
            "setenv rootfs_vers ${TMP_ROOTFS_VERS};" \
            "echo \"INFO: restoring fit_vers in environment...\"; " \
            "setenv fit_vers ${TMP_FIT_VERS};" \
            "saveenv;" \
        "fi; " \
        "echo \"INFO: Completed flash_bootloaders_as_needed.\";\0" \
 \
 \
    "check_system_need_flashing=" \
        "echo \"INFO: Start check_system_need_flashing...\"; " \
        "if sfu chnkhdr ${sfu_load_addr} rootfs; " \
            "then " \
            "echo \"INFO: rootfs in download partition SFU update image\"; " \
            "if fwup fail; then echo \"INFO: Fail flag is set\"; fi; " \
            "if test -z \\\\'${rootfs_vers}\\\\'; then echo \"INFO: rootfs_vers is missing\"; fi; " \
            "if test ${rootfs_vers} != ${SFU_CHNK_VERS}; then echo \"INFO: rootfs_vers(${rootfs_vers}) not equal SFU_CHNK_VERS(${SFU_CHNK_VERS})\";  fi; " \
            "if fwup fail ||  test -z \\\\'${rootfs_vers}\\\\' || test ${rootfs_vers} != ${SFU_CHNK_VERS}; " \
                "then " \
                "echo \"INFO: rootfs needs flashing...\"; " \
                "run handle_encm; " \
                "if test ${SFU_CHNK_ENCM} = 00000001; " \
                    "then " \
                    "SFU_DECRYPT_ROOTFS_CHNK_SIZE=${SFU_CHNK_SIZE}; " \
                "fi; " \
                "ROOTFS_NEEDS_FLASHING=yes; " \
            "fi; " \
        "else " \
            "echo \"INFO: rootfs not in download partition SFU update image\"; " \
        "fi; " \
        "if sfu chnkhdr ${sfu_load_addr} fit; " \
            "then " \
            "echo \"INFO: fit in download partition SFU update image\"; " \
            "if fwup fail; then echo \"INFO: Fail flag is set\"; fi; " \
            "if test -z \\\\'${fit_vers}\\\\'; then echo \"INFO: fit_vers is missing\"; fi; " \
            "if test ${fit_vers} != ${SFU_CHNK_VERS}; then echo \"INFO: fit_vers(${fit_vers}) not equal SFU_CHNK_VERS(${SFU_CHNK_VERS})\";  fi; " \
            "if fwup fail ||  test -z \\\\'${fit_vers}\\\\' || test ${fit_vers} != ${SFU_CHNK_VERS}; " \
                "then " \
                "echo \"INFO: fit needs flashing...\"; " \
                "run handle_encm; " \
                "if test ${SFU_CHNK_ENCM} = 00000001; " \
                    "then " \
                    "SFU_DECRYPT_FIT_CHNK_SIZE=${SFU_CHNK_SIZE}; " \
                "fi; " \
                "FIT_NEEDS_FLASHING=yes; " \
            "fi; " \
        "else " \
            "echo \"INFO: fit not in download partition SFU update image\"; " \
        "fi; " \
        "echo \"INFO: Completed check_system_need_flashing\";\0" \
 \
 \
    "flash_system_as_needed=" \
        "echo \"INFO: Starting flash_system_as_needed...\"; " \
        "if test ${ROOTFS_NEEDS_FLASHING} = yes || test ${FIT_NEEDS_FLASHING} = yes; " \
            "then " \
            "echo \"INFO: fit partition being erased\"; " \
            "nand erase.part fit; " \
            "FIT_NEEDS_FLASHING=yes; " \
        "fi; " \
        "if test ${ROOTFS_NEEDS_FLASHING} = yes; " \
            "then " \
            "if sfu chnkhdr ${sfu_load_addr} rootfs; " \
                "then " \
                "echo \"INFO: rootfs in download partition SFU update image\"; " \
                "bstate dontunplug; " \
                "echo \"INFO: erasing rootfs partition\"; " \
                "nand erase.part rootfs; " \
                "if ubi part rootfs 4096; " \
                    "then " \
                    "echo \"INFO: rootfs is a valid ubi partition\"; " \
                "fi; " \
                "if test ${SFU_CHNK_ENCM} = 00000001; " \
                    "then " \
                    "SFU_CHNK_SIZE=${SFU_DECRYPT_ROOTFS_CHNK_SIZE}; " \
                "fi; " \
                "echo \"INFO: need to create ubi stream800-rootfs volume first...\"; " \
                "if ubi create stream800-rootfs; " \
                    "then " \
                    "echo \"INFO: successfully created volume, stream800-rootfs...\"; " \
                    "echo \"INFO: writing ubi rootfs to flash...\"; " \
                    "if ubi write ${SFU_CHNK_DATA} stream800-rootfs ${SFU_CHNK_SIZE}; " \
                        "then " \
                        "echo \"INFO: successfully wrote rootfs to flash after volume, stream800-rootfs, was created...\"; " \
                    "else " \
                        "echo \"ERROR: failed to write rootfs to flash after volume, stream800-rootfs, was created\"; " \
                        "sfu errstate; " \
                    "fi; " \
                "else " \
                    "echo \"ERROR: failed to create volume, stream800-rootfs\"; " \
                    "sfu errstate; " \
                "fi; " \
                "setenv rootfs_vers ${SFU_CHNK_VERS};" \
                "saveenv;" \
            "else " \
                "echo \"ERROR: rootfs not in download partition SFU update image\"; " \
                "sfu errstate; " \
            "fi; " \
        "fi; " \
        "if test ${FIT_NEEDS_FLASHING} = yes; " \
            "then " \
            "if sfu chnkhdr ${sfu_load_addr} fit; " \
                "then " \
                "echo \"INFO: fit in download partition SFU update image\"; " \
                "bstate dontunplug; " \
                "if test ${SFU_CHNK_ENCM} = 00000001; " \
                    "then " \
                    "SFU_CHNK_SIZE=${SFU_DECRYPT_FIT_CHNK_SIZE}; " \
                "fi; " \
                "echo \"INFO: writing fit to flash...\"; " \
                "if nand write ${SFU_CHNK_DATA} ${SFU_CHNK_DEST} ${SFU_CHNK_SIZE}; " \
                    "then " \
                    "echo \"INFO: nand write successful\"; " \
                "else " \
                    "echo \"ERROR: nand write failed\"; " \
                    "sfu errstate; " \
                "fi; " \
                "setenv fit_vers ${SFU_CHNK_VERS};" \
                "saveenv;" \
            "else " \
                "echo \"ERROR: fit not in download partition SFU update image\"; " \
                "sfu errstate; " \
            "fi; " \
        "fi; " \
        "if test ${ROOTFS_NEEDS_FLASHING} = yes || test ${FIT_NEEDS_FLASHING} = yes; " \
            "then " \
            "echo \"INFO: saving environment...\"; " \
            "saveenv;" \
        "fi; " \
        "echo \"INFO: Completed flash_system_as_needed.\";\0" \
 \
 \
    "sfu_boot=" \
        "echo \"INFO: SFU firmware update process started...\"; " \
        "SFU_IMAGE_LOAD_VALID=no; " \
        "if fwup fail; " \
            "then " \
            "if test ${bootcount} -eq ${bootlimit}; " \
                "then " \
                "echo \"INFO: bootcount(${bootcount}) equals bootlimit(${bootlimit})\"; " \
                "echo \"INFO: erasing settings partition\"; " \
                "run eraseSettings; " \
                "if ubi part settings 4096; " \
                    "then " \
                    "echo \"INFO: settings is a valid ubi partition\"; " \
                    "echo \"INFO: creating volume settings...\"; " \
                    "if ubi create stream800-settings; " \
                        "then " \
                        "echo \"INFO: successfully created volume, settings...\"; " \
                    "else " \
                        "echo \"ERROR: failed to create volume, settings\"; " \
                        "sfu errstate; " \
                    "fi; " \
                "fi; " \
            "fi; " \
            "if test ${bootcount} -gt ${bootlimit}; " \
                "then " \
                "echo \"INFO: bootcount(${bootcount}) greater than bootlimit(${bootlimit})\"; " \
                "bstate hardfailure; " \
                "echo \"ERROR: Maximum boot count reached!\"; " \
                "sfu errstate; " \
            "fi; " \
        "fi; " \
        "if test ${bootcount} -eq 1; " \
            "then " \
	    "run check_factory_state; " \
            "if fwup usb_update_req || test ${factory} -eq 1; " \
                "then " \
                "echo \"INFO: USB update request active, checking USB for update file ...\"; " \
                "run usbmount; " \
                "usbdev=-1; " \
                "if usb storage; " \
                    "then " \
                    "if fatfind usb 0 / sfupdate; " \
                        "then " \
                        "echo \"INFO: sfupdate image found on USB-0 thumbdrive\"; " \
                        "bstate dontunplug; " \
                        "usbdev=0; " \
                        "run update_download_from_usb; " \
                    "else " \
			 "if fatfind usb 1 / sfupdate; "\
			    "then " \
			    "echo \"INFO: sfupdate image found on USB-1 thumbdrive\"; " \
			    "bstate dontunplug; " \
			    "usbdev=1; " \
			    "run update_download_from_usb; " \
			"else " \
                            "echo \"ERROR: sfupdate image not found on any USB thumbdrive\"; " \
                            "sfu errstate; " \
			"fi;" \
		    "fi; " \
		    "else "\
		    "setenv autoload no; " \
		    "setenv autostart no; " \
		    "echo \"INFO: starting tftp update process.\"; " \
		    "if stftpup data; " \
			"then "\
			"if test -z \\\\'${serverip}\\\\'; then " \
			    "stftpup use_local_ip; " \
			    "echo \"INFO: Serverip unknown. Using local ip with last digit .10\"; " \
			"fi; " \
		    "else " \
			"echo \"DHCP failed, use static settings\"; " \
			"stftpup use_static_data; " \
		    "fi; " \
			\
		    "if test -z \\\\'${bootfile}\\\\' || test ${bootfile} = uImage; then " \
			"setenv bootfile sfupdate; " \
		    "fi; " \
			\
		    "if tftp ${sfu_load_addr} ${rootpath}${bootfile}; " \
			"then " \
			"run verify_sfu_image; " \
		    "else " \
			"echo \"INFO: Cannot fetch update file\"; " \
		    "fi; " \
                "fi; " \
            "else " \
                "echo \"INFO: USB update request is not active, not checking USB for FW update file\" ;" \
            "fi; " \
        "else " \
	        "echo \"INFO: Bootcount != 1, not checking USB for FW update file\" ;" \
        "fi; " \
        "if fwup fail || fwup update || test -z \\\\'${uboot_vers}\\\\' || test -z \\\\'${ubootenv_vers}\\\\' || test -z \\\\'${rootfs_vers}\\\\' || test -z \\\\'${fit_vers}\\\\'; " \
            "then " \
            "if fwup fail; then echo \"INFO: Fail flag is set\"; fi; " \
            "if fwup update; then echo \"INFO: Update flag is set\"; fi; " \
            "if test -z \\\\'${uboot_vers}\\\\'; then echo \"INFO: uboot_vers is missing\"; fi; " \
            "if test -z \\\\'${ubootenv_vers}\\\\'; then echo \"INFO: ubootenv_vers is missing\"; fi; " \
            "if test -z \\\\'${fit_vers}\\\\'; then echo \"INFO: fit_vers is missing\"; fi; " \
            "if test -z \\\\'${rootfs_vers}\\\\'; then echo \"INFO: rootfs_vers is missing\"; fi; " \
            "bstate dontunplug; " \
            "VALID_IMAGE=no; " \
            "if test ${SFU_IMAGE_LOAD_VALID} = yes; " \
                "then " \
                "VALID_IMAGE=yes; " \
            "else " \
                "echo \"INFO: Need to read the sfupdate from download partition...\"; " \
                "if nand read ${sfu_load_addr} download 0x64; " \
                    "then " \
                    "if sfu chk_fit_hdr ${sfu_load_addr}; " \
                        "then " \
                        "echo \"INFO: sfupdate image total length is ${SFU_TOTAL_LEN}\"; " \
                        "if nand read ${sfu_load_addr} download ${SFU_TOTAL_LEN}; " \
                            "then " \
                            "if sfu valid ${sfu_load_addr}; " \
                                "then " \
                                "echo \"INFO: SFU image in download partition is valid\"; " \
                                "VALID_IMAGE=yes; " \
                            "else " \
                                "echo \"ERROR: SFU image in download partition is invalid\"; " \
                            "fi; " \
                        "else " \
                            "echo \"ERROR: nand read failed\"; " \
                        "fi; " \
                    "else " \
                        "echo \"ERROR: SFU image in download partition is invalid\"; " \
                    "fi; " \
                "else " \
                    "echo \"ERROR: nand read failed\"; " \
                "fi; " \
            "fi; " \
            "if test ${VALID_IMAGE} = yes; " \
                "then " \
                "UBOOT_NEEDS_FLASHING=no; " \
                "UBOOTENV_NEEDS_FLASHING=no; " \
                "ROOTFS_NEEDS_FLASHING=no; " \
                "FIT_NEEDS_FLASHING=no; " \
                "UBOOT_RESET=no; " \
                "run check_bootloaders_need_flashing; " \
                "run flash_bootloaders_as_needed; " \
                "if test ${UBOOT_RESET} = yes; " \
                "then " \
                    "echo \"INFO: Resetting ...\"; " \
                    "reset; " \
                "fi;" \
                "run check_system_need_flashing; " \
                "run flash_system_as_needed; " \
             "else " \
                "if fwup fail || test -z \\\\'${uboot_vers}\\\\' || test -z \\\\'${ubootenv_vers}\\\\' || test -z \\\\'${rootfs_vers}\\\\' || test -z \\\\'${fit_vers}\\\\'; " \
                "then " \
                    "sfu errstate; " \
                "fi; " \
             "fi; " \
        "fi; " \
        "echo \"INFO: SFU firmware update process completed\"; " \
        "echo \"INFO: clearing update flag...\"; " \
        "fwup clear update; " \
        "echo \"INFO: setting fail flag...\"; " \
        "fwup set fail; " \
        "bstate normal; " \
        "run nand_boot;\0" \

#define SUE_FWUPDATE_BOOTCOMMAND \
	"echo \"INFO: attempting SFU boot...\"; " \
	"echo \"INFO: U-boot version: ${uboot_vers}\"; " \
	"echo \"INFO: FIT version: ${fit_vers}\"; " \
	"echo \"INFO: Rootfs version: ${rootfs_vers}\"; " \
	"fwup flags; "\
	"run sfu_boot;" \

#define SUE_FWUPDATE_ALTBOOTCOMMAND \
	"echo \"ERROR: Maximum boot count reached!\"; sfu errstate;"

#endif /* __SUE_FWUPDATE_COMMON_H */
