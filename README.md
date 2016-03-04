# ZONE_DEVICE Testing Environment

Code for testing and experimenting with the ZONE_DEVICE aspects of the
Linux kernel.

# Introduction

This repository contains code for testing the ZONE_DEVICE
functionality that has recently been added to the Linux kernel [1]. It is
also used to test a potential patchset to the kernel allowing IOMEM to
be included in the ZONE_DEVICE framework [2]

# Running

  1. If running on QEMU download the qemu-minimal repo [3] and run the
  following command:

  ```
  ./runqemu -pv <path to zone-device repo>/kernels/bzImage-zone-device
  ```
  2. Inside the QEMU machine cd to the correct folder. Note the QEMU
  call maps the /home folder on the host machine to the /home folder
  on the guest machine

  ```
  cd <path to zone-device repo>
  ```
  3. Run the shell script that tests PMEM and IOPMEM functionality
  (note this should be run as root).
  ```
  ./run_test
  ```

Note there are a range of options and arguments in run_test and you
should review those for your system. The current settings should work
in the QEMU environment but will almost certainly fail on real
hardware. BUS_ID, VENDOR_ID, DEVICE_ID and BAR_ID will almost
certianly have to be changed on a real system to match your target
IOPMEM device.

# Running in QEMU Environment

We performed testing on a QEMU machine using Keith Busch's fork of
QEMU which implements an NVMe device [4]. The testing was done on a
monolithic kernel based on 4.5-rc4 and our patch and the bzImage for
this kernel can be found in the kernel subfolder.

The root system was a minimal debian system based on the one that can
be found here [3]. Both a PMEM and IOPMEM memory device were
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

[3] https://github.com/sbates130272/qemu-minimal.git

[4] git://git.infradead.org/users/kbusch/qemu-nvme.git

[5] https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/drivers/nvdimm/pmem.c?id=refs/
