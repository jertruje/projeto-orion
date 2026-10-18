#ifndef ORION_KERNEL_MM_USER_SPACE_H
#define ORION_KERNEL_MM_USER_SPACE_H

#include <stdbool.h>
#include <stdint.h>

#include "page_tables.h"

#define ORION_USER_BASE UINT64_C(0x0000010000000000)
#define ORION_USER_IMAGE_LIMIT (ORION_USER_BASE + UINT64_C(0x00100000))
#define ORION_USER_STACK_TOP (ORION_USER_BASE + UINT64_C(0x00200000))
#define ORION_USER_STACK_BYTES UINT64_C(4096)

typedef struct {
    orion_page_table_space tables;
    orion_physical_address root_physical;
    bool initialized;
} orion_user_address_space;

bool orion_user_space_create(orion_user_address_space *space);
bool orion_user_space_map(orion_user_address_space *space,
                          orion_virtual_address address,
                          orion_physical_address frame, uint32_t flags);
bool orion_user_space_unmap(orion_user_address_space *space,
                            orion_virtual_address address,
                            orion_physical_address *frame);
bool orion_user_space_check(const orion_user_address_space *space,
                            uint64_t address, uint64_t length,
                            uint32_t required_flags);
bool orion_user_space_destroy(orion_user_address_space *space);

#endif
