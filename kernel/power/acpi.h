#ifndef ORION_KERNEL_POWER_ACPI_H
#define ORION_KERNEL_POWER_ACPI_H

#include <stdbool.h>
#include <stdint.h>
#include <orion/boot_info.h>

// ACPI 0.1 (RSDT) and ACPI 2.0+ (XSDT) table headers
#define ACPI_RSDP_SIGNATURE_UINT64 UINT64_C(0x2052545020525344) // "RSD PTR " in little-endian
#define ACPI_RSDP_SIGNATURE "RSD PTR "

#define ACPI_DESCRIPTOR_HEADER_SIZE 4
#define ACPI_NAMESEG_SIZE 4

// ACPI table descriptor header (common to all ACPI tables)
typedef struct {
    uint32_t signature;     // ACPI signature (e.g., "FACP", "DSDT")
    uint32_t length;        // Length of table in bytes
    uint8_t revision;       // Minor revision
    uint8_t checksum;       // To make sum of entire table == 0
    uint8_t oem_id[6];      // OEM identification
    uint8_t oem_table_id[8]; // OEM table identification
    uint8_t oem_revision[4]; // OEM revision of table
    uint8_t creator_id[4];   // Vendor ID of utility that created table
    uint8_t creator_rev[4];  // Revision of utility that created table
} __attribute__((packed)) acpi_table_header;

// RSDP (Root System Description Pointer) structure for ACPI 0.1 and 2.0
typedef struct {
    uint8_t signature[8];           // "RSD PTR "
    uint8_t checksum;               // Must add to 0
    uint8_t oem_id[6];              // OEM identification
    uint8_t revision;               // Must be 0 for ACPI 1.0, 2 for ACPI 2.0+
    uint32_t rsdt_physical_address; // 32-bit physical address of RSDT
    uint32_t length;                // Length of the entire RSDP structure (ACPI 2.0+)
    uint64_t xsdt_physical_address; // 64-bit physical address of XSDT (ACPI 2.0+)
    uint8_t extended_checksum;      // Checksum of entire RSDP (ACPI 2.0+)
    uint8_t reserved[3];            // Reserved, must be 0
} __attribute__((packed)) acpi_rsdp;

// FADT (Fixed ACPI Description Table) - contains PM timer and control registers
typedef struct {
    acpi_table_header header;
    // ... minimal set of fields we need for power management
    uint32_t firmware_ctrl;         // Physical address of FIRMWARE_CTRL
    uint32_t dsdt;                  // Physical address of DSDT
    // Reserved field (1 byte)
    uint8_t reserved;
    uint8_t preferred_pm_profile;   // Preferred power management profile
    uint16_t sci_int;               // System interrupt vector
    uint32_t smi_cmd;               // Port address of SMI command
    uint8_t acpi_enable;            // Value to write to smi_cmd to enable ACPI
    uint8_t acpi_disable;           // Value to write to smi_cmd to disable ACPI
    uint8_t s4bios_req;             // Value to write to smi_cmd to enter S4BIOS state
    uint8_t pstate_cnt;             // Value to write to smi_cmd for perf state control
    uint32_t pm1a_evt_blk;          // Port address of PM1a_EVT_BLK
    uint32_t pm1b_evt_blk;          // Port address of PM1b_EVT_BLK
    uint32_t pm1a_ctrl_blk;         // Port address of PM1a_CTRL_BLK
    uint32_t pm1b_ctrl_blk;         // Port address of PM1b_CTRL_BLK
    uint8_t pm1_cnt_len;            // Byte length of PM1x_CNT_BLK and PM1x_EVT_BLK registers
    uint8_t pm2_cnt_len;            // Byte length of PM2_CNT_BLK register
    uint8_t pm_tmr_len;             // Byte length of PM_TMR register
    uint8_t gp0_blk_len;            // Byte length of GP0_BLK registers
    uint8_t gp1_blk_len;            // Byte length of GP1_BLK registers
    uint8_t gp1_base;               // Offset in GP0_BLK where GP1_BLK starts
    uint8_t cst_cnt;                // Number of C-state mappings
    uint16_t cst_latency[1];        // Actually variable length, but we'll just use first for now
} __attribute__((packed)) acpi_fadt;

// Function declarations
bool orion_acpi_init(const orion_boot_info *boot_info);
bool orion_acpi_check_invariants(void);
bool orion_acpi_enter_sleep_state(uint8_t sleep_state); // 1-4 for S1-S4, 5 for S5 (soft off)

#endif
