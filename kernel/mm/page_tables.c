#include "page_tables.h"

#include <stddef.h>

#define PTE_PRESENT (UINT64_C(1) << 0)
#define PTE_WRITABLE (UINT64_C(1) << 1)
#define PTE_USER (UINT64_C(1) << 2)
#define PTE_WRITE_THROUGH (UINT64_C(1) << 3)
#define PTE_CACHE_DISABLE (UINT64_C(1) << 4)
#define PTE_HUGE (UINT64_C(1) << 7)
#define PTE_NO_EXECUTE (UINT64_C(1) << 63)
#define PAGE_OFFSET_MASK UINT64_C(0xfff)
#define PHYSICAL_ADDRESS_LIMIT UINT64_C(0x100000000)

typedef struct {
    uint64_t *table;
    uint16_t index;
    uint64_t physical;
} table_path_entry;

static uint16_t page_index(uint64_t address, unsigned int shift)
{
    return (uint16_t)((address >> shift) & UINT64_C(0x1ff));
}

static bool address_is_page_aligned(uint64_t address)
{
    return (address & PAGE_OFFSET_MASK) == 0;
}

static bool ops_are_valid(const orion_page_table_ops *ops)
{
    return ops != NULL && ops->allocate_table != NULL &&
           ops->resolve_table != NULL && ops->owns_table != NULL &&
           ops->release_table != NULL && ops->invalidate_page != NULL;
}

bool orion_x86_64_page_tables_init(orion_page_table_space *space,
                                   uint64_t *root_table,
                                   orion_physical_address root_physical,
                                   const orion_page_table_ops *ops)
{
    if (space == NULL || root_table == NULL || !ops_are_valid(ops) ||
        !address_is_page_aligned(root_physical.value) ||
        root_physical.value >= PHYSICAL_ADDRESS_LIMIT ||
        ((uintptr_t)root_table & (uintptr_t)PAGE_OFFSET_MASK) != 0) {
        return false;
    }
    space->root_table = root_table;
    space->root_physical = root_physical;
    space->ops = *ops;
    return true;
}

bool orion_x86_64_virtual_address_is_canonical(orion_virtual_address address)
{
    const uint64_t upper = address.value >> 48;
    const bool sign_bit = (address.value & (UINT64_C(1) << 47)) != 0;
    return sign_bit ? upper == UINT64_C(0xffff) : upper == 0;
}

static bool validate_space(const orion_page_table_space *space)
{
    return space != NULL && space->root_table != NULL &&
           ops_are_valid(&space->ops);
}

static bool flags_are_valid(const orion_page_table_space *space,
                            uint32_t flags)
{
    if ((flags & ~ORION_VM_SUPPORTED_FLAGS) != 0 ||
        (flags & ORION_VM_READ) == 0 ||
        ((flags & (ORION_VM_WRITE | ORION_VM_EXECUTE)) ==
         (ORION_VM_WRITE | ORION_VM_EXECUTE))) {
        return false;
    }
    return space->ops.nx_enabled || (flags & ORION_VM_EXECUTE) != 0;
}

static bool validate_mapping(const orion_page_table_space *space,
                             orion_virtual_address virtual_address,
                             orion_physical_address physical_address,
                             uint32_t flags)
{
    return validate_space(space) &&
           orion_x86_64_virtual_address_is_canonical(virtual_address) &&
           address_is_page_aligned(virtual_address.value) &&
           address_is_page_aligned(physical_address.value) &&
           physical_address.value < PHYSICAL_ADDRESS_LIMIT &&
           flags_are_valid(space, flags);
}

static bool resolve_child(orion_page_table_space *space, uint64_t entry,
                          uint64_t **child)
{
    orion_physical_address physical = {
        entry & ORION_X86_64_PAGE_TABLE_PHYS_MASK
    };
    return (entry & PTE_PRESENT) != 0 && (entry & PTE_HUGE) == 0 &&
           physical.value < PHYSICAL_ADDRESS_LIMIT &&
           space->ops.resolve_table(space->ops.context, physical, child) &&
           *child != NULL &&
           ((uintptr_t)*child & (uintptr_t)PAGE_OFFSET_MASK) == 0;
}

static void zero_table(uint64_t *table)
{
    for (uint32_t index = 0; index < ORION_X86_64_PAGE_TABLE_ENTRIES; ++index) {
        table[index] = 0;
    }
}

static void rollback_allocations(orion_page_table_space *space,
                                 table_path_entry *parents,
                                 orion_physical_address *allocated_physical,
                                 uint64_t **allocated_virtual,
                                 uint32_t count)
{
    while (count != 0) {
        --count;
        parents[count].table[parents[count].index] = 0;
        space->ops.release_table(space->ops.context,
                                 allocated_physical[count],
                                 allocated_virtual[count]);
    }
}

bool orion_x86_64_page_map(orion_page_table_space *space,
                           orion_virtual_address virtual_address,
                           orion_physical_address physical_address,
                           uint32_t flags)
{
    if (!validate_mapping(space, virtual_address, physical_address, flags)) {
        return false;
    }

    static const unsigned int shifts[] = {39U, 30U, 21U, 12U};
    uint64_t *table = space->root_table;
    table_path_entry parents[3];
    orion_physical_address allocated_physical[3];
    uint64_t *allocated_virtual[3];
    uint64_t *promoted_parents[3];
    uint64_t path_physical[4] = {space->root_physical.value, 0, 0, 0};
    uint32_t allocated_count = 0;
    uint32_t promoted_count = 0;
    const uint64_t parent_permissions =
        PTE_WRITABLE | ((flags & ORION_VM_USER) != 0 ? PTE_USER : 0U);

    for (uint32_t level = 0; level < 3; ++level) {
        const uint16_t index = page_index(virtual_address.value, shifts[level]);
        uint64_t entry = table[index];
        if ((entry & PTE_PRESENT) == 0) {
            uint64_t *new_table = NULL;
            orion_physical_address new_physical = {0};
            const bool allocated =
                space->ops.allocate_table(space->ops.context, &new_physical,
                                          &new_table);
            if (!allocated || new_table == NULL ||
                !address_is_page_aligned(new_physical.value) ||
                new_physical.value >= PHYSICAL_ADDRESS_LIMIT ||
                ((uintptr_t)new_table & (uintptr_t)PAGE_OFFSET_MASK) != 0 ||
                new_physical.value == path_physical[0] ||
                (level > 0 && new_physical.value == path_physical[1]) ||
                (level > 1 && new_physical.value == path_physical[2]) ||
                (allocated_count > 0 &&
                 new_physical.value == allocated_physical[0].value) ||
                (allocated_count > 1 &&
                 new_physical.value == allocated_physical[1].value) ||
                allocated_count >= 3U) {
                if (allocated && new_table != NULL) {
                    space->ops.release_table(space->ops.context, new_physical,
                                             new_table);
                }
                rollback_allocations(space, parents, allocated_physical,
                                     allocated_virtual, allocated_count);
                return false;
            }
            zero_table(new_table);
            parents[allocated_count] = (table_path_entry){table, index, 0};
            allocated_physical[allocated_count] = new_physical;
            allocated_virtual[allocated_count] = new_table;
            ++allocated_count;
            path_physical[level + 1U] = new_physical.value;
            entry = new_physical.value | PTE_PRESENT | parent_permissions;
            table[index] = entry;
            table = new_table;
            continue;
        }

        if ((entry & PTE_HUGE) != 0) {
            rollback_allocations(space, parents, allocated_physical,
                                 allocated_virtual, allocated_count);
            return false;
        }
        if ((flags & ORION_VM_EXECUTE) != 0 &&
            (entry & PTE_NO_EXECUTE) != 0) {
            rollback_allocations(space, parents, allocated_physical,
                                 allocated_virtual, allocated_count);
            return false;
        }
        promoted_parents[promoted_count++] = &table[index];
        table = NULL;
        if (!resolve_child(space, entry, &table)) {
            rollback_allocations(space, parents, allocated_physical,
                                 allocated_virtual, allocated_count);
            return false;
        }
        path_physical[level + 1U] =
            entry & ORION_X86_64_PAGE_TABLE_PHYS_MASK;
    }

    const uint16_t leaf_index = page_index(virtual_address.value, shifts[3]);
    if ((table[leaf_index] & PTE_PRESENT) != 0) {
        rollback_allocations(space, parents, allocated_physical,
                             allocated_virtual, allocated_count);
        return false;
    }

    uint64_t leaf = physical_address.value | PTE_PRESENT;
    if ((flags & ORION_VM_WRITE) != 0) {
        leaf |= PTE_WRITABLE;
    }
    if ((flags & ORION_VM_USER) != 0) {
        leaf |= PTE_USER;
    }
    if ((flags & ORION_VM_WRITE_THROUGH) != 0) {
        leaf |= PTE_WRITE_THROUGH;
    }
    if ((flags & ORION_VM_CACHE_DISABLE) != 0) {
        leaf |= PTE_CACHE_DISABLE;
    }
    if (space->ops.nx_enabled && (flags & ORION_VM_EXECUTE) == 0) {
        leaf |= PTE_NO_EXECUTE;
    }
    table[leaf_index] = leaf;
    for (uint32_t index = 0; index < promoted_count; ++index) {
        *promoted_parents[index] |= parent_permissions;
    }
    space->ops.invalidate_page(space->ops.context, virtual_address);
    return true;
}

static bool lookup_leaf(orion_page_table_space *space,
                        orion_virtual_address virtual_address,
                        uint64_t **leaf_entry,
                        uint64_t *effective_entry)
{
    if (!validate_space(space) ||
        !orion_x86_64_virtual_address_is_canonical(virtual_address) ||
        !address_is_page_aligned(virtual_address.value)) {
        return false;
    }

    uint64_t *table = space->root_table;
    uint64_t effective_permissions =
        PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    uint64_t effective_execute = 0;
    uint64_t effective_cache = 0;
    static const unsigned int shifts[] = {39U, 30U, 21U};
    for (uint32_t level = 0; level < 3; ++level) {
        const uint64_t entry = table[page_index(virtual_address.value,
                                                shifts[level])];
        if (!resolve_child(space, entry, &table)) {
            return false;
        }
        effective_permissions &= entry;
        effective_execute |= entry & PTE_NO_EXECUTE;
        effective_cache |= entry & (PTE_WRITE_THROUGH | PTE_CACHE_DISABLE);
    }
    *leaf_entry = &table[page_index(virtual_address.value, 12U)];
    if ((**leaf_entry & PTE_PRESENT) == 0) {
        return false;
    }
    effective_permissions &= **leaf_entry;
    effective_execute |= **leaf_entry & PTE_NO_EXECUTE;
    effective_cache |=
        **leaf_entry & (PTE_WRITE_THROUGH | PTE_CACHE_DISABLE);
    *effective_entry = (effective_permissions & (PTE_PRESENT | PTE_WRITABLE |
                                                  PTE_USER)) |
                       effective_execute |
                       effective_cache |
                       (**leaf_entry & ORION_X86_64_PAGE_TABLE_PHYS_MASK);
    return true;
}

bool orion_x86_64_page_lookup(orion_page_table_space *space,
                              orion_virtual_address virtual_address,
                              orion_physical_address *physical_address,
                              uint32_t *flags)
{
    if (physical_address == NULL || flags == NULL) {
        return false;
    }
    uint64_t *leaf_entry = NULL;
    uint64_t entry = 0;
    if (!lookup_leaf(space, virtual_address, &leaf_entry, &entry)) {
        return false;
    }

    uint32_t result = ORION_VM_READ;
    result |= (entry & PTE_WRITABLE) != 0 ? ORION_VM_WRITE : 0U;
    result |= (entry & PTE_USER) != 0 ? ORION_VM_USER : 0U;
    result |= (entry & PTE_WRITE_THROUGH) != 0
                  ? ORION_VM_WRITE_THROUGH
                  : 0U;
    result |= (entry & PTE_CACHE_DISABLE) != 0
                  ? ORION_VM_CACHE_DISABLE
                  : 0U;
    result |= (entry & PTE_NO_EXECUTE) == 0 ? ORION_VM_EXECUTE : 0U;
    *physical_address = (orion_physical_address){
        entry & ORION_X86_64_PAGE_TABLE_PHYS_MASK
    };
    *flags = result;
    return true;
}

static bool table_is_empty(const uint64_t *table)
{
    for (uint32_t index = 0; index < ORION_X86_64_PAGE_TABLE_ENTRIES; ++index) {
        if (table[index] != 0) {
            return false;
        }
    }
    return true;
}

bool orion_x86_64_page_unmap(orion_page_table_space *space,
                             orion_virtual_address virtual_address,
                             orion_physical_address *physical_address)
{
    if (physical_address == NULL || !validate_space(space) ||
        !orion_x86_64_virtual_address_is_canonical(virtual_address) ||
        !address_is_page_aligned(virtual_address.value)) {
        return false;
    }

    uint64_t *tables[4] = {space->root_table, NULL, NULL, NULL};
    uint16_t indices[4];
    static const unsigned int shifts[] = {39U, 30U, 21U, 12U};
    for (uint32_t level = 0; level < 3; ++level) {
        indices[level] = page_index(virtual_address.value, shifts[level]);
        if (!resolve_child(space, tables[level][indices[level]],
                           &tables[level + 1U])) {
            return false;
        }
    }
    indices[3] = page_index(virtual_address.value, shifts[3]);
    uint64_t *leaf = &tables[3][indices[3]];
    if ((*leaf & PTE_PRESENT) == 0) {
        return false;
    }

    *physical_address = (orion_physical_address){
        *leaf & ORION_X86_64_PAGE_TABLE_PHYS_MASK
    };
    *leaf = 0;
    space->ops.invalidate_page(space->ops.context, virtual_address);

    for (int level = 3; level > 0; --level) {
        if (!table_is_empty(tables[level])) {
            break;
        }
        const uint64_t parent_entry = tables[level - 1][indices[level - 1]];
        const orion_physical_address table_physical = {
            parent_entry & ORION_X86_64_PAGE_TABLE_PHYS_MASK
        };
        if (!space->ops.owns_table(space->ops.context, table_physical)) {
            break;
        }
        tables[level - 1][indices[level - 1]] = 0;
        space->ops.release_table(space->ops.context, table_physical,
                                 tables[level]);
    }
    return true;
}
