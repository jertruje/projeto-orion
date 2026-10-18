#ifndef ORION_KERNEL_STORAGE_BLOCK_H
#define ORION_KERNEL_STORAGE_BLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_BLOCK_MIN_SIZE 512U
#define ORION_BLOCK_MAX_SIZE 4096U

typedef enum {
    ORION_BLOCK_OK,
    ORION_BLOCK_INVALID,
    ORION_BLOCK_UNAVAILABLE,
    ORION_BLOCK_READ_ONLY,
    ORION_BLOCK_OUT_OF_RANGE,
    ORION_BLOCK_ALIGNMENT,
    ORION_BLOCK_BUSY,
    ORION_BLOCK_IO_ERROR
} orion_block_status;

typedef orion_block_status (*orion_block_read_fn)(void *context,
                                                   uint64_t first_block,
                                                   uint32_t block_count,
                                                   void *buffer);
typedef orion_block_status (*orion_block_write_fn)(void *context,
                                                    uint64_t first_block,
                                                    uint32_t block_count,
                                                    const void *buffer);
typedef orion_block_status (*orion_block_sync_fn)(void *context);

typedef struct {
    const char *identifier;
    uint64_t block_count;
    uint32_t block_size;
    uint32_t buffer_alignment;
    bool available;
    bool read_only;
    void *context;
    orion_block_read_fn read;
    orion_block_write_fn write;
    orion_block_sync_fn sync;
} orion_block_device;

bool orion_block_device_is_valid(const orion_block_device *device);
orion_block_status orion_block_read(orion_block_device *device,
                                    uint64_t first_block,
                                    uint32_t block_count,
                                    void *buffer, uint64_t buffer_size);
orion_block_status orion_block_write(orion_block_device *device,
                                     uint64_t first_block,
                                     uint32_t block_count,
                                     const void *buffer,
                                     uint64_t buffer_size);
orion_block_status orion_block_sync(orion_block_device *device);

#endif
