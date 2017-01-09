////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Microsemi Corporation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You may
// obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0 Unless required by
// applicable law or agreed to in writing, software distributed under the
// License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for
// the specific language governing permissions and limitations under the
// License.
//
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//
//   Author: Logan Gunthorpe
//           Stephen Bates
//
//   Description:
//     A simple user space program to test ZONE_DEVICE and PMEM and
//     IOPMEM functionality.
//
////////////////////////////////////////////////////////////////////////

#include <argconfig/argconfig.h>

#include <linux/nvme.h>
#include <linux/fs.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

const char program_desc[] = "Simple program for testing ZONE_DEVICE devices.";

struct config {
	char *mmap_file;
	char *odirect_file;
	char *nvme_device;
	unsigned long lba;
	int mmap_len;
	int seed;
	void *rand_buf;
	void *mmap_buf;
	unsigned zone_device;
	unsigned verbose;
	unsigned do_write_submit_io;
};

static const struct config defaults = {
	.nvme_device = "/dev/nvme0n1",
	.lba = 100,
	.mmap_len = 8192,
	.seed = -1,
	.zone_device = 0,
	.verbose = 0,
};

static const struct argconfig_commandline_options command_line_options[] = {
	{"nvme", "STRING", CFG_STRING, &defaults.nvme_device, required_argument,
			"path to the NVMe block device to use"},

	{"mmap", "STRING", CFG_STRING, &defaults.mmap_file, required_argument,
			"path to the file to mmap()"},

	{"odirect", "STRING", CFG_STRING, &defaults.odirect_file, required_argument,
			"path to the O_DIRECT file to use"},

	{"l", "NUM", CFG_LONG_SUFFIX, &defaults.lba, required_argument,
			NULL},
	{"lba", "NUM", CFG_LONG_SUFFIX, &defaults.lba, required_argument,
			"starting LBA to use"},

	{"m", "NUM", CFG_LONG_SUFFIX, &defaults.mmap_len, required_argument,
			NULL},
	{"mmap_len", "NUM", CFG_LONG_SUFFIX, &defaults.mmap_len, required_argument,
			"length of mmap to use"},

	{"z", "", CFG_NONE, &defaults.zone_device, no_argument, NULL},
	{"zone_device", "", CFG_NONE, &defaults.zone_device, no_argument,
			"mmap()ed file must reside in ZONE_DEVICE"},

	{"v", "", CFG_NONE, &defaults.verbose, no_argument, NULL},
	{"verbose", "", CFG_NONE, &defaults.verbose, no_argument,
			"be verbose"},

	{"w", "", CFG_NONE, &defaults.do_write_submit_io, no_argument, NULL},
	{"write-submit_io", "", CFG_NONE, &defaults.do_write_submit_io, no_argument,
			"do a write submit io: this may trash the filesystem "
			"on the nvme device"},

	{0}
};

static int report(struct config *cfg, const char *func, int val)
{
	if (cfg->verbose || errno)
		fprintf(stderr, "%s: %d = %s.\n", func, errno, strerror(errno));

	return val;
}

static int create_random_buf(struct config *cfg)
{
	void *p;
	char *buf;

	int ret = posix_memalign(&p, sysconf(_SC_PAGESIZE), cfg->mmap_len);
	if (ret || p == NULL)
		return ret;

	buf = p;

	for (unsigned i = 0; i < cfg->mmap_len; i++)
		buf[i] = (char)rand();

	cfg->rand_buf = (void *)buf;

	return 0;
}

static int compare_buf(void *a, void *b, size_t len)
{
	unsigned long *ac = (unsigned long *)a, *bc = (unsigned long *)b;

	for (unsigned i = 0; i < len / sizeof(*ac); i++) {
		if (ac[i] != bc[i]) {
			for (int j = 0; j < 16; j++)
				fprintf(stderr, "%d\t0x%08x\t0x%08x\n",
					i+j, ac[i+j],bc[i+j]);
			return i + 1;
		}
	}

	return 0;
}

static int create_mmap(struct config *cfg)
{
	void *buf;
	int tfd = open(cfg->mmap_file, O_RDWR);
	if (tfd < 0)
		return report(cfg, "create_mmap (open)", errno);

	buf = mmap(NULL, cfg->mmap_len, PROT_READ | PROT_WRITE, MAP_SHARED,
		   tfd, 0);
	if (buf == MAP_FAILED)
		return report(cfg, "create_mmap (mmap)", errno);
	close(tfd);
	cfg->mmap_buf = buf;

	return 0;
}

static int test_buf(struct config *cfg)
{
	unsigned long *a = (unsigned long *)cfg->mmap_buf,
	    *b = (unsigned long *)cfg->rand_buf;

	if (cfg->verbose)
		fprintf(stderr, "-- Testing mmap read/write:\n");

	for (unsigned i = 0; i < cfg->mmap_len / sizeof(*a); i++)
		a[i] = b[i];

	int ret = compare_buf(cfg->mmap_buf, cfg->rand_buf, cfg->mmap_len);

	if (cfg->verbose)
		fprintf(stderr, "    result: %d -- %m\n", ret);

	return ret;
}

static int get_phys_sector_size(int fd)
{
	int ret;
	ioctl(fd, BLKPBSZGET, &ret);

	return ret;
}

static int clear_caches(int fd, size_t offset, size_t len)
{
	// Just to be sure, we are a bit heavy handed here...

	if (fsync(fd))
		return errno;

	if (posix_fadvise(fd, offset, len, POSIX_FADV_DONTNEED))
		return errno;

	if (fsync(fd))
		return errno;

	return 0;
}

static int test_read_submit_io(struct config *cfg)
{
	int phys_sector_size;
	int fd = open(cfg->nvme_device, O_RDONLY);
	if (fd < 0)
		return report(cfg, "test_read_submit_io (open)", errno);

	if (clear_caches(fd, cfg->lba * phys_sector_size, cfg->mmap_len))
		return report(cfg, "test_read_submit_io (clear_caches)", errno);

	phys_sector_size = get_phys_sector_size(fd);

	struct nvme_user_io iocmd = {
		.opcode = nvme_cmd_read,
		.slba = cfg->lba,
		.nblocks = cfg->mmap_len / phys_sector_size - 1,
		.addr = (__u64) cfg->mmap_buf,
		.metadata = 0,
	};

	if (cfg->verbose)
		fprintf(stderr, "-- Testing NVME read submit_io ioctl on %s:\n",
			cfg->nvme_device);

	memset(cfg->mmap_buf, 0xaa, cfg->mmap_len);
	int status = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &iocmd);

	if (cfg->verbose)
		fprintf(stderr, "    ioctl result: %d -- %m\n", status);

	char *buf = malloc(cfg->mmap_len);
	if (buf == NULL)
		return report(cfg, "test_read_submit_io (malloc)", errno);

	lseek(fd, cfg->lba*phys_sector_size, SEEK_SET);
	if (read(fd, buf, cfg->mmap_len) != cfg->mmap_len)
		return report(cfg, "test_read_submit_io (read)", errno);

	close(fd);

	return compare_buf((void *)buf, cfg->mmap_buf, cfg->mmap_len);
}

static int test_write_submit_io(struct config *cfg)
{
	int ret;
	int phys_sector_size;
	char oldbuf[cfg->mmap_len];
	char buf[cfg->mmap_len];

	int fd = open(cfg->nvme_device, O_RDWR);
	if (fd < 0)
		return report(cfg, "test_write_submit_io (open)", errno);

	if (clear_caches(fd, cfg->lba * phys_sector_size, cfg->mmap_len))
		return report(cfg, "test_write_submit_io (clear_caches)", errno);

	phys_sector_size = get_phys_sector_size(fd);

	lseek(fd, cfg->lba * phys_sector_size, SEEK_SET);
	if (read(fd, oldbuf, cfg->mmap_len) != cfg->mmap_len)
		return report(cfg, "test_write_submit_io (read)", errno);

	memcpy(cfg->mmap_buf, cfg->rand_buf, cfg->mmap_len);

	struct nvme_user_io iocmd = {
		.opcode = nvme_cmd_write,
		.slba = cfg->lba,
		.nblocks = cfg->mmap_len / phys_sector_size - 1,
		.addr = (__u64) cfg->mmap_buf,
		.metadata = 0,
	};

	if (cfg->verbose)
		fprintf(stderr, "-- Testing NVME write submit_io ioctl on %s:\n",
			cfg->nvme_device);


	int status = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &iocmd);

	if (cfg->verbose)
		fprintf(stderr, "    ioctl result: %d -- %m\n", status);

	if (status)
		return status;

	close(fd);

	fd = open(cfg->nvme_device, O_RDWR);
	if (fd < 0)
		return report(cfg, "test_write_submit_io (open2)", errno);

	if (clear_caches(fd, cfg->lba * phys_sector_size, cfg->mmap_len))
		return report(cfg, "test_write_submit_io (clear_caches)", errno);

	lseek(fd, cfg->lba * phys_sector_size, SEEK_SET);
	if (read(fd, buf, cfg->mmap_len) != cfg->mmap_len) {
		ret = report(cfg, "test_write_submit_io (read)", errno);
		goto write_back;
	}

	ret = compare_buf(buf, cfg->mmap_buf, cfg->mmap_len);

write_back:
	lseek(fd, cfg->lba * phys_sector_size, SEEK_SET);
	write(fd, oldbuf, cfg->mmap_len);
	fsync(fd);
	close(fd);
	return ret;
}

static int test_odirect(struct config *cfg)
{

	char *buf = cfg->mmap_buf;

	if (cfg->verbose)
		fprintf(stderr, "-- Testing O_DIRECT:\n");

	int fd = open(cfg->odirect_file, O_RDWR | O_DIRECT);
	if (fd < 0)
		return report(cfg, "test_odirect (open)", errno);

	memcpy(buf, cfg->rand_buf, cfg->mmap_len);

	if (write(fd, buf, cfg->mmap_len) == -1)
		return report(cfg, "test_odirect (write)", errno);

	memset(buf, 0xAA, cfg->mmap_len);

	if (lseek(fd, SEEK_SET, 0) == -1)
		return report(cfg, "test_odirect (lseek)", errno);

	if (read(fd, buf, cfg->mmap_len) == -1)
		return report(cfg, "test_odirect (read)", errno);

	close(fd);

	int ret = compare_buf(cfg->mmap_buf, cfg->rand_buf, cfg->mmap_len);

	if (cfg->verbose)
		fprintf(stderr, "    result: %d -- %m\n", ret);

	return ret;
}

int main(int argc, char *argv[])
{
	struct config cfg;
	unsigned failed = 0;

	int args = argconfig_parse(argc, argv, program_desc,
				   command_line_options,
				   &defaults, &cfg, sizeof(cfg));

	if (cfg.seed < 0)
		cfg.seed = time(NULL);
	srand(cfg.seed);
	if (cfg.verbose)
		fprintf(stderr, "INFO: seed = %d.\n", cfg.seed);

	if (!cfg.mmap_file) {
		fprintf(stderr, "ERROR: No mmap file specified, "
			"use the -mmap option\n");
		return 1;
	}

	if (cfg.do_write_submit_io && cfg.lba < 20) {
		fprintf(stderr, "ERROR: won't write on LBA's less than 20 to "
			"avoid issues with mounted filesystems.\n");
		return 1;
	}

	/*
	 * Generate a buffer full of random data in this processes VMA. We
	 * will use this is a few places to test data movement.
	 */
	if (create_random_buf(&cfg)) {
		failed = 1;
		goto out_nofree;
	}

	/*
	 * mmap() the file specifed on the command line. The should have
	 * been created beforehand. We then run the mem_map IOCTL on the
	 * virtual address returned by mmap. If the mmaped file is backed
	 * by ZONE_DEVICE memory we will detect that in both dmesg and in
	 * this program.
	 */
	if (create_mmap(&cfg)) {
		failed = 1;
		goto out;
	}

	/*
	 * Test that we can read and write data via the virtual addresses
	 * provided by mmap.
	 */
	if (test_buf(&cfg)) {
		failed = 1;
		goto out;
	}

	/*
	 * Now open a file on a block device in O_DIRECT mode. Ideally
	 * this should be on a NVMe SSD. Check to see if we can read from and
	 * write too this file from our mmaped buffer. Again this will fail
	 * on kernels that do not back IOPMEM with struct page.
	 */
	if (cfg.odirect_file && test_odirect(&cfg)) {
		failed = 1;
		goto out;
	}

	/*
	 * Test that we can issue an NVMe read IO against the mmapped virtual
	 * address range. Note that without struct page backing this IO will
	 * fail.
	 */
	if (test_read_submit_io(&cfg)) {
		failed = 1;
		goto out;
	}

	/*
	 * Test that we can issue an NVMe write IO against the mmapped virtual
	 * address range. Note that without struct page backing this IO will
	 * fail.
	 */
	if (cfg.do_write_submit_io && test_write_submit_io(&cfg)) {
		failed = 1;
		goto out;
	}

out:
	free(cfg.rand_buf);
out_nofree:
	fprintf(stderr, "\n\n");
	if (failed)
		return report(&cfg, "Failed", -1);
	else
		return report(&cfg, "Passed", 0);
}
