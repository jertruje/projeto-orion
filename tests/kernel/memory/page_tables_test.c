#include "page_tables.h"

#include <stdint.h>

#define TEST_TABLE_COUNT 4U

static _Alignas(4096) uint64_t table_storage[TEST_TABLE_COUNT][512];
static bool table_used[TEST_TABLE_COUNT];
static uint32_t invalidation_count;

static uint64_t table_physical(uint32_t index)
{
    return UINT64_C(0x1000) + (uint64_t)index * UINT64_C(0x1000);
}

static bool allocate_table(void *context, orion_physical_address *physical,
                           uint64_t **virtual_table)
{
    (void)context;
    for (uint32_t index = 1; index < TEST_TABLE_COUNT; ++index) {
        if (!table_used[index]) {
            table_used[index] = true;
            *physical = (orion_physical_address){table_physical(index)};
            *virtual_table = table_storage[index];
            return true;
        }
    }
    return false;
}

static bool resolve_table(void *context, orion_physical_address physical,
                          uint64_t **virtual_table)
{
    (void)context;
    if (physical.value < table_physical(0) ||
        physical.value >= table_physical(TEST_TABLE_COUNT - 1U) +
                              UINT64_C(0x1000) ||
        (physical.value & UINT64_C(0xfff)) != 0) {
        return false;
    }
    const uint32_t index =
        (uint32_t)((physical.value - table_physical(0)) / UINT64_C(0x1000));
    if (!table_used[index]) {
        return false;
    }
    *virtual_table = table_storage[index];
    return true;
}

static bool owns_table(void *context, orion_physical_address physical)
{
    (void)context;
    if (physical.value < table_physical(1) ||
        physical.value >= table_physical(TEST_TABLE_COUNT) ||
        (physical.value & UINT64_C(0xfff)) != 0) {
        return false;
    }
    const uint32_t index =
        (uint32_t)((physical.value - table_physical(0)) / UINT64_C(0x1000));
    return table_used[index];
}

static void release_table(void *context, orion_physical_address physical,
                          uint64_t *virtual_table)
{
    (void)context;
    (void)virtual_table;
    const uint32_t index =
        (uint32_t)((physical.value - table_physical(0)) / UINT64_C(0x1000));
    table_used[index] = false;
}

static void invalidate_page(void *context, orion_virtual_address address)
{
    (void)context;
    (void)address;
    ++invalidation_count;
}

static int check(bool condition)
{
    return condition ? 0 : 1;
}

int main(void)
{
    int failures = 0;
    table_used[0] = true;
    orion_page_table_ops ops = {
        0, true, allocate_table, resolve_table, owns_table, release_table,
        invalidate_page
    };
    orion_page_table_space space;
    failures += check(orion_x86_64_page_tables_init(
        &space, table_storage[0], (orion_physical_address){table_physical(0)},
        &ops));

    const orion_virtual_address address = {UINT64_C(0x0000000040203000)};
    const orion_physical_address frame = {UINT64_C(0x00800000)};
    const uint32_t permissions = ORION_VM_READ | ORION_VM_WRITE |
                                 ORION_VM_USER | ORION_VM_WRITE_THROUGH |
                                 ORION_VM_CACHE_DISABLE;
    failures += check(orion_x86_64_virtual_address_is_canonical(address));
    failures += check(orion_x86_64_virtual_address_is_canonical(
        (orion_virtual_address){UINT64_C(0xffff800000000000)}));
    failures += check(!orion_x86_64_virtual_address_is_canonical(
        (orion_virtual_address){UINT64_C(0x0000800000000000)}));
    failures += check(orion_x86_64_page_map(&space, address, frame,
                                            permissions));
    failures += check(invalidation_count == 1);

    orion_physical_address mapped_frame = {0};
    uint32_t mapped_permissions = 0;
    failures += check(orion_x86_64_page_lookup(&space, address, &mapped_frame,
                                               &mapped_permissions));
    failures += check(mapped_frame.value == frame.value);
    failures += check(mapped_permissions == permissions);
    failures += check(!orion_x86_64_page_map(&space, address, frame,
                                             permissions));
    failures += check(!orion_x86_64_page_map(
        &space, (orion_virtual_address){address.value + 1U}, frame,
        permissions));
    failures += check(!orion_x86_64_page_map(
        &space, address, frame, ORION_VM_READ | ORION_VM_WRITE |
                                   ORION_VM_EXECUTE));
    failures += check(!orion_x86_64_page_map(
        &space, address, (orion_physical_address){UINT64_C(0x100000000)},
        ORION_VM_READ));
    failures += check(!orion_x86_64_page_map(
        &space, address, (orion_physical_address){frame.value + 1U},
        ORION_VM_READ));
    failures += check(!orion_x86_64_page_map(
        &space, address, frame, ORION_VM_READ | (1U << 20)));

    orion_physical_address removed_frame = {0};
    failures += check(orion_x86_64_page_unmap(&space, address,
                                              &removed_frame));
    failures += check(removed_frame.value == frame.value);
    failures += check(invalidation_count == 2);
    failures += check(!orion_x86_64_page_lookup(&space, address, &mapped_frame,
                                                &mapped_permissions));
    failures += check(!orion_x86_64_page_unmap(&space, address,
                                               &removed_frame));
    failures += check(!table_used[1] && !table_used[2] && !table_used[3]);

    const uint32_t executable = ORION_VM_READ | ORION_VM_EXECUTE;
    failures += check(orion_x86_64_page_map(&space, address, frame,
                                            executable));
    failures += check(orion_x86_64_page_lookup(&space, address, &mapped_frame,
                                               &mapped_permissions));
    failures += check(mapped_permissions == executable);
    failures += check(orion_x86_64_page_unmap(&space, address,
                                              &removed_frame));

    table_used[1] = false;
    table_used[2] = false;
    table_used[3] = false;
    const orion_virtual_address second_address = {
        UINT64_C(0x0000008000000000)
    };
    table_used[3] = true;
    failures += check(!orion_x86_64_page_map(
        &space, second_address, frame, ORION_VM_READ));
    failures += check(table_storage[0][1] == 0);
    failures += check(!table_used[1] && !table_used[2] && table_used[3]);

    table_used[3] = false;
    ops.nx_enabled = false;
    failures += check(orion_x86_64_page_tables_init(
        &space, table_storage[0], (orion_physical_address){table_physical(0)},
        &ops));
    failures += check(!orion_x86_64_page_map(&space, address, frame,
                                             ORION_VM_READ));
    failures += check(orion_x86_64_page_map(&space, address, frame,
                                            ORION_VM_READ | ORION_VM_EXECUTE));
    failures += check(orion_x86_64_page_unmap(&space, address,
                                              &removed_frame));

    return failures == 0 ? 0 : 1;
}
