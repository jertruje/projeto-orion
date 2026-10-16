#include <stddef.h>
#include <stdint.h>
#include <orion/boot_info.h>

typedef uint64_t efi_status;
typedef void *efi_handle;
typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} efi_guid;

typedef efi_status (*efi_handle_protocol_fn)(efi_handle, const efi_guid *, void **);
typedef efi_status (*efi_locate_handle_buffer_fn)(uint32_t, const efi_guid *, void *,
                                                   uint64_t *, efi_handle **);
typedef efi_status (*efi_get_memory_map_fn)(uint64_t *, void *, uint64_t *, uint64_t *, uint32_t *);
typedef efi_status (*efi_allocate_pages_fn)(uint32_t, uint32_t, uint64_t, uint64_t *);
typedef efi_status (*efi_exit_boot_services_fn)(efi_handle, uint64_t);
typedef efi_status (*efi_open_volume_fn)(void *, void **);
typedef efi_status (*efi_file_open_fn)(void *, void **, const uint16_t *, uint64_t, uint64_t);
typedef efi_status (*efi_file_close_fn)(void *);
typedef efi_status (*efi_file_read_fn)(void *, uint64_t *, void *);

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t reserved;
    void *parent_handle;
    void *system_table;
    void *device_handle;
    void *file_path;
    void *reserved2;
    uint32_t load_options_size;
    uint32_t padding;
    void *load_options;
    void *image_base;
    uint64_t image_size;
    uint32_t image_code_type;
    uint32_t image_data_type;
    void *unload;
} loaded_image;

typedef struct {
    uint64_t revision;
    efi_open_volume_fn open_volume;
} simple_file_system;

typedef struct {
    uint64_t revision;
    efi_file_open_fn open;
    efi_file_close_fn close;
    void *delete_file;
    efi_file_read_fn read;
} efi_file_prefix;

typedef struct {
    uint8_t header[24];
    void *services[40];
} boot_services_prefix;

typedef struct {
    uint8_t header[24];
    void *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t padding;
    void *console_in_handle;
    void *console_in;
    void *console_out_handle;
    void *console_out;
    void *standard_error_handle;
    void *standard_error;
    void *runtime_services;
    boot_services_prefix *boot_services;
} system_table_prefix;

typedef struct {
    unsigned char ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf64_header;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} elf64_program_header;

typedef void (__attribute__((sysv_abi)) *kernel_entry)(const orion_boot_info *);

enum {
    EFI_ERROR_BIT = 0x8000000000000000ULL,
    EFI_INVALID_PARAMETER = 2,
    EFI_BUFFER_TOO_SMALL = 5,
    EFI_ALLOCATE_ADDRESS = 2,
    EFI_LOADER_DATA = 2,
    EFI_PAGE_SIZE = 4096,
    MAX_KERNEL_BYTES = 2 * 1024 * 1024,
    MAX_MEMORY_MAP_BYTES = 256 * 1024,
    MAX_PROGRAM_HEADERS = 32,
    PT_LOAD = 1,
    PF_X = 1
};

#define COM1 0x3f8
extern void orion_outb(uint16_t port, uint8_t value);
extern uint8_t orion_inb(uint16_t port);

static const efi_guid loaded_image_guid = {
    0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
static const efi_guid simple_file_system_guid = {
    0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static uint8_t kernel_image[MAX_KERNEL_BYTES];
_Alignas(8) static uint8_t memory_map[MAX_MEMORY_MAP_BYTES];
static uint16_t kernel_path[] = {'\\', 'k', 'e', 'r', 'n', 'e', 'l', '.', 'e', 'l', 'f', 0};

static void serial_init(void)
{
    orion_outb(COM1 + 1, 0x00);
    orion_outb(COM1 + 3, 0x80);
    orion_outb(COM1 + 0, 0x03);
    orion_outb(COM1 + 1, 0x00);
    orion_outb(COM1 + 3, 0x03);
    orion_outb(COM1 + 2, 0xc7);
    orion_outb(COM1 + 4, 0x0b);
}

static void serial_putc(char c)
{
    uint32_t timeout = 1000000;
    while ((orion_inb(COM1 + 5) & 0x20) == 0 && timeout != 0) {
        --timeout;
    }
    if (timeout != 0) {
        orion_outb(COM1, (uint8_t)c);
    }
}

static void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            serial_putc('\r');
        }
        serial_putc(*text++);
    }
}

static void serial_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_putc(digits[(value >> shift) & 0xf]);
    }
}

static void serial_line_hex(const char *label, uint64_t value)
{
    serial_write(label);
    serial_write("0x");
    serial_hex(value);
    serial_write("\n");
}

_Noreturn static void stop(const char *message)
{
    serial_write("[ORION][UEFI][ERROR] ");
    serial_write(message);
    serial_write("\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

static int add_overflows(uint64_t left, uint64_t right)
{
    return right > UINT64_MAX - left;
}

static int range_valid(uint64_t offset, uint64_t length, uint64_t limit)
{
    return !add_overflows(offset, length) && offset + length <= limit;
}

static efi_status handle_protocol(system_table_prefix *system_table,
                                  efi_handle handle,
                                  const efi_guid *guid,
                                  void **interface)
{
    efi_handle_protocol_fn function =
        (efi_handle_protocol_fn)system_table->boot_services->services[16];
    return function(handle, guid, interface);
}

static uint64_t read_kernel(system_table_prefix *system_table, efi_handle image_handle)
{
    loaded_image *image = 0;
    simple_file_system *file_system = 0;
    void *root = 0;
    void *file = 0;

    if (handle_protocol(system_table, image_handle, &loaded_image_guid, (void **)&image) & EFI_ERROR_BIT) {
        stop("Loaded Image protocol unavailable");
    }
    if (image == 0 || image->device_handle == 0) {
        stop("UEFI image has no source device");
    }
    efi_locate_handle_buffer_fn locate_handle_buffer =
        (efi_locate_handle_buffer_fn)system_table->boot_services->services[36];
    uint64_t handle_count = 0;
    efi_handle *file_system_handles = 0;
    efi_status status = locate_handle_buffer(2, &simple_file_system_guid, 0,
                                             &handle_count, &file_system_handles);
    if (status & EFI_ERROR_BIT) {
        serial_line_hex("[ORION][UEFI][ERROR] SimpleFS handle search status: ", status);
        stop("No Simple File System handles are available");
    }
    if (handle_count == 0 || file_system_handles == 0) {
        stop("UEFI returned an empty Simple File System handle list");
    }
    for (uint64_t index = 0; index < handle_count && root == 0; ++index) {
        file_system = 0;
        if (handle_protocol(system_table, file_system_handles[index],
                            &simple_file_system_guid, (void **)&file_system) &
            EFI_ERROR_BIT) {
            continue;
        }
        if (file_system != 0 && file_system->open_volume != 0 &&
            !(file_system->open_volume(file_system, &root) & EFI_ERROR_BIT)) {
            break;
        }
        root = 0;
    }
    if (root == 0) {
        stop("Could not open boot volume");
    }

    efi_file_prefix *root_file = (efi_file_prefix *)root;
    if (root_file->open(root_file, &file, kernel_path, 1, 0) & EFI_ERROR_BIT || file == 0) {
        stop("kernel.elf not found at boot volume root");
    }

    uint64_t total = 0;
    efi_file_prefix *kernel_file = (efi_file_prefix *)file;
    for (;;) {
        uint64_t chunk = 4096;
        if (total > MAX_KERNEL_BYTES - chunk) {
            kernel_file->close(kernel_file);
            stop("kernel.elf exceeds the 2 MiB loader limit");
        }
        efi_status status = kernel_file->read(kernel_file, &chunk, kernel_image + total);
        if (status & EFI_ERROR_BIT) {
            kernel_file->close(kernel_file);
            stop("Failed while reading kernel.elf");
        }
        if (chunk > 4096) {
            kernel_file->close(kernel_file);
            stop("UEFI file protocol returned an invalid read length");
        }
        total += chunk;
        if (chunk < 4096) {
            break;
        }
    }
    kernel_file->close(kernel_file);
    root_file->close(root_file);
    return total;
}

static const elf64_header *validate_elf(uint64_t image_size)
{
    if (image_size < sizeof(elf64_header)) {
        stop("ELF header is truncated");
    }
    const elf64_header *header = (const elf64_header *)kernel_image;
    if (header->ident[0] != 0x7f || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != 2 || header->ident[5] != 1 ||
        header->type != 2 || header->machine != 62 ||
        header->version != 1 || header->ehsize != sizeof(elf64_header) ||
        header->phentsize != sizeof(elf64_program_header) ||
        header->phnum == 0 || header->phnum > MAX_PROGRAM_HEADERS ||
        !range_valid(header->phoff,
                     (uint64_t)header->phnum * header->phentsize,
                     image_size)) {
        stop("ELF64 x86-64 executable header is invalid or unsupported");
    }
    return header;
}

static void load_elf(system_table_prefix *system_table,
                     const elf64_header *header,
                     uint64_t image_size)
{
    efi_allocate_pages_fn allocate_pages =
        (efi_allocate_pages_fn)system_table->boot_services->services[2];
    const elf64_program_header *programs =
        (const elf64_program_header *)(kernel_image + header->phoff);
    int entry_is_executable = 0;
    uint32_t loaded_segments = 0;

    for (uint16_t index = 0; index < header->phnum; ++index) {
        const elf64_program_header *program = &programs[index];
        if (program->type != PT_LOAD) {
            continue;
        }
        if (program->memsz < program->filesz ||
            !range_valid(program->offset, program->filesz, image_size) ||
            program->memsz == 0 ||
            add_overflows(program->vaddr, program->memsz) ||
            program->vaddr < 0x200000 ||
            program->vaddr + program->memsz > 0x4000000 ||
            (program->vaddr & (EFI_PAGE_SIZE - 1)) != 0 ||
            (program->offset & (EFI_PAGE_SIZE - 1)) != 0 ||
            (program->align > EFI_PAGE_SIZE && program->align != 0)) {
            stop("ELF load segment has invalid bounds, alignment, or size");
        }

        uint64_t pages = (program->memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
        uint64_t address = program->vaddr;
        if (allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, pages, &address) & EFI_ERROR_BIT) {
            stop("Could not reserve the ELF load segment at its declared address");
        }

        const elf64_program_header *earlier =
            (const elf64_program_header *)(kernel_image + header->phoff);
        for (uint16_t prior_index = 0; prior_index < index; ++prior_index) {
            if (earlier[prior_index].type == PT_LOAD) {
                uint64_t prior_pages =
                    (earlier[prior_index].memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
                uint64_t prior_end = earlier[prior_index].vaddr + prior_pages * EFI_PAGE_SIZE;
                uint64_t end = program->vaddr + pages * EFI_PAGE_SIZE;
                if (program->vaddr < prior_end && earlier[prior_index].vaddr < end) {
                    stop("ELF load segments overlap");
                }
            }
        }

        uint8_t *destination = (uint8_t *)(uintptr_t)program->vaddr;
        for (uint64_t byte = 0; byte < program->memsz; ++byte) {
            destination[byte] = 0;
        }
        for (uint64_t byte = 0; byte < program->filesz; ++byte) {
            destination[byte] = kernel_image[program->offset + byte];
        }
        if ((program->flags & PF_X) != 0 &&
            header->entry >= program->vaddr &&
            header->entry < program->vaddr + program->memsz) {
            entry_is_executable = 1;
        }
        ++loaded_segments;
    }
    if (loaded_segments == 0 || !entry_is_executable) {
        stop("ELF entry point is not within an executable load segment");
    }
}

efi_status efi_main(efi_handle image_handle, system_table_prefix *system_table)
{
    serial_init();
    serial_write("[ORION][UEFI][INFO] UEFI x86-64 loader started\n");
    if (system_table == 0 || system_table->boot_services == 0) {
        stop("Invalid UEFI system table");
    }

    uint64_t image_size = read_kernel(system_table, image_handle);
    const elf64_header *header = validate_elf(image_size);
    serial_write("[ORION][UEFI][INFO] kernel ELF validated\n");
    load_elf(system_table, header, image_size);
    serial_write("[ORION][UEFI][INFO] kernel load segments reserved\n");

    efi_get_memory_map_fn get_memory_map =
        (efi_get_memory_map_fn)system_table->boot_services->services[4];
    efi_exit_boot_services_fn exit_boot_services =
        (efi_exit_boot_services_fn)system_table->boot_services->services[26];
    uint64_t map_size = sizeof(memory_map);
    uint64_t map_key = 0;
    uint64_t descriptor_size = 0;
    uint32_t descriptor_version = 0;
    efi_status status = 0;
    orion_boot_info boot_info = {
        ORION_BOOT_INFO_SIGNATURE, ORION_BOOT_INFO_VERSION, 0, memory_map, 0, 0
    };

    for (uint32_t attempt = 0; attempt < 4; ++attempt) {
        map_size = sizeof(memory_map);
        status = get_memory_map(&map_size, memory_map, &map_key,
                                &descriptor_size, &descriptor_version);
        if (status & EFI_ERROR_BIT) {
            if ((status & ~EFI_ERROR_BIT) == EFI_BUFFER_TOO_SMALL) {
                stop("UEFI memory map exceeds the fixed 256 KiB map buffer");
            }
            stop("Could not retrieve the final UEFI memory map");
        }
        boot_info.memory_map_size = map_size;
        boot_info.descriptor_size = descriptor_size;
        status = exit_boot_services(image_handle, map_key);
        if ((status & EFI_ERROR_BIT) == 0) {
            break;
        }
        if ((status & ~EFI_ERROR_BIT) != EFI_INVALID_PARAMETER) {
            stop("ExitBootServices failed");
        }
    }
    if (status & EFI_ERROR_BIT) {
        stop("ExitBootServices map key remained stale after four attempts");
    }

    kernel_entry entry = (kernel_entry)(uintptr_t)header->entry;
    entry(&boot_info);
    stop("Kernel entry unexpectedly returned");
}
