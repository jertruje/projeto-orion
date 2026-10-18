#include <orion/boot_info.h>

#include "memory_map.h"
#include "physical.h"

#include <stdint.h>

static orion_uefi_memory_descriptor
    descriptors[ORION_MEMORY_MAP_MAX_DESCRIPTORS + 1U];
static orion_boot_info boot_info;

static int check(bool condition)
{
    return condition ? 0 : 1;
}

static void set_descriptor(uint32_t index, uint32_t type, uint64_t start,
                           uint64_t pages)
{
    descriptors[index] = (orion_uefi_memory_descriptor){
        type, 0, start, 0, pages, 0
    };
}

static void set_boot_info(uint32_t count)
{
    boot_info = (orion_boot_info){
        ORION_BOOT_INFO_SIGNATURE,
        ORION_BOOT_INFO_VERSION,
        0,
        descriptors,
        (uint64_t)count * sizeof(descriptors[0]),
        sizeof(descriptors[0]),
        NULL,
        0
    };
}

int main(void)
{
    int failures = 0;
    set_descriptor(0, ORION_UEFI_MEMORY_TYPE_CONVENTIONAL,
                   UINT64_C(0x100000), 4);
    set_descriptor(1, 0, UINT64_C(0x104000), 1);
    set_descriptor(2, ORION_UEFI_MEMORY_TYPE_CONVENTIONAL,
                   UINT64_C(0x200000), 2);
    set_boot_info(3);

    orion_memory_map_summary summary;
    failures += check(orion_memory_map_validate(&boot_info, &summary));
    failures += check(summary.descriptor_count == 3);
    failures += check(summary.conventional_descriptor_count == 2);
    failures += check(summary.conventional_page_count_below_limit == 6);
    failures += check(summary.maximum_physical_end == UINT64_C(0x202000));
    failures += check(orion_memory_map_range_is_reserved(
        &boot_info, UINT64_C(0x104000), ORION_MEMORY_PAGE_SIZE));
    failures += check(!orion_memory_map_range_is_reserved(
        &boot_info, UINT64_C(0x100000), ORION_MEMORY_PAGE_SIZE));
    failures += check(!orion_memory_map_range_is_reserved(
        &boot_info, UINT64_C(0x103000), 2U * ORION_MEMORY_PAGE_SIZE));
    failures += check(!orion_memory_map_range_is_reserved(
        &boot_info, UINT64_C(0x104000), 0));
    orion_uefi_memory_descriptor descriptor;
    failures += check(!orion_memory_map_descriptor(&boot_info, 3, &descriptor));

    failures += check(orion_physical_init(&boot_info));
    orion_physical_stats stats;
    failures += check(orion_physical_get_stats(&stats));
    failures += check(stats.free_pages == 6);
    failures += check(stats.allocated_pages == 0);
    failures += check(stats.reserved_pages ==
                      ORION_PHYSICAL_MAX_PAGES - 6U);

    uint64_t pages[6];
    for (uint32_t index = 0; index < 6; ++index) {
        failures += check(orion_physical_allocate(1, &pages[index]));
    }
    uint64_t unavailable = UINT64_MAX;
    failures += check(!orion_physical_allocate(1, &unavailable));
    failures += check(unavailable == UINT64_MAX);
    failures += check(orion_physical_check_invariants());

    failures += check(!orion_physical_release(UINT64_C(0x104000)));
    failures += check(!orion_physical_release(pages[0] + 1U));
    failures += check(orion_physical_release(pages[0]));
    failures += check(!orion_physical_release(pages[0]));
    failures += check(orion_physical_allocate(2, &unavailable));
    failures += check((unavailable & (2U * ORION_MEMORY_PAGE_SIZE - 1U)) == 0);
    failures += check(orion_physical_release(unavailable));
    failures += check(orion_physical_check_invariants());

    failures += check(!orion_physical_allocate(0, &unavailable));
    failures += check(!orion_physical_allocate(3, &unavailable));

    set_descriptor(1, 0, UINT64_C(0x102000), 1);
    set_boot_info(3);
    failures += check(!orion_memory_map_validate(&boot_info, NULL));

    set_descriptor(1, 0, UINT64_C(0x104001), 1);
    set_boot_info(3);
    failures += check(!orion_memory_map_validate(&boot_info, NULL));

    boot_info.descriptor_size = 41;
    boot_info.memory_map_size = 123;
    failures += check(!orion_memory_map_validate(&boot_info, NULL));

    set_boot_info(ORION_MEMORY_MAP_MAX_DESCRIPTORS + 1U);
    failures += check(!orion_memory_map_validate(&boot_info, NULL));

    set_boot_info(3);
    boot_info.memory_map =
        (const void *)(uintptr_t)(UINTPTR_MAX - UINT64_C(7));
    failures += check(!orion_memory_map_validate(&boot_info, NULL));

    return failures == 0 ? 0 : 1;
}
