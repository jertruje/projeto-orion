#include <orion/boot_info.h>
#include <orion/init_state.h>

#include "../arch/x86_64/cpu_state.h"
#include "../arch/x86_64/gdt.h"
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/interrupt_state.h"
#include "../mm/memory_map.h"
#include "../mm/physical.h"
#include "log.h"
#include "panic.h"
#include "serial.h"

static orion_init_stage current_stage = ORION_INIT_ENTRY;
extern char kernel_stack_bottom[];
extern char kernel_stack_top[];

static void advance_stage(orion_init_stage expected, orion_init_stage next)
{
    if (!orion_init_advance(&current_stage, expected, next)) {
        orion_panic_at("INIT", "invalid initialization stage transition");
    }
    if (!orion_log(ORION_LOG_INFO, "INIT", orion_init_stage_name(next))) {
        orion_panic_at("INIT", "failed to log initialization stage");
    }
}

void kernel_main(const orion_boot_info *boot_info)
{
    orion_serial_init();
    if (!orion_log(ORION_LOG_INFO, "BOOT", "Kernel entry reached")) {
        orion_panic_at("BOOT", "serial logging failed at kernel entry");
    }
    advance_stage(ORION_INIT_ENTRY, ORION_INIT_DIAGNOSTICS_CONFIGURED);

    if (!orion_boot_info_is_valid(boot_info)) {
        orion_panic_at("BOOT", "invalid UEFI boot information");
    }
    advance_stage(ORION_INIT_DIAGNOSTICS_CONFIGURED,
                  ORION_INIT_BOOT_INFO_VALID);
    if (!orion_log(ORION_LOG_INFO, "CORE", "Boot information validated")) {
        orion_panic_at("CORE", "serial logging failed after boot-info validation");
    }

    if (!orion_x86_64_validate_entry_state((uintptr_t)kernel_stack_bottom,
                                           (uintptr_t)kernel_stack_top)) {
        orion_panic_at("CPU", "unsupported CPU or kernel entry state");
    }
    const bool nx_enabled = orion_x86_64_nx_enabled();
    advance_stage(ORION_INIT_BOOT_INFO_VALID, ORION_INIT_CPU_STATE_VALID);
    if (!orion_log(ORION_LOG_INFO, "CPU",
                   "Long mode, paging, PAE, stack and disabled interrupts verified")) {
        orion_panic_at("CPU", "serial logging failed after CPU state validation");
    }
    if (!orion_log_u64(ORION_LOG_INFO, "CPU",
                       "NX feature and EFER.NXE enabled:", nx_enabled)) {
        orion_panic_at("CPU", "failed to log NX capability");
    }

    if (!orion_x86_64_gdt_init()) {
        orion_panic_at("GDT", "failed to configure kernel GDT and TSS");
    }
    if (!orion_x86_64_idt_init()) {
        orion_panic_at("IDT", "failed to configure kernel exception IDT");
    }
    if (orion_x86_64_interrupts_enabled()) {
        orion_panic_at("CPU", "maskable interrupts unexpectedly enabled");
    }
    advance_stage(ORION_INIT_CPU_STATE_VALID, ORION_INIT_ARCH_TABLES_READY);
    if (!orion_log(ORION_LOG_INFO, "CPU",
                   "Kernel GDT, TSS and CPU exception IDT installed; IRQs disabled")) {
        orion_panic_at("CPU", "serial logging failed after descriptor setup");
    }

    orion_memory_map_summary memory_summary;
    if (!orion_memory_map_validate(boot_info, &memory_summary)) {
        orion_panic_at("MEMORY", "invalid or unsupported UEFI memory map");
    }
    if (!orion_physical_init(boot_info)) {
        orion_panic_at("MEMORY", "physical page allocator initialization failed");
    }
    orion_physical_stats physical_stats;
    if (!orion_physical_get_stats(&physical_stats) ||
        !orion_physical_check_invariants()) {
        orion_panic_at("MEMORY", "physical page allocator invariant failure");
    }
    uint64_t probe_page = 0;
    if (!orion_physical_allocate(1, &probe_page) ||
        (probe_page & (ORION_MEMORY_PAGE_SIZE - 1U)) != 0 ||
        !orion_physical_release(probe_page) ||
        !orion_physical_check_invariants()) {
        orion_panic_at("MEMORY", "physical allocation smoke test failed");
    }
    if (!orion_log(ORION_LOG_INFO, "MEMORY",
                   "Physical allocate/release smoke test passed")) {
        orion_panic_at("MEMORY", "failed to log physical allocator test");
    }
    if (!orion_log_u64(ORION_LOG_INFO, "MEMORY",
                       "Validated UEFI memory descriptors:",
                       memory_summary.descriptor_count) ||
        !orion_log_u64(ORION_LOG_INFO, "MEMORY",
                       "Free physical pages below 4 GiB:",
                       physical_stats.free_pages)) {
        orion_panic_at("MEMORY", "failed to log memory initialization");
    }

    advance_stage(ORION_INIT_ARCH_TABLES_READY, ORION_INIT_CORE_READY);
    if (!orion_log_u64(ORION_LOG_INFO, "CORE", "Boot memory map bytes:",
                       boot_info->memory_map_size)) {
        orion_panic_at("CORE", "serial logging failed while reporting memory map");
    }
    if (!orion_log(ORION_LOG_INFO, "CORE",
                   "UEFI boot services have exited; interrupts remain disabled")) {
        orion_panic_at("CORE", "serial logging failed after core initialization");
    }

    advance_stage(ORION_INIT_CORE_READY, ORION_INIT_IDLE);
    if (!orion_log(ORION_LOG_INFO, "CORE",
                   "Minimal kernel is running; entering idle loop")) {
        orion_panic_at("CORE", "serial logging failed before idle loop");
    }

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
