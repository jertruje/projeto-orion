#include "pci.h"

#include <stddef.h>

#include "../../arch/x86_64/interrupt_state.h"

#define PCI_CONFIG_ADDRESS UINT16_C(0x0cf8)
#define PCI_CONFIG_DATA UINT16_C(0x0cfc)

static void out32(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0, %1"
                     :
                     : "a"(value), "Nd"(port)
                     : "memory");
}

static uint32_t in32(uint16_t port)
{
    uint32_t value;
    __asm__ volatile("inl %1, %0"
                     : "=a"(value)
                     : "Nd"(port)
                     : "memory");
    return value;
}

bool orion_pci_read_config_legacy(void *context, uint8_t bus, uint8_t device,
                                  uint8_t function, uint8_t offset,
                                  uint32_t *value)
{
    (void)context;
    if (device >= 32U || function >= 8U || offset > 252U ||
        (offset & 3U) != 0 || value == NULL) {
        return false;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const uint32_t address = UINT32_C(0x80000000) |
                             ((uint32_t)bus << 16) |
                             ((uint32_t)device << 11) |
                             ((uint32_t)function << 8) |
                             ((uint32_t)offset & UINT32_C(0xfc));
    out32(PCI_CONFIG_ADDRESS, address);
    *value = in32(PCI_CONFIG_DATA);
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

bool orion_pci_write_config_legacy(void *context, uint8_t bus,
                                   uint8_t device, uint8_t function,
                                   uint8_t offset, uint32_t value)
{
    (void)context;
    if (device >= 32U || function >= 8U || offset > 252U ||
        (offset & 3U) != 0) {
        return false;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const uint32_t address = UINT32_C(0x80000000) |
                             ((uint32_t)bus << 16) |
                             ((uint32_t)device << 11) |
                             ((uint32_t)function << 8) |
                             ((uint32_t)offset & UINT32_C(0xfc));
    out32(PCI_CONFIG_ADDRESS, address);
    out32(PCI_CONFIG_DATA, value);
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

static bool read_config(orion_pci_read_config_fn read, void *context,
                        uint8_t bus, uint8_t device, uint8_t function,
                        uint8_t offset, uint32_t *value)
{
    return read != NULL &&
           read(context, bus, device, function, offset, value);
}

static bool append_device(uint8_t bus, uint8_t device, uint8_t function,
                          orion_pci_read_config_fn read_config_fn,
                          void *read_context,
                          orion_pci_device_fn on_device, void *context,
                          orion_pci_scan_result *result)
{
    uint32_t id;
    uint32_t class_register;
    uint32_t header_register;
    if (!read_config(read_config_fn, read_context, bus, device, function,
                     0, &id) ||
        !read_config(read_config_fn, read_context, bus, device, function,
                     8, &class_register) ||
        !read_config(read_config_fn, read_context, bus, device, function,
                     12, &header_register) ||
        (id & UINT32_C(0xffff)) == UINT16_MAX) {
        return true;
    }

    orion_pci_device found = {
        .bus = bus,
        .device = device,
        .function = function,
        .vendor_id = (uint16_t)id,
        .device_id = (uint16_t)(id >> 16),
        .revision = (uint8_t)class_register,
        .programming_interface = (uint8_t)(class_register >> 8),
        .subclass = (uint8_t)(class_register >> 16),
        .class_code = (uint8_t)(class_register >> 24),
        .header_type = (uint8_t)(header_register >> 16),
        .bar_count = (uint8_t)(((header_register >> 16) & 0x7fU) == 0
                                   ? ORION_PCI_BAR_COUNT
                                   : (((header_register >> 16) & 0x7fU) == 1
                                          ? 2U
                                          : 0U))
    };

    const uint8_t bar_offset = 16;
    for (uint32_t index = 0; index < found.bar_count; ++index) {
        if (!read_config(read_config_fn, read_context, bus, device, function,
                         (uint8_t)(bar_offset + index * 4U),
                         &found.bars[index])) {
            return false;
        }
    }
    if (result->discovered == ORION_PCI_FUNCTION_LIMIT ||
        !on_device(context, &found)) {
        ++result->rejected;
        return true;
    }
    ++result->discovered;
    return true;
}

bool orion_pci_scan_bus(uint8_t bus, orion_pci_read_config_fn read_config_fn,
                        orion_pci_device_fn on_device, void *context,
                        orion_pci_scan_result *result)
{
    if (read_config_fn == NULL || on_device == NULL || result == NULL) {
        return false;
    }
    *result = (orion_pci_scan_result){0};
    for (uint8_t device = 0; device < 32U; ++device) {
        uint32_t id;
        uint32_t header_register;
        if (!read_config_fn(context, bus, device, 0, 0, &id)) {
            return false;
        }
        if ((id & UINT32_C(0xffff)) == UINT16_MAX) {
            continue;
        }
        if (!read_config_fn(context, bus, device, 0, 12,
                            &header_register) ||
            !append_device(bus, device, 0, read_config_fn, context,
                           on_device, context, result)) {
            return false;
        }
        const uint8_t header_type = (uint8_t)(header_register >> 16);
        if ((header_type & 0x80U) == 0) {
            continue;
        }
        for (uint8_t function = 1; function < 8U; ++function) {
            if (!read_config_fn(context, bus, device, function, 0, &id)) {
                return false;
            }
            if ((id & UINT32_C(0xffff)) != UINT16_MAX &&
                !append_device(bus, device, function, read_config_fn,
                               context, on_device, context, result)) {
                return false;
            }
        }
    }
    return true;
}
