#include "idt.h"

#include <stddef.h>
#include <stdint.h>

#include "cpu_state.h"
#include "timer.h"
#include "../../fault/report.h"
#include "../../log/log.h"
#include "../../panic/panic.h"
#include "../../task/scheduler.h"
#include "../../task/thread.h"
#include "../../syscall/syscall.h"

void orion_x86_64_exception_dispatch(orion_x86_64_exception_frame *frame)
{
    if (frame == 0 || frame->vector >= ORION_X86_64_IDT_ENTRY_COUNT) {
        orion_panic_at("EXCEPTION", "invalid exception frame");
    }
    if (frame->vector == ORION_X86_64_TIMER_VECTOR) {
        orion_x86_64_timer_interrupt();
        return;
    }
    if (frame->vector == ORION_X86_64_SYSCALL_VECTOR) {
        orion_syscall_dispatch(frame);
        return;
    }
    if (frame->vector >= ORION_X86_64_EXCEPTION_COUNT) {
        orion_panic_at("EXCEPTION", "unexpected external interrupt");
    }

    const uint64_t exception_vector = frame->vector;
    const bool from_user = (frame->cs & UINT64_C(3)) != 0;
    orion_thread *thread = from_user ? NULL : orion_scheduler_current();
    orion_process *process =
        thread != NULL && orion_thread_is_managed(thread)
            ? orion_process_kernel()
            : NULL;
    uintptr_t fault_address = 0;
    if (exception_vector == 14U) {
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_address));
    }
    const bool user_process_terminated =
        from_user && orion_syscall_terminate_user_exception(frame);
    orion_fault_record fault = {
        .instruction_pointer = frame->rip,
        .process_id = from_user
                          ? orion_syscall_current_process_id()
                          : process != NULL && thread->process == process
                                ? process->id
                                : 0,
        .thread_id = process != NULL && thread->process == process
                         ? thread->id
                         : 0,
        .error_code = frame->error_code,
        .address = fault_address,
        .vector = (uint32_t)exception_vector,
        .category = ORION_FAULT_CPU_EXCEPTION,
        .context = from_user ? ORION_FAULT_CONTEXT_USER
                             : ORION_FAULT_CONTEXT_KERNEL_INTERRUPT,
        .action = user_process_terminated
                      ? ORION_FAULT_ACTION_TERMINATE_PROCESS
                      : exception_vector == 3U
                            ? ORION_FAULT_ACTION_CONTINUE
                            : ORION_FAULT_ACTION_HALT_KERNEL,
        .recovery_result = user_process_terminated ||
                                   exception_vector == 3U
                               ? ORION_FAULT_RECOVERY_SUCCEEDED
                               : ORION_FAULT_RECOVERY_NOT_ATTEMPTED
    };
    uint64_t event_id = 0;
    if (orion_fault_report_record(&fault, &event_id) ==
        ORION_FAULT_STATUS_OK) {
        (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION",
                            "Structured fault event ID", event_id);
        (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION",
                            "Fault instruction pointer", frame->rip);
    } else {
        (void)orion_log(ORION_LOG_ERROR, "EXCEPTION",
                        "Structured fault report unavailable");
    }
    (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION", "CPU exception vector",
                        exception_vector);
    (void)orion_log_u64(ORION_LOG_ERROR, "EXCEPTION", "CPU error code",
                        frame->error_code);
    if (exception_vector == 14U) {
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
    if (user_process_terminated) {
        (void)orion_log(ORION_LOG_ERROR, "EXCEPTION",
                        "User application terminated after CPU exception");
        return;
    }
    if (exception_vector == 3U) {
        (void)orion_log(ORION_LOG_INFO, "EXCEPTION",
                        "Breakpoint handled; resuming interrupted code");
        return;
    }

    orion_panic_at("EXCEPTION", "fatal CPU exception");
}
