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
#include <argconfig/report.h>

#include <linux/nvme.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

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
	unsigned long mmap_len;
	unsigned long iterations;
	void *rand_buf;
	void *mmap_buf;
	int threads;
};

static const struct config defaults = {
	.mmap_len = 8192,
	.iterations = 10,
	.threads = 1,
};

static const struct argconfig_commandline_options command_line_options[] = {
	{"mmap", "FILE", CFG_STRING, &defaults.mmap_file, required_argument,
			"path to the file to mmap()"},

	{"odirect", "FILE", CFG_STRING, &defaults.odirect_file, required_argument,
			"path to the O_DIRECT file to use"},

	{"i", "NUM", CFG_LONG, &defaults.iterations, required_argument,
			"number of io operations to do"},

	{"m", "NUM", CFG_LONG_SUFFIX, &defaults.mmap_len, required_argument,
			NULL},
	{"mmap_len", "NUM", CFG_LONG_SUFFIX, &defaults.mmap_len, required_argument,
			"length of mmap to use"},

	{"t", "NUM", CFG_POSITIVE, &defaults.threads, required_argument,
			"number of threads to use"},

	{0}
};

static int report(struct config *cfg, const char *func, int val)
{
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

static int create_mmap(struct config *cfg)
{
	void *buf;
	int tfd = open(cfg->mmap_file, O_RDWR);
	if (tfd < 0)
		return report(cfg, "create_mmap (open)", errno);

	buf = mmap(NULL, cfg->mmap_len*cfg->threads,
		   PROT_READ | PROT_WRITE, MAP_SHARED,
		   tfd, 0);
	if (buf == MAP_FAILED)
		return report(cfg, "create_mmap (mmap)", errno);
	close(tfd);
	cfg->mmap_buf = buf;

	return 0;
}

static int perf_test(char *buf, unsigned seek, struct config *cfg, int do_write,
		     size_t *count_result)
{
	int fd = open(cfg->odirect_file, O_RDWR | O_DIRECT);
	if (fd < 0)
		return report(cfg, "perf_test (open)", errno);

	lseek(fd, seek, SEEK_SET);

	memcpy(buf, cfg->rand_buf, cfg->mmap_len);

	size_t count = 0;

	for (int i = 0; i < cfg->iterations; i++) {
		size_t r;

		if (do_write)
			r = write(fd, buf, cfg->mmap_len);
		else
			r = read(fd, buf, cfg->mmap_len);

		if (r < 0)
			return report(cfg, "perf_test (io)", errno);

		count += r;
	}

	*count_result = count;

	return 0;
}

struct thread_args {
	pthread_t pthread;
	struct config *cfg;
	int do_write;
	size_t bytes_txfrd;
	void *mmap_buf;
	unsigned seek;
};

static void *thread(void *args)
{
	struct thread_args *targs = args;
	perf_test(targs->mmap_buf, targs->seek, targs->cfg,
		  targs->do_write, &targs->bytes_txfrd);
}

static int start_threaded_test(struct config *cfg, int do_write)
{
	struct timeval start_time, end_time;
	struct thread_args ts[cfg->threads];
	size_t total_bytes = 0;

	fprintf(stderr, "Testing %s performance\n",
		do_write ? "write" : "read");


	for (int i = 0; i < cfg->threads; i++) {
		ts[i].cfg = cfg;
		ts[i].do_write = do_write;
		ts[i].mmap_buf = ((unsigned char *)cfg->mmap_buf) + i*cfg->mmap_len;
		ts[i].seek = i*cfg->mmap_len;
	}

	gettimeofday(&start_time, NULL);
	for (int i = 0; i < cfg->threads; i++)
		pthread_create(&ts[i].pthread, NULL, thread, &ts[i]);


	for (int i = 0; i < cfg->threads; i++) {
		pthread_join(ts[i].pthread, NULL);
		total_bytes += ts[i].bytes_txfrd;
	}

	gettimeofday(&end_time, NULL);

	fprintf(stderr, "  ");
	report_transfer_bin_rate(stderr, &start_time, &end_time, total_bytes);
	fprintf(stderr, "  \n");
}

int main(int argc, char *argv[])
{
	struct config cfg;
	unsigned failed = 0;

	int args = argconfig_parse(argc, argv, program_desc,
				   command_line_options,
				   &defaults, &cfg, sizeof(cfg));

	srand(time(NULL));

	if (!cfg.mmap_file) {
		fprintf(stderr, "ERROR: No mmap file specified, "
			"use the -mmap option\n");
		return 1;
	}

	if (!cfg.odirect_file) {
		fprintf(stderr, "ERROR: No O_DIRECT file specified, "
			"use the -odirect option\n");
		return 1;
	}

	fprintf(stderr, "IO Size: %d\n", cfg.mmap_len);
	fprintf(stderr, "Threads: %d\n", cfg.threads);


	/*
	 * Generate a buffer full of random data in this processes VMA. We
	 * will use this is a few places to test data movement.
	 */
	if (create_random_buf(&cfg)) {
		failed = 1;
		goto out;
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

	start_threaded_test(&cfg, 0);
	start_threaded_test(&cfg, 1);


	fprintf(stderr, "\n");
out:
	return failed;
}
