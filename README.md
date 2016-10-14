# ZONE_DEVICE Testing Environment

Code for testing and experimenting with the ZONE_DEVICE aspects of the
Linux kernel.

# Introduction

This repository contains code for testing the ZONE_DEVICE
functionality that has recently been added to the Linux kernel [1]. It is
also used to test a potential patchset to the kernel allowing IOMEM to
be included in the ZONE_DEVICE framework [2]

# Running

Once you have downloaded this repo and initialized the submodules you
can do the following steps.

  1. Build the test C code and kernel modules:

  ```
  ./build
  ```
  2. If running on QEMU download the qemu-minimal repo [3] and run the
  following command. Note you will need to have ZONE_DEVICE and FS_DAX
  config options set in your kernel for IOPMEM and you will need to
  include additonal config options for PMEM too:

  ```
  ./runqemu -pv <path to zone-device repo>/kernels/bzImage-zone-device-qemu
  ```
  3. Inside the QEMU machine cd to the correct folder. Note the QEMU
  call maps the /home folder on the host machine to the /home folder
  on the guest machine

  ```
  cd <path to zone-device repo>
  ```
  4. Run the shell script that tests PMEM and IOPMEM functionality
  (note this should be run as root).
  ```
  ./run_test
  ```
  5. To perform a clean rerun of the tests run
  ```
  ./run_test -u && ./run_test
  ```

Note there are a range of options and arguments in run_test and you
should review those for your system. The current settings should work
in the QEMU environment but will almost certainly fail on real
hardware. BUS_ID will almost certianly have to be changed on a real
system to match your target IOPMEM device.

# Running in QEMU Environment

We performed testing on a QEMU machine using Keith Busch's fork of
QEMU which implements an NVMe device [4]. The testing was done on a
monolithic kernel based on [2] and the bzImage for this kernel can be
found in the kernel subfolder.

The root system was a minimal debian system based on the one that can
be found here [3]. Both a PMEM and IOPMEM memory device were
created. The devices were created using the -p switch to the
./runqemu script. You might need to tweak the memory settings to get
this to work on your system.

The IOPMEM device was created using a patch to QEMU [NEED REF]. Note
the IOPMEM driver is heavily based off the PMEM driver.

# Running on Hardware

We performed testing on an x86_64 platform which included a range of
PCIe devices either directly connected to the CPU root-complex or
attached behind a PCIe switch. These PCIe devices included:

1. NVMe SSDs (both NAND and DRAM backed).
2. RDMA NICs (RoCe, IB and IWarp).
3. GPGPUs from Nvidia.

A similar approach was taken to testing SW as per the QEMU environment
mentioned above except a non-monolithic kernel was used.

Using the IOPMEM framework we were able to achieve peer-to-peer writes
of 4GB/s and reads of 1.2GB/s. These numbers were the limits of our
hardware.

# Kernels

A kernels subfolder contains the bzImage for the QEMU kernel as well
as example configs for both QEMU and real hardware.

# References

[1] https://lists.01.org/pipermail/linux-nvdimm/2015-August/001810.html

[2] https://github.com/sbates130272/linux-donard/tree/iopmem-v0

[3] https://github.com/sbates130272/qemu-minimal.git

[4] git://git.infradead.org/users/kbusch/qemu-nvme.git
