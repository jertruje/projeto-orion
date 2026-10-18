#include "timer.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"
#include "clocksource.h"

typedef enum {
    TIMER_SLOT_UNUSED,
    TIMER_SLOT_ARMED,
    TIMER_SLOT_FIRING
} timer_slot_state;

typedef struct {
    uint64_t deadline_nanoseconds;
    uint64_t period_nanoseconds;
    uint32_t generation;
    timer_slot_state state;
    orion_timer_callback callback;
    void *context;
    orion_thread *waiting_thread;
} timer_slot;

static timer_slot slots[ORION_TIMER_CAPACITY];
static orion_timer_stats timer_stats;
static bool timer_initialized;
static bool polling;

static bool handle_is_valid(orion_timer_handle handle)
{
    return handle.slot < ORION_TIMER_CAPACITY && handle.generation != 0 &&
           slots[handle.slot].generation == handle.generation;
}

static void clear_slot(timer_slot *slot)
{
    slot->deadline_nanoseconds = 0;
    slot->period_nanoseconds = 0;
    slot->callback = NULL;
    slot->context = NULL;
    slot->waiting_thread = NULL;
    slot->state = TIMER_SLOT_UNUSED;
    --timer_stats.active;
}

bool orion_timer_system_init(void)
{
    if (timer_initialized || !orion_clocksource_is_available()) {
        return false;
    }
    timer_stats = (orion_timer_stats){0};
    for (uint32_t index = 0; index < ORION_TIMER_CAPACITY; ++index) {
        slots[index] = (timer_slot){0};
    }
    polling = false;
    timer_initialized = true;
    return true;
}

orion_timer_status orion_timer_arm_at(uint64_t deadline_nanoseconds,
                                      uint64_t period_nanoseconds,
                                      orion_timer_callback callback,
                                      void *context,
                                      orion_timer_handle *handle)
{
    if (!timer_initialized || callback == NULL || handle == NULL) {
        return timer_initialized ? ORION_TIMER_INVALID
                                 : ORION_TIMER_UNAVAILABLE;
    }
    uint64_t now = 0;
    if (!orion_clocksource_now_nanoseconds(&now)) {
        return ORION_TIMER_UNAVAILABLE;
    }
    (void)now;

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    uint32_t index = 0;
    while (index < ORION_TIMER_CAPACITY &&
           (slots[index].state != TIMER_SLOT_UNUSED ||
            slots[index].generation == UINT32_MAX)) {
        ++index;
    }
    if (index == ORION_TIMER_CAPACITY) {
        orion_x86_64_interrupts_restore(was_enabled);
        return ORION_TIMER_FULL;
    }
    if (timer_stats.active == UINT32_MAX) {
        orion_x86_64_interrupts_restore(was_enabled);
        return ORION_TIMER_INVALID;
    }

    timer_slot *slot = &slots[index];
    ++slot->generation;
    slot->deadline_nanoseconds = deadline_nanoseconds;
    slot->period_nanoseconds = period_nanoseconds;
    slot->callback = callback;
    slot->context = context;
    slot->waiting_thread = NULL;
    slot->state = TIMER_SLOT_ARMED;
    if (timer_stats.armed != UINT64_MAX) {
        ++timer_stats.armed;
    }
    ++timer_stats.active;
    if (timer_stats.active > timer_stats.peak_active) {
        timer_stats.peak_active = timer_stats.active;
    }
    *handle = (orion_timer_handle){
        .slot = (uint16_t)index,
        .generation = slot->generation
    };
    orion_x86_64_interrupts_restore(was_enabled);
    return ORION_TIMER_OK;
}

orion_timer_status orion_timer_arm_after(uint64_t delay_nanoseconds,
                                         uint64_t period_nanoseconds,
                                         orion_timer_callback callback,
                                         void *context,
                                         orion_timer_handle *handle)
{
    uint64_t now = 0;
    if (!orion_clocksource_now_nanoseconds(&now)) {
        return ORION_TIMER_UNAVAILABLE;
    }
    if (delay_nanoseconds > UINT64_MAX - now) {
        return ORION_TIMER_INVALID;
    }
    return orion_timer_arm_at(now + delay_nanoseconds, period_nanoseconds,
                              callback, context, handle);
}

orion_timer_status orion_timer_cancel(orion_timer_handle handle)
{
    if (!timer_initialized || handle.slot >= ORION_TIMER_CAPACITY ||
        handle.generation == 0) {
        return timer_initialized ? ORION_TIMER_INVALID
                                 : ORION_TIMER_UNAVAILABLE;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    if (!handle_is_valid(handle)) {
        orion_x86_64_interrupts_restore(was_enabled);
        return ORION_TIMER_INVALID;
    }
    timer_slot *slot = &slots[handle.slot];
    if (slot->state == TIMER_SLOT_FIRING) {
        orion_x86_64_interrupts_restore(was_enabled);
        return ORION_TIMER_BUSY;
    }
    if (slot->state != TIMER_SLOT_ARMED) {
        orion_x86_64_interrupts_restore(was_enabled);
        return ORION_TIMER_INVALID;
    }
    clear_slot(slot);
    if (timer_stats.cancelled != UINT64_MAX) {
        ++timer_stats.cancelled;
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return ORION_TIMER_OK;
}

static int32_t find_next_expired(uint64_t now)
{
    int32_t selected = -1;
    for (uint32_t index = 0; index < ORION_TIMER_CAPACITY; ++index) {
        if (timer_stats.poll_scans != UINT64_MAX) {
            ++timer_stats.poll_scans;
        }
        const timer_slot *slot = &slots[index];
        if (slot->state != TIMER_SLOT_ARMED ||
            slot->deadline_nanoseconds > now) {
            continue;
        }
        if (selected < 0 ||
            slot->deadline_nanoseconds <
                slots[(uint32_t)selected].deadline_nanoseconds) {
            selected = (int32_t)index;
        }
    }
    return selected;
}

bool orion_timer_run_expired(uint32_t callback_limit, uint32_t *callbacks_run)
{
    if (!timer_initialized || callback_limit == 0 ||
        callback_limit > ORION_TIMER_CAPACITY || callbacks_run == NULL ||
        !orion_x86_64_interrupts_enabled()) {
        return false;
    }
    *callbacks_run = 0;

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    if (polling) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }
    polling = true;
    orion_x86_64_interrupts_restore(was_enabled);

    bool success = true;
    while (*callbacks_run < callback_limit) {
        uint64_t now = 0;
        if (!orion_clocksource_now_nanoseconds(&now)) {
            success = false;
            break;
        }

        const bool interrupts_were_enabled =
            orion_x86_64_interrupts_save_disable();
        const int32_t selected = find_next_expired(now);
        if (selected < 0) {
            orion_x86_64_interrupts_restore(interrupts_were_enabled);
            break;
        }
        timer_slot *slot = &slots[(uint32_t)selected];
        const orion_timer_handle handle = {
            .slot = (uint16_t)selected,
            .generation = slot->generation
        };
        const uint64_t period = slot->period_nanoseconds;
        const uint64_t lateness = now - slot->deadline_nanoseconds;
        orion_timer_callback callback = slot->callback;
        void *context = slot->context;
        slot->state = TIMER_SLOT_FIRING;
        orion_x86_64_interrupts_restore(interrupts_were_enabled);

        const bool callback_succeeded = slot->waiting_thread == NULL
            ? callback(handle, context)
            : orion_thread_transition(slot->waiting_thread, ORION_THREAD_READY);

        const bool restore_interrupts =
            orion_x86_64_interrupts_save_disable();
        slot = &slots[handle.slot];
        if (!handle_is_valid(handle) || slot->state != TIMER_SLOT_FIRING) {
            success = false;
            orion_x86_64_interrupts_restore(restore_interrupts);
            break;
        }
        if (timer_stats.expirations == UINT64_MAX) {
            clear_slot(slot);
            success = false;
            orion_x86_64_interrupts_restore(restore_interrupts);
            break;
        }
        ++timer_stats.expirations;
        if (lateness > timer_stats.max_lateness_nanoseconds) {
            timer_stats.max_lateness_nanoseconds = lateness;
        }
        ++*callbacks_run;

        if (!callback_succeeded) {
            if (timer_stats.callback_failures == UINT64_MAX) {
                success = false;
            } else {
                ++timer_stats.callback_failures;
            }
            clear_slot(slot);
            success = false;
        } else if (slot->waiting_thread == NULL && period != 0) {
            uint64_t callback_time = 0;
            if (!orion_clocksource_now_nanoseconds(&callback_time) ||
                period > UINT64_MAX - callback_time) {
                clear_slot(slot);
                success = false;
            } else {
                slot->deadline_nanoseconds = callback_time + period;
                slot->state = TIMER_SLOT_ARMED;
            }
        } else {
            clear_slot(slot);
        }
        orion_x86_64_interrupts_restore(restore_interrupts);
        if (!success) {
            break;
        }
    }

    const bool restore_poll_interrupts =
        orion_x86_64_interrupts_save_disable();
    polling = false;
    orion_x86_64_interrupts_restore(restore_poll_interrupts);
    return success;
}

bool orion_timer_get_stats(orion_timer_stats *stats)
{
    if (!timer_initialized || stats == NULL) {
        return false;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    *stats = timer_stats;
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

bool orion_timer_check_invariants(void)
{
    if (!timer_initialized) {
        return false;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    uint32_t active = 0;
    bool valid = true;
    for (uint32_t index = 0; index < ORION_TIMER_CAPACITY; ++index) {
        const timer_slot *slot = &slots[index];
        if (slot->state == TIMER_SLOT_ARMED ||
            slot->state == TIMER_SLOT_FIRING) {
            ++active;
            valid = valid && slot->generation != 0 &&
                    (slot->callback != NULL || slot->waiting_thread != NULL);
        } else {
            valid = valid && slot->callback == NULL &&
                    slot->context == NULL &&
                    slot->waiting_thread == NULL;
        }
    }
    valid = valid && active == timer_stats.active &&
            active <= ORION_TIMER_CAPACITY;
    orion_x86_64_interrupts_restore(was_enabled);
    return valid;
}
