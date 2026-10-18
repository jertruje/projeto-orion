#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <io.h>

#include "../kernel/fs/orionfs.h"

typedef struct {
    FILE *file;
} image_context;

static orion_block_status image_read(void *context, uint64_t first_block,
                                     uint32_t block_count, void *buffer)
{
    image_context *image = context;
    if (image == NULL || image->file == NULL ||
        first_block > (uint64_t)INT64_MAX / ORION_BLOCK_MIN_SIZE ||
        _fseeki64(image->file,
                  (int64_t)(first_block * ORION_BLOCK_MIN_SIZE), SEEK_SET) != 0 ||
        fread(buffer, ORION_BLOCK_MIN_SIZE, block_count, image->file) !=
            block_count) {
        return ORION_BLOCK_IO_ERROR;
    }
    return ORION_BLOCK_OK;
}

static orion_block_status image_write(void *context, uint64_t first_block,
                                      uint32_t block_count,
                                      const void *buffer)
{
    image_context *image = context;
    if (image == NULL || image->file == NULL ||
        first_block > (uint64_t)INT64_MAX / ORION_BLOCK_MIN_SIZE ||
        _fseeki64(image->file,
                  (int64_t)(first_block * ORION_BLOCK_MIN_SIZE), SEEK_SET) != 0 ||
        fwrite(buffer, ORION_BLOCK_MIN_SIZE, block_count, image->file) !=
            block_count) {
        return ORION_BLOCK_IO_ERROR;
    }
    return ORION_BLOCK_OK;
}

static orion_block_status image_sync(void *context)
{
    image_context *image = context;
    if (image == NULL || image->file == NULL ||
        fflush(image->file) != 0 || _commit(_fileno(image->file)) != 0) {
        return ORION_BLOCK_IO_ERROR;
    }
    return ORION_BLOCK_OK;
}

int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "format") != 0) {
        (void)fprintf(stderr,
                      "Usage: orionfs-image.exe format <existing-raw-image>\n");
        return 2;
    }
    FILE *file = fopen(argv[2], "r+b");
    if (file == NULL) {
        (void)fprintf(stderr, "Could not open image '%s': %s\n", argv[2],
                      strerror(errno));
        return 1;
    }
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        (void)fprintf(stderr, "Could not determine image size.\n");
        (void)fclose(file);
        return 1;
    }
    const int64_t size = _ftelli64(file);
    if (size < (int64_t)(ORIONFS_DATA_START_BLOCK * ORION_BLOCK_MIN_SIZE) ||
        (size % ORION_BLOCK_MIN_SIZE) != 0) {
        (void)fprintf(stderr,
                      "Image must contain at least %u aligned 512-byte sectors.\n",
                      ORIONFS_DATA_START_BLOCK);
        (void)fclose(file);
        return 1;
    }

    image_context context = {.file = file};
    orion_block_device device = {
        .identifier = "explicit-image-format",
        .block_count = (uint64_t)size / ORION_BLOCK_MIN_SIZE,
        .block_size = ORION_BLOCK_MIN_SIZE,
        .buffer_alignment = 1,
        .available = true,
        .read_only = false,
        .context = &context,
        .read = image_read,
        .write = image_write,
        .sync = image_sync
    };
    const orionfs_status result = orionfs_format(&device);
    const bool close_succeeded = fclose(file) == 0;
    if (result != ORIONFS_OK || !close_succeeded) {
        (void)fprintf(stderr, "ORIONFS format failed: status=%u\n",
                      (unsigned int)result);
        return 1;
    }
    (void)printf("Explicitly formatted ORIONFS v1 on %s (%llu sectors).\n",
                 argv[2], (unsigned long long)device.block_count);
    return 0;
}
