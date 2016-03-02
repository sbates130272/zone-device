# zone-device
Code for testing and experimenting with the ZONE_DEVICE aspects of the Linux kernel

# Running in QEMU Environment

We performed testing on a QEMU machine using Keith Busch's fork of
QEMU which implements an NVMe device [X]. The testing was done on a
monolithic kernel based on 4.5-rc4 and our patch. The root system was
a minimal debian system based on the one that can be found here
[Y]. Both a PMEM and IOPMEM memory device were created. The PMEM
device was created using the -p switch to the ./runqemu script in
[Y]. The IOPMEM device was created using a NVMe instance with a BAR
exposed, unbinding the NVMe driver from this device and then binding
the IOPMEM driver to the device. Note the IOPMEM driver is heavily
based of a slighly older version of the current PMEM driver.

We provide a small GitHub respositorty with some simple test scripts
and submodules to the IOPMEM driver here [Z]. Refer to the README.md
therein for more information on running and testing this setup.

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
