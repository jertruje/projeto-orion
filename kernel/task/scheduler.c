#include "scheduler.h"

#include <stddef.h>

#include "../arch/x86_64/context_switch.h"
#include "../arch/x86_64/interrupt_state.h"
#include "../log/log.h"
#include "../panic/panic.h"

static orion_thread *current_thread;
static orion_thread *bootstrap_thread;
static orion_thread *ready_head;
static orion_thread *ready_tail;
static uint32_t ready_count;
static uint32_t managed_count;
static uint64_t context_switches;
static uint64_t yield_calls;
static uint64_t timer_interrupts;
static uint64_t timer_preemptions;
static bool scheduler_initialized;
static bool bootstrap_run_active;
static bool preemption_enabled;

static bool interrupts_are_disabled(void)
{
    return !orion_x86_64_interrupts_enabled();
}

static bool ready_queue_contains(const orion_thread *thread)
{
    uint32_t visited = 0;
    for (const orion_thread *item = ready_head; item != NULL;
         item = item->ready_next) {
        if (item == thread) {
            return true;
        }
        if (++visited > ORION_THREAD_MAX_COUNT) {
            return false;
        }
    }
    return false;
}

static bool enqueue_ready(orion_thread *thread)
{
    if (!orion_thread_is_managed(thread) ||
        thread->state != ORION_THREAD_READY || thread->ready_next != NULL ||
        ready_queue_contains(thread) ||
        ready_count >= ORION_THREAD_MAX_COUNT) {
        return false;
    }
    if (ready_tail == NULL) {
        ready_head = thread;
    } else {
        ready_tail->ready_next = thread;
    }
    ready_tail = thread;
    ++ready_count;
    return true;
}

static orion_thread *dequeue_ready(void)
{
    orion_thread *thread = ready_head;
    if (thread == NULL) {
        return NULL;
    }
    ready_head = thread->ready_next;
    thread->ready_next = NULL;
    --ready_count;
    if (ready_head == NULL) {
        ready_tail = NULL;
    }
    return thread;
}

static bool dispatch_to(orion_thread *next, orion_thread *previous)
{
    if (next == NULL || !orion_thread_is_managed(next) ||
        next->state != ORION_THREAD_READY ||
        context_switches == UINT64_MAX) {
        return false;
    }
    if (!orion_thread_transition(next, ORION_THREAD_RUNNING)) {
        return false;
    }
    current_thread = next;
    orion_thread_record_dispatch(next);
    ++context_switches;
    orion_x86_64_context_switch(&previous->saved_stack_pointer,
                                next->saved_stack_pointer);
    if (current_thread != previous ||
        previous->state != ORION_THREAD_RUNNING ||
        !orion_scheduler_check_invariants()) {
        return false;
    }
    return true;
}

bool orion_scheduler_init(uintptr_t bootstrap_stack_bottom,
                          uintptr_t bootstrap_stack_top)
{
    if (scheduler_initialized || !interrupts_are_disabled() ||
        !orion_process_system_init()) {
        return false;
    }

    orion_process *process = orion_process_kernel();
    if (process == NULL ||
        !orion_thread_system_init(process, bootstrap_stack_bottom,
                                  bootstrap_stack_top, &bootstrap_thread)) {
        return false;
    }
    current_thread = bootstrap_thread;
    ready_head = NULL;
    ready_tail = NULL;
    ready_count = 0;
    managed_count = 1;
    context_switches = 0;
    yield_calls = 0;
    timer_interrupts = 0;
    timer_preemptions = 0;
    bootstrap_run_active = false;
    preemption_enabled = false;
    scheduler_initialized = true;
    return orion_scheduler_check_invariants();
}

static bool create_kernel_thread_disabled(orion_kernel_thread_entry entry,
                                          void *argument,
                                          orion_thread **thread_out)
{
    if (!scheduler_initialized || !interrupts_are_disabled() ||
        current_thread == NULL ||
        current_thread->state != ORION_THREAD_RUNNING ||
        thread_out == NULL || managed_count >= ORION_THREAD_MAX_COUNT) {
        return false;
    }
    orion_thread *thread = NULL;
    if (!orion_thread_create_kernel(current_thread->process, entry, argument,
                                    &thread)) {
        return false;
    }
    if (!orion_thread_transition(thread, ORION_THREAD_READY)) {
        (void)orion_thread_destroy(thread);
        return false;
    }
    if (!enqueue_ready(thread)) {
        orion_scheduler_fatal("could not add a new thread to the ready queue");
    }
    ++managed_count;
    *thread_out = thread;
    if (!orion_scheduler_check_invariants()) {
        orion_scheduler_fatal("scheduler invariant failed after thread creation");
    }
    return true;
}

bool orion_scheduler_create_kernel_thread(orion_kernel_thread_entry entry,
                                          void *argument,
                                          orion_thread **thread_out)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool result =
        create_kernel_thread_disabled(entry, argument, thread_out);
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

static bool destroy_thread_disabled(orion_thread *thread)
{
    if (!scheduler_initialized || !interrupts_are_disabled() ||
        !orion_thread_is_managed(thread) || thread == current_thread ||
        thread->state == ORION_THREAD_READY ||
        thread->state == ORION_THREAD_RUNNING ||
        ready_queue_contains(thread) || managed_count <= 1U) {
        return false;
    }
    if (!orion_thread_destroy(thread)) {
        return false;
    }
    --managed_count;
    return orion_scheduler_check_invariants();
}

bool orion_scheduler_destroy_thread(orion_thread *thread)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool result = destroy_thread_disabled(thread);
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

static bool run_ready_disabled(void)
{
    if (!scheduler_initialized || !interrupts_are_disabled() ||
        current_thread != bootstrap_thread || bootstrap_run_active ||
        current_thread->state != ORION_THREAD_RUNNING) {
        return false;
    }
    if (ready_head == NULL) {
        return true;
    }
    if (!orion_thread_transition(bootstrap_thread, ORION_THREAD_BLOCKED)) {
        return false;
    }
    bootstrap_run_active = true;
    orion_thread *next = dequeue_ready();
    if (next == NULL || !dispatch_to(next, bootstrap_thread)) {
        return false;
    }
    bootstrap_run_active = false;
    return current_thread == bootstrap_thread &&
           bootstrap_thread->state == ORION_THREAD_RUNNING &&
           orion_scheduler_check_invariants();
}

bool orion_scheduler_run_ready(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool result = run_ready_disabled();
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

static bool yield_disabled(void)
{
    if (!scheduler_initialized || !interrupts_are_disabled() ||
        current_thread == NULL ||
        current_thread->state != ORION_THREAD_RUNNING ||
        yield_calls == UINT64_MAX ||
        current_thread->yield_count == UINT64_MAX) {
        return false;
    }
    ++yield_calls;
    if (ready_head == NULL) {
        ++current_thread->yield_count;
        return true;
    }

    orion_thread *previous = current_thread;
    if (!orion_thread_transition(previous, ORION_THREAD_READY)) {
        return false;
    }
    ++previous->yield_count;
    if (!enqueue_ready(previous)) {
        (void)orion_thread_transition(previous, ORION_THREAD_RUNNING);
        return false;
    }
    orion_thread *next = dequeue_ready();
    if (next == NULL || next == previous) {
        return false;
    }
    return dispatch_to(next, previous);
}

bool orion_scheduler_yield(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool result = yield_disabled();
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

bool orion_scheduler_enable_preemption(void)
{
    if (!scheduler_initialized || !interrupts_are_disabled() ||
        preemption_enabled) {
        return false;
    }
    preemption_enabled = true;
    return true;
}

bool orion_scheduler_disable_preemption(void)
{
    if (!scheduler_initialized || !interrupts_are_disabled() ||
        !preemption_enabled) {
        return false;
    }
    preemption_enabled = false;
    return true;
}

bool orion_scheduler_preemption_enabled(void)
{
    return preemption_enabled;
}

bool orion_scheduler_timer_interrupt(bool *preempted)
{
    if (preempted != NULL) {
        *preempted = false;
    }
    if (!scheduler_initialized || !preemption_enabled ||
        !interrupts_are_disabled() || current_thread == NULL ||
        preempted == NULL ||
        current_thread->state != ORION_THREAD_RUNNING ||
        timer_interrupts == UINT64_MAX ||
        current_thread->runtime_ticks == UINT64_MAX) {
        return false;
    }
    ++timer_interrupts;
    ++current_thread->runtime_ticks;
    if (ready_head == NULL) {
        return orion_scheduler_check_invariants();
    }
    if (timer_preemptions == UINT64_MAX) {
        return false;
    }

    orion_thread *previous = current_thread;
    if (!orion_thread_transition(previous, ORION_THREAD_READY) ||
        !enqueue_ready(previous)) {
        return false;
    }
    orion_thread *next = dequeue_ready();
    if (next == NULL || next == previous) {
        return false;
    }
    ++timer_preemptions;
    *preempted = true;
    return dispatch_to(next, previous);
}

_Noreturn void orion_scheduler_exit_current(uint64_t exit_code)
{
    orion_x86_64_interrupts_disable();
    if (!scheduler_initialized || !interrupts_are_disabled() ||
        current_thread == NULL || current_thread == bootstrap_thread ||
        current_thread->state != ORION_THREAD_RUNNING) {
        orion_scheduler_fatal("invalid kernel-thread termination");
    }
    orion_thread *previous = current_thread;
    previous->exit_code = exit_code;
    if (!orion_thread_transition(previous, ORION_THREAD_TERMINATED)) {
        orion_scheduler_fatal("invalid thread state at termination");
    }

    orion_thread *next = dequeue_ready();
    if (next != NULL) {
        if (!orion_thread_transition(next, ORION_THREAD_RUNNING)) {
            orion_scheduler_fatal("invalid ready thread at termination");
        }
    } else if (bootstrap_run_active &&
               bootstrap_thread->state == ORION_THREAD_BLOCKED &&
               orion_thread_transition(bootstrap_thread,
                                       ORION_THREAD_RUNNING)) {
        next = bootstrap_thread;
    }
    if (next == NULL || context_switches == UINT64_MAX) {
        orion_scheduler_fatal("no safe thread available after termination");
    }

    current_thread = next;
    orion_thread_record_dispatch(next);
    ++context_switches;
    if (!orion_scheduler_check_invariants()) {
        orion_scheduler_fatal("scheduler invariant failed at thread exit");
    }
    orion_x86_64_context_switch(&previous->saved_stack_pointer,
                                next->saved_stack_pointer);
    orion_scheduler_fatal("terminated thread was scheduled again");
}

orion_thread *orion_scheduler_current(void)
{
    return current_thread;
}

bool orion_scheduler_get_stats(orion_scheduler_stats *stats)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool result = stats != NULL && scheduler_initialized &&
                        orion_scheduler_check_invariants();
    if (result) {
        *stats = (orion_scheduler_stats){
            .context_switches = context_switches,
            .yield_calls = yield_calls,
            .timer_interrupts = timer_interrupts,
            .timer_preemptions = timer_preemptions,
            .ready_threads = ready_count,
            .managed_threads = managed_count
        };
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

static bool scheduler_check_invariants_disabled(void)
{
    if (!scheduler_initialized && current_thread == NULL) {
        return ready_head == NULL && ready_tail == NULL && ready_count == 0;
    }
    if (!orion_thread_is_managed(current_thread) ||
        current_thread->state != ORION_THREAD_RUNNING ||
        (ready_count == 0 && ready_head != NULL) ||
        (ready_count == 0 && ready_tail != NULL) ||
        (ready_count != 0 && (ready_head == NULL || ready_tail == NULL)) ||
        !orion_process_check_invariants(current_thread->process) ||
        !orion_thread_check_invariants()) {
        return false;
    }

    uint32_t count = 0;
    orion_thread *last = NULL;
    for (orion_thread *thread = ready_head; thread != NULL;
         thread = thread->ready_next) {
        if (thread == current_thread || thread->state != ORION_THREAD_READY ||
            !orion_thread_is_managed(thread) ||
            ++count > ORION_THREAD_MAX_COUNT) {
            return false;
        }
        for (orion_thread *other = thread->ready_next; other != NULL;
             other = other->ready_next) {
            if (other == thread) {
                return false;
            }
        }
        last = thread;
    }
    return count == ready_count && last == ready_tail &&
           current_thread->process->thread_count == managed_count &&
           (bootstrap_thread->state == ORION_THREAD_RUNNING ||
            (bootstrap_run_active &&
             bootstrap_thread->state == ORION_THREAD_BLOCKED));
}

bool orion_scheduler_check_invariants(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool result = scheduler_check_invariants_disabled();
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

_Noreturn void orion_scheduler_fatal(const char *reason)
{
    orion_panic_at("TASK", reason);
}
