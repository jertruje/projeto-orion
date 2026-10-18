#ifndef ORION_MEMORY_MAP_H
#define ORION_MEMORY_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <orion/boot_info.h>

#define ORION_MEMORY_PAGE_SIZE UINT64_C(4096)
#define ORION_MEMORY_PHYSICAL_LIMIT UINT64_C(0x100000000)
#define ORION_UEFI_MEMORY_TYPE_CONVENTIONAL UINT32_C(7)
#define ORION_MEMORY_MAP_MAX_DESCRIPTORS 1024U

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attributes;
} orion_uefi_memory_descriptor;

_Static_assert(sizeof(orion_uefi_memory_descriptor) ==
                    ORION_EFI_MEMORY_DESCRIPTOR_MIN_SIZE,
                "UEFI descriptor prefix layout changed");
_Static_assert(offsetof(orion_uefi_memory_descriptor, physical_start) == 8,
                "UEFI descriptor physical-start offset changed");
_Static_assert(offsetof(orion_uefi_memory_descriptor, number_of_pages) == 24,
                "UEFI descriptor page-count offset changed");

typedef struct {
    uint32_t descriptor_count;
    uint32_t conventional_descriptor_count;
    uint64_t conventional_page_count_below_limit;
    uint64_t maximum_physical_end;
} orion_memory_map_summary;

bool orion_memory_map_validate(const orion_boot_info *boot_info,
                               orion_memory_map_summary *summary);
bool orion_memory_map_descriptor(const orion_boot_info *boot_info,
                                 uint32_t index,
                                 orion_uefi_memory_descriptor *descriptor);
bool orion_memory_map_range_is_reserved(const orion_boot_info *boot_info,
                                        uint64_t physical_start,
                                        uint64_t size);
bool orion_memory_map_initialize_kernel_page_tables(const orion_boot_info *boot_info);

#endif
