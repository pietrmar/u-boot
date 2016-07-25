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
	{ 0x00900000, 0x00800000, "settings" },
	{ 0x01100000, 0x00C00000, "fit" },
	{ 0x01D00000, 0x07300000, "rootfs" },
	{ 0x09000000, 0x07000000, "download" },
};

const struct sue_partition sue_part_512M[] = {
	{ 0x00000000, 0x00800000, "u-boot" },
	{ 0x00800000, 0x00080000, "u-boot-env" },
	{ 0x00880000, 0x00080000, "constants" },
	{ 0x00900000, 0x08000000, "settings" },
	{ 0x08900000, 0x00C00000, "fit" },
	{ 0x09500000, 0x0C100000, "rootfs" },
	{ 0x15600000, 0x0AA00000, "download" },
};

static int create_mtd_part(char *buf, const struct sue_partition *part)
{
	int ret;

	if (part->size < SZ_1M) {
		ret = sprintf(buf, "%dk(%s)", (part->size / SZ_1K), part->name);
	} else {
		ret = sprintf(buf, "%dM(%s)", (part->size / SZ_1M), part->name);
	}

	return ret;
}

/*
 * Detect flash size and setup mtdparts variable
 * accordingly from defined partition table
 *
 * This was 90% taken from stream800.
 */
int sue_setup_mtdparts(void)
{
	const char *mtdparts_prefix = "mtdparts=gpmi-nand:";
	const struct sue_partition *part;
	char *mtdparts;
	struct mtd_info *mtd;
	int part_size, ret = 0;
	int i, j;

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
		char buf[30];
		create_mtd_part(buf, &part[i]);
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

	/* Find and set `mtd_download_size` according to the size of the download partition, in kiB */
	for (i = 0; i < part_size; i++) {
		if (!strcmp(part[i].name, "download")) {
			char buf[30];
			sprintf(buf, "%u", part[i].size / SZ_1K);
			setenv("mtd_download_size", buf);
			break;
		}
	}

	return ret;
}

