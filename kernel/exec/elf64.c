#include "elf64.h"

#include <stddef.h>

#include <orion/boot_info.h>

#include "../arch/x86_64/cpu_state.h"
#include "../arch/x86_64/gdt.h"
#include "../arch/x86_64/interrupt_state.h"
#include "../arch/x86_64/user_mode.h"
#include "../log/log.h"
#include "../mm/memory_map.h"
#include "../mm/physical.h"
#include "../syscall/syscall.h"

#define ELF_HEADER_BYTES 64U
#define ELF_PROGRAM_HEADER_BYTES 56U
#define ELF_ET_EXEC 2U
#define ELF_EM_X86_64 62U
#define ELF_PT_LOAD 1U
#define ELF_PT_DYNAMIC 2U
#define ELF_PT_INTERP 3U
#define ELF_PT_TLS 7U
#define ELF_PF_X 1U
#define ELF_PF_W 2U
#define ELF_PF_R 4U
#define ELF_PF_MASK (ELF_PF_X | ELF_PF_W | ELF_PF_R)
#define CR3_ROOT_MASK UINT64_C(0x000ffffffffff000)
#define USER_SEGMENT_FLAGS (ORION_VM_USER | ORION_VM_READ)
#define USER_EXIT_STACK_TOP ORION_USER_STACK_TOP
#define PAGE_MASK (ORION_MEMORY_PAGE_SIZE - 1U)

typedef struct {
    uint64_t address;
    uint64_t physical;
} user_page;

static user_page mapped_pages[ORION_ELF64_MAX_USER_PAGES];
static uint32_t mapped_page_count;

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint64_t read_le64(const uint8_t *bytes)
{
    return (uint64_t)read_le32(bytes) |
           ((uint64_t)read_le32(bytes + 4U) << 32);
}

static bool range_fits(uint64_t start, uint64_t length, uint64_t limit)
{
    return start <= limit && length <= limit - start;
}

static bool segment_page_ranges_overlap(uint64_t first_start,
                                        uint64_t first_bytes,
                                        uint64_t second_start,
                                        uint64_t second_bytes)
{
    const uint64_t first_end = first_start + first_bytes;
    const uint64_t second_end = second_start + second_bytes;
    return first_start < second_end && second_start < first_end;
}

orion_elf64_status orion_elf64_validate(const void *image_pointer,
                                         uint64_t image_bytes,
                                         orion_elf64_image *validated)
{
    if (image_pointer == NULL || validated == NULL ||
        image_bytes < ELF_HEADER_BYTES) {
        return ORION_ELF64_INVALID;
    }
    if (image_bytes > ORION_BOOT_APPLICATION_MAX_BYTES) {
        return ORION_ELF64_TOO_LARGE;
    }
    if ((uintptr_t)image_pointer > UINTPTR_MAX - image_bytes) {
        return ORION_ELF64_INVALID;
    }
    const uint8_t *image = image_pointer;
    if (image[0] != 0x7f || image[1] != 'E' || image[2] != 'L' ||
        image[3] != 'F' || image[4] != 2U || image[5] != 1U ||
        image[6] != 1U || image[7] != 0U ||
        read_le16(image + 16U) != ELF_ET_EXEC ||
        read_le16(image + 18U) != ELF_EM_X86_64 ||
        read_le32(image + 20U) != 1U ||
        read_le16(image + 52U) != ELF_HEADER_BYTES ||
        read_le16(image + 54U) != ELF_PROGRAM_HEADER_BYTES) {
        return ORION_ELF64_UNSUPPORTED;
    }
    const uint64_t entry = read_le64(image + 24U);
    const uint64_t program_offset = read_le64(image + 32U);
    const uint16_t program_count = read_le16(image + 56U);
    if (program_count == 0 || program_count > 32U ||
        !range_fits(program_offset,
                    (uint64_t)program_count * ELF_PROGRAM_HEADER_BYTES,
                    image_bytes)) {
        return ORION_ELF64_INVALID;
    }

    orion_elf64_image parsed;
    parsed.image = image;
    parsed.image_bytes = image_bytes;
    parsed.entry = entry;
    parsed.segment_count = 0;
    parsed.page_count = 0;
    bool entry_is_executable = false;
    for (uint16_t index = 0; index < program_count; ++index) {
        const uint8_t *program =
            image + program_offset +
            (uint64_t)index * ELF_PROGRAM_HEADER_BYTES;
        const uint32_t type = read_le32(program);
        const uint32_t flags = read_le32(program + 4U);
        if (type == ELF_PT_DYNAMIC || type == ELF_PT_INTERP ||
            type == ELF_PT_TLS) {
            return ORION_ELF64_UNSUPPORTED;
        }
        if (type != ELF_PT_LOAD) {
            continue;
        }
        const uint64_t file_offset = read_le64(program + 8U);
        const uint64_t virtual_address = read_le64(program + 16U);
        const uint64_t file_bytes = read_le64(program + 32U);
        const uint64_t memory_bytes = read_le64(program + 40U);
        const uint64_t alignment = read_le64(program + 48U);
        if (parsed.segment_count >= ORION_ELF64_MAX_SEGMENTS ||
            memory_bytes == 0 || file_bytes > memory_bytes ||
            (flags & ~ELF_PF_MASK) != 0 ||
            (flags & ELF_PF_R) == 0 ||
            (flags & (ELF_PF_W | ELF_PF_X)) ==
                (ELF_PF_W | ELF_PF_X) ||
            !range_fits(file_offset, file_bytes, image_bytes) ||
            !range_fits(virtual_address, memory_bytes,
                        ORION_USER_IMAGE_LIMIT) ||
            virtual_address < ORION_USER_BASE ||
            (virtual_address & PAGE_MASK) != 0 ||
            (file_offset & PAGE_MASK) != 0 ||
            (file_offset & PAGE_MASK) != (virtual_address & PAGE_MASK) ||
            (alignment > 1U &&
             (alignment != ORION_MEMORY_PAGE_SIZE ||
              (file_offset & (alignment - 1U)) !=
                  (virtual_address & (alignment - 1U))))) {
            return ORION_ELF64_INVALID;
        }
        const uint64_t rounded_memory =
            (memory_bytes + PAGE_MASK) & ~PAGE_MASK;
        if (rounded_memory < memory_bytes ||
            rounded_memory > ORION_USER_IMAGE_LIMIT - virtual_address) {
            return ORION_ELF64_INVALID;
        }
        const uint64_t segment_pages =
            rounded_memory / ORION_MEMORY_PAGE_SIZE;
        if (segment_pages > ORION_ELF64_MAX_USER_PAGES ||
            parsed.page_count >
                ORION_ELF64_MAX_USER_PAGES - segment_pages) {
            return ORION_ELF64_TOO_LARGE;
        }
        for (uint32_t prior = 0; prior < parsed.segment_count; ++prior) {
            const orion_elf64_segment *existing = &parsed.segments[prior];
            const uint64_t existing_rounded =
                (existing->memory_bytes + PAGE_MASK) & ~PAGE_MASK;
            if (segment_page_ranges_overlap(
                    virtual_address, rounded_memory,
                    existing->virtual_address, existing_rounded)) {
                return ORION_ELF64_INVALID;
            }
        }
        parsed.segments[parsed.segment_count++] = (orion_elf64_segment){
            .offset = file_offset,
            .virtual_address = virtual_address,
            .file_bytes = file_bytes,
            .memory_bytes = memory_bytes,
            .flags = flags
        };
        parsed.page_count +=
            (uint32_t)(rounded_memory / ORION_MEMORY_PAGE_SIZE);
        if ((flags & ELF_PF_X) != 0 &&
            entry >= virtual_address &&
            entry - virtual_address < memory_bytes) {
            entry_is_executable = true;
        }
    }
    if (parsed.segment_count == 0 || parsed.page_count == 0 ||
        parsed.page_count >= ORION_ELF64_MAX_USER_PAGES ||
        !entry_is_executable) {
        return ORION_ELF64_INVALID;
    }
    ++parsed.page_count;
    validated->image = parsed.image;
    validated->image_bytes = parsed.image_bytes;
    validated->entry = parsed.entry;
    validated->segment_count = parsed.segment_count;
    validated->page_count = parsed.page_count;
    for (uint32_t index = 0; index < parsed.segment_count; ++index) {
        validated->segments[index] = parsed.segments[index];
    }
    return ORION_ELF64_OK;
}

static bool map_page(orion_user_address_space *space, uint64_t address,
                     uint32_t flags)
{
    if (mapped_page_count >= ORION_ELF64_MAX_USER_PAGES) {
        return false;
    }
    uint64_t physical = 0;
    if (!orion_physical_allocate(1, &physical)) {
        return false;
    }
    uint8_t *page = (uint8_t *)(uintptr_t)physical;
    for (uint32_t index = 0; index < ORION_MEMORY_PAGE_SIZE; ++index) {
        page[index] = 0;
    }
    if (!orion_user_space_map(space, (orion_virtual_address){address},
                              (orion_physical_address){physical}, flags)) {
        (void)orion_physical_release(physical);
        return false;
    }
    mapped_pages[mapped_page_count++] = (user_page){address, physical};
    return true;
}

static bool copy_into_user_space(orion_user_address_space *space,
                                 uint64_t destination,
                                 const uint8_t *source, uint64_t bytes)
{
    if (!orion_user_space_check(space, destination, bytes,
                                ORION_VM_READ)) {
        return false;
    }
    uint64_t copied = 0;
    while (copied < bytes) {
        const uint64_t address = destination + copied;
        const uint64_t offset = address & PAGE_MASK;
        const uint64_t chunk =
            bytes - copied < ORION_MEMORY_PAGE_SIZE - offset
                ? bytes - copied
                : ORION_MEMORY_PAGE_SIZE - offset;
        orion_physical_address frame = {0};
        uint32_t flags = 0;
        if (!orion_x86_64_page_lookup(
                &space->tables,
                (orion_virtual_address){address & ~PAGE_MASK}, &frame,
                &flags) ||
            (flags & ORION_VM_USER) == 0) {
            return false;
        }
        uint8_t *target =
            (uint8_t *)(uintptr_t)(frame.value + offset);
        for (uint64_t index = 0; index < chunk; ++index) {
            target[index] = source[copied + index];
        }
        copied += chunk;
    }
    return true;
}

bool orion_elf64_load_user(const orion_elf64_image *image,
                           orion_user_address_space *space,
                           uint64_t *user_entry, uint64_t *user_stack)
{
    if (image == NULL || space == NULL || user_entry == NULL ||
        user_stack == NULL || space->initialized ||
        orion_x86_64_interrupts_enabled() ||
        image->segment_count == 0 ||
        image->segment_count > ORION_ELF64_MAX_SEGMENTS) {
        return false;
    }
    mapped_page_count = 0;
    if (!orion_user_space_create(space)) {
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "Could not create the user page-table root");
        return false;
    }

    for (uint32_t segment_index = 0;
         segment_index < image->segment_count; ++segment_index) {
        const orion_elf64_segment *segment =
            &image->segments[segment_index];
        uint32_t flags = USER_SEGMENT_FLAGS;
        flags |= (segment->flags & ELF_PF_W) != 0 ? ORION_VM_WRITE : 0U;
        flags |= (segment->flags & ELF_PF_X) != 0 ? ORION_VM_EXECUTE : 0U;
        const uint64_t rounded_bytes =
            (segment->memory_bytes + PAGE_MASK) & ~PAGE_MASK;
        for (uint64_t offset = 0; offset < rounded_bytes;
             offset += ORION_MEMORY_PAGE_SIZE) {
            if (!map_page(space, segment->virtual_address + offset, flags)) {
                (void)orion_log_u64(ORION_LOG_ERROR, "USER",
                                    "Could not map user segment page:",
                                    segment_index);
                (void)orion_elf64_destroy_user(space);
                return false;
            }
        }
        if (!copy_into_user_space(space, segment->virtual_address,
                                  image->image + segment->offset,
                                  segment->file_bytes)) {
            (void)orion_log_u64(ORION_LOG_ERROR, "USER",
                                "Could not populate user segment:",
                                segment_index);
            (void)orion_elf64_destroy_user(space);
            return false;
        }
    }

    const uint64_t stack_bottom =
        USER_EXIT_STACK_TOP - ORION_USER_STACK_BYTES;
    if (!map_page(space, stack_bottom,
                  ORION_VM_USER | ORION_VM_READ | ORION_VM_WRITE) ||
        !orion_user_space_check(space, image->entry, 1,
                                ORION_VM_READ | ORION_VM_EXECUTE)) {
        (void)orion_log(ORION_LOG_ERROR, "USER",
                        "Could not map user stack or validate entry");
        (void)orion_elf64_destroy_user(space);
        return false;
    }
    *user_entry = image->entry;
    *user_stack = USER_EXIT_STACK_TOP;
    return true;
}

bool orion_elf64_destroy_user(orion_user_address_space *space)
{
    if (space == NULL || !space->initialized ||
        (orion_x86_64_read_cr3() & CR3_ROOT_MASK) ==
            space->root_physical.value) {
        return false;
    }
    bool result = true;
    while (mapped_page_count != 0) {
        const user_page page = mapped_pages[--mapped_page_count];
        orion_physical_address unmapped = {0};
        if (!orion_user_space_unmap(
                space, (orion_virtual_address){page.address}, &unmapped) ||
            unmapped.value != page.physical ||
            !orion_physical_release(page.physical)) {
            result = false;
        }
    }
    return orion_user_space_destroy(space) && result;
}
