#include "vfs.h"

orion_vfs_status orion_vfs_mount(orion_block_device *device)
{
    return orionfs_mount(device);
}

orion_vfs_status orion_vfs_unmount(void)
{
    return orionfs_unmount();
}

orion_vfs_status orion_vfs_create(const char *path, orionfs_node_type type)
{
    return orionfs_create(path, type);
}

orion_vfs_status orion_vfs_remove(const char *path)
{
    return orionfs_remove(path);
}

orion_vfs_status orion_vfs_stat(const char *path, orion_vfs_info *info)
{
    return orionfs_stat(path, info);
}

orion_vfs_status orion_vfs_open(const char *path, orion_vfs_handle *handle)
{
    return orionfs_open(path, handle);
}

orion_vfs_status orion_vfs_close(orion_vfs_handle handle)
{
    return orionfs_close(handle);
}

orion_vfs_status orion_vfs_read(orion_vfs_handle handle, uint64_t offset,
                                void *buffer, uint64_t length,
                                uint64_t *bytes_read)
{
    return orionfs_read(handle, offset, buffer, length, bytes_read);
}

orion_vfs_status orion_vfs_write(orion_vfs_handle handle, uint64_t offset,
                                 const void *buffer, uint64_t length,
                                 uint64_t *bytes_written)
{
    return orionfs_write(handle, offset, buffer, length, bytes_written);
}

orion_vfs_status orion_vfs_list(const char *path,
                                orion_vfs_list_fn callback, void *context)
{
    return orionfs_list(path, callback, context);
}
