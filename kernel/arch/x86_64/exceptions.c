#include "idt.h"

#include <stdint.h>

#include "cpu_state.h"
#include "timer.h"
#include "../../log/log.h"
#include "../../panic/panic.h"

void orion_x86_64_exception_dispatch(orion_x86_64_exception_frame *frame)
{
    if (frame == 0 || frame->vector >= ORION_X86_64_IDT_ENTRY_COUNT) {
        orion_panic_at("EXCEPTION", "invalid exception frame");
    }
    if (frame->vector == ORION_X86_64_TIMER_VECTOR) {
        orion_x86_64_timer_interrupt();
        return;
    }
    if (frame->vector >= ORION_X86_64_EXCEPTION_COUNT) {
        orion_panic_at("EXCEPTION", "unexpected external interrupt");
    }

    (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION", "CPU exception vector",
                        frame->vector);
    (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION", "CPU error code",
                        frame->error_code);
    if (frame->vector == 14U) {
        uintptr_t fault_address;
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_address));
        (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION", "Page fault CR2",
                            fault_address);
        (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION",
                            "Page fault: protection violation",
                            frame->error_code & 1U);
        (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION",
                            "Page fault: write access",
                            (frame->error_code >> 1U) & 1U);
        (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION",
                            "Page fault: user access",
                            (frame->error_code >> 2U) & 1U);
        (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION",
                            "Page fault: reserved-bit violation",
                            (frame->error_code >> 3U) & 1U);
        if (orion_x86_64_nx_enabled()) {
            (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION",
                                "Page fault: instruction fetch",
                                (frame->error_code >> 4U) & 1U);
        }
    }
    if (frame->vector == 3U) {
        (void)orion_log(ORION_LOG_INFO, "EXCEPTION",
                        "Breakpoint handled; resuming interrupted code");
        return;
    }

    orion_panic_at("EXCEPTION", "fatal CPU exception");
}
