#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../kernel/arch/x86_64/interrupt_state.h"
#include "../../../kernel/fs/orionfs.h"
#include "../../../kernel/storage/block.h"

#define TEST_BLOCKS 32U
#define TEST_BLOCK_SIZE 512U

static uint8_t disk[TEST_BLOCKS][TEST_BLOCK_SIZE] __attribute__((aligned(8)));
static bool fail_next_read;
static bool fail_next_write;
static uint32_t listed_entries;

static orion_block_status memory_read(void *context, uint64_t first_block,
                                      uint32_t block_count, void *buffer)
{
    (void)context;
    if (fail_next_read) {
        fail_next_read = false;
        return ORION_BLOCK_IO_ERROR;
    }
    for (uint32_t index = 0; index < block_count; ++index) {
        const uint8_t *source = disk[first_block + index];
        uint8_t *destination =
            (uint8_t *)buffer + (uint64_t)index * TEST_BLOCK_SIZE;
        for (uint32_t byte = 0; byte < TEST_BLOCK_SIZE; ++byte) {
            destination[byte] = source[byte];
        }
    }
    return ORION_BLOCK_OK;
}

static orion_block_status memory_write(void *context, uint64_t first_block,
                                       uint32_t block_count,
                                       const void *buffer)
{
    (void)context;
    if (fail_next_write) {
        fail_next_write = false;
        return ORION_BLOCK_IO_ERROR;
    }
    for (uint32_t index = 0; index < block_count; ++index) {
        const uint8_t *source =
            (const uint8_t *)buffer + (uint64_t)index * TEST_BLOCK_SIZE;
        uint8_t *destination = disk[first_block + index];
        for (uint32_t byte = 0; byte < TEST_BLOCK_SIZE; ++byte) {
            destination[byte] = source[byte];
        }
    }
    return ORION_BLOCK_OK;
}

static orion_block_status memory_sync(void *context)
{
    (void)context;
    return ORION_BLOCK_OK;
}

static bool require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "storage test failed: %s\n", message);
        return false;
    }
    return true;
}

static bool collect_entry(void *context,
                          const orionfs_directory_entry *entry)
{
    (void)context;
    if (entry == NULL || entry->name == NULL) {
        return false;
    }
    ++listed_entries;
    return true;
}

int main(void)
{
    orion_block_device device = {
        .identifier = "host-memory-test",
        .block_count = TEST_BLOCKS,
        .block_size = TEST_BLOCK_SIZE,
        .buffer_alignment = 2,
        .available = true,
        .read_only = false,
        .context = NULL,
        .read = memory_read,
        .write = memory_write,
        .sync = memory_sync
    };
    uint8_t aligned_buffer[TEST_BLOCK_SIZE * 2U] __attribute__((aligned(8)));
    uint8_t unaligned_buffer[TEST_BLOCK_SIZE + 1U];
    if (!require(orion_block_device_is_valid(&device),
                 "valid block device accepted") ||
        !require(orion_block_read(&device, TEST_BLOCKS - 1U, 1,
                                  aligned_buffer, TEST_BLOCK_SIZE) ==
                     ORION_BLOCK_OK,
                 "read last block") ||
        !require(orion_block_read(&device, TEST_BLOCKS, 1, aligned_buffer,
                                  TEST_BLOCK_SIZE) ==
                     ORION_BLOCK_OUT_OF_RANGE,
                 "reject read beyond device") ||
        !require(orion_block_read(&device, 0, 0, aligned_buffer,
                                  sizeof(aligned_buffer)) ==
                     ORION_BLOCK_INVALID,
                 "reject zero-block read") ||
        !require(orion_block_read(&device, UINT64_MAX, UINT32_MAX,
                                  aligned_buffer, sizeof(aligned_buffer)) ==
                     ORION_BLOCK_OUT_OF_RANGE,
                 "reject overflowing block range") ||
        !require(orion_block_read(&device, 0, 1, unaligned_buffer + 1,
                                  TEST_BLOCK_SIZE) ==
                     ORION_BLOCK_ALIGNMENT,
                 "reject misaligned data buffer")) {
        return 1;
    }
    const uint8_t write_value[TEST_BLOCK_SIZE] = {0x6d};
    if (!require(orion_block_write(&device, 0, 1, write_value,
                                   sizeof(write_value)) == ORION_BLOCK_OK,
                 "write valid block") ||
        !require(orion_block_read(&device, 0, 1, aligned_buffer,
                                  TEST_BLOCK_SIZE) == ORION_BLOCK_OK &&
                     aligned_buffer[0] == 0x6d,
                 "read written block") ||
        !require(orion_block_read(&device, 0, 1, aligned_buffer,
                                  TEST_BLOCK_SIZE - 1U) ==
                     ORION_BLOCK_INVALID,
                 "reject undersized buffer")) {
        return 1;
    }

    orion_block_device read_only = device;
    read_only.read_only = true;
    read_only.write = NULL;
    if (!require(orion_block_device_is_valid(&read_only) &&
                     orion_block_write(&read_only, 0, 1, write_value,
                                       sizeof(write_value)) ==
                         ORION_BLOCK_READ_ONLY,
                 "reject write to readonly device")) {
        return 1;
    }
    fail_next_read = true;
    if (!require(orion_block_read(&device, 0, 1, aligned_buffer,
                                  TEST_BLOCK_SIZE) == ORION_BLOCK_IO_ERROR,
                 "propagate device read failure")) {
        return 1;
    }
    fail_next_write = true;
    if (!require(orion_block_write(&device, 0, 1, write_value,
                                   sizeof(write_value)) ==
                     ORION_BLOCK_IO_ERROR,
                 "propagate device write failure")) {
        return 1;
    }

    if (!require(orionfs_format(&device) == ORIONFS_OK,
                 "explicitly format disk") ||
        !require(orionfs_mount(&device) == ORIONFS_OK,
                 "mount formatted filesystem") ||
        !require(orionfs_create("/docs", ORIONFS_NODE_DIRECTORY) ==
                     ORIONFS_OK,
                 "create directory") ||
        !require(orionfs_create("/docs/data.bin", ORIONFS_NODE_FILE) ==
                     ORIONFS_OK,
                 "create nested file") ||
        !require(orionfs_create("/docs/data.bin", ORIONFS_NODE_FILE) ==
                     ORIONFS_EXISTS,
                 "reject duplicate name")) {
        return 1;
    }

    uint8_t contents[700];
    uint8_t readback[700];
    for (uint32_t index = 0; index < sizeof(contents); ++index) {
        contents[index] = (uint8_t)(index * 17U + 3U);
    }
    orionfs_handle handle;
    uint64_t transferred = 0;
    if (!require(orionfs_open("/docs/data.bin", &handle) == ORIONFS_OK,
                 "open file") ||
        !require(orionfs_unmount() == ORIONFS_BUSY,
                 "refuse unmount with active handle") ||
        !require(orionfs_write(handle, 0, contents, sizeof(contents),
                               &transferred) == ORIONFS_OK &&
                     transferred == sizeof(contents),
                 "write data crossing block boundary") ||
        !require(orionfs_read(handle, 0, readback, sizeof(readback),
                              &transferred) == ORIONFS_OK &&
                     transferred == sizeof(readback),
                 "read file data") ||
        !require(readback[0] == contents[0] &&
                     readback[511] == contents[511] &&
                     readback[512] == contents[512] &&
                     readback[699] == contents[699],
                 "verify cross-block contents") ||
        !require(orionfs_write(handle, 1, "update", 6, &transferred) ==
                     ORIONFS_OK &&
                     transferred == 6,
                 "update existing file range") ||
        !require(orionfs_close(handle) == ORIONFS_OK,
                 "close file") ||
        !require(orionfs_read(handle, 0, readback, 1, &transferred) ==
                     ORIONFS_INVALID,
                 "reject stale file handle") ||
        !require(orionfs_remove("/docs") == ORIONFS_NOT_EMPTY,
                 "refuse removal of non-empty directory")) {
        return 1;
    }

    if (!require(orionfs_create("/empty", ORIONFS_NODE_FILE) == ORIONFS_OK,
                 "create empty file") ||
        !require(orionfs_open("/empty", &handle) == ORIONFS_OK,
                 "open empty file") ||
        !require(orionfs_read(handle, 0, readback, sizeof(readback),
                              &transferred) == ORIONFS_OK &&
                     transferred == 0,
                 "read empty file") ||
        !require(orionfs_close(handle) == ORIONFS_OK,
                 "close empty file") ||
        !require(orionfs_stat("/missing", &(orionfs_info){0}) ==
                     ORIONFS_NOT_FOUND,
                 "report missing path") ||
        !require(orionfs_stat("relative", &(orionfs_info){0}) ==
                     ORIONFS_INVALID,
                 "reject relative path") ||
        !require(orionfs_create("/docs/../escape", ORIONFS_NODE_FILE) ==
                     ORIONFS_INVALID,
                 "reject traversal components") ||
        !require(orionfs_create(
                     "/this-name-is-longer-than-thirty-one-bytes", 
                     ORIONFS_NODE_FILE) == ORIONFS_INVALID,
                 "reject long component")) {
        return 1;
    }

    listed_entries = 0;
    if (!require(orionfs_list("/", collect_entry, NULL) == ORIONFS_OK &&
                     listed_entries == 2,
                 "list root directory") ||
        !require(orionfs_remove("/docs/data.bin") == ORIONFS_OK,
                 "remove file") ||
        !require(orionfs_remove("/docs") == ORIONFS_OK,
                 "remove empty directory") ||
        !require(orionfs_unmount() == ORIONFS_OK,
                 "unmount filesystem") ||
        !require(orionfs_mount(&device) == ORIONFS_OK,
                 "remount filesystem from disk metadata") ||
        !require(orionfs_stat("/empty", &(orionfs_info){0}) == ORIONFS_OK,
                 "metadata survives remount")) {
        return 1;
    }

    uint32_t file_index = 0;
    char file_path[16];
    uint8_t full_contents[ORIONFS_MAX_FILE_BYTES];
    for (uint32_t index = 0; index < sizeof(full_contents); ++index) {
        full_contents[index] = (uint8_t)(index * 7U + 1U);
    }
    while (file_index < 4U) {
        file_path[0] = '/';
        file_path[1] = 'f';
        file_path[2] = (char)('0' + file_index);
        file_path[3] = '\0';
        if (orionfs_create(file_path, ORIONFS_NODE_FILE) != ORIONFS_OK ||
            orionfs_open(file_path, &handle) != ORIONFS_OK) {
            return 1;
        }
        const orionfs_status write_status =
            orionfs_write(handle, 0, full_contents, sizeof(full_contents),
                           &transferred);
        if (file_index < 3U &&
            (write_status != ORIONFS_OK ||
             transferred != sizeof(full_contents))) {
            return 1;
        }
        if (file_index == 3U &&
            (write_status != ORIONFS_NO_SPACE || transferred != 0)) {
            return 1;
        }
        if (orionfs_close(handle) != ORIONFS_OK) {
            return 1;
        }
        ++file_index;
    }
    if (!require(orionfs_unmount() == ORIONFS_OK,
                 "unmount after filling data area")) {
        return 1;
    }

    if (!require(orionfs_format(&device) == ORIONFS_OK,
                 "reformat only in explicit test operation") ||
        !require(orionfs_mount(&device) == ORIONFS_OK,
                 "mount clean volume before corruption check")) {
        return 1;
    }
    if (!require(orionfs_unmount() == ORIONFS_OK,
                 "unmount before corrupting test image")) {
        return 1;
    }
    disk[0][0] ^= 0xffU;
    if (!require(orionfs_mount(&device) == ORIONFS_CORRUPT,
                 "reject corrupt superblock")) {
        return 1;
    }
    disk[0][0] ^= 0xffU;
    orion_block_device truncated = device;
    truncated.block_count = 9;
    if (!require(orionfs_mount(&truncated) == ORIONFS_INVALID,
                 "reject truncated device")) {
        return 1;
    }
    fail_next_read = true;
    if (!require(orionfs_mount(&device) == ORIONFS_IO_ERROR,
                 "propagate metadata read failure")) {
        return 1;
    }
    if (!require(orionfs_mount(&device) == ORIONFS_OK,
                 "mount after transient metadata read error")) {
        return 1;
    }
    fail_next_write = true;
    if (!require(orionfs_create("/io-write", ORIONFS_NODE_FILE) ==
                     ORIONFS_IO_ERROR &&
                     !orionfs_is_mounted(),
                 "fail closed after metadata write failure") ||
        !require(orionfs_mount(&device) == ORIONFS_OK &&
                     orionfs_stat("/io-write", &(orionfs_info){0}) ==
                         ORIONFS_NOT_FOUND,
                 "remount after failed metadata write")) {
        return 1;
    }
    static const char io_payload[] = "read-failure";
    if (!require(orionfs_create("/io-read", ORIONFS_NODE_FILE) == ORIONFS_OK,
                 "create read-failure test file") ||
        !require(orionfs_open("/io-read", &handle) == ORIONFS_OK,
                 "open read-failure test file") ||
        !require(orionfs_write(handle, 0, io_payload,
                               sizeof(io_payload) - 1U, &transferred) ==
                     ORIONFS_OK &&
                     transferred == sizeof(io_payload) - 1U,
                 "write read-failure test payload") ||
        !require(orionfs_close(handle) == ORIONFS_OK,
                 "close read-failure test file") ||
        !require(orionfs_open("/io-read", &handle) == ORIONFS_OK,
                 "reopen read-failure test file")) {
        return 1;
    }
    fail_next_read = true;
    if (!require(orionfs_read(handle, 0, readback, sizeof(io_payload) - 1U,
                              &transferred) == ORIONFS_IO_ERROR &&
                     !orionfs_is_mounted(),
                 "fail closed after data read failure") ||
        !require(orionfs_mount(&device) == ORIONFS_OK &&
                     orionfs_stat("/io-read", &(orionfs_info){0}) ==
                         ORIONFS_OK,
                 "remount after failed data read")) {
        return 1;
    }
    (void)puts("Block, ORIONFS, and VFS tests: PASS");
    return 0;
}
