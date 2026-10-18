#include "driver.h"

#include <stddef.h>

#include "../../arch/x86_64/interrupt_state.h"
#include "../../fault/report.h"

typedef struct {
    orion_device device;
    const orion_driver *driver;
    orion_driver_state state;
    void *driver_data;
} driver_slot;

static const orion_driver *registered_drivers[ORION_DRIVER_LIMIT];
static uint32_t registered_driver_count;
static driver_slot devices[ORION_DRIVER_DEVICE_LIMIT];
static uint32_t device_count;
static bool initialized;

static bool driver_name_is_valid(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    for (uint32_t index = 0; index < ORION_DRIVER_NAME_LIMIT; ++index) {
        if (name[index] == '\0') {
            return true;
        }
    }
    return false;
}

static bool platform_name_is_valid(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    for (uint32_t index = 0; index < ORION_DRIVER_NAME_LIMIT; ++index) {
        if (name[index] == '\0') {
            return true;
        }
    }
    return false;
}

static bool names_equal(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return left == right;
    }
    for (uint32_t index = 0; index < ORION_DRIVER_NAME_LIMIT; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
        if (left[index] == '\0') {
            return true;
        }
    }
    return false;
}

bool orion_driver_system_init(void)
{
    if (initialized) {
        return false;
    }
    registered_driver_count = 0;
    device_count = 0;
    initialized = true;
    return true;
}

bool orion_driver_register(const orion_driver *driver)
{
    if (!initialized || driver == NULL ||
        !driver_name_is_valid(driver->name) || driver->match == NULL ||
        driver->start == NULL || driver->cleanup == NULL ||
        registered_driver_count >= ORION_DRIVER_LIMIT) {
        return false;
    }
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    for (uint32_t index = 0; index < registered_driver_count; ++index) {
        if (registered_drivers[index] == driver ||
            names_equal(registered_drivers[index]->name, driver->name)) {
            orion_x86_64_interrupts_restore(was_enabled);
            return false;
        }
    }
    registered_drivers[registered_driver_count++] = driver;
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

bool orion_driver_add_device(const orion_device *device)
{
    if (!initialized || device == NULL ||
        (device->bus != ORION_DEVICE_PLATFORM &&
         device->bus != ORION_DEVICE_PCI) ||
        (device->bus == ORION_DEVICE_PLATFORM &&
         !platform_name_is_valid(device->platform_name)) ||
        (device->bus == ORION_DEVICE_PCI &&
         (device->vendor_id == UINT16_MAX ||
          device->device_id == UINT16_MAX ||
          device->device_number >= 32U || device->function_number >= 8U ||
          device->bar_count > 6U)) ||
        device_count >= ORION_DRIVER_DEVICE_LIMIT) {
        return false;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    for (uint32_t index = 0; index < device_count; ++index) {
        const orion_device *existing = &devices[index].device;
        if (existing->bus == device->bus &&
            existing->bus_number == device->bus_number &&
            existing->device_number == device->device_number &&
            existing->function_number == device->function_number &&
            (device->bus == ORION_DEVICE_PCI ||
             names_equal(existing->platform_name, device->platform_name))) {
            orion_x86_64_interrupts_restore(was_enabled);
            return false;
        }
    }
    devices[device_count++] = (driver_slot){
        .device = *device,
        .driver = NULL,
        .state = ORION_DRIVER_DISCOVERED,
        .driver_data = NULL
    };
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

bool orion_driver_bind_device(uint32_t device_index)
{
    if (!initialized || device_index >= device_count) {
        return false;
    }
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    driver_slot *slot = &devices[device_index];
    if (slot->state != ORION_DRIVER_DISCOVERED) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }

    slot->state = ORION_DRIVER_MATCHED;
    const orion_device device_copy = slot->device;
    const uint32_t driver_count = registered_driver_count;
    const orion_driver *driver_snapshot[ORION_DRIVER_LIMIT];
    for (uint32_t index = 0; index < driver_count; ++index) {
        driver_snapshot[index] = registered_drivers[index];
    }
    orion_x86_64_interrupts_restore(was_enabled);

    const orion_driver *selected = NULL;
    for (uint32_t index = 0; index < driver_count; ++index) {
        const orion_driver *candidate = driver_snapshot[index];
        if (candidate->match(&device_copy)) {
            selected = candidate;
            break;
        }
    }
    const bool match_was_enabled =
        orion_x86_64_interrupts_save_disable();
    if (slot->state != ORION_DRIVER_MATCHED) {
        orion_x86_64_interrupts_restore(match_was_enabled);
        return false;
    }
    if (selected == NULL) {
        slot->state = ORION_DRIVER_DISCOVERED;
        orion_x86_64_interrupts_restore(match_was_enabled);
        return false;
    }

    slot->driver = selected;
    slot->state = ORION_DRIVER_STARTING;
    orion_x86_64_interrupts_restore(match_was_enabled);

    void *driver_data = NULL;
    const bool started = selected->start(&device_copy, &driver_data);
    const bool finalize_was_enabled =
        orion_x86_64_interrupts_save_disable();
    if (slot->state != ORION_DRIVER_STARTING || slot->driver != selected) {
        orion_x86_64_interrupts_restore(finalize_was_enabled);
        selected->cleanup(&device_copy, driver_data);
        return false;
    }
    if (!started) {
        slot->driver = NULL;
        slot->driver_data = NULL;
        slot->state = ORION_DRIVER_FAILED;
        orion_x86_64_interrupts_restore(finalize_was_enabled);
        selected->cleanup(&device_copy, driver_data);
        orion_fault_record fault = {
            .detail = ((uint64_t)device_copy.vendor_id << 16) |
                      device_copy.device_id,
            .category = ORION_FAULT_DRIVER_FAILURE,
            .context = ORION_FAULT_CONTEXT_BOOT,
            .action = ORION_FAULT_ACTION_NONE,
            .recovery_result = ORION_FAULT_RECOVERY_FAILED
        };
        uint64_t event_id = 0;
        (void)orion_fault_report_record(&fault, &event_id);
        return false;
    }
    slot->driver_data = driver_data;
    slot->state = ORION_DRIVER_ONLINE;
    orion_x86_64_interrupts_restore(finalize_was_enabled);
    return true;
}

bool orion_driver_stop_device(uint32_t device_index)
{
    if (!initialized || device_index >= device_count) {
        return false;
    }
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    driver_slot *slot = &devices[device_index];
    if (slot->state != ORION_DRIVER_ONLINE || slot->driver == NULL) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }
    const orion_driver *driver = slot->driver;
    const orion_device device = slot->device;
    void *driver_data = slot->driver_data;
    slot->state = ORION_DRIVER_STOPPED;
    slot->driver_data = NULL;
    orion_x86_64_interrupts_restore(was_enabled);
    driver->cleanup(&device, driver_data);
    return true;
}

uint32_t orion_driver_device_count(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const uint32_t count = device_count;
    orion_x86_64_interrupts_restore(was_enabled);
    return count;
}

bool orion_driver_get_binding(uint32_t device_index,
                              orion_driver_binding *binding)
{
    if (binding == NULL || !initialized || device_index >= device_count) {
        return false;
    }
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const driver_slot *slot = &devices[device_index];
    *binding = (orion_driver_binding){
        .driver = slot->driver,
        .device = slot->device,
        .state = slot->state,
        .driver_data = slot->driver_data
    };
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

bool orion_driver_check_invariants(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    bool valid = initialized &&
                 registered_driver_count <= ORION_DRIVER_LIMIT &&
                 device_count <= ORION_DRIVER_DEVICE_LIMIT;
    for (uint32_t index = 0; valid && index < device_count; ++index) {
        const driver_slot *slot = &devices[index];
        valid = slot->state <= ORION_DRIVER_STOPPED;
        if (slot->state == ORION_DRIVER_DISCOVERED) {
            valid = valid && slot->driver == NULL &&
                    slot->driver_data == NULL;
        }
        if (slot->state == ORION_DRIVER_ONLINE ||
            slot->state == ORION_DRIVER_STOPPED) {
            valid = valid && slot->driver != NULL;
        }
        for (uint32_t other = index + 1U;
             valid && other < device_count; ++other) {
            const orion_device *left = &slot->device;
            const orion_device *right = &devices[other].device;
            valid = !(left->bus == right->bus &&
                      left->bus_number == right->bus_number &&
                      left->device_number == right->device_number &&
                      left->function_number == right->function_number &&
                      (left->bus == ORION_DEVICE_PCI ||
                       names_equal(left->platform_name,
                                   right->platform_name)));
        }
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return valid;
}
