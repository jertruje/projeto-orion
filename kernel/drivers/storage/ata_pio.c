#include "ata_pio.h"

#include <stddef.h>
#include <stdint.h>

#include "../../arch/x86_64/interrupt_state.h"

#define ATA_PRIMARY_DATA UINT16_C(0x1f0)
#define ATA_PRIMARY_ERROR UINT16_C(0x1f1)
#define ATA_PRIMARY_SECTOR_COUNT UINT16_C(0x1f2)
#define ATA_PRIMARY_LBA_LOW UINT16_C(0x1f3)
#define ATA_PRIMARY_LBA_MID UINT16_C(0x1f4)
#define ATA_PRIMARY_LBA_HIGH UINT16_C(0x1f5)
#define ATA_PRIMARY_DRIVE UINT16_C(0x1f6)
#define ATA_PRIMARY_STATUS UINT16_C(0x1f7)
#define ATA_PRIMARY_COMMAND UINT16_C(0x1f7)
#define ATA_PRIMARY_CONTROL UINT16_C(0x3f6)
#define ATA_COMMAND_IDENTIFY UINT8_C(0xec)
#define ATA_COMMAND_READ_SECTORS UINT8_C(0x20)
#define ATA_COMMAND_WRITE_SECTORS UINT8_C(0x30)
#define ATA_COMMAND_CACHE_FLUSH UINT8_C(0xe7)
#define ATA_STATUS_ERROR UINT8_C(0x01)
#define ATA_STATUS_DRQ UINT8_C(0x08)
#define ATA_STATUS_DEVICE_FAULT UINT8_C(0x20)
#define ATA_STATUS_BUSY UINT8_C(0x80)
#define ATA_POLL_LIMIT UINT32_C(1000000)
#define ATA_LBA28_LIMIT UINT64_C(0x10000000)
#define ATA_SECTOR_BYTES 512U

static uint8_t identify_buffer[ATA_SECTOR_BYTES] __attribute__((aligned(8)));
static orion_block_device ata_block_device;
static bool ata_initialized;
static bool ata_command_busy;

static void out8(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1"
                     :
                     : "a"(value), "Nd"(port)
                     : "memory");
}

static uint8_t in8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0"
                     : "=a"(value)
                     : "Nd"(port)
                     : "memory");
    return value;
}

static uint16_t in16(uint16_t port)
{
    uint16_t value;
    __asm__ volatile("inw %1, %0"
                     : "=a"(value)
                     : "Nd"(port)
                     : "memory");
    return value;
}

static void out16(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %0, %1"
                     :
                     : "a"(value), "Nd"(port)
                     : "memory");
}

static void ata_delay_400ns(void)
{
    (void)in8(ATA_PRIMARY_CONTROL);
    (void)in8(ATA_PRIMARY_CONTROL);
    (void)in8(ATA_PRIMARY_CONTROL);
    (void)in8(ATA_PRIMARY_CONTROL);
}

static orion_block_status wait_status(bool require_data)
{
    for (uint32_t attempt = 0; attempt < ATA_POLL_LIMIT; ++attempt) {
        const uint8_t status = in8(ATA_PRIMARY_STATUS);
        if (status == 0 || status == UINT8_MAX) {
            return ORION_BLOCK_IO_ERROR;
        }
        if ((status & ATA_STATUS_BUSY) != 0) {
            continue;
        }
        if ((status & ATA_STATUS_ERROR) != 0 ||
            (status & ATA_STATUS_DEVICE_FAULT) != 0) {
            (void)in8(ATA_PRIMARY_ERROR);
            return ORION_BLOCK_IO_ERROR;
        }
        if (!require_data || (status & ATA_STATUS_DRQ) != 0) {
            return ORION_BLOCK_OK;
        }
    }
    return ORION_BLOCK_IO_ERROR;
}

static bool command_acquire(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    if (ata_command_busy) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }
    ata_command_busy = true;
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

static void command_release(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    ata_command_busy = false;
    orion_x86_64_interrupts_restore(was_enabled);
}

static void select_lba28(uint64_t lba)
{
    out8(ATA_PRIMARY_DRIVE,
         (uint8_t)(UINT8_C(0xe0) | ((lba >> 24) & UINT64_C(0x0f))));
    ata_delay_400ns();
}

static void program_lba28(uint64_t lba, uint8_t sector_count)
{
    out8(ATA_PRIMARY_SECTOR_COUNT, sector_count);
    out8(ATA_PRIMARY_LBA_LOW, (uint8_t)lba);
    out8(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    out8(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16));
}

static bool device_identify(uint64_t *sector_count)
{
    const uint8_t initial_status = in8(ATA_PRIMARY_STATUS);
    if (initial_status == 0 || initial_status == UINT8_MAX) {
        return false;
    }
    select_lba28(0);
    out8(ATA_PRIMARY_SECTOR_COUNT, 0);
    out8(ATA_PRIMARY_LBA_LOW, 0);
    out8(ATA_PRIMARY_LBA_MID, 0);
    out8(ATA_PRIMARY_LBA_HIGH, 0);
    out8(ATA_PRIMARY_COMMAND, ATA_COMMAND_IDENTIFY);
    const uint8_t status = in8(ATA_PRIMARY_STATUS);
    if (status == 0 || status == UINT8_MAX) {
        return false;
    }
    if (wait_status(false) != ORION_BLOCK_OK ||
        in8(ATA_PRIMARY_LBA_MID) != 0 ||
        in8(ATA_PRIMARY_LBA_HIGH) != 0 ||
        wait_status(true) != ORION_BLOCK_OK) {
        return false;
    }
    for (uint32_t index = 0; index < 256U; ++index) {
        const uint16_t word = in16(ATA_PRIMARY_DATA);
        identify_buffer[index * 2U] = (uint8_t)word;
        identify_buffer[index * 2U + 1U] = (uint8_t)(word >> 8);
    }
    const uint16_t word106 =
        (uint16_t)identify_buffer[212] |
        (uint16_t)((uint16_t)identify_buffer[213] << 8);
    if ((word106 & UINT16_C(0xc000)) == UINT16_C(0x4000) &&
        (word106 & UINT16_C(0x1000)) != 0) {
        const uint32_t logical_sector_words =
            (uint32_t)identify_buffer[234] |
            ((uint32_t)identify_buffer[235] << 8) |
            ((uint32_t)identify_buffer[236] << 16) |
            ((uint32_t)identify_buffer[237] << 24);
        if (logical_sector_words != ATA_SECTOR_BYTES / 2U) {
            return false;
        }
    }
    const uint32_t lba28_sectors =
        (uint32_t)identify_buffer[120] |
        ((uint32_t)identify_buffer[121] << 8) |
        ((uint32_t)identify_buffer[122] << 16) |
        ((uint32_t)identify_buffer[123] << 24);
    if (lba28_sectors == 0) {
        return false;
    }
    *sector_count = lba28_sectors < ATA_LBA28_LIMIT
                        ? lba28_sectors
                        : ATA_LBA28_LIMIT;
    return true;
}

static orion_block_status ata_read(void *context, uint64_t first_block,
                                   uint32_t block_count, void *buffer)
{
    orion_block_device *device = context;
    if (device == NULL || buffer == NULL || block_count == 0 ||
        first_block >= device->block_count ||
        block_count > device->block_count - first_block ||
        first_block >= ATA_LBA28_LIMIT ||
        block_count > ATA_LBA28_LIMIT - first_block) {
        return ORION_BLOCK_OUT_OF_RANGE;
    }
    if (!command_acquire()) {
        return ORION_BLOCK_BUSY;
    }
    uint8_t *destination = buffer;
    orion_block_status result = ORION_BLOCK_OK;
    for (uint32_t index = 0; index < block_count; ++index) {
        const uint64_t lba = first_block + index;
        select_lba28(lba);
        program_lba28(lba, 1);
        out8(ATA_PRIMARY_COMMAND, ATA_COMMAND_READ_SECTORS);
        if (wait_status(true) != ORION_BLOCK_OK) {
            result = ORION_BLOCK_IO_ERROR;
            break;
        }
        for (uint32_t word = 0; word < ATA_SECTOR_BYTES / 2U; ++word) {
            const uint16_t value = in16(ATA_PRIMARY_DATA);
            destination[index * ATA_SECTOR_BYTES + word * 2U] =
                (uint8_t)value;
            destination[index * ATA_SECTOR_BYTES + word * 2U + 1U] =
                (uint8_t)(value >> 8);
        }
        ata_delay_400ns();
    }
    command_release();
    return result;
}

static orion_block_status ata_write(void *context, uint64_t first_block,
                                    uint32_t block_count, const void *buffer)
{
    orion_block_device *device = context;
    if (device == NULL || buffer == NULL || block_count == 0 ||
        first_block >= device->block_count ||
        block_count > device->block_count - first_block ||
        first_block >= ATA_LBA28_LIMIT ||
        block_count > ATA_LBA28_LIMIT - first_block) {
        return ORION_BLOCK_OUT_OF_RANGE;
    }
    if (!command_acquire()) {
        return ORION_BLOCK_BUSY;
    }
    const uint8_t *source = buffer;
    orion_block_status result = ORION_BLOCK_OK;
    for (uint32_t index = 0; index < block_count; ++index) {
        const uint64_t lba = first_block + index;
        select_lba28(lba);
        program_lba28(lba, 1);
        out8(ATA_PRIMARY_COMMAND, ATA_COMMAND_WRITE_SECTORS);
        if (wait_status(true) != ORION_BLOCK_OK) {
            result = ORION_BLOCK_IO_ERROR;
            break;
        }
        for (uint32_t word = 0; word < ATA_SECTOR_BYTES / 2U; ++word) {
            const uint16_t value =
                (uint16_t)source[index * ATA_SECTOR_BYTES + word * 2U] |
                (uint16_t)((uint16_t)source[index * ATA_SECTOR_BYTES +
                                             word * 2U + 1U]
                           << 8);
            out16(ATA_PRIMARY_DATA, value);
        }
        ata_delay_400ns();
        if (wait_status(false) != ORION_BLOCK_OK) {
            result = ORION_BLOCK_IO_ERROR;
            break;
        }
    }
    if (result == ORION_BLOCK_OK) {
        select_lba28(first_block + block_count - 1U);
        out8(ATA_PRIMARY_COMMAND, ATA_COMMAND_CACHE_FLUSH);
        result = wait_status(false);
    }
    command_release();
    return result;
}

static orion_block_status ata_sync(void *context)
{
    orion_block_device *device = context;
    if (device == NULL || !device->available) {
        return ORION_BLOCK_UNAVAILABLE;
    }
    if (!command_acquire()) {
        return ORION_BLOCK_BUSY;
    }
    select_lba28(0);
    out8(ATA_PRIMARY_COMMAND, ATA_COMMAND_CACHE_FLUSH);
    const orion_block_status result = wait_status(false);
    command_release();
    return result;
}

bool orion_ata_pio_prepare(void)
{
    if (ata_initialized || !command_acquire()) {
        return false;
    }
    uint64_t sectors = 0;
    const bool detected = device_identify(&sectors);
    command_release();
    if (!detected) {
        return false;
    }
    ata_block_device = (orion_block_device){
        .identifier = "ata-primary-master",
        .block_count = sectors,
        .block_size = ATA_SECTOR_BYTES,
        .buffer_alignment = 2,
        .available = true,
        .read_only = false,
        .context = &ata_block_device,
        .read = ata_read,
        .write = ata_write,
        .sync = ata_sync
    };
    ata_initialized = true;
    return true;
}

orion_block_device *orion_ata_pio_block_device(void)
{
    return ata_initialized ? &ata_block_device : NULL;
}

static bool driver_match(const orion_device *device)
{
    static const char expected[] = "ata-primary-master";
    if (device == NULL || device->bus != ORION_DEVICE_PLATFORM ||
        device->platform_name == NULL) {
        return false;
    }
    for (uint32_t index = 0; index < sizeof(expected); ++index) {
        if (device->platform_name[index] != expected[index]) {
            return false;
        }
        if (expected[index] == '\0') {
            return true;
        }
    }
    return false;
}

static bool driver_start(const orion_device *device, void **driver_data)
{
    (void)device;
    if (!ata_initialized || driver_data == NULL ||
        !orion_block_device_is_valid(&ata_block_device)) {
        return false;
    }
    *driver_data = &ata_block_device;
    return true;
}

static void driver_cleanup(const orion_device *device, void *driver_data)
{
    (void)device;
    (void)driver_data;
}

const orion_driver *orion_ata_pio_driver(void)
{
    static const orion_driver driver = {
        .name = "ata-pio-primary",
        .match = driver_match,
        .start = driver_start,
        .cleanup = driver_cleanup
    };
    return &driver;
}
