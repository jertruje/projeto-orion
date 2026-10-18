#ifndef ORION_PHYSICAL_H
#define ORION_PHYSICAL_H

#include <stdbool.h>
#include <stdint.h>

#include "memory_map.h"

#define ORION_PHYSICAL_MAX_ADDRESS ORION_MEMORY_PHYSICAL_LIMIT
#define ORION_PHYSICAL_MAX_PAGES \
    (ORION_PHYSICAL_MAX_ADDRESS / ORION_MEMORY_PAGE_SIZE)
#define ORION_PHYSICAL_BITMAP_BYTES \
    ((ORION_PHYSICAL_MAX_PAGES + 7U) / 8U)

typedef struct {
    uint64_t total_pages;
    uint64_t free_pages;
    uint64_t allocated_pages;
    uint64_t reserved_pages;
} orion_physical_stats;

bool orion_physical_init(const orion_boot_info *boot_info);
bool orion_physical_allocate(uint64_t alignment_pages,
                             uint64_t *physical_address);
bool orion_physical_release(uint64_t physical_address);
bool orion_physical_is_allocated(uint64_t physical_address);
bool orion_physical_get_stats(orion_physical_stats *stats);
bool orion_physical_check_invariants(void);

#endif
