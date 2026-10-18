#ifndef ORION_BOOT_INFO_H
#define ORION_BOOT_INFO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ORION_BOOT_INFO_SIGNATURE UINT64_C(0x314f52494f4f5442)
#define ORION_BOOT_INFO_VERSION 2U
#define ORION_EFI_MEMORY_DESCRIPTOR_MIN_SIZE 40U
#define ORION_BOOT_APPLICATION_MAX_BYTES UINT64_C(262144)

typedef struct {
    uint64_t signature;
    uint32_t version;
    uint32_t reserved;
    const void *memory_map;
    uint64_t memory_map_size;
    uint64_t descriptor_size;
    const void *application_image;
    uint64_t application_image_size;
} orion_boot_info;

_Static_assert(sizeof(void *) == 8, "ORION boot contract requires x86-64 pointers");
_Static_assert(sizeof(orion_boot_info) == 56, "boot-info ABI size changed");
_Static_assert(offsetof(orion_boot_info, memory_map) == 16,
               "boot-info memory-map offset changed");
_Static_assert(offsetof(orion_boot_info, memory_map_size) == 24,
               "boot-info memory-map size offset changed");
_Static_assert(offsetof(orion_boot_info, descriptor_size) == 32,
               "boot-info descriptor-size offset changed");
_Static_assert(offsetof(orion_boot_info, application_image) == 40,
               "boot-info application-image offset changed");
_Static_assert(offsetof(orion_boot_info, application_image_size) == 48,
               "boot-info application-size offset changed");

static inline bool orion_boot_info_is_valid(const orion_boot_info *info)
{
    if (info == NULL || info->signature != ORION_BOOT_INFO_SIGNATURE ||
        info->version != ORION_BOOT_INFO_VERSION || info->memory_map == NULL ||
        info->descriptor_size < ORION_EFI_MEMORY_DESCRIPTOR_MIN_SIZE ||
        info->memory_map_size < info->descriptor_size ||
        info->memory_map_size % info->descriptor_size != 0 ||
        ((info->application_image == NULL) !=
         (info->application_image_size == 0)) ||
        info->application_image_size > ORION_BOOT_APPLICATION_MAX_BYTES ||
        (info->application_image != NULL &&
         (uintptr_t)info->application_image >
             UINTPTR_MAX - info->application_image_size)) {
        return false;
    }

    return (uintptr_t)info->memory_map % sizeof(uint64_t) == 0;
}

#endif
