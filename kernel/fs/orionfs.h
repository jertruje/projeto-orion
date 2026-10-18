#ifndef ORION_KERNEL_FS_ORIONFS_H
#define ORION_KERNEL_FS_ORIONFS_H

#include <stdbool.h>
#include <stdint.h>

#include "../storage/block.h"

#define ORIONFS_MAX_BLOCKS 4096U
#define ORIONFS_MAX_INODES 64U
#define ORIONFS_DATA_START_BLOCK 10U
#define ORIONFS_MAX_NAME_BYTES 31U
#define ORIONFS_MAX_FILE_BYTES (6U * ORION_BLOCK_MIN_SIZE)
#define ORIONFS_MAX_PATH_BYTES 256U
#define ORIONFS_MAX_PATH_DEPTH 8U
#define ORIONFS_OPEN_HANDLE_LIMIT 16U

typedef enum {
    ORIONFS_OK,
    ORIONFS_INVALID,
    ORIONFS_NOT_MOUNTED,
    ORIONFS_NOT_FOUND,
    ORIONFS_EXISTS,
    ORIONFS_NOT_DIRECTORY,
    ORIONFS_IS_DIRECTORY,
    ORIONFS_NOT_EMPTY,
    ORIONFS_NO_SPACE,
    ORIONFS_IO_ERROR,
    ORIONFS_CORRUPT,
    ORIONFS_BUSY,
    ORIONFS_TOO_LARGE,
    ORIONFS_READ_ONLY
} orionfs_status;

typedef enum {
    ORIONFS_NODE_FILE = 1,
    ORIONFS_NODE_DIRECTORY = 2
} orionfs_node_type;

typedef struct {
    uint32_t inode;
    uint64_t size;
    orionfs_node_type type;
} orionfs_info;

typedef struct {
    const char *name;
    orionfs_node_type type;
    uint64_t size;
} orionfs_directory_entry;

typedef bool (*orionfs_list_fn)(void *context,
                                const orionfs_directory_entry *entry);

typedef struct {
    uint16_t slot;
    uint32_t generation;
} orionfs_handle;

orionfs_status orionfs_format(orion_block_device *device);
orionfs_status orionfs_mount(orion_block_device *device);
orionfs_status orionfs_unmount(void);
orionfs_status orionfs_create(const char *path, orionfs_node_type type);
orionfs_status orionfs_remove(const char *path);
orionfs_status orionfs_stat(const char *path, orionfs_info *info);
orionfs_status orionfs_open(const char *path, orionfs_handle *handle);
orionfs_status orionfs_close(orionfs_handle handle);
orionfs_status orionfs_read(orionfs_handle handle, uint64_t offset,
                            void *buffer, uint64_t length,
                            uint64_t *bytes_read);
orionfs_status orionfs_write(orionfs_handle handle, uint64_t offset,
                             const void *buffer, uint64_t length,
                             uint64_t *bytes_written);
orionfs_status orionfs_list(const char *path, orionfs_list_fn callback,
                            void *context);
bool orionfs_is_mounted(void);

#endif
