#define _GNU_SOURCE

#include <linux/nvme.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void test_mem_map_gup(void *buf)
{
    fprintf(stderr, "-- Testing mem_map GUP:\n");

    int fd = open("/dev/mem_map", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "    Could not open /dev/mem_map: %m\n");
        return;
    }

    unsigned long ret = ioctl(fd, 0, buf);
    fprintf(stderr, "    mem_map result: %lx, %m\n", ret);

    close(fd);
}

static void test_submit_io(void *buf, unsigned long lba)
{
    char meta[8192];

    struct nvme_user_io iocmd = {
        .opcode = nvme_cmd_read,
        .slba = lba,
        .nblocks = 0,
        .addr = (__u64) buf,
        .metadata = (__u64) meta,
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

    fwrite(buf, 128, 1, stdout);

}

static void test_odirect(const char *fname, unsigned char *buf)
{
    fprintf(stderr, "-- Testing O_DIRECT:\n");

    int fd = open(fname, O_RDWR | O_DIRECT);
    if (fd < 0) {
        fprintf(stderr, "    Could not open odirect file (%s): %m\n", fname);
        return;
    }

    fprintf(stderr, "    read result: %d, %m\n", read(fd, &buf[4096], 4096));

    fwrite(&buf[4096], 128, 1, stdout);

    memset(&buf[8192], 0xaa, 4096);
    fprintf(stderr, "    write result: %d, %m\n", write(fd, &buf[8192], 4096));

    close(fd);
}

static void *create_test_mmap(const char *fname)
{
    void * buf;
    int tfd = open(fname, O_RDWR);
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

static void test_read_write_buf(unsigned char *buf)
{
    int i;
    unsigned int *ibuf = (unsigned int *) buf;

    fprintf(stderr, "-- Testing mmap read/write:\n");

    fprintf(stderr, "    Read: ");
    for (i = 0; i < 4; i++)
        fprintf(stderr, " %08x", ibuf[i]);
    fprintf(stderr, "\n");

    strcpy((unsigned char *)buf, "876543210");

    fprintf(stderr, "    Wrote:");
    for (i = 0; i < 4; i++)
        fprintf(stderr, " %08x", ibuf[i]);
    fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
    unsigned char *buf;

    const char *fname, *odirect_file;
    unsigned long lba = 0;

    if (argc == 4) {
        lba = strtol(argv[3], NULL, 0);
    } else if (argc != 3) {
        fprintf(stderr, "USAGE: %s MMAP_FILE ODIRECT_FILE LBA.\n", argv[0]);
        exit(-1);
    }

    fname = argv[1];
    odirect_file = argv[2];

    buf = create_test_mmap(fname);
    test_read_write_buf(buf);
    test_mem_map_gup(buf);
    test_submit_io(buf, lba);
    test_odirect(odirect_file, buf);

    fprintf(stderr, "\n\n");

    return 0;
}
