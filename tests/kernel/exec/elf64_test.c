#include "kernel/exec/elf64.h"

#include <stddef.h>
#include <stdint.h>

#include "kernel/arch/x86_64/cpu_state.h"
#include "kernel/log/log.h"
#include "kernel/mm/physical.h"
#include <orion/boot_info.h>

#define TEST_ELF_BYTES 4112U
#define TEST_PROGRAM_HEADER_OFFSET 64U
#define TEST_PROGRAM_HEADER_BYTES 56U

static uint8_t elf[TEST_ELF_BYTES];
static uint8_t oversized_elf[ORION_BOOT_APPLICATION_MAX_BYTES + 1U];
static unsigned int failures;

bool orion_log(orion_log_level level, const char *component,
               const char *message)
{
    (void)level;
    (void)component;
    (void)message;
    return true;
}

bool orion_log_u64(orion_log_level level, const char *component,
                   const char *message, uint64_t value)
{
    (void)level;
    (void)component;
    (void)message;
    (void)value;
    return true;
}

bool orion_x86_64_interrupts_enabled(void)
{
    return false;
}

uint64_t orion_x86_64_read_cr3(void)
{
    return 0;
}

bool orion_physical_allocate(uint64_t alignment_pages,
                             uint64_t *physical_address)
{
    (void)alignment_pages;
    (void)physical_address;
    return false;
}

bool orion_physical_release(uint64_t physical_address)
{
    (void)physical_address;
    return false;
}

bool orion_user_space_create(orion_user_address_space *space)
{
    (void)space;
    return false;
}

bool orion_user_space_map(orion_user_address_space *space,
                          orion_virtual_address address,
                          orion_physical_address frame, uint32_t flags)
{
    (void)space;
    (void)address;
    (void)frame;
    (void)flags;
    return false;
}

bool orion_user_space_unmap(orion_user_address_space *space,
                            orion_virtual_address address,
                            orion_physical_address *frame)
{
    (void)space;
    (void)address;
    (void)frame;
    return false;
}

bool orion_user_space_check(const orion_user_address_space *space,
                            uint64_t address, uint64_t length,
                            uint32_t required_flags)
{
    (void)space;
    (void)address;
    (void)length;
    (void)required_flags;
    return false;
}

bool orion_user_space_destroy(orion_user_address_space *space)
{
    (void)space;
    return false;
}

bool orion_x86_64_page_lookup(orion_page_table_space *space,
                              orion_virtual_address virtual_address,
                              orion_physical_address *physical_address,
                              uint32_t *flags)
{
    (void)space;
    (void)virtual_address;
    (void)physical_address;
    (void)flags;
    return false;
}

static void write16(uint8_t *address, uint16_t value)
{
    address[0] = (uint8_t)value;
    address[1] = (uint8_t)(value >> 8);
}

static void write32(uint8_t *address, uint32_t value)
{
    for (uint32_t index = 0; index < 4; ++index) {
        address[index] = (uint8_t)(value >> (index * 8U));
    }
}

static void write64(uint8_t *address, uint64_t value)
{
    for (uint32_t index = 0; index < 8; ++index) {
        address[index] = (uint8_t)(value >> (index * 8U));
    }
}

static void initialize_elf(uint16_t program_count)
{
    for (uint32_t index = 0; index < sizeof(elf); ++index) {
        elf[index] = 0;
    }
    elf[0] = 0x7f;
    elf[1] = 'E';
    elf[2] = 'L';
    elf[3] = 'F';
    elf[4] = 2;
    elf[5] = 1;
    elf[6] = 1;
    write16(elf + 16, 2);
    write16(elf + 18, 62);
    write32(elf + 20, 1);
    write64(elf + 24, ORION_USER_BASE);
    write64(elf + 32, TEST_PROGRAM_HEADER_OFFSET);
    write16(elf + 52, 64);
    write16(elf + 54, TEST_PROGRAM_HEADER_BYTES);
    write16(elf + 56, program_count);

    uint8_t *program = elf + TEST_PROGRAM_HEADER_OFFSET;
    write32(program, 1);
    write32(program + 4, 5);
    write64(program + 8, 0x1000);
    write64(program + 16, ORION_USER_BASE);
    write64(program + 32, 16);
    write64(program + 40, 32);
    write64(program + 48, 0x1000);
}

static void expect_status(orion_elf64_status expected, const char *name)
{
    orion_elf64_image parsed;
    const orion_elf64_status actual =
        orion_elf64_validate(elf, sizeof(elf), &parsed);
    if (actual != expected) {
        (void)name;
        ++failures;
    }
}

int main(void)
{
    initialize_elf(1);
    orion_elf64_image parsed;
    failures += orion_elf64_validate(elf, sizeof(elf), &parsed) !=
                ORION_ELF64_OK;
    failures += parsed.entry != ORION_USER_BASE;
    failures += parsed.segment_count != 1;
    failures += parsed.page_count != 2;

    failures += orion_elf64_validate(NULL, sizeof(elf), &parsed) !=
                ORION_ELF64_INVALID;
    failures += orion_elf64_validate(elf, 63, &parsed) !=
                ORION_ELF64_INVALID;
    failures += orion_elf64_validate(
                    (const void *)(UINTPTR_MAX - 8U), sizeof(elf),
                    &parsed) != ORION_ELF64_INVALID;
    failures += orion_elf64_validate(elf, sizeof(elf), &parsed) !=
                ORION_ELF64_OK;

    elf[0] = 0;
    expect_status(ORION_ELF64_UNSUPPORTED, "bad magic");
    initialize_elf(1);
    write16(elf + 18, 3);
    expect_status(ORION_ELF64_UNSUPPORTED, "wrong machine");
    initialize_elf(1);
    write64(elf + 32, UINT64_MAX);
    expect_status(ORION_ELF64_INVALID, "program table overflow");

    initialize_elf(1);
    write64(elf + TEST_PROGRAM_HEADER_OFFSET + 8, 0x2000);
    expect_status(ORION_ELF64_INVALID, "file range outside image");
    initialize_elf(1);
    write32(elf + TEST_PROGRAM_HEADER_OFFSET + 4, 7);
    expect_status(ORION_ELF64_INVALID, "writable executable segment");
    initialize_elf(1);
    write32(elf + TEST_PROGRAM_HEADER_OFFSET + 4, 1);
    expect_status(ORION_ELF64_INVALID, "segment not readable");
    initialize_elf(1);
    write64(elf + TEST_PROGRAM_HEADER_OFFSET + 16,
            ORION_USER_IMAGE_LIMIT - 0x1000);
    write64(elf + TEST_PROGRAM_HEADER_OFFSET + 40, 0x2000);
    expect_status(ORION_ELF64_INVALID, "segment outside user image window");
    initialize_elf(2);
    uint8_t *second = elf + TEST_PROGRAM_HEADER_OFFSET +
                      TEST_PROGRAM_HEADER_BYTES;
    for (uint32_t index = 0; index < TEST_PROGRAM_HEADER_BYTES; ++index) {
        second[index] = elf[TEST_PROGRAM_HEADER_OFFSET + index];
    }
    expect_status(ORION_ELF64_INVALID, "overlapping load segments");
    initialize_elf(1);
    write32(elf + TEST_PROGRAM_HEADER_OFFSET, 3);
    expect_status(ORION_ELF64_UNSUPPORTED, "interpreter segment");
    initialize_elf(1);
    write64(elf + 24, ORION_USER_BASE + 0x1000);
    expect_status(ORION_ELF64_INVALID, "entry outside executable segment");
    initialize_elf(1);
    failures += orion_elf64_validate(
                    oversized_elf,
                    ORION_BOOT_APPLICATION_MAX_BYTES + 1U, &parsed) !=
                ORION_ELF64_TOO_LARGE;

    return failures == 0 ? 0 : 1;
}
