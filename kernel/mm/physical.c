#include "physical.h"

#include <stddef.h>

static uint8_t available_bitmap[ORION_PHYSICAL_BITMAP_BYTES];
static uint8_t allocated_bitmap[ORION_PHYSICAL_BITMAP_BYTES];
static orion_physical_stats allocator_stats;
static bool allocator_initialized;

static bool bitmap_get(const uint8_t *bitmap, uint64_t page)
{
    return (bitmap[page / 8U] & (uint8_t)(1U << (page % 8U))) != 0;
}

static void bitmap_set(uint8_t *bitmap, uint64_t page, bool value)
{
    const uint8_t mask = (uint8_t)(1U << (page % 8U));
    if (value) {
        bitmap[page / 8U] |= mask;
    } else {
        bitmap[page / 8U] &= (uint8_t)~mask;
    }
}

bool orion_physical_init(const orion_boot_info *boot_info)
{
    allocator_initialized = false;
    orion_memory_map_summary summary;
    if (!orion_memory_map_validate(boot_info, &summary)) {
        allocator_initialized = false;
        return false;
    }

    for (size_t index = 0; index < ORION_PHYSICAL_BITMAP_BYTES; ++index) {
        available_bitmap[index] = 0;
        allocated_bitmap[index] = 0;
    }
    allocator_stats.total_pages = ORION_PHYSICAL_MAX_PAGES;
    allocator_stats.free_pages = 0;
    allocator_stats.allocated_pages = 0;
    allocator_stats.reserved_pages = ORION_PHYSICAL_MAX_PAGES;

    for (uint32_t index = 0; index < summary.descriptor_count; ++index) {
        orion_uefi_memory_descriptor descriptor;
        if (!orion_memory_map_descriptor(boot_info, index, &descriptor)) {
            allocator_initialized = false;
            return false;
        }
        if (descriptor.type != ORION_UEFI_MEMORY_TYPE_CONVENTIONAL ||
            descriptor.physical_start >= ORION_PHYSICAL_MAX_ADDRESS) {
            continue;
        }

        uint64_t end = descriptor.physical_start +
                       descriptor.number_of_pages * ORION_MEMORY_PAGE_SIZE;
        if (end > ORION_PHYSICAL_MAX_ADDRESS) {
            end = ORION_PHYSICAL_MAX_ADDRESS;
        }
        const uint64_t first_page =
            descriptor.physical_start / ORION_MEMORY_PAGE_SIZE;
        const uint64_t end_page = end / ORION_MEMORY_PAGE_SIZE;
        for (uint64_t page = first_page; page < end_page; ++page) {
            if (page == 0) {
                continue;
            }
            if (!bitmap_get(available_bitmap, page)) {
                bitmap_set(available_bitmap, page, true);
                --allocator_stats.reserved_pages;
                ++allocator_stats.free_pages;
            }
        }
    }

    allocator_initialized = allocator_stats.free_pages != 0;
    return allocator_initialized && orion_physical_check_invariants();
}

bool orion_physical_allocate(uint64_t alignment_pages,
                             uint64_t *physical_address)
{
    if (!allocator_initialized || physical_address == NULL ||
        alignment_pages == 0 ||
        (alignment_pages & (alignment_pages - 1U)) != 0) {
        return false;
    }

    for (uint64_t page = 0; page < ORION_PHYSICAL_MAX_PAGES;) {
        const uint64_t remainder = page & (alignment_pages - 1U);
        if (remainder != 0) {
            page += alignment_pages - remainder;
        }
        if (page >= ORION_PHYSICAL_MAX_PAGES) {
            break;
        }
        if (bitmap_get(available_bitmap, page)) {
            bitmap_set(available_bitmap, page, false);
            bitmap_set(allocated_bitmap, page, true);
            --allocator_stats.free_pages;
            ++allocator_stats.allocated_pages;
            *physical_address = page * ORION_MEMORY_PAGE_SIZE;
            return true;
        }
        ++page;
    }
    return false;
}

bool orion_physical_release(uint64_t physical_address)
{
    if (!allocator_initialized ||
        (physical_address & (ORION_MEMORY_PAGE_SIZE - 1U)) != 0 ||
        physical_address >= ORION_PHYSICAL_MAX_ADDRESS) {
        return false;
    }

    const uint64_t page = physical_address / ORION_MEMORY_PAGE_SIZE;
    if (!bitmap_get(allocated_bitmap, page) ||
        bitmap_get(available_bitmap, page)) {
        return false;
    }
    bitmap_set(allocated_bitmap, page, false);
    bitmap_set(available_bitmap, page, true);
    --allocator_stats.allocated_pages;
    ++allocator_stats.free_pages;
    return true;
}

bool orion_physical_is_allocated(uint64_t physical_address)
{
    if (!allocator_initialized ||
        (physical_address & (ORION_MEMORY_PAGE_SIZE - 1U)) != 0 ||
        physical_address >= ORION_PHYSICAL_MAX_ADDRESS) {
        return false;
    }

    const uint64_t page = physical_address / ORION_MEMORY_PAGE_SIZE;
    return bitmap_get(allocated_bitmap, page) &&
           !bitmap_get(available_bitmap, page);
}

bool orion_physical_get_stats(orion_physical_stats *stats)
{
    if (!allocator_initialized || stats == NULL) {
        return false;
    }
    *stats = allocator_stats;
    return true;
}

bool orion_physical_check_invariants(void)
{
    if (!allocator_initialized) {
        return false;
    }

    uint64_t free_pages = 0;
    uint64_t allocated_pages = 0;
    for (uint64_t page = 0; page < ORION_PHYSICAL_MAX_PAGES; ++page) {
        const bool available = bitmap_get(available_bitmap, page);
        const bool allocated = bitmap_get(allocated_bitmap, page);
        if (available && allocated) {
            return false;
        }
        free_pages += available ? 1U : 0U;
        allocated_pages += allocated ? 1U : 0U;
    }
    return free_pages == allocator_stats.free_pages &&
           allocated_pages == allocator_stats.allocated_pages &&
           free_pages + allocated_pages + allocator_stats.reserved_pages ==
               allocator_stats.total_pages;
}
