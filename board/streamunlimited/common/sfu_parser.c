/*
 * (C) Copyright 2011 Attero Tech LLC
 *
 * Written by: Dan Brunswick <dan.brunswick@atterotech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#include "sfu_parser.h"
#include <common.h>
#if 0
#include <const_env_common.h>
#endif

#ifdef CONFIG_SFU_FILE
#include <stdio.h>
#include <string.h>
#endif // CONFIG_SFU_FILE

#ifdef CONFIG_SFU_RAM
#include <linux/string.h>
#endif // CONFIG_SFU_RAM

#ifdef CONFIG_BLOWFISH
#include "blowfish.h"
#endif // CONFIG_BLOWFISH

#include <cli_hush.h>

#include <u-boot/md5.h>
#include <u-boot/sha256.h>

extern uint32_t crc32(uint32_t crc, const uint8_t* baseaddr, uint32_t length);
extern void md5(unsigned char *input, int len, unsigned char output[16]);


static int32_t ImageValid(uint32_t startAddr, uint32_t maxSize);
static int32_t GetGlobalVersion(uint32_t startAddr,
		sfu_cpuver_t* pGlobVer);
static int32_t ExtractChunkSfuHdr(sfu_option_t* pOption,
		sfu_cpuhdr_t* pHdr);


#define MAX_HUSH_SET_STRING_LEN (512)
int set_hush_var_with_str_value(const char *var_name,
		char       *var_value)
{
	static char var_and_value_string[MAX_HUSH_SET_STRING_LEN];
	int status;

	memset(var_and_value_string, 0x0, MAX_HUSH_SET_STRING_LEN);
	strncpy(var_and_value_string, var_name, (MAX_HUSH_SET_STRING_LEN-1));
	strncat(var_and_value_string, "=", (MAX_HUSH_SET_STRING_LEN-1));
	strncat(var_and_value_string, var_value, (MAX_HUSH_SET_STRING_LEN-1));

	status = set_local_var(var_and_value_string, 0);

	return ((status < 0) ? 1 : 0);
}

//----------------------------------------------------------------------------
//  Function: DownloadMtdPartSize
//
//  Description:
//      	This function gets the size of download partition
//
//  Returns:
//	        int32_t status    where !0 indicates size
//                                  0 indicates unknown
//----------------------------------------------------------------------------
static uint32_t DownloadMtdPartSize(void)
{
	unsigned long mtdDownloadSize = 0;
	char* buf = getenv("mtd_download_size");
	if (buf != NULL)
	{
		if (strict_strtoul(buf, 10, &mtdDownloadSize) != 0)
		{
			mtdDownloadSize = 0;
		}
	}
	return mtdDownloadSize * 1024;
}

#ifdef CONFIG_SFU_FILE
#include <sys/types.h>
#include <unistd.h>

static int fileRead(int fd, uint8_t* destAddr, size_t dataLen);

//----------------------------------------------------------------------------
//  Function: sfu_ImageValid
//
//  Description:
//      	This function determines if the SFU image is valid.
//
//  Returns:
//	        int32_t imageValid   where 0 indicates valid and
//                                     <0 indicates invalid
//----------------------------------------------------------------------------
int32_t sfu_ImageValid(
		int fd // Image file descriptor
		)
{
	uint8_t                  header[GLOBAL_HDR_LEN];
	sfu_global_hdr_w_data_t* pGlobHdr = (sfu_global_hdr_w_data_t*)header;
	uint32_t                 totalLen;
	uint32_t                 calcCrc32;
	int32_t                  imageValid;
	uint8_t                  byte;
	uint32_t                 maxSfuSize

	if (0 > fd)
	{
		SFU_ERR("ERROR: File descriptor is %d\n", fd);
		return IMG_VALIDITY__FILE_ERR;
	}

	// Seek to the beginning of the file
	if ((off_t)-1 == lseek(fd, 0, SEEK_SET))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR ");
		SFU_ERR("errno(0x%08x) (%s)\n", errno, strerror(errno));
		return IMG_VALIDITY__FILE_ERR;
	}

	// Retrieve the global header
	if (0 > fileRead(fd, header, sizeof(header)))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
		return IMG_VALIDITY__FILE_ERR;
	}

	maxSfuSize = DownloadMtdPartSize();
	imageValid = ImageValid((uint32_t)header, maxSfuSize);
	if (imageValid)
	{
		return imageValid;
	}

	// Verify that the SFU image's CRC32 matches the calculated CRC32
	totalLen = SFU_32BIT_VALUE(pGlobHdr->total_length);
	calcCrc32 = 0;
	totalLen -= GLOBAL_CRC_CALC_OFFSET;
	while (totalLen--)
	{
		// Retrieve byte from file for CRC32 calculation
		if (0 > fileRead(fd, &byte, sizeof(byte)))
		{
			SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
			return IMG_VALIDITY__FILE_ERR;
		}
		calcCrc32 = crc32(calcCrc32,
				&byte,
				sizeof(byte));
	}
	if (calcCrc32 != SFU_32BIT_VALUE(pGlobHdr->global_crc32))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__CRC32_ERR\n");
		SFU_ERR("\tCalcCRC(%08x) FileCRC(%08x)\n",
				calcCrc32, SFU_32BIT_VALUE(pGlobHdr->global_crc32));
		return IMG_VALIDITY__CRC32_ERR;
	}

	return IMG_VALIDITY__VALID;
} // end sfu_ImageValid


//----------------------------------------------------------------------------
//  Function: sfu_GetGlobalVersion
//
//  Description:
//      	This function gets the global version of the SFU file.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
int32_t sfu_GetGlobalVersion(
		int           fd,        // Image file descriptor
		sfu_cpuver_t* pGlobalVer // Address to store version
		)
{
	uint8_t                  header[GLOBAL_HDR_LEN];
	sfu_global_hdr_w_data_t* pGlobHdr = (sfu_global_hdr_w_data_t*)header;

	if (0 > fd)
	{
		SFU_ERR("ERROR: File descriptor is %d\n", fd);
		return IMG_VALIDITY__FILE_ERR;
	}

	// Seek to the beginning of the file
	if ((off_t)-1 == lseek(fd, 0, SEEK_SET))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR ");
		SFU_ERR("errno(0x%08x) (%s)\n", errno, strerror(errno));
		return IMG_VALIDITY__FILE_ERR;
	}

	// Retrieve the global header
	if (0 > fileRead(fd, header, sizeof(header)))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
		return IMG_VALIDITY__FILE_ERR;
	}

	return GetGlobalVersion((uint32_t)header, pGlobalVer);
} // end sfu_GetGlobalVersion


//----------------------------------------------------------------------------
//  Function: sfu_GetNumChunks
//
//  Description:
//      	This function gets the number of chunks in the SFU file.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
int32_t sfu_GetNumChunks(
		int       fd,        // Image file descriptor
		uint32_t* pNumChunks // Address to store number of chunks in SFU
		)
{
	uint8_t          globOptCnt[SFU_COUNT_BYTE_LEN];
	uint32_t         globOptCnt32;
	uint32_t         globOptIndex;
	bool             notfound = true;
	uint8_t          chnkCnt[SFU_CHNK_CNT_BYTE_LEN];
	uint32_t         optLen32;
	sfu_option_hdr_t optionHdr;

	if (0 > fd)
	{
		SFU_ERR("ERROR: File descriptor is %d\n", fd);
		return IMG_VALIDITY__FILE_ERR;
	}

	// Seek to the global opt count offset in the file
	if ((off_t)-1 == lseek(fd, GLOBAL_OPT_CNT_OFFSET, SEEK_SET))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR ");
		SFU_ERR("errno(0x%08x) (%s)\n", errno, strerror(errno));
		return IMG_VALIDITY__FILE_ERR;
	}

	// Retrieve the global option count
	if (0 > fileRead(fd, globOptCnt, sizeof(globOptCnt)))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
		return IMG_VALIDITY__FILE_ERR;
	}
	globOptCnt32 = SFU_32BIT_VALUE(globOptCnt);

	for (globOptIndex = 0;
			((globOptCnt32 > globOptIndex) && notfound);
			globOptIndex++)
	{
		// Retrieve the option header
		if (0 > fileRead(fd, optionHdr.id, sizeof(optionHdr)))
		{
			SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
			return IMG_VALIDITY__FILE_ERR;
		}

		// Extract chunk count from CHNK global option SFU
		if (0 == strncmp((char*)optionHdr.id,
					SFU_GLBL_CHNK_ID,
					SFU_OPTION_ID_BYTE_LEN))
		{
			// Retrieve the chunk count
			if (0 > fileRead(fd, chnkCnt, sizeof(chnkCnt)))
			{
				SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
				return IMG_VALIDITY__FILE_ERR;
			}
			*pNumChunks = SFU_32BIT_VALUE(chnkCnt);
			notfound = false;
		}
		else
		{
			// Seek to next option header
			if ((off_t)-1 == lseek(fd,
						SFU_32BIT_VALUE(optionHdr.length),
						SEEK_CUR))
			{
				SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR ");
				SFU_ERR("errno(0x%08x) (%s)\n", errno, strerror(errno));
				return IMG_VALIDITY__FILE_ERR;
			}
		}
	}

	if (notfound)
	{
		return -1;
	}

	return 0;
} // end sfu_GetNumChunks


//----------------------------------------------------------------------------
//  Function: sfu_GetChunkHeader
//
//  Description:
//      	This function gets the chunk header of the user requested chunk
//      	from the SFU file.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
int32_t sfu_GetChunkHeader(
		int           fd,       // Image file descriptor
		uint32_t      chunkNum, // Chunk number
		sfu_cpuhdr_t* pHdr      // Address to store header
		)
{
	uint8_t      globOptCnt[SFU_COUNT_BYTE_LEN];
	uint32_t     globOptCnt32;
	uint32_t     globOptIndex;
	uint8_t      chnkOptCnt[SFU_COUNT_BYTE_LEN];
	uint32_t     chnkOptCnt32;
	uint32_t     chnkIndex;
	sfu_option_t option;
	uint32_t     optIndex;
	uint32_t     optLen32;

	if (0 > fd)
	{
		SFU_ERR("ERROR: File descriptor is %d\n", fd);
		return IMG_VALIDITY__FILE_ERR;
	}

	// Seek to the global opt count offset in the file
	if ((off_t)-1 == lseek(fd, GLOBAL_OPT_CNT_OFFSET, SEEK_SET))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR ");
		SFU_ERR("errno(0x%08x) (%s)\n", errno, strerror(errno));
		return IMG_VALIDITY__FILE_ERR;
	}

	// Retrieve the global option count
	if (0 > fileRead(fd, globOptCnt, sizeof(globOptCnt)))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
		return IMG_VALIDITY__FILE_ERR;
	}
	globOptCnt32 = SFU_32BIT_VALUE(globOptCnt);

	for (globOptIndex = 0;
			(globOptCnt32 > globOptIndex);
			globOptIndex++)
	{
		// Retrieve the option header
		if (0 > fileRead(fd, option.hdr.id, SFU_OPTION_HDR_LEN))
		{
			SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
			return IMG_VALIDITY__FILE_ERR;
		}

		// Seek to next option header
		if ((off_t)-1 == lseek(fd,
					SFU_32BIT_VALUE(option.hdr.length),
					SEEK_CUR))
		{
			SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR ");
			SFU_ERR("errno(0x%08x) (%s)\n", errno, strerror(errno));
			return IMG_VALIDITY__FILE_ERR;
		}
	}

	// Go through chunk options until desired chunk is reached.
	// Extract chunk information and store in user requested location
	pHdr->hdr_flag = 0;
	for (chnkIndex = 0; chunkNum > chnkIndex; chnkIndex++)
	{
		// Retrieve the chunk option count
		if (0 > fileRead(fd, chnkOptCnt, sizeof(chnkOptCnt)))
		{
			SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
			return IMG_VALIDITY__FILE_ERR;
		}
		chnkOptCnt32 = SFU_32BIT_VALUE(chnkOptCnt);

		for (optIndex = 0;
				(chnkOptCnt32 > optIndex);
				optIndex++)
		{
			// Retrieve the option header
			if (0 > fileRead(fd, option.hdr.id, SFU_OPTION_HDR_LEN))
			{
				SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
				return IMG_VALIDITY__FILE_ERR;
			}

			optLen32 = SFU_32BIT_VALUE(option.hdr.length);
			if (MAX_SFU_OPTION_DATA_LEN < optLen32)
			{
				SFU_ERR("ERROR: option length(%d) exceeds maximum(%d)\n",
						optLen32, MAX_SFU_OPTION_DATA_LEN);
				return -1;
			}

			// Retrieve the option variable length data
			if (0 > fileRead(fd,
						option.var_len_data,
						optLen32))
			{
				SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR\n");
				return IMG_VALIDITY__FILE_ERR;
			}

			if ((chunkNum - 1) == chnkIndex)
			{
				// Extract chunk SFU header information
				if (0 > ExtractChunkSfuHdr(&option, pHdr))
				{
					SFU_ERR("ERROR: ExtractChunkSfuHdr() reported failure\n");
					return -1;
				}
			}
			else
			{
				// Seek to next option header
				if ((off_t)-1 == lseek(fd,
							SFU_32BIT_VALUE(option.hdr.length),
							SEEK_CUR))
				{
					SFU_ERR("ERROR: IMG_VALIDITY__FILE_ERR ");
					SFU_ERR("errno(0x%08x) (%s)\n", errno, strerror(errno));
					return IMG_VALIDITY__FILE_ERR;
				}
			}
		}
	}

	return 0;
} // end sfu_GetChunkHeader


#ifdef CONFIG_BLOWFISH
//----------------------------------------------------------------------------
//  Function: sfu_Decrypt
//
//  Description:
//      	This function ...
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
int32_t sfu_Decrypt(
		int      fd,    // Image file descriptor
		uint32_t data,  // Chunk data offset within file
		uint32_t size   // Chunk data size
		)
{
	// TODO
	return -1;
} // sfu_Decrypt
#endif // CONFIG_BLOWFISH


//----------------------------------------------------------------------------
//  Function: fileRead
//
//  Description:
//      	This function reads the user requested number of bytes from the
//      	user requested file and	stores it in the user requested address.
//
//  Returns:
//	        int numberOfBytes  where >=0 indicates number of bytes read and
//                                   <0 indicates error
//----------------------------------------------------------------------------
static int fileRead(
		int      fd,       // Image file descriptor
		uint8_t* destAddr, // Address to store data
		size_t   dataLen   // Length of data to store
		)
{
	// TODO FIXME - to be filled in by SUE
	// This is a FLASH access function
	// This function will take care of handling bad NAND sectors.
	return -1;
} // end fileRead
#endif // CONFIG_SFU_FILE


#ifdef CONFIG_SFU_RAM
extern int set_hush_var_with_str_value(const char *, char *);
//----------------------------------------------------------------------------
//  Function: sfu_MagicValid
//
//  Description:
//      	This function determines if the SFU image's magic number is valid.
//
//  Returns:
//	        int32_t magicValid   where 0 indicates valid and
//                                     <0 indicates invalid
//----------------------------------------------------------------------------
int32_t sfu_MagicValid(
		uint32_t startAddr // RAM start address
		)
{
	int                      status = 0;
	sfu_global_hdr_w_data_t* pGlobHdr = (sfu_global_hdr_w_data_t*)startAddr;
	uint32_t                 totalLen;
	uint32_t                 maxSfuSize;
	char                     strbuf[16];

	// Verify that the SFU image's magic field is valid
	if (0 != strncmp((char*)pGlobHdr->magic,
				SFU_FILE_HEADER_MAGIC,
				SFU_MAGIC_BYTE_LEN))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__MAGIC_ERR  expect(%s) file(%c%c%c%c%c%c%c%c)\n",
				SFU_FILE_HEADER_MAGIC,
				((char*)pGlobHdr->magic)[0],
				((char*)pGlobHdr->magic)[1],
				((char*)pGlobHdr->magic)[2],
				((char*)pGlobHdr->magic)[3],
				((char*)pGlobHdr->magic)[4],
				((char*)pGlobHdr->magic)[5],
				((char*)pGlobHdr->magic)[6],
				((char*)pGlobHdr->magic)[7]);
		return IMG_VALIDITY__MAGIC_ERR;
	}

	// Verify that the SFU image's total length is less or equal to the
	// maximum download partition byte length
	maxSfuSize = DownloadMtdPartSize();
	totalLen = SFU_32BIT_VALUE(pGlobHdr->total_length);
	if (maxSfuSize != 0 && totalLen > maxSfuSize)
	{
		SFU_ERR("ERROR: IMG_VALIDITY__TOTLEN_ERR  max(%08x) file(%08x)\n",
				maxSfuSize, totalLen);
		return IMG_VALIDITY__TOTLEN_ERR;
	}

	sprintf(strbuf, "0x%08x", totalLen);
	status = set_hush_var_with_str_value("SFU_TOTAL_LEN", strbuf);

	return ((0 == status) ? IMG_VALIDITY__VALID : IMG_VALIDITY__TOTLEN_ERR);
} // end sfu_MagicValid


#define SFU_BUF_LEN     (80)

//----------------------------------------------------------------------------
//  Function: sfu_ImageValid
//
//  Description:
//      	This function determines if the SFU image is valid.
//
//  Returns:
//	        int32_t imageValid   where 0 indicates valid and
//                                     <0 indicates invalid
//----------------------------------------------------------------------------
int32_t sfu_ImageValid(
		uint32_t startAddr // RAM start address
		)
{
	sfu_global_hdr_w_data_t* pGlobHdr = (sfu_global_hdr_w_data_t*)startAddr;
	uint32_t                 totalLen;
	uint32_t                 calcCrc32;
	int32_t                  imageValid;

	uint32_t                 num_chunks;
	uint32_t                 chunk;
	sfu_cpuhdr_t             chunk_hdr;
	uint32_t        chnk_data_addr;
	uint32_t        maxSfuSize;
	char            md5_output[16];
	char            sha256_output[32];
	sha256_context sha256;


	maxSfuSize = DownloadMtdPartSize();
	imageValid = ImageValid(startAddr, maxSfuSize);
	if (imageValid)
	{
		return imageValid;
	}

	totalLen = SFU_32BIT_VALUE(pGlobHdr->total_length);

	// Verify that the SFU image's CRC32 matches the calculated CRC32
	calcCrc32 = crc32(0,
			pGlobHdr->global_version.major,
			(totalLen - GLOBAL_CRC_CALC_OFFSET));
	if (calcCrc32 != SFU_32BIT_VALUE(pGlobHdr->global_crc32))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__CRC32_ERR  calc(%08x) file(%08x)\n",
				calcCrc32, SFU_32BIT_VALUE(pGlobHdr->global_crc32));
		return IMG_VALIDITY__CRC32_ERR;
	}

	if (0 > sfu_GetNumChunks(startAddr, &num_chunks)) {
		return IMG_VALIDITY__CRC32_ERR;
	}

	for (chunk = 1; num_chunks >= chunk; chunk++) {
		if (0 > sfu_GetChunkHeader(startAddr, chunk, &chunk_hdr)) {
			return 1;
		}
		if (SFU_HDR_FLAG_MINIMUM == (chunk_hdr.hdr_flag &
						SFU_HDR_FLAG_MINIMUM)) {

			chnk_data_addr = startAddr + chunk_hdr.data;

			if (chunk_hdr.hdr_flag & SFU_HDR_FLAG__CR32) {
				calcCrc32 = crc32(0, (unsigned char*)chnk_data_addr, chunk_hdr.size);
				if (calcCrc32 != chunk_hdr.crc32)
					return IMG_VALIDITY__CRC32_ERR;
			}

			if (chunk_hdr.hdr_flag & SFU_HDR_FLAG__MD5_) {

				md5((unsigned char*)chnk_data_addr, chunk_hdr.size, (unsigned char*)md5_output);
				if (strncmp(md5_output, (const char*)chunk_hdr.md5, 16) != 0)
					return IMG_VALIDITY__MD5_ERR;
			}

			if (chunk_hdr.hdr_flag & SFU_HDR_FLAG__SHA2) {
				sha256_starts(&sha256);
				sha256_update(&sha256, (unsigned char*)chnk_data_addr, chunk_hdr.size);
				sha256_finish(&sha256, (uint8_t*)sha256_output);

				if (strncmp(sha256_output, (const char*)chunk_hdr.sha256, 32) != 0)
					return IMG_VALIDITY__SH256_ERR;
			}
		}
	}

	return IMG_VALIDITY__VALID;
} // end sfu_ImageValid


//----------------------------------------------------------------------------
//  Function: sfu_GetGlobalVersion
//
//  Description:
//      	This function gets the global version of the SFU file in RAM.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
int32_t sfu_GetGlobalVersion(
		uint32_t      startAddr, // RAM start address
		sfu_cpuver_t* pGlobalVer // Address to store version
		)
{
	return GetGlobalVersion(startAddr, pGlobalVer);
} // end sfu_GetGlobalVersion


//----------------------------------------------------------------------------
//  Function: sfu_GetNumChunks
//
//  Description:
//      	This function gets the number of chunks in the SFU file in RAM.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
int32_t sfu_GetNumChunks(
		uint32_t  startAddr, // RAM start address
		uint32_t* pNumChunks // Address to store number of chunks in SFU
		)
{
	uint8_t*          pGlobOptCnt = (uint8_t*)(startAddr +
			GLOBAL_OPT_CNT_OFFSET);
	uint32_t          globOptCnt32;
	uint32_t          globOptIndex;
	bool              notfound = true;
	sfu_option_hdr_t* pOptionHdr = (sfu_option_hdr_t*)(startAddr +
			GLOBAL_HDR_LEN);
	uint32_t          optLen32;
	uint8_t*          pData = (uint8_t*)pOptionHdr;

	// Retrieve the global option count
	globOptCnt32 = SFU_32BIT_VALUE(pGlobOptCnt);

	for (globOptIndex = 0;
			((globOptCnt32 > globOptIndex) && notfound);
			globOptIndex++)
	{
		// Extract chunk count from CHNK global option SFU
		if (0 == strncmp((char*)pOptionHdr->id,
					SFU_GLBL_CHNK_ID,
					SFU_OPTION_ID_BYTE_LEN))
		{
			// Retrieve the chunk count
			*pNumChunks =
				SFU_32BIT_VALUE(((sfu_option_t*)pOptionHdr)->var_len_data);
			notfound = false;
		}
		else
		{
			// Advance pointer to next option header
			optLen32 = SFU_32BIT_VALUE(pOptionHdr->length);
			pData += (SFU_OPTION_HDR_LEN + optLen32);
			pOptionHdr = (sfu_option_hdr_t*)pData;
		}
	}

	if (notfound)
	{
		return -1;
	}

	return 0;
} // end sfu_GetNumChunks


//----------------------------------------------------------------------------
//  Function: sfu_GetChunkHeader
//
//  Description:
//      	This function gets the chunk header of the user requested chunk
//      	from the SFU file in RAM.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
int32_t sfu_GetChunkHeader(
		uint32_t      startAddr, // RAM start address
		uint32_t      chunkNum,  // Chunk number
		sfu_cpuhdr_t* pHdr       // Address to store header
		)
{
	uint8_t*          pGlobOptCnt = (uint8_t*)(startAddr +
			GLOBAL_OPT_CNT_OFFSET);
	uint32_t          globOptCnt32;
	uint32_t          globOptIndex;
	sfu_option_hdr_t* pOptionHdr = (sfu_option_hdr_t*)(startAddr +
			GLOBAL_HDR_LEN);
	uint8_t*          pData = (uint8_t*)pOptionHdr;
	uint32_t          chnkOptCnt32;
	uint32_t          chnkIndex;
	uint32_t          optIndex;
	uint32_t          optLen32;

	// Retrieve the global option count
	globOptCnt32 = SFU_32BIT_VALUE(pGlobOptCnt);

	for (globOptIndex = 0;
			(globOptCnt32 > globOptIndex);
			globOptIndex++)
	{
		// Advance pointer to next option header
		optLen32 = SFU_32BIT_VALUE(pOptionHdr->length);
		pData += (SFU_OPTION_HDR_LEN + optLen32);
		pOptionHdr = (sfu_option_hdr_t*)pData;
	}

	// Go through chunk options until desired chunk is reached.
	// Extract chunk information and store in user requested location
	pHdr->hdr_flag = 0;
	for (chnkIndex = 0; chunkNum > chnkIndex; chnkIndex++)
	{
		// Retrieve the chunk option count
		chnkOptCnt32 = SFU_32BIT_VALUE(pData);

		// Advance pointer to the next option header
		pData += SFU_COUNT_BYTE_LEN;
		pOptionHdr = (sfu_option_hdr_t*)pData;

		for (optIndex = 0;
				(chnkOptCnt32 > optIndex);
				optIndex++)
		{
			if ((chunkNum - 1) == chnkIndex)
			{
				// Extract chunk SFU header information
				if (0 > ExtractChunkSfuHdr((sfu_option_t*)pOptionHdr, pHdr))
				{
					SFU_ERR("ERROR: ExtractChunkSfuHdr() reported failure\n");
					return -1;
				}
			}

			// Advance pointer to next option header
			optLen32 = SFU_32BIT_VALUE(pOptionHdr->length);
			pData += (SFU_OPTION_HDR_LEN + optLen32);
			pOptionHdr = (sfu_option_hdr_t*)pData;
		}
	}

	return 0;
} // end sfu_GetChunkHeader


#ifdef CONFIG_BLOWFISH
//----------------------------------------------------------------------------
//  Function: sfu_Decrypt
//
//  Description:
//      	This function gets the sfu_chnk_data and sfu_chnk_size from the
//      	u-boot environment, decrypts the image starting at <sfu_chnk_data>
//      	address with <sfu_chnk_size> length, overwrites RAM area with
//      	decrypted image, and set the <sfu_chnk_size> to the decrypted
//      	image length.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
int32_t sfu_Decrypt(
		uint32_t data,  // SFU Chunk Data Base Address
		uint32_t size   // SFU Chunk Data Size
		)
{
	char*    keystr;
	char     sfubuf[40];
	uint32_t idx;
	uint32_t orig_image_len = 0;
	int      status = 0;

	keystr = getconst("key");
	if ((NULL == keystr) || ('\0' == *keystr)) {
		SFU_ERR("ERROR: key constants variable not set\n");
		return -1;
	}
	InitializeBlowfish(keystr, strlen(keystr));

	for (idx = 0; size > idx; idx+=8)
	{
		Blowfish_decipher((unsigned long*)(data + idx),
				(unsigned long*)(data + idx + 4));
	}
	orig_image_len =
		(((*((uint8_t*)(data + size - 4))) << 24) |
		 ((*((uint8_t*)(data + size - 3))) << 16) |
		 ((*((uint8_t*)(data + size - 2))) << 8) |
		 ((*((uint8_t*)(data + size - 1))) << 0));
	if ((size-8) >= orig_image_len)
	{
		sprintf(sfubuf, "0x%08x", orig_image_len);
		status |= set_hush_var_with_str_value("SFU_CHNK_SIZE", sfubuf);
		SFU_DBG("SFU_CHNK_SIZE=%s\n", sfubuf);
		if (status) {
			SFU_ERR("ERROR: %s -> failed to set HUSH variable, SFU_CHNK_SIZE\n",
					__FUNCTION__);
			return -1;
		}
	}
	else
	{
		// decryption failed if original length is not less than
		// or equal to the padded 64-bit length minus eight
		SFU_ERR("ERROR: %s -> padded_len(0x%08x)  orig_len(0x%08x)\n",
				__FUNCTION__, size, orig_image_len);
		return -1;
	}

	return 0;
} // sfu_Decrypt
#endif // CONFIG_BLOWFISH
#endif // CONFIG_SFU_RAM


//----------------------------------------------------------------------------
//  Function: ImageValid
//
//  Description:
//      	This function determines if the SFU image is valid.
//
//  Returns:
//	        int32_t imageValid   where 0 indicates valid and
//                                     <0 indicates invalid
//----------------------------------------------------------------------------
static int32_t ImageValid(
		uint32_t startAddr, // RAM start address
		uint32_t maxSize
		)
{
	sfu_global_hdr_w_data_t* pGlobHdr = (sfu_global_hdr_w_data_t*)startAddr;
	uint32_t                 totalLen;

	// Verify that the SFU image's magic field is valid
	if (0 != strncmp((char*)pGlobHdr->magic,
				SFU_FILE_HEADER_MAGIC,
				SFU_MAGIC_BYTE_LEN))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__MAGIC_ERR  expect(%s) file(%c%c%c%c%c%c%c%c)\n",
				SFU_FILE_HEADER_MAGIC,
				((char*)pGlobHdr->magic)[0],
				((char*)pGlobHdr->magic)[1],
				((char*)pGlobHdr->magic)[2],
				((char*)pGlobHdr->magic)[3],
				((char*)pGlobHdr->magic)[4],
				((char*)pGlobHdr->magic)[5],
				((char*)pGlobHdr->magic)[6],
				((char*)pGlobHdr->magic)[7]);
		return IMG_VALIDITY__MAGIC_ERR;
	}

	// Verify that the SFU image's total length is less or equal to the
	// maximum download partition byte length
	totalLen = SFU_32BIT_VALUE(pGlobHdr->total_length);
	if (maxSize != 0 && totalLen > maxSize)
	{
		SFU_ERR("ERROR: IMG_VALIDITY__TOTLEN_ERR  max(%08x) file(%08x)\n",
				maxSize, totalLen);
		return IMG_VALIDITY__TOTLEN_ERR;
	}

	// Verify that the SFU image can be parsed by ensuring image format
	// version is less or equal to the maximum image format version
	if (MAX_SFU_IMG_FMT_VERSION < SFU_32BIT_VALUE(pGlobHdr->img_fmt_version))
	{
		SFU_ERR("ERROR: IMG_VALIDITY__IMGFMTVER_ERR  max(%08x) file(%08x)\n",
				(uint32_t)MAX_SFU_IMG_FMT_VERSION, SFU_32BIT_VALUE(pGlobHdr->img_fmt_version));
		return IMG_VALIDITY__IMGFMTVER_ERR;
	}

	return IMG_VALIDITY__VALID;
} // end ImageValid


//----------------------------------------------------------------------------
//  Function: GetGlobalVersion
//
//  Description:
//      	This function gets the global version of the SFU file in RAM.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
static int32_t GetGlobalVersion(
		uint32_t      startAddr, // RAM start address
		sfu_cpuver_t* pGlobVer   // Address to store version
		)
{
	sfu_global_hdr_w_data_t* pGlobHdr = (sfu_global_hdr_w_data_t*)startAddr;

	pGlobVer->major = SFU_32BIT_VALUE(pGlobHdr->global_version.major);
	pGlobVer->minor = SFU_32BIT_VALUE(pGlobHdr->global_version.minor);
	pGlobVer->commitcnt = SFU_32BIT_VALUE(pGlobHdr->global_version.commitcnt);
	pGlobVer->githash = SFU_32BIT_VALUE(pGlobHdr->global_version.githash);

	return 0;
} // end GetGlobalVersion


//----------------------------------------------------------------------------
//  Function: ExtractChunkSfuHdr
//
//  Description:
//      	This function extracts chunk SFU option data to the user
//      	requested chunk header data storage area.
//
//  Returns:
//	        int32_t status    where 0 indicates success and
//                                  <0 indicates error
//----------------------------------------------------------------------------
static int32_t ExtractChunkSfuHdr(
		sfu_option_t* pOption, // Address of option
		sfu_cpuhdr_t* pHdr     // Address to store header
		)
{
	int32_t    status = 0;
	sfu_ver_t* pVer;
	int        idx;

	if (0 == strncmp((char*)pOption->hdr.id,
				SFU_CHNK_OPT_ID_VERS,
				SFU_OPTION_ID_BYTE_LEN))
	{
		pVer = (sfu_ver_t*)&pOption->var_len_data;
		pHdr->version.major = SFU_32BIT_VALUE(pVer->major);
		pHdr->version.minor = SFU_32BIT_VALUE(pVer->minor);
		pHdr->version.commitcnt = SFU_32BIT_VALUE(pVer->commitcnt);
		pHdr->version.githash = SFU_32BIT_VALUE(pVer->githash);

		// Set the header data flag
		pHdr->hdr_flag |= SFU_HDR_FLAG__VERS;

		SFU_DBG("INFO: VERSION (%08x.%08x.%08x.%08x) HDRFLAG(%08x)\n",
				pHdr->version.major,
				pHdr->version.minor,
				pHdr->version.commitcnt,
				pHdr->version.githash,
				pHdr->hdr_flag);
	}
	else if (0 == strncmp((char*)pOption->hdr.id,
				SFU_CHNK_OPT_ID_SIZE,
				SFU_OPTION_ID_BYTE_LEN))
	{
		pHdr->size = SFU_32BIT_VALUE(pOption->var_len_data);

		// Set the header data flag
		pHdr->hdr_flag |= SFU_HDR_FLAG__SIZE;

		SFU_DBG("INFO: SIZE (%08x) HDRFLAG(%08x)\n",
				pHdr->size, pHdr->hdr_flag);
	}
	else if (0 == strncmp((char*)pOption->hdr.id,
				SFU_CHNK_OPT_ID_DATA,
				SFU_OPTION_ID_BYTE_LEN))
	{
		pHdr->data = SFU_32BIT_VALUE(pOption->var_len_data);

		// Set the header data flag
		pHdr->hdr_flag |= SFU_HDR_FLAG__DATA;

		SFU_DBG("INFO: DATA (%08x) HDRFLAG(%08x)\n",
				pHdr->data, pHdr->hdr_flag);
	}
	else if (0 == strncmp((char*)pOption->hdr.id,
				SFU_CHNK_OPT_ID_DEST,
				SFU_OPTION_ID_BYTE_LEN))
	{
		memcpy(pHdr->dest, pOption->var_len_data, sizeof(pHdr->dest));

		// Set the header data flag
		pHdr->hdr_flag |= SFU_HDR_FLAG__DEST;

		SFU_DBG("INFO: DEST (%s) HDRFLAG(%08x)\n",
				pHdr->dest, pHdr->hdr_flag);
	}
	else if (0 == strncmp((char*)pOption->hdr.id,
				SFU_CHNK_OPT_ID_MD5_,
				SFU_OPTION_ID_BYTE_LEN))
	{
		memcpy(pHdr->md5, pOption->var_len_data, sizeof(pHdr->md5));

		// Set the header data flag
		pHdr->hdr_flag |= SFU_HDR_FLAG__MD5_;

		SFU_DBG("INFO: MD5_ (");
		for (idx = 0; sizeof(pHdr->md5) > idx ; idx++)
		{
			SFU_DBG("%02x", pHdr->md5[idx]);
		}
		SFU_DBG(") HDRFLAG(%08x)\n", pHdr->hdr_flag);
	}
	else if (0 == strncmp((char*)pOption->hdr.id,
				SFU_CHNK_OPT_ID_SHA2,
				SFU_OPTION_ID_BYTE_LEN))
	{
		memcpy(pHdr->sha256, pOption->var_len_data, sizeof(pHdr->sha256));

		// Set the header data flag
		pHdr->hdr_flag |= SFU_HDR_FLAG__SHA2;

		SFU_DBG("INFO: SHA256 (");
		for (idx = 0; sizeof(pHdr->sha256) > idx ; idx++)
		{
			SFU_DBG("%02x", pHdr->sha256[idx]);
		}
		SFU_DBG(") HDRFLAG(%08x)\n", pHdr->hdr_flag);
	}
	else if (0 == strncmp((char*)pOption->hdr.id,
				SFU_CHNK_OPT_ID_CR32,
				SFU_OPTION_ID_BYTE_LEN))
	{
		pHdr->crc32 = SFU_32BIT_VALUE(pOption->var_len_data);

		// Set the header data flag
		pHdr->hdr_flag |= SFU_HDR_FLAG__CR32;

		SFU_DBG("INFO: CRC32 (%08x) HDRFLAG(%08x)\n",
				pHdr->crc32, pHdr->hdr_flag);
	}
	else if (0 == strncmp((char*)pOption->hdr.id,
				SFU_CHNK_OPT_ID_ENCM,
				SFU_OPTION_ID_BYTE_LEN))
	{
		pHdr->encryption_method = SFU_32BIT_VALUE(pOption->var_len_data);

		// Set the header data flag
		pHdr->hdr_flag |= SFU_HDR_FLAG__ENCM;

		SFU_DBG("INFO: ENCM (%08x) HDRFLAG(%08x)\n",
				pHdr->encryption_method, pHdr->hdr_flag);
	}
	else
	{
		// Invalid/Unknown chunk header option
		status = -1;

		SFU_ERR("ERROR: Unknown chunk header option (%c%c%c%c)\n",
				(char)pOption->hdr.id[0], (char)pOption->hdr.id[1],
				(char)pOption->hdr.id[2], (char)pOption->hdr.id[3]);
	}

	return status;
} // end ExtractChunkSfuHdr

#define SFU_BUF_LEN     (80)

static int check_sfu_signature(uint32_t addr, uint32_t *sfu_addr, int verify)
{
#ifdef CONFIG_FIT
	const char *fit_uname = "kernel";
	ulong		len;
	const image_header_t *hdr;
	ulong		*data;
	void *buf;
	const void*	fit_hdr;
	int		noffset;
	const void	*fit_data;
	size_t		fit_len;

	*sfu_addr = addr;

	buf = map_sysmem(addr, 0);
	switch (genimg_get_format(buf)) {
	case IMAGE_FORMAT_FIT:
		puts("   FIT image found\n");
		if (fit_uname == NULL) {
			puts ("No FIT subimage unit name\n");
			return 1;
		}

		fit_hdr = buf;
		if (!fit_check_format(fit_hdr)) {
			puts ("Bad FIT image format\n");
			return 1;
		}

		if (verify) {
			fit_print_contents(fit_hdr);
		}

		/* get script component image node offset */
		noffset = fit_image_get_node (fit_hdr, fit_uname);
		if (noffset < 0) {
			printf ("Can't find '%s' FIT subimage\n", fit_uname);
			return 1;
		}

		if (!fit_image_check_type (fit_hdr, noffset, IH_TYPE_KERNEL)) {
			puts ("Not a image image\n");
			return 1;
		}

		/* verify integrity */
		if (verify) {
			puts("Checking signature:");
			if (!fit_image_verify(fit_hdr, noffset)) {
				puts ("Bad Data Hash\n");
				return 1;
			}
			puts("\n");
		}

		/* get script subimage data address and length */
		if (fit_image_get_data(fit_hdr, noffset, &fit_data, &fit_len)) {
			puts ("Could not find subimage data\n");
			return 1;
		}
		*sfu_addr = (uint32_t *)fit_data;
		break;
	default:
		puts ("Wrong image format\n");
		return 1;
	}
#endif
	return 0;
}

static int do_sfu(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	char*           cmd;
	uint32_t        addr;
	sfu_cpuver_t    glob_ver;
	uint32_t        num_chunks;
	sfu_cpuhdr_t    chunk_hdr;
	uint32_t        chunk;
	uint32_t        chnklen;
	char            sfubuf[SFU_BUF_LEN];
	int             idx;
	char            hexchar[3];
	int             status = 0;
	uint32_t sfu_addr;
	int verify = 0, ret;

	if (argc == 2) {
		/*
		 * Syntax is:
		 *   0   1
		 *   sfu errstate
		 */
		if (strcmp(argv[1], "errstate") == 0) {
			// Set error state (status LED to solid red, ...)
#if 0
			set_board_state(BS_HardFailure);
#endif
			printf("SERIOUS ERROR\n");

			//}

			// DO NOT RETURN FROM AN ERROR STATE
			SFU_ERR("SFU: ERROR STATE  -");
			while(1) {
				SFU_ERR("\b\\");
				udelay(1 * 1000000);
				SFU_ERR("\b|");
				udelay(1 * 1000000);
				SFU_ERR("\b/");
				udelay(1 * 1000000);
				SFU_ERR("\b-");
				udelay(1 * 1000000);
			}
			return 0;
		} else {
			goto usage;
		}
	}
	/* at least three arguments for the remaining commands please */
	else if (argc < 3)
		goto usage;

	cmd = argv[1];
	addr = simple_strtoul(argv[2], NULL, 16);

	if (strcmp(cmd, "valid") == 0) {
		verify = 1;
	}

	/* skip FIT bytes to get correct sfu address */
	ret = check_sfu_signature(addr, &sfu_addr, verify);
	if (ret) {
		return ret;
	} else {
		addr = sfu_addr;
	}

	/*
	 * Syntax is:
	 *   0   1     2
	 *   sfu magic addr
	 */
	if (strcmp(cmd, "magic") == 0) {

		if (0 > sfu_MagicValid(addr)) {
			return 1;
		}
		return 0;
	}

	/*
	 * Syntax is:
	 *   0   1     2
	 *   sfu valid addr
	 */
	if (strcmp(cmd, "valid") == 0) {

		if (0 > sfu_ImageValid(addr)) {
			return 1;
		}
		return 0;
	}

	/*
	 * Syntax is:
	 *   0   1       2
	 *   sfu globver addr
	 */
	if (strcmp(cmd, "globver") == 0) {

		if (0 > sfu_GetGlobalVersion(addr, &glob_ver)) {
			return 1;
		}
		sprintf(sfubuf, "%08x.%08x.%08x.%08x",
				glob_ver.major,
				glob_ver.minor,
				glob_ver.commitcnt,
				glob_ver.githash);
		status |= set_hush_var_with_str_value("SFU_GLOB_VERS", sfubuf);
		SFU_DBG("SFU_GLOB_VERS=%s\n", sfubuf);

		return status;
	}

	/*
	 * Syntax is:
	 *   0   1       2
	 *   sfu chnknum addr
	 */
	if (strcmp(cmd, "chnknum") == 0) {

		if (0 > sfu_GetNumChunks(addr, &num_chunks)) {
			return 1;
		}
		sprintf(sfubuf, "0x%08x", num_chunks);
		status |= set_hush_var_with_str_value("SFU_NUM_CHNKS", sfubuf);
		SFU_DBG("SFU_NUM_CHNKS=%s\n", sfubuf);

		return status;
	}

	/*
	 * Syntax is:
	 *   0   1       2    3
	 *   sfu chnkhdr addr id
	 */
	if (strcmp(cmd, "chnkhdr") == 0) {
		if (argc != 4)
			goto usage;

		if (0 > sfu_GetNumChunks(addr, &num_chunks)) {
			return 1;
		}
		SFU_DBG("num chunks=%d\n", num_chunks);
		SFU_DBG("Searching for \"%s\" chunk...\n", argv[3]);

		for (chunk = 1; num_chunks >= chunk; chunk++) {
			if (0 > sfu_GetChunkHeader(addr, chunk, &chunk_hdr)) {
				return 1;
			}
			SFU_DBG("chunk %d header name(%s) hdr_flag(0x%08x)\n",
					chunk, chunk_hdr.dest, chunk_hdr.hdr_flag);

			if ((SFU_HDR_FLAG_MINIMUM == (chunk_hdr.hdr_flag &
							SFU_HDR_FLAG_MINIMUM)) &&
					(strlen(argv[3]) == strlen(chunk_hdr.dest)) &&
					(0 == strncmp(argv[3], chunk_hdr.dest, strlen(argv[3])))) {
				// chunk found
				SFU_DBG("chunk found\n");
				break;
			}
		}

		if (chunk > num_chunks) {
			return 1;
		} else {
			SFU_DBG("\n\n--------- CHUNK NAME(%s) ---------\n", chunk_hdr.dest);
			sprintf(sfubuf, "0x%08x", (addr + chunk_hdr.data));
			status |= set_hush_var_with_str_value("SFU_CHNK_DATA", sfubuf);
			SFU_DBG("SFU_CHNK_DATA=%s\n", sfubuf);
			sprintf(sfubuf, "%s", chunk_hdr.dest);
			status |= set_hush_var_with_str_value("SFU_CHNK_DEST", sfubuf);
			SFU_DBG("SFU_CHNK_DEST=%s\n", sfubuf);
			sprintf(sfubuf, "0x%08x", chunk_hdr.size);
			status |= set_hush_var_with_str_value("SFU_CHNK_SIZE", sfubuf);
			SFU_DBG("SFU_CHNK_SIZE=%s\n", sfubuf);
			sprintf(sfubuf, "%08x.%08x.%08x.%08x",
					chunk_hdr.version.major,
					chunk_hdr.version.minor,
					chunk_hdr.version.commitcnt,
					chunk_hdr.version.githash);
			status |= set_hush_var_with_str_value("SFU_CHNK_VERS", sfubuf);
			SFU_DBG("SFU_CHNK_VERS=%s\n", sfubuf);
			sprintf(sfubuf, "%08x", chunk_hdr.encryption_method);
			status |= set_hush_var_with_str_value("SFU_CHNK_ENCM", sfubuf);
			SFU_DBG("SFU_CHNK_ENCM=%s\n", sfubuf);
			if (chunk_hdr.hdr_flag & SFU_HDR_FLAG__CR32) {
				sprintf(sfubuf, "%08x", chunk_hdr.crc32);
				status |= set_hush_var_with_str_value("SFU_CHNK_CR32", sfubuf);
				SFU_DBG("SFU_CHNK_CR32=%s\n", sfubuf);
			}
			else
			{
				status |= set_hush_var_with_str_value("SFU_CHNK_CR32", "");
				SFU_DBG("SFU_CHNK_CR32=%s\n", "");
			}
			if (chunk_hdr.hdr_flag & SFU_HDR_FLAG__MD5_) {
				memset(sfubuf, 0x00, SFU_BUF_LEN);
				for (idx = 0; sizeof(chunk_hdr.md5) > idx; idx++) {
					sprintf(hexchar, "%02x", chunk_hdr.md5[idx]);
					strncat(sfubuf, hexchar, (SFU_BUF_LEN-1));
				}
				status |= set_hush_var_with_str_value("SFU_CHNK_MD5_", sfubuf);
				SFU_DBG("SFU_CHNK_MD5_=%s\n", sfubuf);
			}
			else
			{
				status |= set_hush_var_with_str_value("SFU_CHNK_MD5_", "");
				SFU_DBG("SFU_CHNK_MD5_=%s\n", "");
			}
			if (chunk_hdr.hdr_flag & SFU_HDR_FLAG__SHA2) {
				memset(sfubuf, 0x00, SFU_BUF_LEN);
				for (idx = 0; sizeof(chunk_hdr.sha256) > idx; idx++) {
					sprintf(hexchar, "%02x", chunk_hdr.sha256[idx]);
					strncat(sfubuf, hexchar, (SFU_BUF_LEN-1));
				}
				status |= set_hush_var_with_str_value("SFU_CHNK_SHA2", sfubuf);
				SFU_DBG("SFU_CHNK_SHA2=%s\n", sfubuf);
			}
			else
			{
				status |= set_hush_var_with_str_value("SFU_CHNK_SHA2", "");
				SFU_DBG("SFU_CHNK_SHA2=%s\n", "");
			}
			SFU_DBG("--- --- --- --- --- --- ---\n\n");
		}

		return status;
	}

#ifdef CONFIG_BLOWFISH
	/*
	 * Syntax is:
	 *   0   1       2    3
	 *   sfu decrypt addr len
	 */
	if (strcmp(cmd, "decrypt") == 0) {
		if (argc != 4)
			goto usage;

		chnklen = simple_strtoul(argv[3], NULL, 16);
		if (0 > sfu_Decrypt(addr, chnklen)) {
			return 1;
		}

		return 0;
	}
#endif // CONFIG_BLOWFISH

usage:
	return cmd_usage(cmdtp);
}

U_BOOT_CMD(
		sfu, CONFIG_SYS_MAXARGS, 1, do_sfu,
		"Streamunlimited Firmware Update parser",
		"magic addr           - determine if SFU magic number is valid\n"
		"sfu valid addr       - determine if SFU image is valid\n"
		"sfu globver addr     - get the global version of the SFU image\n"
		"sfu chnknum addr     - get the number of chunks in SFU image\n"
		"sfu chnkhdr addr id  - get chunk header with id\n"
		"sfu errstate         - handle error state condition\n"
#ifdef CONFIG_BLOWFISH
		"sfu decrypt addr len - decrypt ram starting at <addr> and <len> bytes\n"
#endif // CONFIG_BLOWFISH
		);
