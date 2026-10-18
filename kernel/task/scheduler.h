#ifndef ORION_KERNEL_TASK_SCHEDULER_H
#define ORION_KERNEL_TASK_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "thread.h"

typedef struct {
    uint64_t context_switches;
    uint64_t yield_calls;
    uint64_t timer_interrupts;
    uint64_t timer_preemptions;
    uint32_t ready_threads;
    uint32_t managed_threads;
} orion_scheduler_stats;

bool orion_scheduler_init(uintptr_t bootstrap_stack_bottom,
                          uintptr_t bootstrap_stack_top);
bool orion_scheduler_create_kernel_thread(orion_kernel_thread_entry entry,
                                          void *argument,
                                          orion_thread **thread);
bool orion_scheduler_destroy_thread(orion_thread *thread);
bool orion_scheduler_run_ready(void);
bool orion_scheduler_yield(void);
bool orion_scheduler_enable_preemption(void);
bool orion_scheduler_disable_preemption(void);
bool orion_scheduler_preemption_enabled(void);
bool orion_scheduler_timer_interrupt(bool *preempted);
_Noreturn void orion_scheduler_exit_current(uint64_t exit_code);
orion_thread *orion_scheduler_current(void);
bool orion_scheduler_get_stats(orion_scheduler_stats *stats);
bool orion_scheduler_check_invariants(void);
_Noreturn void orion_scheduler_fatal(const char *reason);

#endif
