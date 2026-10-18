#include "active_page_tables.h"

#include <stdint.h>

#include "../arch/x86_64/cpu_state.h"
#include "memory_map.h"
#include "page_tables.h"
#include "physical.h"

#define CR3_ROOT_MASK UINT64_C(0x000ffffffffff000)
#define PTE_PRESENT UINT64_C(1)
#define PML4_HIGHER_HALF_START 256U

static bool allocate_table(void *context, orion_physical_address *physical,
                           uint64_t **virtual_table)
{
    (void)context;
    if (physical == NULL || virtual_table == NULL ||
        !orion_physical_allocate(1, &physical->value)) {
        return false;
    }
    *virtual_table = (uint64_t *)(uintptr_t)physical->value;
    return true;
}

static bool resolve_table(void *context, orion_physical_address physical,
                          uint64_t **virtual_table)
{
    (void)context;
    if (virtual_table == NULL ||
        (physical.value & (ORION_MEMORY_PAGE_SIZE - 1U)) != 0 ||
        physical.value >= ORION_PHYSICAL_MAX_ADDRESS) {
        return false;
    }
    *virtual_table = (uint64_t *)(uintptr_t)physical.value;
    return true;
}

static bool owns_table(void *context, orion_physical_address physical)
{
    (void)context;
    return orion_physical_is_allocated(physical.value);
}

static void release_table(void *context, orion_physical_address physical,
                          uint64_t *virtual_table)
{
    (void)context;
    (void)virtual_table;
    (void)orion_physical_release(physical.value);
}

static void invalidate_page(void *context, orion_virtual_address address)
{
    (void)context;
    __asm__ volatile("invlpg (%0)" : : "r"(address.value) : "memory");
}

static bool find_empty_kernel_slot(uint64_t *root_table,
                                   orion_virtual_address *address)
{
    for (uint32_t index = PML4_HIGHER_HALF_START; index < 512U; ++index) {
        if ((root_table[index] & PTE_PRESENT) != 0) {
            continue;
        }
        uint64_t base = (uint64_t)index << 39U;
        base |= UINT64_C(0xffff000000000000);
        *address = (orion_virtual_address){base + UINT64_C(0x00123000)};
        return orion_x86_64_virtual_address_is_canonical(*address);
    }
    return false;
}

bool orion_x86_64_active_page_tables_smoke_test(
    const orion_boot_info *boot_info, bool nx_enabled, bool *performed)
{
    if (performed == NULL) {
        return false;
    }
    *performed = false;
    if (!nx_enabled) {
        return true;
    }
    if (!orion_x86_64_uses_four_level_paging()) {
        return false;
    }

    const uint64_t root_physical = orion_x86_64_read_cr3() & CR3_ROOT_MASK;
    if (root_physical == 0 ||
        !orion_memory_map_range_is_reserved(
            boot_info, root_physical, ORION_MEMORY_PAGE_SIZE)) {
        return false;
    }

    uint64_t *root_table = (uint64_t *)(uintptr_t)root_physical;
    orion_virtual_address test_address;
    if (!find_empty_kernel_slot(root_table, &test_address)) {
        return false;
    }

    orion_page_table_ops ops = {
        .context = NULL,
        .nx_enabled = nx_enabled,
        .allocate_table = allocate_table,
        .resolve_table = resolve_table,
        .owns_table = owns_table,
        .release_table = release_table,
        .invalidate_page = invalidate_page
    };
    orion_page_table_space space;
    if (!orion_x86_64_page_tables_init(
            &space, root_table, (orion_physical_address){root_physical},
            &ops)) {
        return false;
    }

    uint64_t data_physical = 0;
    if (!orion_physical_allocate(
            1, &data_physical)) {
        return false;
    }

    const orion_physical_address data_frame = {data_physical};
    const uint32_t permissions = ORION_VM_READ | ORION_VM_WRITE;
    if (!orion_x86_64_page_map(&space, test_address, data_frame,
                               permissions)) {
        (void)orion_physical_release(data_physical);
        return false;
    }

    volatile uint64_t *mapped = (volatile uint64_t *)(uintptr_t)test_address.value;
    const uint64_t pattern = UINT64_C(0x4f52494f4e4d454d);
    *mapped = pattern;

    orion_physical_address observed_frame = {0};
    uint32_t observed_flags = 0;
    const bool lookup_ok =
        orion_x86_64_page_lookup(&space, test_address, &observed_frame,
                                 &observed_flags) &&
        observed_frame.value == data_physical &&
        observed_flags == permissions &&
        *mapped == pattern;

    orion_physical_address unmapped_frame = {0};
    const bool unmap_ok = orion_x86_64_page_unmap(
        &space, test_address, &unmapped_frame);
    const bool release_ok =
        unmap_ok && unmapped_frame.value == data_physical &&
        orion_physical_release(data_physical);
    if (!lookup_ok || !release_ok || !orion_physical_check_invariants()) {
        return false;
    }

    *performed = true;
    return true;
}
