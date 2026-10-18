#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../kernel/arch/x86_64/interrupt_state.h"
#include "../../../kernel/time/clocksource.h"
#include "../../../kernel/time/timer.h"

static uint64_t fake_ticks;
static bool reader_available = true;
static bool fake_interrupts_enabled;

typedef struct {
    uint32_t calls;
    bool result;
    bool cancel_self;
    orion_timer_status self_cancel_status;
} callback_state;

bool orion_x86_64_interrupts_enabled(void)
{
    return fake_interrupts_enabled;
}

bool orion_x86_64_interrupts_save_disable(void)
{
    const bool was_enabled = fake_interrupts_enabled;
    fake_interrupts_enabled = false;
    return was_enabled;
}

void orion_x86_64_interrupts_restore(bool was_enabled)
{
    fake_interrupts_enabled = was_enabled;
}

void orion_x86_64_interrupts_disable(void)
{
    fake_interrupts_enabled = false;
}

void orion_x86_64_interrupts_enable(void)
{
    fake_interrupts_enabled = true;
}

bool orion_x86_64_timer_read_ticks(uint64_t *ticks)
{
    (void)ticks;
    return false;
}

static bool read_fake_ticks(void *context, uint64_t *ticks)
{
    (void)context;
    if (!reader_available || ticks == NULL) {
        return false;
    }
    *ticks = fake_ticks;
    return true;
}

static bool test_callback(orion_timer_handle handle, void *context)
{
    callback_state *state = context;
    if (state == NULL) {
        return false;
    }
    ++state->calls;
    if (state->cancel_self) {
        state->self_cancel_status = orion_timer_cancel(handle);
    }
    return state->result;
}

static bool require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "time test failed: %s\n", message);
        return false;
    }
    return true;
}

int main(void)
{
    reader_available = false;
    uint64_t converted = UINT64_C(0xfeed);
    if (!require(!orion_clocksource_now_nanoseconds(&converted),
                 "clock is unavailable before initialization") ||
        !require(!orion_timer_system_init(),
                 "timer service rejects unavailable clock") ||
        !require(!orion_clocksource_ticks_to_nanoseconds(
                     1, 1, 0, &converted),
                 "reject zero conversion denominator") ||
        !require(!orion_clocksource_ticks_to_nanoseconds(
                     1, UINT64_MAX, 1, &converted),
                 "reject overflowing conversion scale") ||
        !require(!orion_clocksource_ticks_to_nanoseconds(
                     UINT64_MAX, 11932, 1193182, &converted),
                 "reject overflowing converted uptime") ||
        !require(converted == UINT64_C(0xfeed),
                 "failed conversion does not overwrite output")) {
        return 1;
    }

    const uint64_t expected_pit_ns =
        (UINT64_C(11932) * UINT64_C(1000000000)) / UINT64_C(1193182);
    if (!require(orion_clocksource_ticks_to_nanoseconds(
                     1, 11932, 1193182, &converted) &&
                     converted == expected_pit_ns,
                 "PIT tick converts with its rational frequency") ||
        !require(!orion_clocksource_init(read_fake_ticks, NULL, 1, 1000000000),
                 "failed reader keeps clock unavailable")) {
        return 1;
    }

    reader_available = true;
    fake_ticks = 10;
    if (!require(orion_clocksource_init(read_fake_ticks, NULL, 1,
                                        1000000000),
                 "initialize injected nanosecond source")) {
        return 1;
    }
    uint64_t first = 0;
    uint64_t second = 0;
    fake_ticks = 20;
    if (!require(orion_clocksource_now_nanoseconds(&first) &&
                     first == 20,
                 "monotonic clock reads source time")) {
        return 1;
    }
    fake_ticks = 5;
    if (!require(orion_clocksource_now_nanoseconds(&second) &&
                     second == first,
                 "monotonic clock clamps backward source movement")) {
        return 1;
    }
    orion_clocksource_stats clock_stats;
    if (!require(orion_clocksource_get_stats(&clock_stats) &&
                     clock_stats.monotonic_clamps == 1 &&
                     clock_stats.reads == 2,
                 "clock diagnostics count reads and clamps")) {
        return 1;
    }
    reader_available = false;
    converted = UINT64_C(0xfeed);
    if (!require(!orion_clocksource_now_nanoseconds(&converted) &&
                     converted == UINT64_C(0xfeed),
                 "unavailable initialized source fails without a fake value")) {
        return 1;
    }
    reader_available = true;

    if (!require(orion_timer_system_init(), "initialize timer service")) {
        return 1;
    }
    fake_interrupts_enabled = true;
    callback_state state = {
        .calls = 0,
        .result = true,
        .cancel_self = false,
        .self_cancel_status = ORION_TIMER_INVALID
    };
    orion_timer_handle cancelled;
    if (!require(orion_timer_arm_after(100, 0, test_callback, &state,
                                       &cancelled) == ORION_TIMER_OK,
                 "arm relative one-shot timer") ||
        !require(orion_timer_cancel(cancelled) == ORION_TIMER_OK,
                 "cancel armed timer") ||
        !require(orion_timer_cancel(cancelled) == ORION_TIMER_INVALID,
                 "reject stale handle after cancellation") ||
        !require(state.calls == 0, "cancelled timer does not execute")) {
        return 1;
    }
    if (!require(orion_timer_arm_after(UINT64_MAX, 0, test_callback, &state,
                                       &cancelled) == ORION_TIMER_INVALID,
                 "reject relative deadline overflow")) {
        return 1;
    }

    fake_ticks = 30;
    state.cancel_self = true;
    orion_timer_handle one_shot;
    if (!require(orion_timer_arm_at(30, 0, test_callback, &state,
                                    &one_shot) == ORION_TIMER_OK,
                 "arm already-due one-shot") ||
        !require(orion_timer_cancel((orion_timer_handle){UINT16_MAX, 1}) ==
                     ORION_TIMER_INVALID,
                 "reject out-of-range handle")) {
        return 1;
    }
    uint32_t callbacks_run = 0;
    if (!require(orion_timer_run_expired(ORION_TIMER_CAPACITY,
                                         &callbacks_run) &&
                     callbacks_run == 1 && state.calls == 1 &&
                     state.self_cancel_status == ORION_TIMER_BUSY,
                 "one-shot executes once and firing cancellation is busy") ||
        !require(orion_timer_cancel(one_shot) == ORION_TIMER_INVALID,
                 "expired handle is stale") ||
        !require(orion_timer_check_invariants(),
                 "invariants hold after one-shot expiration")) {
        return 1;
    }

    state.cancel_self = false;
    orion_timer_handle periodic;
    if (!require(orion_timer_arm_after(10, 5, test_callback, &state,
                                       &periodic) == ORION_TIMER_OK,
                 "arm periodic timer")) {
        return 1;
    }
    fake_ticks = 39;
    if (!require(orion_timer_run_expired(ORION_TIMER_CAPACITY,
                                         &callbacks_run) &&
                     callbacks_run == 0,
                 "periodic timer does not fire before deadline")) {
        return 1;
    }
    fake_ticks = 40;
    if (!require(orion_timer_run_expired(ORION_TIMER_CAPACITY,
                                         &callbacks_run) &&
                     callbacks_run == 1 && state.calls == 2,
                 "periodic timer fires at deadline")) {
        return 1;
    }
    fake_ticks = 45;
    if (!require(orion_timer_run_expired(ORION_TIMER_CAPACITY,
                                         &callbacks_run) &&
                     callbacks_run == 1 && state.calls == 3 &&
                     orion_timer_cancel(periodic) == ORION_TIMER_OK,
                 "periodic timer rearms and cancels cleanly")) {
        return 1;
    }

    state.result = false;
    orion_timer_handle failed;
    if (!require(orion_timer_arm_after(0, 0, test_callback, &state,
                                       &failed) == ORION_TIMER_OK,
                 "arm callback-failure timer") ||
        !require(!orion_timer_run_expired(ORION_TIMER_CAPACITY,
                                          &callbacks_run) &&
                     callbacks_run == 1 && state.calls == 4,
                 "callback failure is surfaced")) {
        return 1;
    }
    state.result = true;

    orion_timer_handle handles[ORION_TIMER_CAPACITY];
    for (uint32_t index = 0; index < ORION_TIMER_CAPACITY; ++index) {
        if (!require(orion_timer_arm_at(UINT64_MAX, 0, test_callback, &state,
                                        &handles[index]) == ORION_TIMER_OK,
                     "fill bounded timer capacity")) {
            return 1;
        }
    }
    if (!require(orion_timer_arm_at(UINT64_MAX, 0, test_callback, &state,
                                    &failed) == ORION_TIMER_FULL,
                 "timer capacity exhaustion is explicit")) {
        return 1;
    }
    for (uint32_t index = 0; index < ORION_TIMER_CAPACITY; ++index) {
        if (!require(orion_timer_cancel(handles[index]) == ORION_TIMER_OK,
                     "release timer slot")) {
            return 1;
        }
    }

    state.cancel_self = false;
    state.result = true;
    orion_timer_handle overflowing_periodic;
    if (!require(orion_timer_arm_at(UINT64_MAX - 1, 2, test_callback, &state,
                                    &overflowing_periodic) == ORION_TIMER_OK,
                 "arm periodic timer near representable limit")) {
        return 1;
    }
    fake_ticks = UINT64_MAX - 1;
    if (!require(!orion_timer_run_expired(ORION_TIMER_CAPACITY,
                                          &callbacks_run) &&
                     callbacks_run == 1 &&
                     orion_timer_cancel(overflowing_periodic) ==
                         ORION_TIMER_INVALID,
                 "periodic overflow is explicit and disarms the timer")) {
        return 1;
    }

    orion_timer_stats timer_stats;
    if (!require(orion_timer_get_stats(&timer_stats) &&
                     timer_stats.active == 0 &&
                     timer_stats.expirations == 5 &&
                     timer_stats.callback_failures == 1 &&
                     timer_stats.max_lateness_nanoseconds == 0 &&
                     orion_timer_check_invariants(),
                 "timer metrics and final invariants")) {
        return 1;
    }

    fake_interrupts_enabled = false;
    if (!require(!orion_timer_run_expired(1, &callbacks_run),
                 "callbacks are not dispatched with interrupts disabled") ||
        !require(!orion_timer_run_expired(ORION_TIMER_CAPACITY + 1,
                                         &callbacks_run),
                 "callback batch is bounded by timer capacity")) {
        return 1;
    }

    (void)puts("Time and timer unit tests: PASS");
    return 0;
}
