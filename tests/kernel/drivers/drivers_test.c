#include "../../../kernel/drivers/bus/pci.h"
#include "../../../kernel/drivers/core/driver.h"
#include "../../../kernel/arch/x86_64/interrupt_state.h"
#include "../../../kernel/fault/report.h"

#include <string.h>

static bool interrupts_enabled = true;
static uint32_t start_count;
static uint32_t cleanup_count;
static bool start_succeeds = true;

bool orion_x86_64_interrupts_save_disable(void)
{
    const bool previous = interrupts_enabled;
    interrupts_enabled = false;
    return previous;
}

void orion_x86_64_interrupts_restore(bool enabled)
{
    interrupts_enabled = enabled;
}

static bool platform_matches(const orion_device *device)
{
    return device != NULL && device->bus == ORION_DEVICE_PLATFORM &&
           device->platform_name != NULL &&
           strcmp(device->platform_name, "unit-device") == 0;
}

static bool failed_device_matches(const orion_device *device)
{
    return device != NULL && device->bus == ORION_DEVICE_PLATFORM &&
           device->platform_name != NULL &&
           strcmp(device->platform_name, "failing-device") == 0;
}

static bool driver_start(const orion_device *device, void **data)
{
    if (device == NULL || data == NULL) {
        return false;
    }
    ++start_count;
    *data = (void *)(uintptr_t)UINT64_C(0x1234);
    return start_succeeds;
}

static void driver_cleanup(const orion_device *device, void *data)
{
    (void)device;
    (void)data;
    ++cleanup_count;
}

static const orion_driver unit_driver = {
    "unit-driver", platform_matches, driver_start, driver_cleanup
};
static const orion_driver failing_driver = {
    "failing-driver", failed_device_matches, driver_start, driver_cleanup
};
static const orion_driver invalid_driver = {
    "", platform_matches, driver_start, driver_cleanup
};

typedef struct {
    uint32_t config[32][8][16];
    orion_pci_device found[4];
    uint32_t found_count;
} pci_test_context;

static bool pci_read(void *opaque, uint8_t bus, uint8_t device,
                     uint8_t function, uint8_t offset, uint32_t *value)
{
    pci_test_context *context = opaque;
    if (context == NULL || value == NULL || bus != 0 || device >= 32 ||
        function >= 8 || offset > 252 || (offset & 3U) != 0) {
        return false;
    }
    *value = context->config[device][function][offset / 4U];
    return true;
}

static bool pci_collect(void *opaque, const orion_pci_device *device)
{
    pci_test_context *context = opaque;
    if (context == NULL || device == NULL ||
        context->found_count >= 4U) {
        return false;
    }
    context->found[context->found_count++] = *device;
    return true;
}

static void set_pci_function(pci_test_context *context, uint8_t device,
                             uint8_t function, uint32_t id,
                             uint32_t class_register, uint8_t header_type)
{
    context->config[device][function][0] = id;
    context->config[device][function][2] = class_register;
    context->config[device][function][3] =
        (uint32_t)header_type << 16;
    for (uint32_t index = 4; index < 16; ++index) {
        context->config[device][function][index] =
            UINT32_C(0x80000000) | index;
    }
}

int main(void)
{
    if (!orion_driver_system_init() ||
        orion_driver_system_init() ||
        orion_driver_register(&invalid_driver) ||
        orion_driver_register(NULL) ||
        !orion_driver_register(&failing_driver) ||
        !orion_driver_register(&unit_driver) ||
        orion_driver_register(&unit_driver) ||
        orion_driver_device_count() != 0 ||
        !orion_driver_check_invariants()) {
        return 1;
    }

    const orion_device absent = {
        .bus = ORION_DEVICE_PLATFORM,
        .platform_name = "absent-device"
    };
    const orion_device failed = {
        .bus = ORION_DEVICE_PLATFORM,
        .platform_name = "failing-device"
    };
    const orion_device active = {
        .bus = ORION_DEVICE_PLATFORM,
        .platform_name = "unit-device"
    };
    const orion_device invalid_pci = {
        .bus = ORION_DEVICE_PCI,
        .vendor_id = UINT16_MAX,
        .device_id = 1,
        .device_number = 32
    };
    if (orion_driver_add_device(NULL) ||
        orion_driver_add_device(&invalid_pci) ||
        !orion_driver_add_device(&absent) ||
        !orion_driver_add_device(&failed) ||
        !orion_driver_add_device(&active) ||
        orion_driver_add_device(&active) ||
        orion_driver_bind_device(99) ||
        orion_driver_bind_device(0) ||
        (start_succeeds = false, orion_driver_bind_device(1))) {
        return 2;
    }
    start_succeeds = true;
    orion_driver_binding binding;
    if (!orion_driver_get_binding(0, &binding) ||
        binding.driver != NULL || binding.state != ORION_DRIVER_DISCOVERED)
        return 31;
    if (!orion_driver_get_binding(1, &binding) ||
        binding.driver != NULL || binding.state != ORION_DRIVER_FAILED ||
        cleanup_count != 1U || start_count != 1U)
        return 32;
    orion_fault_record fault;
    if (orion_fault_report_count() != 1U ||
        orion_fault_report_get(1, &fault) != ORION_FAULT_STATUS_OK ||
        fault.category != ORION_FAULT_DRIVER_FAILURE ||
        fault.action != ORION_FAULT_ACTION_NONE ||
        fault.recovery_result != ORION_FAULT_RECOVERY_FAILED)
        return 37;
    if (!orion_driver_bind_device(2))
        return 33;
    if (!orion_driver_get_binding(2, &binding) ||
        binding.state != ORION_DRIVER_ONLINE ||
        binding.driver != &unit_driver ||
        binding.driver_data != (void *)(uintptr_t)UINT64_C(0x1234))
        return 34;
    if (!orion_driver_stop_device(2) ||
        orion_driver_stop_device(2))
        return 35;
    if (!orion_driver_get_binding(2, &binding) ||
        binding.state != ORION_DRIVER_STOPPED ||
        cleanup_count != 2U || !orion_driver_check_invariants())
        return 36;

    pci_test_context pci = {0};
    for (uint32_t device = 0; device < 32; ++device) {
        for (uint32_t function = 0; function < 8; ++function) {
            for (uint32_t offset = 0; offset < 16; ++offset) {
                pci.config[device][function][offset] = UINT32_MAX;
            }
        }
    }
    set_pci_function(&pci, 0, 0, UINT32_C(0x12348086),
                     UINT32_C(0x06000001), 0x80);
    set_pci_function(&pci, 0, 1, UINT32_C(0x56781234),
                     UINT32_C(0x03000002), 0x00);
    set_pci_function(&pci, 1, 0, UINT32_C(0x11111234),
                     UINT32_C(0x01060103), 0x01);
    orion_pci_scan_result result;
    if (!orion_pci_scan_bus(0, pci_read, pci_collect, &pci, &result) ||
        result.discovered != 3U || result.rejected != 0U ||
        pci.found_count != 3U ||
        pci.found[0].vendor_id != UINT16_C(0x8086) ||
        pci.found[0].device_id != UINT16_C(0x1234) ||
        pci.found[0].class_code != UINT8_C(0x06) ||
        pci.found[0].bar_count != 6U ||
        pci.found[0].bars[0] != (UINT32_C(0x80000000) | 4U) ||
        pci.found[1].function != 1U ||
        pci.found[2].device != 1U ||
        pci.found[2].bar_count != 2U ||
        orion_pci_scan_bus(0, NULL, pci_collect, &pci, &result) ||
        orion_pci_read_config_legacy(NULL, 0, 32, 0, 0,
                                     &pci.config[0][0][0]) ||
        orion_pci_read_config_legacy(NULL, 0, 0, 0, 3,
                                     &pci.config[0][0][0]) ||
        !interrupts_enabled) {
        return 4;
    }
    return 0;
}
