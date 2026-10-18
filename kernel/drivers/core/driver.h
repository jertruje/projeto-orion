#ifndef ORION_KERNEL_DRIVERS_CORE_DRIVER_H
#define ORION_KERNEL_DRIVERS_CORE_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_DRIVER_LIMIT 8U
#define ORION_DRIVER_DEVICE_LIMIT 32U
#define ORION_DRIVER_NAME_LIMIT 32U

typedef enum {
    ORION_DEVICE_PLATFORM,
    ORION_DEVICE_PCI
} orion_device_bus;

typedef enum {
    ORION_DRIVER_DISCOVERED,
    ORION_DRIVER_MATCHED,
    ORION_DRIVER_STARTING,
    ORION_DRIVER_ONLINE,
    ORION_DRIVER_FAILED,
    ORION_DRIVER_STOPPED
} orion_driver_state;

typedef struct {
    orion_device_bus bus;
    uint8_t bus_number;
    uint8_t device_number;
    uint8_t function_number;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t programming_interface;
    uint8_t revision;
    uint32_t bars[6];
    uint8_t bar_count;
    const char *platform_name;
} orion_device;

typedef bool (*orion_driver_match_fn)(const orion_device *device);
typedef bool (*orion_driver_start_fn)(const orion_device *device,
                                      void **driver_data);
typedef void (*orion_driver_cleanup_fn)(const orion_device *device,
                                        void *driver_data);

typedef struct {
    const char *name;
    orion_driver_match_fn match;
    orion_driver_start_fn start;
    orion_driver_cleanup_fn cleanup;
} orion_driver;

typedef struct {
    const orion_driver *driver;
    orion_device device;
    orion_driver_state state;
    void *driver_data;
} orion_driver_binding;

bool orion_driver_system_init(void);
bool orion_driver_register(const orion_driver *driver);
bool orion_driver_add_device(const orion_device *device);
bool orion_driver_bind_device(uint32_t device_index);
bool orion_driver_stop_device(uint32_t device_index);
uint32_t orion_driver_device_count(void);
bool orion_driver_get_binding(uint32_t device_index,
                              orion_driver_binding *binding);
bool orion_driver_check_invariants(void);

#endif
