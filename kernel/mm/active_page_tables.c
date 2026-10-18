#include "active_page_tables.h"

#include <stdint.h>

#include "../arch/x86_64/cpu_state.h"
#include "memory_map.h"
#include "page_tables.h"
#include "physical.h"

#define CR3_ROOT_MASK UINT64_C(0x000ffffffffff000)
#define PTE_PRESENT UINT64_C(1)
#define PML4_HIGHER_HALF_START 256U
#define PAGE_TABLE_POOL_COUNT 32U

static _Alignas(4096) uint64_t bootstrap_root[512];
static _Alignas(4096) uint64_t page_table_pool[PAGE_TABLE_POOL_COUNT][512];
static bool page_table_pool_used[PAGE_TABLE_POOL_COUNT];

static bool allocate_table(void *context, orion_physical_address *physical,
                           uint64_t **virtual_table)
{
    (void)context;
    if (physical == NULL || virtual_table == NULL) {
        return false;
    }

    for (uint32_t index = 0; index < PAGE_TABLE_POOL_COUNT; ++index) {
        if (!page_table_pool_used[index]) {
            const uintptr_t address = (uintptr_t)page_table_pool[index];
            if (address >= ORION_PHYSICAL_MAX_ADDRESS) {
                return false;
            }
            page_table_pool_used[index] = true;
            *virtual_table = page_table_pool[index];
            physical->value = address;
            return true;
        }
    }
    return false;
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
    for (uint32_t index = 0; index < PAGE_TABLE_POOL_COUNT; ++index) {
        if (page_table_pool_used[index] &&
            physical.value == (uintptr_t)page_table_pool[index]) {
            *virtual_table = page_table_pool[index];
            return true;
        }
    }
    return false;
}

static bool owns_table(void *context, orion_physical_address physical)
{
    (void)context;
    if ((physical.value & (ORION_MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }
    for (uint32_t index = 0; index < PAGE_TABLE_POOL_COUNT; ++index) {
        if (page_table_pool_used[index] &&
            physical.value == (uintptr_t)page_table_pool[index]) {
            return true;
        }
    }
    return false;
}

static void release_table(void *context, orion_physical_address physical,
                          uint64_t *virtual_table)
{
    (void)context;
    (void)virtual_table;
    for (uint32_t index = 0; index < PAGE_TABLE_POOL_COUNT; ++index) {
        if (physical.value == (uintptr_t)page_table_pool[index]) {
            page_table_pool_used[index] = false;
            return;
        }
    }
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
            boot_info, root_physical, ORION_MEMORY_PAGE_SIZE) ||
        !orion_memory_map_range_is_reserved(
            boot_info, (uintptr_t)bootstrap_root, sizeof(bootstrap_root)) ||
        !orion_memory_map_range_is_reserved(
            boot_info, (uintptr_t)page_table_pool, sizeof(page_table_pool))) {
        return false;
    }

    const uint64_t *inherited_root =
        (const uint64_t *)(uintptr_t)root_physical;
    for (uint32_t index = 0; index < ORION_X86_64_PAGE_TABLE_ENTRIES; ++index) {
        bootstrap_root[index] = inherited_root[index];
    }

    const uint64_t bootstrap_root_physical = (uintptr_t)bootstrap_root;
    uint64_t *root_table = bootstrap_root;
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
            &space, root_table,
            (orion_physical_address){bootstrap_root_physical},
            &ops)) {
        return false;
    }

    const uint64_t inherited_cr3 = orion_x86_64_read_cr3();
    const uint64_t bootstrap_cr3 =
        bootstrap_root_physical | (inherited_cr3 & UINT64_C(0xfff));
    orion_x86_64_write_cr3(bootstrap_cr3);
    if ((orion_x86_64_read_cr3() & CR3_ROOT_MASK) !=
        bootstrap_root_physical) {
        orion_x86_64_write_cr3(inherited_cr3);
        return false;
    }

    uint64_t data_physical = 0;
    if (!orion_physical_allocate(
            1, &data_physical)) {
        orion_x86_64_write_cr3(inherited_cr3);
        return false;
    }

    const orion_physical_address data_frame = {data_physical};
#if defined(ORION_TEST_READONLY_WRITE_FAULT)
    const uint32_t permissions = ORION_VM_READ;
#else
    const uint32_t permissions = ORION_VM_READ | ORION_VM_WRITE;
#endif
    if (!orion_x86_64_page_map(&space, test_address, data_frame,
                               permissions)) {
        (void)orion_physical_release(data_physical);
        orion_x86_64_write_cr3(inherited_cr3);
        return false;
    }

#if defined(ORION_TEST_NX_EXECUTION_FAULT)
    ((void (*)(void))(uintptr_t)test_address.value)();
    return false;
#elif defined(ORION_TEST_READONLY_WRITE_FAULT)
    volatile uint64_t *mapped =
        (volatile uint64_t *)(uintptr_t)test_address.value;
    *mapped = UINT64_C(0x4f52494f4e4d454d);
    return false;
#else
    volatile uint64_t *mapped =
        (volatile uint64_t *)(uintptr_t)test_address.value;
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
    const bool allocator_invariants_ok = orion_physical_check_invariants();
    orion_x86_64_write_cr3(inherited_cr3);
    const bool root_restored =
        (orion_x86_64_read_cr3() & CR3_ROOT_MASK) ==
        (inherited_cr3 & CR3_ROOT_MASK);
    if (!lookup_ok || !release_ok || !allocator_invariants_ok ||
        !root_restored) {
        return false;
    }

    *performed = true;
    return true;
#endif
}
