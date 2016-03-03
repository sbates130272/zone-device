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

#include <linux/nvme.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "../argconfig/argconfig.h"

const char program_desc[] =
    "Simple program for testing ZONE_DEVICE devices.";

struct config {
    char          *mmap_file;
    char          *odirect_file;
    char          *nvme_device;
    unsigned long lba;
    int           mmap_len;
    int           seed;
    void          *rand_buf;
    void          *mmap_buf;
    unsigned      zone_device;
    unsigned      verbose;
};

static const struct config defaults = {
    .nvme_device = "/dev/nvme0n1",
    .lba         = 0,
    .mmap_len    = 8192,
    .seed        = -1,
    .zone_device = 0,
    .verbose     = 0,
};

static const struct argconfig_commandline_options command_line_options[] = {
    {"nvme",       "STRING", CFG_STRING, &defaults.nvme_device, required_argument,
            "path to the NVMe block device to use"},
    {"mmap",       "STRING", CFG_STRING, &defaults.mmap_file, required_argument,
            "path to the file to mmap()"},
    {"odirect",    "STRING", CFG_STRING, &defaults.odirect_file, required_argument,
            "path to the O_DIRECT file to use"},
    {"m",             "NUM", CFG_LONG_SUFFIX, &defaults.mmap_len, required_argument, NULL},
    {"mmap_len",      "NUM", CFG_LONG_SUFFIX, &defaults.mmap_len, required_argument,
            "length of mmap to use"},
    {"z",             "", CFG_NONE, &defaults.zone_device, no_argument, NULL},
    {"zone_device",   "", CFG_NONE, &defaults.zone_device, no_argument,
            "mmap()ed file must reside in ZONE_DEVICE"},
    {"v",             "", CFG_NONE, &defaults.verbose, no_argument, NULL},
    {"verbose",       "", CFG_NONE, &defaults.verbose, no_argument,
            "be verbose"},
    {0}
};

static int report(struct config *cfg, const char *func, int val) {

    if ( cfg->verbose )
        fprintf(stderr,"%s: %d = %s.\n", func, errno, strerror(errno));

    return val;
}

static int create_random_buf(struct config *cfg)
{
    char *buf = malloc(cfg->mmap_len);
    if ( buf == NULL )
        return errno;
    for ( unsigned i=0; i<cfg->mmap_len; i++ )
        buf[i] = (char)rand();

    cfg->rand_buf = (void *)buf;

    return 0;
}

static int compare_buf(void *a, void *b, size_t len)
{
    char *ac = (char *)a, *bc = (char *)b;

    for ( unsigned i=0; i<len; i++ )
        if (ac[i] != bc[i])
            return i+1;

    return 0;
}

static int test_mem_map(struct config *cfg)
{
    if ( cfg->verbose )
        fprintf(stderr, "-- Testing mem_map GUP:\n");

    int fd = open("/dev/mem_map", O_RDWR);
    if ( fd < 0 )
        return report(cfg, "test_mem_map", errno);

    int pfn      = ioctl(fd, 0, cfg->mmap_buf);
    int isdevice = ioctl(fd, 1, cfg->mmap_buf);
    if (cfg->verbose)
        fprintf(stderr, "    mem_map result: %lx, (%s) %m\n", pfn,
                isdevice ? "device" : "not device");

    close(fd);

    if ( (cfg->zone_device && !isdevice) ||
         (!cfg->zone_device && isdevice) ) {
            if (cfg->verbose)
                fprintf(stderr, "    Memory zone type mis-match!\n");
        return 1;
    }
    return 0;
}

static int test_submit_io(struct config *cfg)
{
    char meta[8192];

    struct nvme_user_io iocmd = {
        .opcode = nvme_cmd_read,
        .slba = cfg->lba,
        .nblocks = 0,
        .addr = (__u64) cfg->mmap_buf,
        .metadata = 0,//(__u64) meta,
    };

    if ( cfg->verbose )
        fprintf(stderr, "-- Testing NVME submit_io ioctl on %s:\n",
                cfg->nvme_device);

    int fd = open(cfg->nvme_device, O_RDONLY);
    if (fd < 0)
        return report(cfg, "test_submit_io", errno);

    int status = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &iocmd);

    if (cfg->verbose)
        fprintf(stderr, "    ioctl result: %d -- %m\n", status);

    close(fd);

    fwrite(cfg->mmap_buf, 128, 1, stdout);

    return 0;

}

static int test_odirect(struct config *cfg)
{

    unsigned char *buf = cfg->mmap_buf;

    if (cfg->verbose)
        fprintf(stderr, "-- Testing O_DIRECT:\n");

    int fd = open(cfg->odirect_file, O_RDWR | O_DIRECT);
    if (fd < 0)
        return report(cfg, "test_odirect", errno);

    if (cfg->verbose)
        fprintf(stderr, "    read result: %d, %m\n", read(fd, &buf[4096], 4096));

    fwrite(&buf[4096], 128, 1, stdout);

    memset(&buf[8192], 0xaa, 4096);
    if (cfg->verbose)
        fprintf(stderr, "    write result: %d, %m\n", write(fd, &buf[8192], 4096));

    close(fd);
    return 0;
}

static int create_mmap(struct config *cfg)
{
    void * buf;
    int tfd = open(cfg->mmap_file, O_RDWR);
    if (tfd < 0) {
        fprintf(stderr, "ERROR: Could not open test file: %m\n");
        return errno;
    }

    buf = mmap(NULL, cfg->mmap_len, PROT_READ | PROT_WRITE, MAP_SHARED, tfd, 0);
    if (buf == MAP_FAILED)
        return report(cfg, "create_mmap", errno);

    close(tfd);

    cfg->mmap_buf = buf;

    return 0;

}

static int test_buf(struct config *cfg)
{
    unsigned char *a = (unsigned char *) cfg->mmap_buf,
        *b = (unsigned char *) cfg->rand_buf;

    if (cfg->verbose)
        fprintf(stderr, "-- Testing mmap read/write:\n");

    for (unsigned i = 0; i<cfg->mmap_len; i++)
        a[i] = b[i];

    return compare_buf(cfg->mmap_buf, cfg->rand_buf, cfg->mmap_len);
}

int main(int argc, char *argv[])
{
    struct config cfg;
    unsigned failed = 0;

    int args = argconfig_parse(argc, argv, program_desc, command_line_options,
                               &defaults, &cfg, sizeof(cfg));

    if (  cfg.seed < 0 )
        cfg.seed = time(NULL);
    srand(cfg.seed);
    if ( cfg.verbose )
        fprintf(stderr,"INFO: seed = %d.\n", cfg.seed);

    /*
     * Generate a buffer full of random data in this processes VMA. We
     * will use this is a few places to test data movement.
     */
    if ( create_random_buf(&cfg) ) {
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
    if ( create_mmap(&cfg) ) {
        failed = 1;
        goto out;
    }
    if ( test_mem_map(&cfg) ) {
        failed = 1;
        goto out;
    }

    /*
     * Test that we can read and write data via the virtual addresses
     * provided by mmap. This is a pretty trivial test.
     */
    if ( test_buf(&cfg) ) {
        failed = 1;
        goto out;
    }

    /*
     * Test that we can issue a NVMe IO against the mmapped virtual
     * address range. Note that without struct page backing this IO will
     * fail.
    */
    if ( test_submit_io(&cfg) ) {
        failed = 1;
        goto out;
    }

    /*
     * Now open a file on a block device in O_DIRECT mode. Ideally
     * this should be on a NVMe SSD. Check to see if we can read from and
     * write too this file from our mmaped buffer. Again this will fail
     * on kernels that do not back IOPMEM with struct page.
     */
    if ( test_odirect(&cfg) )
        failed = 1;


out:
    free(cfg.rand_buf);
    fprintf(stderr, "\n\n");
    if ( failed )
        return report(&cfg, "Failed", -1);
    else
        return report(&cfg, "Passed", 0);
}
