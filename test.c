////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Microsemi Corporation, Inc.
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
    void          *buf;
    unsigned      verbose;
};

static const struct config defaults = {
    .nvme_device = "/dev/nvme0n1",
    .lba         = 0,
    .mmap_len    = 8192,
    .seed        = -1,
    .verbose     = 0,
};

static const struct argconfig_commandline_options command_line_options[] = {
    {"nmve",       "STRING", CFG_STRING, &defaults.nvme_device, required_argument,
            "path to the NVMe block device to use"},
    {"mmap",       "STRING", CFG_STRING, &defaults.mmap_file, required_argument,
            "path to the file to mmap()"},
    {"odirect",    "STRING", CFG_STRING, &defaults.odirect_file, required_argument,
            "path to the O_DIRECT file to use"},
    {"m",             "NUM", CFG_LONG_SUFFIX, &defaults.mmap_len, required_argument, NULL},
    {"mmap_len",      "NUM", CFG_LONG_SUFFIX, &defaults.mmap_len, required_argument,
            "length of mmap to use"},
    {0}
};


static int test_mem_map_gup(struct config *cfg)
{
    fprintf(stderr, "-- Testing mem_map GUP:\n");

    int fd = open("/dev/mem_map", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "    Could not open /dev/mem_map: %m\n");
        return errno;
    }

    int pfn = ioctl(fd, 0, cfg->buf);
    int isdevice = ioctl(fd, 1, cfg->buf);
    fprintf(stderr, "    mem_map result: %lx, (%s) %m\n", pfn,
            isdevice ? "device" : "not device");

    close(fd);
    return 0;
}

static int test_submit_io(struct config *cfg)
{
    char meta[8192];

    struct nvme_user_io iocmd = {
        .opcode = nvme_cmd_read,
        .slba = cfg->lba,
        .nblocks = 0,
        .addr = (__u64) cfg->buf,
        .metadata = 0,//(__u64) meta,
    };

    fprintf(stderr, "-- Testing NVME submit_io ioctl on %s:\n",
        cfg->nvme_device);

    int fd = open(cfg->nvme_device, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "    Could not open nvme device: %m\n");
        return errno;
    }

    int status = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &iocmd);

    fprintf(stderr, "    ioctl result: %d -- %m\n", status);

    close(fd);

    fwrite(cfg->buf, 128, 1, stdout);

    return 0;

}

static int test_odirect(struct config *cfg)
{

    unsigned char *buf = cfg->buf;

    fprintf(stderr, "-- Testing O_DIRECT:\n");

    int fd = open(cfg->odirect_file, O_RDWR | O_DIRECT);
    if (fd < 0) {
        fprintf(stderr, "    Could not open odirect file (%s): %m\n",
                cfg->odirect_file);
        return errno;
    }

    fprintf(stderr, "    read result: %d, %m\n", read(fd, &buf[4096], 4096));

    fwrite(&buf[4096], 128, 1, stdout);

    memset(&buf[8192], 0xaa, 4096);
    fprintf(stderr, "    write result: %d, %m\n", write(fd, &buf[8192], 4096));

    close(fd);
    return 0;
}

static int create_test_mmap(struct config *cfg)
{
    void * buf;
    int tfd = open(cfg->mmap_file, O_RDWR);
    if (tfd < 0) {
        fprintf(stderr, "ERROR: Could not open test file: %m\n");
        return errno;
    }

    buf = mmap(NULL, cfg->mmap_len, PROT_READ | PROT_WRITE, MAP_SHARED, tfd, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "ERROR: Could not map file: %m\n");
        return errno;
    }

    close(tfd);

    cfg->buf = buf;

    return 0;

}

static int test_read_write_buf(struct config *cfg)
{
    int i;
    unsigned int *ibuf = (unsigned int *) cfg->buf;

    fprintf(stderr, "-- Testing mmap read/write:\n");

    fprintf(stderr, "    Read: ");
    for (i = 0; i < 4; i++)
        fprintf(stderr, " %08x", ibuf[i]);
    fprintf(stderr, "\n");

    strcpy((unsigned char *)cfg->buf, "876543210");

    fprintf(stderr, "    Wrote:");
    for (i = 0; i < 4; i++)
        fprintf(stderr, " %08x", ibuf[i]);
    fprintf(stderr, "\n");

    return 0;
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
    fprintf(stderr,"INFO: seed = %d.\n", cfg.seed);

    /*
     * mmap() the file specifed on the command line. The should have
     * been created beforehand. We then run the mem_map IOCTL on the
     * virtual address returned by mmap. If the mmaped file is backed
     * by ZONE_DEVICE memory we will detect that in both dmesg and in
     * this program.
     */
    if ( create_test_mmap(&cfg) ) {
        failed = 1;
        goto out;
    }
    if ( test_mem_map_gup(&cfg) ) {
        failed = 1;
        goto out;
    }

    /*
     * Test that we can read and write data via the virtual addresses
     *provided by mmap. This is a pretty trivial test.
     */
    if ( test_read_write_buf(&cfg) ) {
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
    fprintf(stderr, "\n\n");
    if ( failed ){
        if (cfg.verbose)
            fprintf(stderr,"Failed\n");
        return -1;
    } else {
        if (cfg.verbose)
            fprintf(stderr,"Passed\n");
        return 0;
    }
}
