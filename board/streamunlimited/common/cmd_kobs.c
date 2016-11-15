#include <common.h>
#include <nand.h>
#include <linux/mtd/mtd.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/imx-common/regs-bch.h>

#include "BootControlBlocks.h"
#include "fsl_bch.h"

#define KOBS_DEFAULT_NAND_DEV		0
#define KOBS_DEFAULT_BOOT_PART_SIZE	(8 * SZ_1M)
#define KOBS_DEFAULT_IMAGE_PADDING	0x400

#define KOBS_DEFAULT_SEARCH_EXPONENT	2
#define KOBS_DEFAULT_DATA_SETUP_TIME	80
#define KOBS_DEFAULT_DATA_HOLD_TIME	60
#define KOBS_DEFAULT_ADDRESS_SETUP_TIME	25
#define KOBS_DEFAULT_DATA_SAMPLE_TIME	6

static int nand_erase_write(nand_info_t *nand, const char *data, u32 offset, u32 length, loff_t maxsize)
{
	int ret;
	u32 aligned_len, page_len;
	size_t write_len;
	void *buffer;
	nand_erase_options_t opts;

	if ((offset % nand->writesize)) {
		printf("%s: offset is not page aligned\n", __func__);
		return 1;
	}

	page_len = length / nand->writesize;
	if ((page_len * nand->writesize) < length)
		page_len += 1;

	aligned_len = page_len * nand->writesize;

	memset(&opts, 0, sizeof(opts));
	opts.offset = offset;
	opts.length = aligned_len;

	/* This is a hack
	 * eraseonly will be only set when writing the FCB, and there we want to
	 * ignore bad blocks and all markers and everything.
	 */

	ret = nand_erase_opts(nand, &opts);
	if (ret) {
		printf("%s: nand_erase_opts() call failed (%d)\n", __func__, ret);
		return ret;
	}

	buffer = malloc(aligned_len);
	if (buffer == 0)
		return -ENOMEM;

	memset(buffer, 0, aligned_len);
	memcpy(buffer, data, length);

	write_len = aligned_len;
	ret = nand_write_skip_bad(nand, offset, &write_len, NULL, maxsize, buffer, WITH_WR_VERIFY);
	if (ret) {
		printf("%s: nand_write_skip_bad() call failed (%d)\n", __func__, ret);
		free(buffer);
		return ret;
	}

	free(buffer);
	return ret;
}

/**
 * fcb_encrypt - Encrypt the FCB block, assuming that target system uses NCB
 * version 'version'
 *
 * fcb:     Points to valid imx28_BootBlockStruct_t structure.
 * target:  Points to a buffer large enough to contain an entire NAND Flash page
 *          (both data and OOB).
 * size:    The size of an entire NAND Flash page (both data and OOB).
 * version: The version number of the NCB.
 *
 */
int fcb_encrypt(BCB_ROM_BootBlockStruct_t *fcb, void *target, size_t size, int version)
{
	uint32_t  accumulator;
	uint8_t   *p;
	uint8_t   *q;

	//----------------------------------------------------------------------
	// Check for nonsense.
	//----------------------------------------------------------------------

	assert(size >= sizeof(BCB_ROM_BootBlockStruct_t));

	//----------------------------------------------------------------------
	// Clear out the target.
	//----------------------------------------------------------------------

	memset(target, 0, size);

	//----------------------------------------------------------------------
	// Compute the checksum.
	//
	// Note that we're computing the checksum only over the FCB itself,
	// whereas it's actually supposed to reflect the entire 508 bytes
	// in the FCB page between the base of the of FCB and the base of the
	// ECC bytes. However, the entire space between the top of the FCB and
	// the base of the ECC bytes will be all zeros, so this is OK.
	//----------------------------------------------------------------------

	p = ((uint8_t *) fcb) + 4;
	q = (uint8_t *) (fcb + 1);

	accumulator = 0;

	for (; p < q; p++) {
		accumulator += *p;
	}

	accumulator ^= 0xffffffff;

	fcb->m_u32Checksum = accumulator;

	//----------------------------------------------------------------------
	// Compute the ECC bytes.
	//----------------------------------------------------------------------

	switch (version)
	{
	case 0:
		memcpy(target, fcb, sizeof(*fcb));
		return size;
	case 2:
		return encode_bch_ecc(fcb, sizeof(*fcb), target, size, version);
	case 3:
		return encode_bch_ecc(fcb, sizeof(*fcb), target, size, version);
	default:
		printf("FCB version == %d? Something is wrong!\n", version);
		return -EINVAL;
	}
}

/*
 * Usage:
 * 	kobs init <addr> <size>
 * Example call:
 *	kobs init $loadaddr $filesize
 */
static int do_kobs(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *cmd;
	u32 addr, size;

	if (argc < 2)
		goto usage;

	cmd = argv[1];

	if (!strcmp(cmd, "init")) {
		int ndev;
		u32 nblocksize, npagesize, neccsize;
		u64 nsize, nsize_blocks;

		u32 i;
		u32 search_exponent, cfg_copies, dbbt_start;
		u32 max_img_size, img_size_pages, img_size_blocks;
		u32 img1_start_block, img2_start_block, img1_start, img2_start;
		BCB_ROM_BootBlockStruct_t fcb, dbbt;

		void *buffer;
		u32 padded_img_size;

		if (argc < 4)
			goto usage;

		addr = simple_strtoul(argv[2], NULL, 16);
		size = simple_strtoul(argv[3], NULL, 16);

		ndev = KOBS_DEFAULT_NAND_DEV;
		nblocksize = nand_info[ndev].erasesize;
		npagesize = nand_info[ndev].writesize;
		neccsize = nand_info[ndev].oobsize;
		nsize = KOBS_DEFAULT_BOOT_PART_SIZE;
		nsize_blocks = lldiv(nsize, nblocksize);


		search_exponent = KOBS_DEFAULT_SEARCH_EXPONENT;
		cfg_copies = (1 << search_exponent);

		dbbt_start = nblocksize * cfg_copies;

		img_size_pages = size / npagesize;
		if ((img_size_pages * npagesize) < size)
			img_size_pages += 1;

		img_size_blocks = size / nblocksize;
		if ((img_size_blocks * nblocksize) < size)
			img_size_blocks += 1;

		/* First image copy starts right after the end of the DBBT */
		img1_start_block = 2 * cfg_copies;
		/* Second image copy starts in the second half of the free space after the DBBT */
		img2_start_block = img1_start_block + ((nsize_blocks - img1_start_block) / 2);

		img1_start = img1_start_block * nblocksize;
		img2_start = img2_start_block * nblocksize;

		max_img_size = img2_start - img1_start;

		printf("kobs: Using NAND device %d\n", ndev);
		printf("kobs: NAND size is %llu B\n", nsize);
		printf("kobs: NAND blocksize is %u B\n", nblocksize);
		printf("kobs: NAND pages are %u B + %u B ecc\n", npagesize, neccsize);
		printf("kobs: DBBT search area will start at 0x%08X (page %u)\n", dbbt_start, dbbt_start / npagesize);
		printf("kobs: first image will be at 0x%08X (page %u) size %u pages\n", img1_start, img1_start / npagesize, img_size_pages);
		printf("kobs: second image will be at 0x%08X (page %u) size %u pages\n", img2_start, img2_start / npagesize, img_size_pages);
		printf("kobs: maximum image size is %u B\n", max_img_size);

		if (npagesize != 2048 && npagesize != 4096) {
			printf("kobs: ERROR: NAND page size is not supported\n");
			return -EINVAL;
		}

		if (size > max_img_size) {
			printf("kobs: ERROR: image is too big (%u B)\n", size);
			return -ENOSPC;
		}

		memset(&fcb, 0, sizeof(fcb));

		/* Fill the struct */
		fcb.m_u32FingerPrint				= FCB_FINGERPRINT;
		fcb.m_u32Version				= FCB_VERSION_1;

		fcb.FCB_Block.m_NANDTiming.m_u8DataSetup	= KOBS_DEFAULT_DATA_SETUP_TIME;
		fcb.FCB_Block.m_NANDTiming.m_u8DataHold		= KOBS_DEFAULT_DATA_HOLD_TIME;
		fcb.FCB_Block.m_NANDTiming.m_u8AddressSetup	= KOBS_DEFAULT_ADDRESS_SETUP_TIME;
		fcb.FCB_Block.m_NANDTiming.m_u8DSAMPLE_TIME	= KOBS_DEFAULT_DATA_SAMPLE_TIME;


		fcb.FCB_Block.m_u32PageDataSize			= npagesize;
		fcb.FCB_Block.m_u32TotalPageSize		= npagesize + neccsize;
		fcb.FCB_Block.m_u32SectorsPerBlock		= nblocksize / npagesize;

		/*
		 * TODO: Check with userpace program how to fill the following values properly for
		 * different NAND geometries.
		 */
		fcb.FCB_Block.m_u32EccBlockNEccType		= 4;
		fcb.FCB_Block.m_u32EccBlock0Size		= 512;
		fcb.FCB_Block.m_u32EccBlockNSize		= 512;
		fcb.FCB_Block.m_u32EccBlock0EccType		= 4;
		fcb.FCB_Block.m_u32MetadataBytes		= 10;

		/* Yes, this is ugly. */
		if (npagesize == 4096) {
			fcb.FCB_Block.m_u32NumEccBlocksPerPage		= 7;
			fcb.FCB_Block.m_u32BadBlockMarkerByte		= 3995;
		} else if (npagesize == 2048) {
			fcb.FCB_Block.m_u32NumEccBlocksPerPage		= 3;
			fcb.FCB_Block.m_u32BadBlockMarkerByte		= 1999;
		}

		fcb.FCB_Block.m_u32BadBlockMarkerStartBit	= 0;
		fcb.FCB_Block.m_u32BBMarkerPhysicalOffset	= npagesize;
		fcb.FCB_Block.m_u32BCHType			= 0;

		fcb.FCB_Block.m_u32Firmware1_startingPage	= img1_start / npagesize;
		fcb.FCB_Block.m_u32Firmware2_startingPage	= img2_start / npagesize;
		fcb.FCB_Block.m_u32PagesInFirmware1		= img_size_pages;
		fcb.FCB_Block.m_u32PagesInFirmware2		= img_size_pages;

#if 0
		fcb.FCB_Block.m_u32DBBTSearchAreaStartAddress	= dbbt_start / npagesize;
#endif
		/* Disable the DBBT for now, the ROM bootloader will use the marker byte */
		fcb.FCB_Block.m_u32DBBTSearchAreaStartAddress	= 0;

		memset(&dbbt, 0, sizeof(dbbt));
		dbbt.m_u32FingerPrint				= DBBT_FINGERPRINT2;
		dbbt.m_u32Version				= DBBT_VERSION_1;

		/* TODO: Properly fill the table */
		dbbt.DBBT_Block.v3.m_u32DBBTNumOfPages		= 0;

		/* Write FCB */
		printf("kobs: Writing FCB\n");
		buffer = malloc(npagesize + neccsize);
		memset(buffer, 0, npagesize + neccsize);

		/*
		 * Because why not kobs-ng, why not, it's actually not encrypting but adding a BCH to
		 * the raw NAND buffer
		 */
		fcb_encrypt(&fcb, buffer, npagesize + neccsize, 2);

		for (i = 0; i < cfg_copies; i++) {
			struct mxs_bch_regs *bch_regs = (struct mxs_bch_regs *)MXS_BCH_BASE;
			u32 l;
			u32 old_flash_layout0, new_flash_layout0;
			u32 old_flash_layout1, new_flash_layout1;
			size_t write_len;

			nand_erase_options_t opts;
			mtd_oob_ops_t ops;

			/* Erase the page */
			memset(&opts, 0, sizeof(opts));
			opts.offset = i * nblocksize;
			opts.length = npagesize;
			opts.scrub = 1;

			nand_erase_opts(&nand_info[ndev], &opts);

			/* This works, but do not forget about setting the BBM at 4096 */
			memset(&ops, 0, sizeof(ops));
			ops.datbuf = (u8 *)buffer;
			ops.oobbuf = ((u8 *)buffer) + npagesize;
			ops.len = npagesize;
			ops.ooblen = neccsize;
			ops.mode = MTD_OPS_RAW;
			ops.flags = MTD_OOB_FLAG_RANDOMIZE;

			/*
			 * This does not work, because we randomize the whole data, and when we access
			 * it without the randomizer enabled we will not get 0xFF, as a workaround
			 * we could read back the page randomized change the BBM and the write the
			 * page back without the randomizer.
			 */
			*((u8 *)buffer + npagesize) = 0xFF;


			/* Since we will write using the randomizer, ecc has to be enabled,
			 * but we set the ECC layouts, etc. to 0.
			 */
			old_flash_layout0 = readl(&bch_regs->hw_bch_flash0layout0);
			old_flash_layout1 = readl(&bch_regs->hw_bch_flash0layout1);

			new_flash_layout0 = old_flash_layout0 & ~(BCH_FLASHLAYOUT0_ECC0_MASK | BCH_FLASHLAYOUT0_META_SIZE_MASK);
			new_flash_layout1 = old_flash_layout1 & ~(BCH_FLASHLAYOUT1_ECCN_MASK);

			writel(new_flash_layout0, &bch_regs->hw_bch_flash0layout0);
			writel(new_flash_layout1, &bch_regs->hw_bch_flash0layout1);


			mtd_write_oob(&nand_info[ndev], i * nblocksize, &ops);


			/* Reset the old flash layout */
			writel(old_flash_layout0, &bch_regs->hw_bch_flash0layout0);
			writel(old_flash_layout1, &bch_regs->hw_bch_flash0layout1);
		}

		free(buffer);

		/* Write DBBT ... or not, we just keep it empty or do not touch the existing one ... */
		/*
		printf("kobs: Writing DBBT\n");
		for (i = 0; i < cfg_copies; i++) {
			nand_erase_write(&nand_info[ndev], &dbbt, dbbt_start + i * nblocksize, sizeof(dbbt), nblocksize);
		}
		*/

		/* Write image */
		printf("kobs: Writing image\n");
		padded_img_size = KOBS_DEFAULT_IMAGE_PADDING + size;

		buffer = malloc(padded_img_size);
		if (buffer == NULL) {
			printf("kobs: malloc() failed\n");
			return -ENOMEM;
		}

		memset(buffer, 0, padded_img_size);
		memcpy(buffer + KOBS_DEFAULT_IMAGE_PADDING, (void *)addr, size);

		nand_erase_write(&nand_info[ndev], buffer, img1_start, padded_img_size, max_img_size);
		nand_erase_write(&nand_info[ndev], buffer, img2_start, padded_img_size, max_img_size);

		free(buffer);

		return 0;
	} else {
		goto usage;
	}

usage:
	return cmd_usage(cmdtp);
}

U_BOOT_CMD(kobs, CONFIG_SYS_MAXARGS, 0, do_kobs, "short description", "usage");
