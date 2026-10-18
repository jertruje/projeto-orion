#ifndef ORION_KERNEL_DRIVERS_BUS_PCI_H
#define ORION_KERNEL_DRIVERS_BUS_PCI_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_PCI_FUNCTION_LIMIT 256U
#define ORION_PCI_BAR_COUNT 6U

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision;
    uint8_t programming_interface;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t header_type;
    uint32_t bars[ORION_PCI_BAR_COUNT];
    uint8_t bar_count;
} orion_pci_device;

typedef bool (*orion_pci_read_config_fn)(void *context, uint8_t bus,
                                         uint8_t device, uint8_t function,
                                         uint8_t offset, uint32_t *value);
typedef bool (*orion_pci_device_fn)(void *context,
                                   const orion_pci_device *device);

typedef struct {
    uint32_t discovered;
    uint32_t rejected;
} orion_pci_scan_result;

bool orion_pci_scan_bus(uint8_t bus, orion_pci_read_config_fn read_config,
                        orion_pci_device_fn on_device, void *context,
                        orion_pci_scan_result *result);
bool orion_pci_read_config_legacy(void *context, uint8_t bus, uint8_t device,
                                  uint8_t function, uint8_t offset,
                                  uint32_t *value);
bool orion_pci_write_config_legacy(void *context, uint8_t bus, uint8_t device,
                                   uint8_t function, uint8_t offset,
                                   uint32_t value);

#endif
