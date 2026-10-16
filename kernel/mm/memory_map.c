#include "memory_map.h"

#include <limits.h>

static bool ranges_overlap(uint64_t left_start, uint64_t left_end,
                           uint64_t right_start, uint64_t right_end)
{
    return left_start < right_end && right_start < left_end;
}

bool orion_memory_map_descriptor(const orion_boot_info *boot_info,
                                 uint32_t index,
                                 orion_uefi_memory_descriptor *descriptor)
{
    if (!orion_boot_info_is_valid(boot_info) || descriptor == NULL ||
        boot_info->descriptor_size <
            sizeof(orion_uefi_memory_descriptor) ||
        boot_info->descriptor_size % sizeof(uint64_t) != 0 ||
        (uintptr_t)boot_info->memory_map >
            UINTPTR_MAX - boot_info->memory_map_size ||
        boot_info->descriptor_size > UINT64_MAX / ((uint64_t)index + 1U)) {
        return false;
    }

    const uint64_t offset = (uint64_t)index * boot_info->descriptor_size;
    if (offset > boot_info->memory_map_size ||
        boot_info->descriptor_size > boot_info->memory_map_size - offset) {
        return false;
    }

    const uint8_t *bytes = (const uint8_t *)boot_info->memory_map + offset;
    const orion_uefi_memory_descriptor *source =
        (const orion_uefi_memory_descriptor *)(const void *)bytes;
    *descriptor = *source;
    return true;
}

bool orion_memory_map_validate(const orion_boot_info *boot_info,
                               orion_memory_map_summary *summary)
{
    if (!orion_boot_info_is_valid(boot_info) ||
        boot_info->descriptor_size < sizeof(orion_uefi_memory_descriptor) ||
        boot_info->descriptor_size % sizeof(uint64_t) != 0) {
        return false;
    }

    const uint64_t count64 =
        boot_info->memory_map_size / boot_info->descriptor_size;
    if (count64 == 0 || count64 > ORION_MEMORY_MAP_MAX_DESCRIPTORS) {
        return false;
    }

    orion_memory_map_summary result = {0};
    result.descriptor_count = (uint32_t)count64;
    for (uint32_t index = 0; index < result.descriptor_count; ++index) {
        orion_uefi_memory_descriptor current;
        if (!orion_memory_map_descriptor(boot_info, index, &current) ||
            current.number_of_pages == 0 ||
            (current.physical_start & (ORION_MEMORY_PAGE_SIZE - 1U)) != 0 ||
            current.number_of_pages >
                (UINT64_MAX - current.physical_start) / ORION_MEMORY_PAGE_SIZE) {
            return false;
        }

        const uint64_t current_end =
            current.physical_start +
            current.number_of_pages * ORION_MEMORY_PAGE_SIZE;
        if (current_end > result.maximum_physical_end) {
            result.maximum_physical_end = current_end;
        }

        if (current.type == ORION_UEFI_MEMORY_TYPE_CONVENTIONAL) {
            ++result.conventional_descriptor_count;
            if (current.physical_start < UINT64_C(0x100000000)) {
                const uint64_t clipped_end =
                    current_end < UINT64_C(0x100000000)
                        ? current_end
                        : UINT64_C(0x100000000);
                result.conventional_page_count_below_limit +=
                    (clipped_end - current.physical_start) /
                    ORION_MEMORY_PAGE_SIZE;
            }
        }

        for (uint32_t prior_index = 0; prior_index < index; ++prior_index) {
            orion_uefi_memory_descriptor prior;
            if (!orion_memory_map_descriptor(boot_info, prior_index, &prior)) {
                return false;
            }
            const uint64_t prior_end =
                prior.physical_start +
                prior.number_of_pages * ORION_MEMORY_PAGE_SIZE;
            if (ranges_overlap(current.physical_start, current_end,
                               prior.physical_start, prior_end)) {
                return false;
            }
        }
    }

    if (summary != NULL) {
        *summary = result;
    }
    return result.conventional_page_count_below_limit != 0;
}
