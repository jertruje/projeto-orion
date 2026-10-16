# UEFI boot memory map

`orion_boot_info` version 1 carries the final UEFI memory-map pointer, byte
length, and descriptor stride across `ExitBootServices`. The loader's static
256 KiB map buffer and loader-owned structures remain reserved because only
UEFI type 7 (`EfiConventionalMemory`) is made allocatable.

The kernel validates the boot-info signature/version, descriptor-size
minimum/alignment, integral descriptor count (1–1024), pointer arithmetic,
nonzero page counts, 4 KiB physical-start alignment, range overflow, and
pairwise descriptor overlap. The first 40 bytes of each descriptor are
consumed; larger UEFI descriptor strides are accepted. Malformed maps fail
closed and stop kernel initialization.

Only conventional ranges below 4 GiB contribute pages to the physical
allocator. Every other UEFI type, and conventional memory above the cap,
remains unavailable. This excludes the loaded kernel segments, loader image,
boot-info/map storage, and firmware-owned ranges without relying on hard-coded
kernel addresses. UEFI boot-services memory is conservatively retained even
though it may be reclaimable after `ExitBootServices`.

The contract still does not carry a UEFI descriptor-version field or a
separate inventory of each loader allocation. This version relies on UEFI
memory types and does not support arbitrary non-UEFI bootloaders.
