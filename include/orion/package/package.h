#ifndef ORION_PACKAGE_PACKAGE_H
#define ORION_PACKAGE_PACKAGE_H

#include <stdint.h>
#include <stdbool.h>

#define ORION_PACKAGE_FORMAT_VERSION 1

typedef enum {
    ORION_PACKAGE_TYPE_APPLICATION = 1,
    ORION_PACKAGE_TYPE_LIBRARY,
    ORION_PACKAGE_TYPE_DRIVER,
    ORION_PACKAGE_TYPE_FIRMWARE,
    ORION_PACKAGE_TYPE_KERNEL_MODULE
} orion_package_type_t;

typedef struct {
    char name[64];
    char version[32];
} orion_package_id_t;

typedef struct {
    orion_package_id_t package;
    char min_version[32];
    char max_version[32];
} orion_package_dependency_t;

typedef struct {
    char source_path[256];
    char dest_path[256];
    uint64_t size;
    uint32_t permissions;
} orion_package_file_t;

typedef struct {
    uint32_t format_version;
    orion_package_id_t id;
    orion_package_type_t type;
    char description[512];
    char vendor[128];
    uint32_t file_count;
    orion_package_file_t* files;
    uint32_t dependency_count;
    orion_package_dependency_t* dependencies;
    uint8_t signature[64];
    uint32_t signature_length;
} orion_package_manifest_t;

typedef enum {
    ORION_PACKAGE_STATUS_UNKNOWN = 0,
    ORION_PACKAGE_STATUS_AVAILABLE,
    ORION_PACKAGE_STATUS_INSTALLED,
    ORION_PACKAGE_STATUS_UPDATE_AVAILABLE,
    ORION_PACKAGE_STATUS_BROKEN
} orion_package_status_t;

typedef struct {
    orion_package_id_t id;
    orion_package_type_t type;
    char install_date[32];
    orion_package_status_t status;
    uint32_t file_count;
    char** files;
} orion_installed_package_t;

typedef struct {
    bool (*initialize)(void);
    void (*shutdown)(void);
    bool (*install)(const char* package_path);
    bool (*remove)(const orion_package_id_t* package_id);
    bool (*update)(const char* package_path);
    uint32_t (*query_installed)(orion_installed_package_t** packages);
    bool (*get_info)(const orion_package_id_t* package_id, orion_installed_package_t* package_info);
    bool (*verify_integrity)(const char* package_path);
    bool (*check_for_updates)(const orion_package_id_t* package_id);
} orion_package_manager_t;

extern orion_package_manager_t orion_package_manager;

