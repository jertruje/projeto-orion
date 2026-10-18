#ifndef ORION_KERNEL_VFS_VFS_H
#define ORION_KERNEL_VFS_VFS_H

#include "../fs/orionfs.h"

typedef orionfs_handle orion_vfs_handle;
typedef orionfs_info orion_vfs_info;
typedef orionfs_directory_entry orion_vfs_directory_entry;
typedef orionfs_list_fn orion_vfs_list_fn;
typedef orionfs_status orion_vfs_status;

orion_vfs_status orion_vfs_mount(orion_block_device *device);
orion_vfs_status orion_vfs_unmount(void);
orion_vfs_status orion_vfs_create(const char *path, orionfs_node_type type);
orion_vfs_status orion_vfs_remove(const char *path);
orion_vfs_status orion_vfs_stat(const char *path, orion_vfs_info *info);
orion_vfs_status orion_vfs_open(const char *path, orion_vfs_handle *handle);
orion_vfs_status orion_vfs_close(orion_vfs_handle handle);
orion_vfs_status orion_vfs_read(orion_vfs_handle handle, uint64_t offset,
                                void *buffer, uint64_t length,
                                uint64_t *bytes_read);
orion_vfs_status orion_vfs_write(orion_vfs_handle handle, uint64_t offset,
                                 const void *buffer, uint64_t length,
                                 uint64_t *bytes_written);
orion_vfs_status orion_vfs_list(const char *path,
                                orion_vfs_list_fn callback, void *context);

#endif
