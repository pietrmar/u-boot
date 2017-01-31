#include <linux/sizes.h>
#include <linux/mtd/mtd.h>
#include <common.h>

struct sue_partition {
	u32 offset;
	u32 size;
	const char *name;
};

const struct sue_partition sue_part_256M[] = {
	{ 0x00000000, 0x00800000, "u-boot" },
	{ 0x00800000, 0x00080000, "u-boot-env" },
	{ 0x00880000, 0x00080000, "constants" },
	{ 0x00900000, 0x02000000, "swufit" },
	{ 0x02900000, 0x00C00000, "fit" },
	{ 0x03500000, 0x0CB00000, "data" },
};

const struct sue_partition sue_part_512M[] = {
	{ 0x00000000, 0x00800000, "u-boot" },
	{ 0x00800000, 0x00080000, "u-boot-env" },
	{ 0x00880000, 0x00080000, "constants" },
	{ 0x00900000, 0x02000000, "swufit" },
	{ 0x02900000, 0x00C00000, "fit" },
	{ 0x03500000, 0x1CB00000, "data" },
};

static int create_mtd_part(char *buf, size_t buflen, const struct sue_partition *part)
{
	int ret;

	if (part->size < SZ_1M) {
		ret = snprintf(buf, buflen, "%dk(%s)", (part->size / SZ_1K), part->name);
	} else {
		ret = snprintf(buf, buflen, "%dM(%s)", (part->size / SZ_1M), part->name);
	}

	/* String was truncated */
	if (ret >= buflen) {
		return -EINVAL;
	}

	return 0;
}

/*
 * Detect flash size and setup `mtdparts`, `mtd_pagesize` and `mtd_download_size`
 * env variables accordingly.
 *
 * Return 0 on success and a negative error code on failure;
 */
int sue_setup_mtdparts(void)
{
	const char *mtdparts_prefix = "mtdparts=gpmi-nand:";
	const struct sue_partition *part;
	char *mtdparts;
	struct mtd_info *mtd;
	int part_size, ret = 0;
	int i, j;
	char buf[30];

	mtd = get_mtd_device_nm("nand0");

	if (IS_ERR(mtd)) {
		printf("Device %s not found!\n", "nand0");
		return -ENODEV;
	}

	switch (mtd->size) {
		case SZ_256M:
			part = sue_part_256M;
			part_size = ARRAY_SIZE(sue_part_256M);
			break;

		case SZ_512M:
			part = sue_part_512M;
			part_size = ARRAY_SIZE(sue_part_512M);
			break;

		/* Currently use 512M parition layout for 2GiB flash */
		case SZ_2G:
			part = sue_part_512M;
			part_size = ARRAY_SIZE(sue_part_512M);
			break;

		default:
			printf("Unknown NAND size of %llu MiB\n", (mtd->size / SZ_1M));
			part = NULL;
			part_size = 0;
			break;
	};

	if (part == NULL) {
		return -EIO;
	}

	printf("Setting partitions for %llu MiB flash\n", (mtd->size / SZ_1M));

	/* prepare buffer for mtdparts env*/
	mtdparts = malloc(strlen(mtdparts_prefix) + (part_size * 30));
	j = strlen(mtdparts_prefix);

	strcpy(mtdparts, mtdparts_prefix);

	for (i = 0; i < part_size; i++) {
		ret = create_mtd_part(buf, sizeof(buf), &part[i]);

		if (ret) {
			printf("Could not create partition entry\n");
			return ret;
		}

		if (i != (part_size - 1)) {
			int len = strlen(buf);
			buf[len] = ',';
			buf[len + 1] = '\0';
		}

		strncpy(&mtdparts[j], buf, strlen(buf));
		j += strlen(buf);
	}

	mtdparts[j] = '\0';
	printf("Setting mtdparts: %s\n", mtdparts);
	setenv("mtdparts", mtdparts);

	printf("Setting mtd_pagesize: %u\n", mtd->writesize);
	snprintf(buf, sizeof(buf), "%u", mtd->writesize);
	setenv("mtd_pagesize", buf);

	/* Find and set `mtd_download_size` according to the size of the download partition, in kiB */
	for (i = 0; i < part_size; i++) {
		if (!strcmp(part[i].name, "download")) {
			snprintf(buf, sizeof(buf), "%u", part[i].size / SZ_1K);
			setenv("mtd_download_size", buf);
			break;
		}
	}

	return ret;
}

