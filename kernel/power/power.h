#ifndef ORION_KERNEL_POWER_H
#define ORION_KERNEL_POWER_H

#include <stdbool.h>

// Forward declaration for ACPI functions
bool orion_acpi_init(const orion_boot_info *boot_info);
bool orion_acpi_check_invariants(void);
bool orion_acpi_enter_sleep_state(uint8_t sleep_state);

#endif
