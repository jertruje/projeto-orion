#include "block.h"

#include <stddef.h>

static bool block_range_is_valid(const orion_block_device *device,
                                 uint64_t first_block,
                                 uint32_t block_count,
                                 const void *buffer,
                                 uint64_t buffer_size)
{
    if (!orion_block_device_is_valid(device) || !device->available ||
        buffer == NULL || block_count == 0) {
        return false;
    }
    if (first_block >= device->block_count ||
        (uint64_t)block_count > device->block_count - first_block) {
        return false;
    }
    if ((uint64_t)block_count > UINT64_MAX / device->block_size) {
        return false;
    }
    const uint64_t required_bytes =
        (uint64_t)block_count * device->block_size;
    if (buffer_size < required_bytes) {
        return false;
    }
    return (uintptr_t)buffer % device->buffer_alignment == 0;
}

bool orion_block_device_is_valid(const orion_block_device *device)
{
    return device != NULL && device->identifier != NULL &&
           device->identifier[0] != '\0' && device->block_count != 0 &&
           device->block_size >= ORION_BLOCK_MIN_SIZE &&
           device->block_size <= ORION_BLOCK_MAX_SIZE &&
           (device->block_size & (device->block_size - 1U)) == 0 &&
           device->buffer_alignment != 0 &&
           (device->buffer_alignment & (device->buffer_alignment - 1U)) == 0 &&
           device->read != NULL &&
           (device->read_only || device->write != NULL);
}

orion_block_status orion_block_read(orion_block_device *device,
                                    uint64_t first_block,
                                    uint32_t block_count,
                                    void *buffer, uint64_t buffer_size)
{
    if (device == NULL || !device->available) {
        return ORION_BLOCK_UNAVAILABLE;
    }
    if (!block_range_is_valid(device, first_block, block_count, buffer,
                              buffer_size)) {
        if (device->block_count != 0 &&
            (first_block >= device->block_count ||
             (uint64_t)block_count > device->block_count - first_block)) {
            return ORION_BLOCK_OUT_OF_RANGE;
        }
        if (buffer != NULL && device->buffer_alignment != 0 &&
            (uintptr_t)buffer % device->buffer_alignment != 0) {
            return ORION_BLOCK_ALIGNMENT;
        }
        return ORION_BLOCK_INVALID;
    }
    return device->read(device->context, first_block, block_count, buffer);
}

orion_block_status orion_block_write(orion_block_device *device,
                                     uint64_t first_block,
                                     uint32_t block_count,
                                     const void *buffer,
                                     uint64_t buffer_size)
{
    if (device == NULL || !device->available) {
        return ORION_BLOCK_UNAVAILABLE;
    }
    if (device->read_only || device->write == NULL) {
        return ORION_BLOCK_READ_ONLY;
    }
    if (!block_range_is_valid(device, first_block, block_count, buffer,
                              buffer_size)) {
        if (device->block_count != 0 &&
            (first_block >= device->block_count ||
             (uint64_t)block_count > device->block_count - first_block)) {
            return ORION_BLOCK_OUT_OF_RANGE;
        }
        if (buffer != NULL && device->buffer_alignment != 0 &&
            (uintptr_t)buffer % device->buffer_alignment != 0) {
            return ORION_BLOCK_ALIGNMENT;
        }
        return ORION_BLOCK_INVALID;
    }
    return device->write(device->context, first_block, block_count, buffer);
}

orion_block_status orion_block_sync(orion_block_device *device)
{
    if (device == NULL || !device->available) {
        return ORION_BLOCK_UNAVAILABLE;
    }
    if (device->read_only) {
        return ORION_BLOCK_READ_ONLY;
    }
    return device->sync == NULL ? ORION_BLOCK_OK
                                : device->sync(device->context);
}
