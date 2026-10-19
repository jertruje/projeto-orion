#include "acpi.h"

#include <stddef.h>
#include <string.h>

#include "../arch/x86_64/interrupt_state.h"
#include "../mm/physical.h"
#include "../time/clocksource.h"

// Static variables to hold ACPI table pointers
static uintptr_t g_rsdp_address = 0;
static uintptr_t g_xsdt_address = 0;
static bool g_acpi_enabled = false;

// Forward declarations
static bool validate_acpi_checksum(const void *table, size_t length);
static uintptr_t find_rsdp_in_memory(void);
static bool parse_rsdp(const acpi_rsdp *rsdp);
static bool parse_xsdt(void);


// Implementation

bool orion_acpi_init(const orion_boot_info *boot_info)
{
    // We don't actually need the boot_info for our ACPI scanning approach,
    // but we keep the parameter for compatibility with the expected interface
    (void)boot_info;
    
    // Disable interrupts during ACPI initialization to prevent race conditions
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    
    // Reset state
    g_rsdp_address = 0;
    g_xsdt_address = 0;
    g_acpi_enabled = false;
    
    // Step 1: Find the RSDP by scanning memory
    g_rsdp_address = find_rsdp_in_memory();
    if (g_rsdp_address == 0) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }
    
    // Step 2: Parse the RSDP to find XSDT/RSDT
    // We need to map the RSDP address to virtual memory to access it
    // For simplicity, we'll use direct physical mapping assuming identity mapping
    // In a real kernel with virtual memory, we'd need to map it properly
    void *rsdp_virt = (void *)(uintptr_t)g_rsdp_address; // Assuming direct mapping
    if (!parse_rsdp((const acpi_rsdp *)rsdp_virt)) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }
    
    // Step 3: Parse XSDT to find ACPI tables (we'll implement this minimally)
    if (!parse_xsdt()) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }
    
    g_acpi_enabled = true;
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

bool orion_acpi_check_invariants(void)
{
    // Simple invariant: if we claims to have ACPI enabled, we should have valid addresses
    if (g_acpi_enabled) {
        return g_rsdp_address != 0 && g_xsdt_address != 0;
    }
    return true; // If not enabled, that's OK
}

bool orion_acpi_enter_sleep_state(uint8_t sleep_state)
{
    // This would require parsing the FADT to get the PM1 control register values
    // and then writing the appropriate sleep type/sl flags to enter the requested state
    // For now, we'll just return false to indicate not implemented
    (void)sleep_state;
    return false;
}

// Helper functions

static bool validate_acpi_checksum(const void *table, size_t length)
{
    if (table == NULL || length == 0) {
        return false;
    }
    
    uint8_t sum = 0;
    const uint8_t *bytes = (const uint8_t *)table;
    for (size_t i = 0; i < length; i++) {
        sum += bytes[i];
    }
    return sum == 0;
}

static uintptr_t find_rsdp_in_memory(void)
{
    // Search for RSDP in EBDA (Extended BIOS Data Area) first (0x0E0000-0x0FFFFF)
    // Then search in BIOS ROM area (0x0E0000-0x0FFFFF) 
    // Then search in upper memory area (0x100000-0xFFFFFFFF)
    
    // For simplicity in this implementation, we'll search a reasonable range
    // In a real implementation, we'd check EBDA first per ACPI spec
    
    // Search from 0x000E0000 to 0x000FFFFF (EBDA and BIOS area)
    // and from 0x00100000 to 0x00FFFFFF (lower extended memory)
    // and from 0xFFFFFFFF - 0x003FFFFF to 0xFFFFFFFF (top of 4GB space)
    
    const uintptr_t start_addr1 = 0x000E0000;
    const uintptr_t end_addr1 = 0x000FFFFF;
    const uintptr_t start_addr2 = 0x00100000;
    const uintptr_t end_addr2 = 0x00FFFFFF;
    const uintptr_t start_addr3 = 0xFFFFFFFF - 0x003FFFFF;
    const uintptr_t end_addr3 = 0xFFFFFFFF;
    
    // Search each range
    uintptr_t addr = start_addr1;
    while (addr <= end_addr1) {
        // Check if this address could be an RSDP (must be 16-byte aligned)
        if ((addr & 0xF) == 0) {
            // Map this physical address to virtual to check signature
            // Again, assuming direct mapping for simplicity
            const void *ptr = (void *)(uintptr_t)addr;
            if (((const acpi_rsdp *)ptr)->signature[0] == 'R' &&
                ((const acpi_rsdp *)ptr)->signature[1] == 'S' &&
                ((const acpi_rsdp *)ptr)->signature[2] == 'D' &&
                ((const acpi_rsdp *)ptr)->signature[3] == ' ' &&
                ((const acpi_rsdp *)ptr)->signature[4] == 'P' &&
                ((const acpi_rsdp *)ptr)->signature[5] == 'T' &&
                ((const acpi_rsdp *)ptr)->signature[6] == 'R' &&
                ((const acpi_rsdp *)ptr)->signature[7] == ' ') {
                // Found potential RSDP, validate checksum
                if (validate_acpi_checksum(ptr, sizeof(acpi_rsdp))) {
                    return addr;
                }
            }
        }
        addr++;
    }
    
    addr = start_addr2;
    while (addr <= end_addr2) {
        if ((addr & 0xF) == 0) {
            const void *ptr = (void *)(uintptr_t)addr;
            if (((const acpi_rsdp *)ptr)->signature[0] == 'R' &&
                ((const acpi_rsdp *)ptr)->signature[1] == 'S' &&
                ((const acpi_rsdp *)ptr)->signature[2] == 'D' &&
                ((const acpi_rsdp *)ptr)->signature[3] == ' ' &&
                ((const acpi_rsdp *)ptr)->signature[4] == 'P' &&
                ((const acpi_rsdp *)ptr)->signature[5] == 'T' &&
                ((const acpi_rsdp *)ptr)->signature[6] == 'R' &&
                ((const acpi_rsdp *)ptr)->signature[7] == ' ') {
                if (validate_acpi_checksum(ptr, sizeof(acpi_rsdp))) {
                    return addr;
                }
            }
        }
        addr++;
    }
    
    addr = start_addr3;
    while (addr <= end_addr3) {
        if ((addr & 0xF) == 0) {
            const void *ptr = (void *)(uintptr_t)addr;
            if (((const acpi_rsdp *)ptr)->signature[0] == 'R' &&
                ((const acpi_rsdp *)ptr)->signature[1] == 'S' &&
                ((const acpi_rsdp *)ptr)->signature[2] == 'D' &&
                ((const acpi_rsdp *)ptr)->signature[3] == ' ' &&
                ((const acpi_rsdp *)ptr)->signature[4] == 'P' &&
                ((const acpi_rsdp *)ptr)->signature[5] == 'T' &&
                ((const acpi_rsdp *)ptr)->signature[6] == 'R' &&
                ((const acpi_rsdp *)ptr)->signature[7] == ' ') {
                if (validate_acpi_checksum(ptr, sizeof(acpi_rsdp))) {
                    return addr;
                }
            }
        }
        addr++;
    }
    
    return 0; // Not found
}

static bool parse_rsdp(const acpi_rsdp *rsdp)
{
    if (rsdp == NULL) {
        return false;
    }
    
    // Validate RSDP checksum
    if (!validate_acpi_checksum(rsdp, sizeof(acpi_rsdp))) {
        return false;
    }
    
    // Check revision - we support both 1.0 and 2.0+
    if (rsdp->revision != 0 && rsdp->revision < 2) {
        return false;
    }
    
    // For ACPI 2.0+, validate the extended checksum and length
    if (rsdp->revision >= 2) {
        // The full RSDP structure length is in the length field
        if (!validate_acpi_checksum(rsdp, rsdp->length)) {
            return false;
        }
        // Use the 64-bit XSDT address
        g_xsdt_address = rsdp->xsdt_physical_address;
    } else {
        // ACPI 1.0 - use the 32-bit RSDT address
        g_xsdt_address = rsdp->rsdt_physical_address;
    }
    
    return true;
}

static bool parse_xsdt(void)
{
    // In a full implementation, we would parse the XSDT to find individual ACPI tables
    // like FADT, DSDT, etc. For now, we'll just validate that we can read the XSDT header
    if (g_xsdt_address == 0) {
        return false;
    }
    
    // Map XSDT to virtual memory (assuming direct mapping)
    const void *xsdt_virt = (void *)(uintptr_t)g_xsdt_address;
    
    // Validate basic XSDT structure - it should start with "XSDT" signature
    const uint32_t *signature = (const uint32_t *)xsdt_virt;
    if (*signature != 0x54445358) { // "XSDT" in little-endian
        return false;
    }
    
    // Validate checksum
    // We would need to get the length from the XSDT header to validate properly
    // For simplicity, we'll just do a basic check
    const acpi_table_header *header = (const acpi_table_header *)xsdt_virt;
    if (!validate_acpi_checksum(header, sizeof(acpi_table_header))) {
        return false;
    }
    
    return true;
}


