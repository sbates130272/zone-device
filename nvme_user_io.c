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

#define _GNU_SOURCE

#include <linux/nvme.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct config {
    char          *mmap_file;
    char          *odirect_file;
    unsigned long lba;
    void          *buf;
};

static void test_mem_map_gup(struct config *cfg)
{
    fprintf(stderr, "-- Testing mem_map GUP:\n");

    int fd = open("/dev/mem_map", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "    Could not open /dev/mem_map: %m\n");
        return;
    }

    unsigned long ret = ioctl(fd, 0, cfg->buf);
    fprintf(stderr, "    mem_map result: %lx, %m\n", ret);

    close(fd);
}

static void test_submit_io(struct config *cfg)
{
    char meta[8192];

    struct nvme_user_io iocmd = {
        .opcode = nvme_cmd_read,
        .slba = cfg->lba,
        .nblocks = 0,
        .addr = (__u64) cfg->buf,
        .metadata = 0,//(__u64) meta,
    };

    fprintf(stderr, "-- Testing NVME submit_io ioctl:\n");

    int fd = open("/dev/nvme0n1", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "    Could not open nvme device: %m\n");
        return;
    }

    int status = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &iocmd);

    fprintf(stderr, "    ioctl result: %d -- %m\n", status);

    close(fd);

    fwrite(cfg->buf, 128, 1, stdout);

}

static void test_odirect(struct config *cfg)
{

    unsigned char *buf = cfg->buf;

    fprintf(stderr, "-- Testing O_DIRECT:\n");

    int fd = open(cfg->odirect_file, O_RDWR | O_DIRECT);
    if (fd < 0) {
        fprintf(stderr, "    Could not open odirect file (%s): %m\n",
                cfg->odirect_file);
        return;
    }

    fprintf(stderr, "    read result: %d, %m\n", read(fd, &buf[4096], 4096));

    fwrite(&buf[4096], 128, 1, stdout);

    memset(&buf[8192], 0xaa, 4096);
    fprintf(stderr, "    write result: %d, %m\n", write(fd, &buf[8192], 4096));

    close(fd);
}

static void *create_test_mmap(struct config *cfg)
{
    void * buf;
    int tfd = open(cfg->mmap_file, O_RDWR);
    if (tfd < 0) {
        fprintf(stderr, "ERROR: Could not open test file: %m\n");
        exit(-1);
    }

    buf = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, tfd, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "ERROR: Could not map file: %m\n");
        exit(-1);
    }

    close(tfd);

    return buf;

}

static void test_read_write_buf(struct config *cfg)
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
}

int main(int argc, char *argv[])
{
    struct config cfg = {
        .lba = 0,
    };

    if (argc == 4) {
        cfg.lba = strtol(argv[3], NULL, 0);
    } else if (argc != 3) {
        fprintf(stderr, "USAGE: %s MMAP_FILE ODIRECT_FILE LBA.\n", argv[0]);
        exit(-1);
    }

    cfg.mmap_file    = argv[1];
    cfg.odirect_file = argv[2];

    cfg.buf = create_test_mmap(&cfg);
    test_read_write_buf(&cfg);
    test_mem_map_gup(&cfg);
    test_submit_io(&cfg);
    test_odirect(&cfg);

    fprintf(stderr, "\n\n");

    return 0;
}
