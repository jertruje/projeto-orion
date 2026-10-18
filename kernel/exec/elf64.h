#ifndef ORION_KERNEL_EXEC_ELF64_H
#define ORION_KERNEL_EXEC_ELF64_H

#include <stdbool.h>
#include <stdint.h>

#include "../mm/user_space.h"

#define ORION_ELF64_MAX_SEGMENTS 8U
#define ORION_ELF64_MAX_USER_PAGES 32U

typedef enum {
    ORION_ELF64_OK,
    ORION_ELF64_INVALID,
    ORION_ELF64_UNSUPPORTED,
    ORION_ELF64_TOO_LARGE
} orion_elf64_status;

typedef struct {
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t file_bytes;
    uint64_t memory_bytes;
    uint32_t flags;
} orion_elf64_segment;

typedef struct {
    const uint8_t *image;
    uint64_t image_bytes;
    uint64_t entry;
    uint32_t segment_count;
    orion_elf64_segment segments[ORION_ELF64_MAX_SEGMENTS];
    uint32_t page_count;
} orion_elf64_image;

orion_elf64_status orion_elf64_validate(const void *image,
                                         uint64_t image_bytes,
                                         orion_elf64_image *validated);
bool orion_elf64_load_user(const orion_elf64_image *image,
                           orion_user_address_space *space,
                           uint64_t *user_entry, uint64_t *user_stack);
bool orion_elf64_destroy_user(orion_user_address_space *space);

#endif
