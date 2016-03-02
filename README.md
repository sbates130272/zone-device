# ZONE_DEVICE Testing Environment

Code for testing and experimenting with the ZONE_DEVICE aspects of the
Linux kernel.

# Introduction

This repository contains code for testing the ZONE_DEVICE
functionality that has recently been added to the Linux kernel [1]. It is
also used to test a potential patchset to the kernel allowing IOMEM to
be included in the ZONE_DEVICE framework [2]

# Installing and Running

  ```
  ./runqemu -pv ../zone-device/kernels/bzImage-zone-device
  ```

# Running in QEMU Environment

We performed testing on a QEMU machine using Keith Busch's fork of
QEMU which implements an NVMe device [3]. The testing was done on a
monolithic kernel based on 4.5-rc4 and our patch and the bzImage for
this kernel can be found in the kernel subfolder.

The root system was a minimal debian system based on the one that can
be found here [4]. Both a PMEM and IOPMEM memory device were
created. The PMEM device was created using the -p switch to the
./runqemu script. You might need to tweak the memory settings to get
this to work on your system.

The IOPMEM device was created using a NVMe instance with a BAR
exposed, unbinding the NVMe driver from this device and then binding
the IOPMEM driver to the device. Note the IOPMEM driver is heavily
based of a slighly older version of the current PMEM driver [5].

# Running on Hardware

We performed testing on an x86_64 platform which included a range of
PCIe devices either directly connected to the CPU root-complex or
attached behind a PCIe switch. These PCIe devices included:

1. NVMe SSDs (both NAND and DRAM backed).
2. RDMA NICs (RoCe, IB and IWarp).
3. GPGPUs from Nvidia.

A similar approach was taken to testing SW as per the QEMU environment
mentioned above except a non-monolithic kernel was used.

# References

[1] https://lists.01.org/pipermail/linux-nvdimm/2015-August/001810.html
[2] https://github.com/sbates130272/linux-donard/tree/nvme-cmb-rfc-v0
[3] git://git.infradead.org/users/kbusch/qemu-nvme.git
[4] https://github.com/sbates130272/qemu-minimal.git
[5] https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/drivers/nvdimm/pmem.c?id=refs/
