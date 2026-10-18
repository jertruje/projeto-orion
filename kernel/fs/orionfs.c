#include "orionfs.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"

#define ORIONFS_VERSION 1U
#define ORIONFS_BITMAP_BLOCK 1U
#define ORIONFS_INODE_START_BLOCK 2U
#define ORIONFS_INODE_BLOCK_COUNT 8U
#define ORIONFS_INODE_SIZE 64U
#define ORIONFS_SUPERBLOCK_CRC_OFFSET 508U
#define ORIONFS_OPEN_DIRECTORY_LIMIT ORIONFS_MAX_INODES

enum {
    INODE_TYPE_OFFSET = 0,
    INODE_NAME_LENGTH_OFFSET = 1,
    INODE_PARENT_OFFSET = 2,
    INODE_FILE_SIZE_OFFSET = 4,
    INODE_BLOCKS_OFFSET = 8,
    INODE_NAME_OFFSET = 32
};

typedef struct {
    uint32_t inode;
    uint32_t generation;
    bool active;
} open_handle_slot;

typedef struct {
    char name[ORIONFS_MAX_NAME_BYTES + 1U];
    orionfs_node_type type;
    uint64_t size;
} list_snapshot_entry;

static const uint8_t filesystem_magic[8] = {
    'O', 'R', 'I', 'O', 'N', 'F', 'S', 0
};
static uint8_t superblock[ORION_BLOCK_MIN_SIZE] __attribute__((aligned(8)));
static uint8_t allocation_bitmap[ORION_BLOCK_MIN_SIZE]
    __attribute__((aligned(8)));
static uint8_t inode_table[ORIONFS_INODE_BLOCK_COUNT * ORION_BLOCK_MIN_SIZE]
    __attribute__((aligned(8)));
static uint8_t referenced_bitmap[ORION_BLOCK_MIN_SIZE]
    __attribute__((aligned(8)));
static uint8_t io_buffer[ORION_BLOCK_MIN_SIZE] __attribute__((aligned(8)));
static open_handle_slot open_handles[ORIONFS_OPEN_HANDLE_LIMIT];
static uint32_t open_counts[ORIONFS_MAX_INODES];
static uint32_t filesystem_block_count;
static orion_block_device *mounted_device;
static bool mounted;
static bool filesystem_busy;

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void write_le16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void bytes_clear(uint8_t *bytes, uint32_t count)
{
    for (uint32_t index = 0; index < count; ++index) {
        bytes[index] = 0;
    }
}

static void bytes_copy(uint8_t *destination, const uint8_t *source,
                       uint32_t count)
{
    for (uint32_t index = 0; index < count; ++index) {
        destination[index] = source[index];
    }
}

static bool bytes_equal(const uint8_t *left, const uint8_t *right,
                        uint32_t count)
{
    for (uint32_t index = 0; index < count; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static uint32_t crc32(const uint8_t *bytes, uint32_t count)
{
    uint32_t crc = UINT32_MAX;
    for (uint32_t index = 0; index < count; ++index) {
        crc ^= bytes[index];
        for (uint32_t bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static bool acquire_filesystem(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    if (filesystem_busy) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }
    filesystem_busy = true;
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

static void release_filesystem(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    filesystem_busy = false;
    orion_x86_64_interrupts_restore(was_enabled);
}

static void fail_closed(void)
{
    mounted = false;
    mounted_device = NULL;
    filesystem_block_count = 0;
    for (uint32_t index = 0; index < ORIONFS_OPEN_HANDLE_LIMIT; ++index) {
        open_handles[index].active = false;
    }
    for (uint32_t index = 0; index < ORIONFS_MAX_INODES; ++index) {
        open_counts[index] = 0;
    }
}

static bool bitmap_get(const uint8_t *bitmap, uint32_t block)
{
    return (bitmap[block / 8U] & (uint8_t)(1U << (block % 8U))) != 0;
}

static void bitmap_set(uint8_t *bitmap, uint32_t block, bool value)
{
    const uint8_t mask = (uint8_t)(1U << (block % 8U));
    if (value) {
        bitmap[block / 8U] |= mask;
    } else {
        bitmap[block / 8U] &= (uint8_t)~mask;
    }
}

static uint8_t *inode_record(uint32_t inode)
{
    return inode_table + (uint64_t)inode * ORIONFS_INODE_SIZE;
}

static uint32_t inode_data_block(const uint8_t *record, uint32_t index)
{
    return read_le32(record + INODE_BLOCKS_OFFSET + index * 4U);
}

static void inode_set_data_block(uint8_t *record, uint32_t index,
                                 uint32_t block)
{
    write_le32(record + INODE_BLOCKS_OFFSET + index * 4U, block);
}

static orionfs_status write_inode_table(void)
{
    if (orion_block_write(mounted_device, ORIONFS_INODE_START_BLOCK,
                          ORIONFS_INODE_BLOCK_COUNT, inode_table,
                          sizeof(inode_table)) != ORION_BLOCK_OK) {
        return ORIONFS_IO_ERROR;
    }
    return ORIONFS_OK;
}

static orionfs_status write_bitmap(void)
{
    if (orion_block_write(mounted_device, ORIONFS_BITMAP_BLOCK, 1,
                          allocation_bitmap,
                          sizeof(allocation_bitmap)) != ORION_BLOCK_OK) {
        return ORIONFS_IO_ERROR;
    }
    return ORIONFS_OK;
}

static orionfs_status flush_device(void)
{
    return orion_block_sync(mounted_device) == ORION_BLOCK_OK
               ? ORIONFS_OK
               : ORIONFS_IO_ERROR;
}

static bool name_is_valid(const char *name, uint32_t length)
{
    if (length == 0 || length > ORIONFS_MAX_NAME_BYTES) {
        return false;
    }
    for (uint32_t index = 0; index < length; ++index) {
        const uint8_t value = (uint8_t)name[index];
        if (value == 0 || value == '/' || value < 0x20U) {
            return false;
        }
    }
    return !(length == 1U && name[0] == '.') &&
           !(length == 2U && name[0] == '.' && name[1] == '.');
}

static bool path_is_valid(const char *path, uint32_t *length)
{
    if (path == NULL || path[0] != '/') {
        return false;
    }
    uint32_t count = 0;
    while (path[count] != '\0' && count < ORIONFS_MAX_PATH_BYTES) {
        ++count;
    }
    if (path[count] != '\0' || count == 0) {
        return false;
    }
    *length = count;
    return true;
}

static bool inode_is_used(uint32_t inode)
{
    return inode < ORIONFS_MAX_INODES &&
           inode_record(inode)[INODE_TYPE_OFFSET] != 0;
}

static int32_t find_child(uint32_t parent, const char *name,
                          uint32_t name_length)
{
    for (uint32_t inode = 1; inode < ORIONFS_MAX_INODES; ++inode) {
        const uint8_t *record = inode_record(inode);
        if (record[INODE_TYPE_OFFSET] == 0 ||
            read_le16(record + INODE_PARENT_OFFSET) != parent ||
            record[INODE_NAME_LENGTH_OFFSET] != name_length) {
            continue;
        }
        if (bytes_equal(record + INODE_NAME_OFFSET,
                        (const uint8_t *)name, name_length)) {
            return (int32_t)inode;
        }
    }
    return -1;
}

static orionfs_status resolve_path(const char *path, uint32_t path_length,
                                   uint32_t *inode_out)
{
    if (path_length == 1U) {
        *inode_out = 0;
        return ORIONFS_OK;
    }

    uint32_t current = 0;
    uint32_t cursor = 1;
    uint32_t depth = 0;
    while (cursor < path_length) {
        const uint32_t name_start = cursor;
        while (cursor < path_length && path[cursor] != '/') {
            ++cursor;
        }
        const uint32_t name_length = cursor - name_start;
        if (!name_is_valid(path + name_start, name_length) ||
            ++depth > ORIONFS_MAX_PATH_DEPTH) {
            return ORIONFS_INVALID;
        }
        if (inode_record(current)[INODE_TYPE_OFFSET] !=
            ORIONFS_NODE_DIRECTORY) {
            return ORIONFS_NOT_DIRECTORY;
        }
        const int32_t child =
            find_child(current, path + name_start, name_length);
        if (child < 0) {
            return ORIONFS_NOT_FOUND;
        }
        current = (uint32_t)child;
        if (cursor < path_length) {
            if (cursor + 1U == path_length) {
                return ORIONFS_INVALID;
            }
            ++cursor;
        }
    }
    *inode_out = current;
    return ORIONFS_OK;
}

static orionfs_status resolve_parent(const char *path, uint32_t path_length,
                                    uint32_t *parent_out,
                                    const char **name_out,
                                    uint32_t *name_length_out)
{
    if (path_length < 2U || path[path_length - 1U] == '/') {
        return ORIONFS_INVALID;
    }
    uint32_t separator = path_length;
    while (separator > 1U && path[separator - 1U] != '/') {
        --separator;
    }
    const uint32_t name_start = separator;
    const uint32_t name_length = path_length - name_start;
    if (!name_is_valid(path + name_start, name_length)) {
        return ORIONFS_INVALID;
    }
    const uint32_t parent_length = separator == 1U ? 1U : separator - 1U;
    const orionfs_status status =
        resolve_path(path, parent_length, parent_out);
    if (status != ORIONFS_OK) {
        return status;
    }
    if (inode_record(*parent_out)[INODE_TYPE_OFFSET] !=
        ORIONFS_NODE_DIRECTORY) {
        return ORIONFS_NOT_DIRECTORY;
    }
    *name_out = path + name_start;
    *name_length_out = name_length;
    return ORIONFS_OK;
}

static bool all_zero(const uint8_t *bytes, uint32_t count)
{
    for (uint32_t index = 0; index < count; ++index) {
        if (bytes[index] != 0) {
            return false;
        }
    }
    return true;
}

static bool validate_mounted_metadata(void)
{
    bytes_clear(referenced_bitmap, sizeof(referenced_bitmap));
    for (uint32_t block = 0; block < ORIONFS_DATA_START_BLOCK; ++block) {
        bitmap_set(referenced_bitmap, block, true);
    }

    const uint8_t *root = inode_record(0);
    if (root[INODE_TYPE_OFFSET] != ORIONFS_NODE_DIRECTORY ||
        root[INODE_NAME_LENGTH_OFFSET] != 0 ||
        read_le16(root + INODE_PARENT_OFFSET) != 0 ||
        read_le32(root + INODE_FILE_SIZE_OFFSET) != 0 ||
        !all_zero(root + INODE_BLOCKS_OFFSET, 24) ||
        !all_zero(root + INODE_NAME_OFFSET, 32)) {
        return false;
    }

    for (uint32_t inode = 1; inode < ORIONFS_MAX_INODES; ++inode) {
        const uint8_t *record = inode_record(inode);
        const uint8_t type = record[INODE_TYPE_OFFSET];
        if (type == 0) {
            if (!all_zero(record, ORIONFS_INODE_SIZE)) {
                return false;
            }
            continue;
        }
        const uint8_t name_length = record[INODE_NAME_LENGTH_OFFSET];
        const uint32_t parent = read_le16(record + INODE_PARENT_OFFSET);
        const uint32_t size = read_le32(record + INODE_FILE_SIZE_OFFSET);
        if ((type != ORIONFS_NODE_FILE &&
             type != ORIONFS_NODE_DIRECTORY) ||
            !name_is_valid((const char *)record + INODE_NAME_OFFSET,
                           name_length) ||
            parent >= ORIONFS_MAX_INODES || parent == inode ||
            !inode_is_used(parent) ||
            inode_record(parent)[INODE_TYPE_OFFSET] !=
                ORIONFS_NODE_DIRECTORY) {
            return false;
        }
        if (find_child(parent,
                       (const char *)record + INODE_NAME_OFFSET,
                       name_length) != (int32_t)inode) {
            return false;
        }
        if (type == ORIONFS_NODE_DIRECTORY) {
            if (size != 0 ||
                !all_zero(record + INODE_BLOCKS_OFFSET, 24)) {
                return false;
            }
            continue;
        }
        if (size > ORIONFS_MAX_FILE_BYTES) {
            return false;
        }
        const uint32_t blocks_needed =
            (size + ORION_BLOCK_MIN_SIZE - 1U) / ORION_BLOCK_MIN_SIZE;
        for (uint32_t index = 0; index < 6U; ++index) {
            const uint32_t block = inode_data_block(record, index);
            if (index >= blocks_needed) {
                if (block != 0) {
                    return false;
                }
                continue;
            }
            if (block < ORIONFS_DATA_START_BLOCK ||
                block >= filesystem_block_count ||
                !bitmap_get(allocation_bitmap, block) ||
                bitmap_get(referenced_bitmap, block)) {
                return false;
            }
            bitmap_set(referenced_bitmap, block, true);
        }
    }

    for (uint32_t block = 0; block < ORIONFS_MAX_BLOCKS; ++block) {
        const bool allocated = bitmap_get(allocation_bitmap, block);
        if (block >= filesystem_block_count) {
            if (!allocated) {
                return false;
            }
        } else if (allocated != bitmap_get(referenced_bitmap, block)) {
            return false;
        }
    }
    return true;
}

static orionfs_status validate_superblock(orion_block_device *device)
{
    if (!orion_block_device_is_valid(device) ||
        device->block_size != ORION_BLOCK_MIN_SIZE ||
        device->block_count < ORIONFS_DATA_START_BLOCK) {
        return ORIONFS_INVALID;
    }
    if (orion_block_read(device, 0, 1, superblock, sizeof(superblock)) !=
        ORION_BLOCK_OK) {
        return ORIONFS_IO_ERROR;
    }
    const uint32_t stored_crc =
        read_le32(superblock + ORIONFS_SUPERBLOCK_CRC_OFFSET);
    write_le32(superblock + ORIONFS_SUPERBLOCK_CRC_OFFSET, 0);
    const uint32_t calculated_crc =
        crc32(superblock, ORIONFS_SUPERBLOCK_CRC_OFFSET);
    write_le32(superblock + ORIONFS_SUPERBLOCK_CRC_OFFSET, stored_crc);
    if (!bytes_equal(superblock, filesystem_magic, sizeof(filesystem_magic)) ||
        read_le32(superblock + 8) != ORIONFS_VERSION ||
        read_le32(superblock + 12) != ORION_BLOCK_MIN_SIZE ||
        read_le32(superblock + 20) != ORIONFS_MAX_INODES ||
        read_le32(superblock + 24) != ORIONFS_BITMAP_BLOCK ||
        read_le32(superblock + 28) != ORIONFS_INODE_START_BLOCK ||
        read_le32(superblock + 32) != ORIONFS_INODE_BLOCK_COUNT ||
        read_le32(superblock + 36) != ORIONFS_DATA_START_BLOCK ||
        stored_crc != calculated_crc) {
        return ORIONFS_CORRUPT;
    }
    const uint32_t block_count = read_le32(superblock + 16);
    if (block_count < ORIONFS_DATA_START_BLOCK ||
        block_count > ORIONFS_MAX_BLOCKS ||
        block_count > device->block_count) {
        return ORIONFS_CORRUPT;
    }
    filesystem_block_count = block_count;
    return ORIONFS_OK;
}

orionfs_status orionfs_format(orion_block_device *device)
{
    if (mounted || !orion_block_device_is_valid(device) ||
        !device->available || device->read_only ||
        device->block_size != ORION_BLOCK_MIN_SIZE ||
        device->block_count < ORIONFS_DATA_START_BLOCK ||
        !acquire_filesystem()) {
        return ORIONFS_INVALID;
    }
    filesystem_block_count = device->block_count < ORIONFS_MAX_BLOCKS
                                 ? (uint32_t)device->block_count
                                 : ORIONFS_MAX_BLOCKS;
    mounted_device = device;
    bytes_clear(allocation_bitmap, sizeof(allocation_bitmap));
    bytes_clear(inode_table, sizeof(inode_table));
    for (uint32_t block = 0; block < ORIONFS_DATA_START_BLOCK; ++block) {
        bitmap_set(allocation_bitmap, block, true);
    }
    for (uint32_t block = filesystem_block_count;
         block < ORIONFS_MAX_BLOCKS; ++block) {
        bitmap_set(allocation_bitmap, block, true);
    }
    uint8_t *root = inode_record(0);
    root[INODE_TYPE_OFFSET] = ORIONFS_NODE_DIRECTORY;

    orionfs_status status = write_bitmap();
    if (status == ORIONFS_OK) {
        status = write_inode_table();
    }
    if (status == ORIONFS_OK) {
        status = flush_device();
    }
    if (status == ORIONFS_OK) {
        bytes_clear(superblock, sizeof(superblock));
        bytes_copy(superblock, filesystem_magic, sizeof(filesystem_magic));
        write_le32(superblock + 8, ORIONFS_VERSION);
        write_le32(superblock + 12, ORION_BLOCK_MIN_SIZE);
        write_le32(superblock + 16, filesystem_block_count);
        write_le32(superblock + 20, ORIONFS_MAX_INODES);
        write_le32(superblock + 24, ORIONFS_BITMAP_BLOCK);
        write_le32(superblock + 28, ORIONFS_INODE_START_BLOCK);
        write_le32(superblock + 32, ORIONFS_INODE_BLOCK_COUNT);
        write_le32(superblock + 36, ORIONFS_DATA_START_BLOCK);
        write_le32(superblock + ORIONFS_SUPERBLOCK_CRC_OFFSET,
                   crc32(superblock, ORIONFS_SUPERBLOCK_CRC_OFFSET));
        if (orion_block_write(device, 0, 1, superblock,
                              sizeof(superblock)) != ORION_BLOCK_OK) {
            status = ORIONFS_IO_ERROR;
        } else {
            status = flush_device();
        }
    }
    mounted_device = NULL;
    release_filesystem();
    return status;
}

orionfs_status orionfs_mount(orion_block_device *device)
{
    if (mounted || !acquire_filesystem()) {
        return ORIONFS_BUSY;
    }
    if (device == NULL || !device->available || device->read_only) {
        release_filesystem();
        return device != NULL && device->read_only ? ORIONFS_READ_ONLY
                                                   : ORIONFS_INVALID;
    }
    mounted_device = device;
    orionfs_status status = validate_superblock(device);
    if (status == ORIONFS_OK &&
        orion_block_read(device, ORIONFS_BITMAP_BLOCK, 1, allocation_bitmap,
                         sizeof(allocation_bitmap)) != ORION_BLOCK_OK) {
        status = ORIONFS_IO_ERROR;
    }
    if (status == ORIONFS_OK &&
        orion_block_read(device, ORIONFS_INODE_START_BLOCK,
                         ORIONFS_INODE_BLOCK_COUNT, inode_table,
                         sizeof(inode_table)) != ORION_BLOCK_OK) {
        status = ORIONFS_IO_ERROR;
    }
    if (status == ORIONFS_OK && !validate_mounted_metadata()) {
        status = ORIONFS_CORRUPT;
    }
    if (status == ORIONFS_OK) {
        for (uint32_t index = 0; index < ORIONFS_OPEN_HANDLE_LIMIT; ++index) {
            open_handles[index].active = false;
        }
        for (uint32_t index = 0; index < ORIONFS_MAX_INODES; ++index) {
            open_counts[index] = 0;
        }
        mounted = true;
    } else {
        mounted_device = NULL;
        filesystem_block_count = 0;
    }
    release_filesystem();
    return status;
}

orionfs_status orionfs_unmount(void)
{
    if (!mounted) {
        return ORIONFS_NOT_MOUNTED;
    }
    if (!acquire_filesystem()) {
        return ORIONFS_BUSY;
    }
    for (uint32_t index = 0; index < ORIONFS_OPEN_HANDLE_LIMIT; ++index) {
        if (open_handles[index].active) {
            release_filesystem();
            return ORIONFS_BUSY;
        }
    }
    const orionfs_status status = flush_device();
    if (status == ORIONFS_OK) {
        mounted = false;
        mounted_device = NULL;
        filesystem_block_count = 0;
    } else {
        fail_closed();
    }
    release_filesystem();
    return status;
}

orionfs_status orionfs_create(const char *path, orionfs_node_type type)
{
    if (!mounted || !acquire_filesystem()) {
        return mounted ? ORIONFS_BUSY : ORIONFS_NOT_MOUNTED;
    }
    if (type != ORIONFS_NODE_FILE && type != ORIONFS_NODE_DIRECTORY) {
        release_filesystem();
        return ORIONFS_INVALID;
    }
    uint32_t path_length = 0;
    uint32_t parent = 0;
    const char *name = NULL;
    uint32_t name_length = 0;
    orionfs_status status =
        path_is_valid(path, &path_length)
            ? resolve_parent(path, path_length, &parent, &name, &name_length)
            : ORIONFS_INVALID;
    if (status == ORIONFS_OK &&
        find_child(parent, name, name_length) >= 0) {
        status = ORIONFS_EXISTS;
    }
    uint32_t free_inode = 1;
    while (status == ORIONFS_OK && free_inode < ORIONFS_MAX_INODES &&
           inode_is_used(free_inode)) {
        ++free_inode;
    }
    if (status == ORIONFS_OK && free_inode == ORIONFS_MAX_INODES) {
        status = ORIONFS_NO_SPACE;
    }
    if (status == ORIONFS_OK) {
        uint8_t *record = inode_record(free_inode);
        bytes_clear(record, ORIONFS_INODE_SIZE);
        record[INODE_TYPE_OFFSET] = (uint8_t)type;
        record[INODE_NAME_LENGTH_OFFSET] = (uint8_t)name_length;
        write_le16(record + INODE_PARENT_OFFSET, (uint16_t)parent);
        bytes_copy(record + INODE_NAME_OFFSET, (const uint8_t *)name,
                   name_length);
        status = write_inode_table();
        if (status == ORIONFS_OK) {
            status = flush_device();
        }
        if (status != ORIONFS_OK) {
            bytes_clear(record, ORIONFS_INODE_SIZE);
        }
    }
    if (status == ORIONFS_IO_ERROR) {
        fail_closed();
    }
    release_filesystem();
    return status;
}

orionfs_status orionfs_remove(const char *path)
{
    if (!mounted || !acquire_filesystem()) {
        return mounted ? ORIONFS_BUSY : ORIONFS_NOT_MOUNTED;
    }
    uint32_t path_length = 0;
    uint32_t inode = 0;
    orionfs_status status =
        path_is_valid(path, &path_length)
            ? resolve_path(path, path_length, &inode)
            : ORIONFS_INVALID;
    if (status == ORIONFS_OK && inode == 0) {
        status = ORIONFS_INVALID;
    }
    uint8_t *record = status == ORIONFS_OK ? inode_record(inode) : NULL;
    if (status == ORIONFS_OK &&
        record[INODE_TYPE_OFFSET] == ORIONFS_NODE_DIRECTORY) {
        for (uint32_t child = 1; child < ORIONFS_MAX_INODES; ++child) {
            if (inode_is_used(child) &&
                read_le16(inode_record(child) + INODE_PARENT_OFFSET) == inode) {
                status = ORIONFS_NOT_EMPTY;
                break;
            }
        }
    }
    if (status == ORIONFS_OK && open_counts[inode] != 0) {
        status = ORIONFS_BUSY;
    }
    if (status == ORIONFS_OK) {
        const uint8_t old_record[ORIONFS_INODE_SIZE] = {
            record[0], record[1], record[2], record[3], record[4], record[5],
            record[6], record[7], record[8], record[9], record[10], record[11],
            record[12], record[13], record[14], record[15], record[16],
            record[17], record[18], record[19], record[20], record[21],
            record[22], record[23], record[24], record[25], record[26],
            record[27], record[28], record[29], record[30], record[31],
            record[32], record[33], record[34], record[35], record[36],
            record[37], record[38], record[39], record[40], record[41],
            record[42], record[43], record[44], record[45], record[46],
            record[47], record[48], record[49], record[50], record[51],
            record[52], record[53], record[54], record[55], record[56],
            record[57], record[58], record[59], record[60], record[61],
            record[62], record[63]
        };
        uint32_t freed_blocks[6];
        uint32_t freed_count = 0;
        if (record[INODE_TYPE_OFFSET] == ORIONFS_NODE_FILE) {
            for (uint32_t index = 0; index < 6U; ++index) {
                const uint32_t block = inode_data_block(record, index);
                if (block != 0) {
                    freed_blocks[freed_count++] = block;
                }
            }
        }
        bytes_clear(record, ORIONFS_INODE_SIZE);
        status = write_inode_table();
        if (status == ORIONFS_OK) {
            status = flush_device();
        }
        if (status == ORIONFS_OK) {
            for (uint32_t index = 0; index < freed_count; ++index) {
                bitmap_set(allocation_bitmap, freed_blocks[index], false);
            }
            status = write_bitmap();
            if (status == ORIONFS_OK) {
                status = flush_device();
            }
        }
        if (status != ORIONFS_OK) {
            bytes_copy(record, old_record, ORIONFS_INODE_SIZE);
            for (uint32_t index = 0; index < freed_count; ++index) {
                bitmap_set(allocation_bitmap, freed_blocks[index], true);
            }
        }
    }
    if (status == ORIONFS_IO_ERROR) {
        fail_closed();
    }
    release_filesystem();
    return status;
}

orionfs_status orionfs_stat(const char *path, orionfs_info *info)
{
    if (!mounted || !acquire_filesystem()) {
        return mounted ? ORIONFS_BUSY : ORIONFS_NOT_MOUNTED;
    }
    uint32_t path_length = 0;
    uint32_t inode = 0;
    orionfs_status status =
        info != NULL && path_is_valid(path, &path_length)
            ? resolve_path(path, path_length, &inode)
            : ORIONFS_INVALID;
    if (status == ORIONFS_OK) {
        const uint8_t *record = inode_record(inode);
        info->inode = inode;
        info->size = read_le32(record + INODE_FILE_SIZE_OFFSET);
        info->type = (orionfs_node_type)record[INODE_TYPE_OFFSET];
    }
    release_filesystem();
    return status;
}

orionfs_status orionfs_open(const char *path, orionfs_handle *handle)
{
    if (!mounted || !acquire_filesystem()) {
        return mounted ? ORIONFS_BUSY : ORIONFS_NOT_MOUNTED;
    }
    uint32_t path_length = 0;
    uint32_t inode = 0;
    orionfs_status status =
        handle != NULL && path_is_valid(path, &path_length)
            ? resolve_path(path, path_length, &inode)
            : ORIONFS_INVALID;
    if (status == ORIONFS_OK &&
        inode_record(inode)[INODE_TYPE_OFFSET] != ORIONFS_NODE_FILE) {
        status = ORIONFS_IS_DIRECTORY;
    }
    uint32_t slot = 0;
    while (status == ORIONFS_OK && slot < ORIONFS_OPEN_HANDLE_LIMIT &&
           (open_handles[slot].active ||
            open_handles[slot].generation == UINT32_MAX)) {
        ++slot;
    }
    if (status == ORIONFS_OK && slot == ORIONFS_OPEN_HANDLE_LIMIT) {
        status = ORIONFS_NO_SPACE;
    }
    if (status == ORIONFS_OK && open_counts[inode] == UINT32_MAX) {
        status = ORIONFS_NO_SPACE;
    }
    if (status == ORIONFS_OK) {
        open_handle_slot *entry = &open_handles[slot];
        ++entry->generation;
        entry->inode = inode;
        entry->active = true;
        ++open_counts[inode];
        *handle = (orionfs_handle){
            .slot = (uint16_t)slot,
            .generation = entry->generation
        };
    }
    release_filesystem();
    return status;
}

static bool open_handle_is_valid(orionfs_handle handle)
{
    return handle.slot < ORIONFS_OPEN_HANDLE_LIMIT &&
           handle.generation != 0 &&
           open_handles[handle.slot].active &&
           open_handles[handle.slot].generation == handle.generation;
}

orionfs_status orionfs_close(orionfs_handle handle)
{
    if (!mounted || !acquire_filesystem()) {
        return mounted ? ORIONFS_BUSY : ORIONFS_NOT_MOUNTED;
    }
    orionfs_status status = ORIONFS_INVALID;
    if (open_handle_is_valid(handle)) {
        open_handle_slot *entry = &open_handles[handle.slot];
        if (open_counts[entry->inode] != 0) {
            --open_counts[entry->inode];
            entry->active = false;
            status = ORIONFS_OK;
        }
    }
    release_filesystem();
    return status;
}

orionfs_status orionfs_read(orionfs_handle handle, uint64_t offset,
                            void *buffer, uint64_t length,
                            uint64_t *bytes_read)
{
    if (!mounted || !acquire_filesystem()) {
        return mounted ? ORIONFS_BUSY : ORIONFS_NOT_MOUNTED;
    }
    if (bytes_read == NULL || (length != 0 && buffer == NULL) ||
        length > UINT64_MAX - offset) {
        release_filesystem();
        return ORIONFS_INVALID;
    }
    if (!open_handle_is_valid(handle)) {
        release_filesystem();
        return ORIONFS_INVALID;
    }
    const uint8_t *record = inode_record(open_handles[handle.slot].inode);
    const uint64_t file_size = read_le32(record + INODE_FILE_SIZE_OFFSET);
    *bytes_read = 0;
    if (offset >= file_size || length == 0) {
        release_filesystem();
        return ORIONFS_OK;
    }
    uint64_t remaining = file_size - offset;
    if (remaining > length) {
        remaining = length;
    }
    uint8_t *destination = buffer;
    while (remaining != 0) {
        const uint32_t block_index =
            (uint32_t)(offset / ORION_BLOCK_MIN_SIZE);
        const uint32_t block_offset =
            (uint32_t)(offset % ORION_BLOCK_MIN_SIZE);
        const uint32_t block = inode_data_block(record, block_index);
        if (block < ORIONFS_DATA_START_BLOCK ||
            block >= filesystem_block_count ||
            orion_block_read(mounted_device, block, 1, io_buffer,
                             sizeof(io_buffer)) != ORION_BLOCK_OK) {
            fail_closed();
            release_filesystem();
            return ORIONFS_IO_ERROR;
        }
        uint64_t chunk = ORION_BLOCK_MIN_SIZE - block_offset;
        if (chunk > remaining) {
            chunk = remaining;
        }
        bytes_copy(destination, io_buffer + block_offset, (uint32_t)chunk);
        destination += chunk;
        offset += chunk;
        remaining -= chunk;
        *bytes_read += chunk;
    }
    release_filesystem();
    return ORIONFS_OK;
}

static int32_t allocate_data_block(void)
{
    for (uint32_t block = ORIONFS_DATA_START_BLOCK;
         block < filesystem_block_count; ++block) {
        if (!bitmap_get(allocation_bitmap, block)) {
            bitmap_set(allocation_bitmap, block, true);
            return (int32_t)block;
        }
    }
    return -1;
}

orionfs_status orionfs_write(orionfs_handle handle, uint64_t offset,
                             const void *buffer, uint64_t length,
                             uint64_t *bytes_written)
{
    if (!mounted || !acquire_filesystem()) {
        return mounted ? ORIONFS_BUSY : ORIONFS_NOT_MOUNTED;
    }
    if (bytes_written == NULL || (length != 0 && buffer == NULL) ||
        length > UINT64_MAX - offset || length > ORIONFS_MAX_FILE_BYTES ||
        offset > ORIONFS_MAX_FILE_BYTES ||
        length > ORIONFS_MAX_FILE_BYTES - offset) {
        release_filesystem();
        return ORIONFS_TOO_LARGE;
    }
    *bytes_written = 0;
    if (!open_handle_is_valid(handle)) {
        release_filesystem();
        return ORIONFS_INVALID;
    }
    uint8_t *record = inode_record(open_handles[handle.slot].inode);
    const uint64_t old_size = read_le32(record + INODE_FILE_SIZE_OFFSET);
    if (offset > old_size || length == 0) {
        release_filesystem();
        return offset > old_size ? ORIONFS_INVALID : ORIONFS_OK;
    }
    const uint64_t new_size = offset + length > old_size
                                  ? offset + length
                                  : old_size;
    const uint32_t blocks_needed =
        (uint32_t)((new_size + ORION_BLOCK_MIN_SIZE - 1U) /
                   ORION_BLOCK_MIN_SIZE);
    const uint8_t old_record[ORIONFS_INODE_SIZE] = {
        record[0], record[1], record[2], record[3], record[4], record[5],
        record[6], record[7], record[8], record[9], record[10], record[11],
        record[12], record[13], record[14], record[15], record[16],
        record[17], record[18], record[19], record[20], record[21],
        record[22], record[23], record[24], record[25], record[26],
        record[27], record[28], record[29], record[30], record[31],
        record[32], record[33], record[34], record[35], record[36],
        record[37], record[38], record[39], record[40], record[41],
        record[42], record[43], record[44], record[45], record[46],
        record[47], record[48], record[49], record[50], record[51],
        record[52], record[53], record[54], record[55], record[56],
        record[57], record[58], record[59], record[60], record[61],
        record[62], record[63]
    };
    uint32_t new_blocks[6];
    uint32_t new_block_count = 0;
    for (uint32_t index = 0; index < blocks_needed; ++index) {
        if (inode_data_block(record, index) == 0) {
            const int32_t allocated = allocate_data_block();
            if (allocated < 0) {
                for (uint32_t undo = 0; undo < new_block_count; ++undo) {
                    bitmap_set(allocation_bitmap, new_blocks[undo], false);
                }
                release_filesystem();
                return ORIONFS_NO_SPACE;
            }
            new_blocks[new_block_count++] = (uint32_t)allocated;
            inode_set_data_block(record, index, (uint32_t)allocated);
        }
    }

    const uint8_t *source = buffer;
    uint64_t copy_offset = offset;
    uint64_t remaining = length;
    while (remaining != 0) {
        const uint32_t block_index =
            (uint32_t)(copy_offset / ORION_BLOCK_MIN_SIZE);
        const uint32_t block_offset =
            (uint32_t)(copy_offset % ORION_BLOCK_MIN_SIZE);
        const uint32_t block = inode_data_block(record, block_index);
        bool newly_allocated = false;
        for (uint32_t index = 0; index < new_block_count; ++index) {
            newly_allocated = newly_allocated || new_blocks[index] == block;
        }
        if (newly_allocated) {
            bytes_clear(io_buffer, sizeof(io_buffer));
        } else if (orion_block_read(mounted_device, block, 1, io_buffer,
                                    sizeof(io_buffer)) != ORION_BLOCK_OK) {
            for (uint32_t undo = 0; undo < new_block_count; ++undo) {
                bitmap_set(allocation_bitmap, new_blocks[undo], false);
            }
            bytes_copy(record, old_record, ORIONFS_INODE_SIZE);
            release_filesystem();
            return ORIONFS_IO_ERROR;
        }
        uint64_t chunk = ORION_BLOCK_MIN_SIZE - block_offset;
        if (chunk > remaining) {
            chunk = remaining;
        }
        bytes_copy(io_buffer + block_offset, source, (uint32_t)chunk);
        if (orion_block_write(mounted_device, block, 1, io_buffer,
                              sizeof(io_buffer)) != ORION_BLOCK_OK) {
            for (uint32_t undo = 0; undo < new_block_count; ++undo) {
                bitmap_set(allocation_bitmap, new_blocks[undo], false);
            }
            bytes_copy(record, old_record, ORIONFS_INODE_SIZE);
            release_filesystem();
            return ORIONFS_IO_ERROR;
        }
        source += chunk;
        copy_offset += chunk;
        remaining -= chunk;
    }
    if (flush_device() != ORIONFS_OK) {
        for (uint32_t undo = 0; undo < new_block_count; ++undo) {
            bitmap_set(allocation_bitmap, new_blocks[undo], false);
        }
        bytes_copy(record, old_record, ORIONFS_INODE_SIZE);
        release_filesystem();
        return ORIONFS_IO_ERROR;
    }
    write_le32(record + INODE_FILE_SIZE_OFFSET, (uint32_t)new_size);
    orionfs_status status = ORIONFS_OK;
    if (new_block_count != 0) {
        status = write_bitmap();
    }
    if (status == ORIONFS_OK) {
        status = flush_device();
    }
    if (status == ORIONFS_OK) {
        status = write_inode_table();
    }
    if (status == ORIONFS_OK) {
        status = flush_device();
    }
    if (status != ORIONFS_OK) {
        for (uint32_t undo = 0; undo < new_block_count; ++undo) {
            bitmap_set(allocation_bitmap, new_blocks[undo], false);
        }
        bytes_copy(record, old_record, ORIONFS_INODE_SIZE);
    } else {
        *bytes_written = length;
    }
    if (status == ORIONFS_IO_ERROR) {
        fail_closed();
    }
    release_filesystem();
    return status;
}

orionfs_status orionfs_list(const char *path, orionfs_list_fn callback,
                            void *context)
{
    if (!mounted || !acquire_filesystem()) {
        return mounted ? ORIONFS_BUSY : ORIONFS_NOT_MOUNTED;
    }
    uint32_t path_length = 0;
    uint32_t inode = 0;
    orionfs_status status =
        callback != NULL && path_is_valid(path, &path_length)
            ? resolve_path(path, path_length, &inode)
            : ORIONFS_INVALID;
    if (status == ORIONFS_OK &&
        inode_record(inode)[INODE_TYPE_OFFSET] != ORIONFS_NODE_DIRECTORY) {
        status = ORIONFS_NOT_DIRECTORY;
    }
    list_snapshot_entry entries[ORIONFS_OPEN_DIRECTORY_LIMIT];
    uint32_t entry_count = 0;
    if (status == ORIONFS_OK) {
        for (uint32_t child = 1; child < ORIONFS_MAX_INODES; ++child) {
            const uint8_t *record = inode_record(child);
            if (record[INODE_TYPE_OFFSET] == 0 ||
                read_le16(record + INODE_PARENT_OFFSET) != inode) {
                continue;
            }
            const uint8_t name_length = record[INODE_NAME_LENGTH_OFFSET];
            if (entry_count >= ORIONFS_OPEN_DIRECTORY_LIMIT ||
                name_length == 0 || name_length > ORIONFS_MAX_NAME_BYTES) {
                status = ORIONFS_CORRUPT;
                break;
            }
            list_snapshot_entry *entry = &entries[entry_count++];
            bytes_clear((uint8_t *)entry->name, sizeof(entry->name));
            bytes_copy((uint8_t *)entry->name,
                       record + INODE_NAME_OFFSET, name_length);
            entry->type = (orionfs_node_type)record[INODE_TYPE_OFFSET];
            entry->size = read_le32(record + INODE_FILE_SIZE_OFFSET);
        }
    }
    release_filesystem();
    if (status != ORIONFS_OK) {
        return status;
    }
    for (uint32_t index = 0; index < entry_count; ++index) {
        const orionfs_directory_entry entry = {
            .name = entries[index].name,
            .type = entries[index].type,
            .size = entries[index].size
        };
        if (!callback(context, &entry)) {
                return ORIONFS_INVALID;
        }
    }
    return ORIONFS_OK;
}

bool orionfs_is_mounted(void)
{
    return mounted;
}
