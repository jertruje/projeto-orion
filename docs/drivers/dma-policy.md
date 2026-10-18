# DMA policy

DMA is not enabled by this driver framework. The PCI scanner performs
configuration reads only and never sets the PCI bus-master bit. There is no
DMA-safe allocator, pinning/lifetime contract, cache-coherency API, IOMMU
configuration, device address-width negotiation or teardown proof.

Do not pass kernel virtual addresses to devices as DMA addresses. A future DMA
driver must establish physical ownership, alignment, address-width bounds,
coherency requirements, IOMMU mappings where available, device quiescence and
safe reclamation after failure before enabling bus mastering.
