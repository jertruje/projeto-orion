#include "thread.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"
#include "scheduler.h"

#define STACK_GUARD_BYTE UINT8_C(0xa5)
#define INITIAL_CONTEXT_WORDS 8U

static orion_thread thread_slots[ORION_THREAD_MAX_COUNT];
static uint8_t thread_stacks[ORION_THREAD_MAX_COUNT][ORION_THREAD_STACK_SIZE]
    __attribute__((aligned(4096)));
static bool thread_slot_used[ORION_THREAD_MAX_COUNT];
static bool thread_slot_retired[ORION_THREAD_MAX_COUNT];
static uint64_t next_thread_id = 1;

extern void orion_x86_64_context_switch(uint64_t **old_stack_pointer,
                                        uint64_t *new_stack_pointer);
extern void orion_x86_64_thread_start_trampoline(void);
extern void orion_x86_64_thread_returned(void);

static bool stack_bounds_are_valid(uintptr_t bottom, uintptr_t top)
{
    return bottom < top && top - bottom >= ORION_THREAD_STACK_SIZE / 2U;
}

static void prepare_initial_context(orion_thread *thread)
{
    uintptr_t top = thread->stack_top & ~(uintptr_t)0xfU;
    uint64_t *frame = (uint64_t *)(top -
                                  INITIAL_CONTEXT_WORDS * sizeof(uint64_t));
    for (uint32_t index = 0; index < INITIAL_CONTEXT_WORDS; ++index) {
        frame[index] = 0;
    }
    frame[3] = (uintptr_t)thread;
    frame[6] = (uintptr_t)orion_x86_64_thread_start_trampoline;
    frame[7] = (uintptr_t)orion_x86_64_thread_returned;
    thread->saved_stack_pointer = frame;
}

static void initialize_stack_guard(orion_thread *thread)
{
    uint8_t *guard = (uint8_t *)thread->stack_bottom;
    for (uint32_t index = 0; index < ORION_THREAD_STACK_GUARD_SIZE; ++index) {
        guard[index] = STACK_GUARD_BYTE;
    }
}

static bool stack_guard_is_intact(const orion_thread *thread)
{
    if (!thread->owns_stack) {
        return true;
    }
    const uint8_t *guard = (const uint8_t *)thread->stack_bottom;
    for (uint32_t index = 0; index < ORION_THREAD_STACK_GUARD_SIZE; ++index) {
        if (guard[index] != STACK_GUARD_BYTE) {
            return false;
        }
    }
    return true;
}

bool orion_thread_system_init(orion_process *process,
                              uintptr_t bootstrap_stack_bottom,
                              uintptr_t bootstrap_stack_top,
                              orion_thread **bootstrap_thread)
{
    if (process == NULL || bootstrap_thread == NULL ||
        !stack_bounds_are_valid(bootstrap_stack_bottom,
                                bootstrap_stack_top) ||
        thread_slot_used[0]) {
        return false;
    }
    thread_slot_used[0] = true;
    orion_thread *thread = &thread_slots[0];
    *thread = (orion_thread){
        .id = next_thread_id++,
        .state = ORION_THREAD_NEW,
        .saved_stack_pointer = NULL,
        .stack_bottom = bootstrap_stack_bottom,
        .stack_top = bootstrap_stack_top,
        .entry = NULL,
        .argument = NULL,
        .process = NULL,
        .process_next = NULL,
        .ready_next = NULL,
        .dispatch_count = 0,
        .yield_count = 0,
        .runtime_ticks = 0,
        .exit_code = 0,
        .bootstrap_thread = true,
        .owns_stack = false
    };
    if (!orion_process_attach_thread(process, thread) ||
        !orion_thread_transition(thread, ORION_THREAD_RUNNING)) {
        (void)orion_process_detach_thread(process, thread);
        thread_slot_used[0] = false;
        *thread = (orion_thread){0};
        return false;
    }
    *bootstrap_thread = thread;
    return true;
}

bool orion_thread_create_kernel(orion_process *process,
                                orion_kernel_thread_entry entry,
                                void *argument, orion_thread **thread_out)
{
    if (process == NULL || entry == NULL || thread_out == NULL ||
        next_thread_id == UINT64_MAX) {
        return false;
    }

    uint32_t slot = 1;
    while (slot < ORION_THREAD_MAX_COUNT &&
           (thread_slot_used[slot] || thread_slot_retired[slot])) {
        ++slot;
    }
    if (slot == ORION_THREAD_MAX_COUNT) {
        return false;
    }

    thread_slot_used[slot] = true;
    orion_thread *thread = &thread_slots[slot];
    uintptr_t bottom = (uintptr_t)thread_stacks[slot];
    uintptr_t top = bottom + sizeof(thread_stacks[slot]);
    *thread = (orion_thread){
        .id = next_thread_id++,
        .state = ORION_THREAD_NEW,
        .saved_stack_pointer = NULL,
        .stack_bottom = bottom,
        .stack_top = top,
        .entry = entry,
        .argument = argument,
        .process = NULL,
        .process_next = NULL,
        .ready_next = NULL,
        .dispatch_count = 0,
        .yield_count = 0,
        .runtime_ticks = 0,
        .exit_code = 0,
        .bootstrap_thread = false,
        .owns_stack = true
    };
    initialize_stack_guard(thread);
    prepare_initial_context(thread);
    if (!orion_process_attach_thread(process, thread)) {
        thread_slot_used[slot] = false;
        *thread = (orion_thread){0};
        return false;
    }
    *thread_out = thread;
    return true;
}

bool orion_thread_destroy(orion_thread *thread)
{
    if (!orion_thread_is_managed(thread) || thread->bootstrap_thread ||
        thread == orion_scheduler_current() ||
        thread->state == ORION_THREAD_RUNNING ||
        (thread->state != ORION_THREAD_NEW &&
         thread->state != ORION_THREAD_TERMINATED) ||
        !stack_guard_is_intact(thread)) {
        return false;
    }

    for (uint32_t slot = 1; slot < ORION_THREAD_MAX_COUNT; ++slot) {
        if (&thread_slots[slot] == thread) {
            if (!orion_process_detach_thread(thread->process, thread)) {
                return false;
            }
            thread_slot_used[slot] = false;
            thread_slot_retired[slot] = true;
            *thread = (orion_thread){0};
            return true;
        }
    }
    return false;
}

bool orion_thread_transition(orion_thread *thread,
                             orion_thread_state next_state)
{
    if (!orion_thread_is_managed(thread)) {
        return false;
    }
    bool valid = false;
    switch (thread->state) {
    case ORION_THREAD_NEW:
        valid = next_state == ORION_THREAD_READY ||
                next_state == ORION_THREAD_RUNNING;
        break;
    case ORION_THREAD_READY:
        valid = next_state == ORION_THREAD_RUNNING;
        break;
    case ORION_THREAD_RUNNING:
        valid = next_state == ORION_THREAD_READY ||
                next_state == ORION_THREAD_BLOCKED ||
                next_state == ORION_THREAD_TERMINATED;
        break;
    case ORION_THREAD_BLOCKED:
        valid = next_state == ORION_THREAD_READY ||
                next_state == ORION_THREAD_RUNNING ||
                next_state == ORION_THREAD_TERMINATED;
        break;
    case ORION_THREAD_TERMINATED:
        valid = false;
        break;
    default:
        return false;
    }
    if (!valid) {
        return false;
    }
    thread->state = next_state;
    return true;
}

bool orion_thread_is_managed(const orion_thread *thread)
{
    if (thread == NULL) {
        return false;
    }
    for (uint32_t slot = 0; slot < ORION_THREAD_MAX_COUNT; ++slot) {
        if (thread_slot_used[slot] && &thread_slots[slot] == thread) {
            return true;
        }
    }
    return false;
}

bool orion_thread_stack_is_valid(const orion_thread *thread)
{
    return orion_thread_is_managed(thread) &&
           stack_bounds_are_valid(thread->stack_bottom, thread->stack_top) &&
           thread->saved_stack_pointer != NULL &&
           (uintptr_t)thread->saved_stack_pointer >= thread->stack_bottom &&
           (uintptr_t)thread->saved_stack_pointer < thread->stack_top &&
           stack_guard_is_intact(thread);
}

bool orion_thread_check_all_stacks(void)
{
    for (uint32_t slot = 0; slot < ORION_THREAD_MAX_COUNT; ++slot) {
        if (thread_slot_used[slot] &&
            (!stack_bounds_are_valid(thread_slots[slot].stack_bottom,
                                     thread_slots[slot].stack_top) ||
             (thread_slots[slot].saved_stack_pointer != NULL &&
              ((uintptr_t)thread_slots[slot].saved_stack_pointer <
                   thread_slots[slot].stack_bottom ||
               (uintptr_t)thread_slots[slot].saved_stack_pointer >=
                   thread_slots[slot].stack_top)) ||
             !stack_guard_is_intact(&thread_slots[slot]))) {
            return false;
        }
    }
    return true;
}

void orion_thread_record_dispatch(orion_thread *thread)
{
    if (thread != NULL && thread->dispatch_count != UINT64_MAX) {
        ++thread->dispatch_count;
    }
}

bool orion_thread_check_invariants(void)
{
    for (uint32_t left = 0; left < ORION_THREAD_MAX_COUNT; ++left) {
        if (!thread_slot_used[left]) {
            continue;
        }
        const orion_thread *thread = &thread_slots[left];
        if (thread->id == 0 ||
            thread->process == NULL || thread->ready_next == thread ||
            (thread->state == ORION_THREAD_RUNNING &&
             thread->process->state != ORION_PROCESS_ACTIVE)) {
            return false;
        }
        for (uint32_t right = left + 1; right < ORION_THREAD_MAX_COUNT;
             ++right) {
            if (thread_slot_used[right] &&
                thread_slots[right].id == thread->id) {
                return false;
            }
        }
    }
    return orion_thread_check_all_stacks();
}

_Noreturn void orion_thread_start(orion_thread *thread)
{
    if (!orion_thread_is_managed(thread) ||
        thread != orion_scheduler_current() ||
        thread->state != ORION_THREAD_RUNNING || thread->entry == NULL ||
        !orion_thread_stack_is_valid(thread)) {
        orion_scheduler_fatal("invalid initial kernel-thread context");
    }
    if (orion_scheduler_preemption_enabled()) {
        orion_x86_64_interrupts_enable();
    }
    thread->entry(thread->argument);
    orion_x86_64_interrupts_disable();
    orion_scheduler_exit_current(0);
}
