#ifndef ORION_PAGE_TABLES_H
#define ORION_PAGE_TABLES_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_X86_64_PAGE_TABLE_ENTRIES 512U
#define ORION_X86_64_PAGE_TABLE_PHYS_MASK UINT64_C(0x000ffffffffff000)

typedef struct {
    uint64_t value;
} orion_virtual_address;

typedef struct {
    uint64_t value;
} orion_physical_address;

typedef enum {
    ORION_VM_READ = 1U << 0,
    ORION_VM_WRITE = 1U << 1,
    ORION_VM_USER = 1U << 2,
    ORION_VM_EXECUTE = 1U << 3,
    ORION_VM_WRITE_THROUGH = 1U << 4,
    ORION_VM_CACHE_DISABLE = 1U << 5
} orion_vm_flags;

#define ORION_VM_SUPPORTED_FLAGS \
    (ORION_VM_READ | ORION_VM_WRITE | ORION_VM_USER | ORION_VM_EXECUTE | \
     ORION_VM_WRITE_THROUGH | ORION_VM_CACHE_DISABLE)

typedef struct {
    void *context;
    bool nx_enabled;
    bool (*allocate_table)(void *context, orion_physical_address *physical,
                           uint64_t **virtual_table);
    bool (*resolve_table)(void *context, orion_physical_address physical,
                          uint64_t **virtual_table);
    bool (*owns_table)(void *context, orion_physical_address physical);
    void (*release_table)(void *context, orion_physical_address physical,
                          uint64_t *virtual_table);
    void (*invalidate_page)(void *context, orion_virtual_address address);
} orion_page_table_ops;

typedef struct {
    uint64_t *root_table;
    orion_physical_address root_physical;
    orion_page_table_ops ops;
} orion_page_table_space;

bool orion_x86_64_page_tables_init(orion_page_table_space *space,
                                   uint64_t *root_table,
                                   orion_physical_address root_physical,
                                   const orion_page_table_ops *ops);
bool orion_x86_64_virtual_address_is_canonical(orion_virtual_address address);
bool orion_x86_64_page_map(orion_page_table_space *space,
                           orion_virtual_address virtual_address,
                           orion_physical_address physical_address,
                           uint32_t flags);
bool orion_x86_64_page_lookup(orion_page_table_space *space,
                              orion_virtual_address virtual_address,
                              orion_physical_address *physical_address,
                              uint32_t *flags);
bool orion_x86_64_page_unmap(orion_page_table_space *space,
                             orion_virtual_address virtual_address,
                             orion_physical_address *physical_address);

#endif
