#include "clocksource.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"
#include "../arch/x86_64/timer.h"

#define NANOSECONDS_PER_SECOND UINT64_C(1000000000)

static orion_clocksource_reader source_reader;
static void *source_context;
static uint64_t source_tick_numerator;
static uint64_t source_tick_denominator;
static uint64_t last_nanoseconds;
static uint64_t read_count;
static uint64_t monotonic_clamp_count;
static bool source_initialized;

static bool pit_tick_reader(void *context, uint64_t *ticks)
{
    (void)context;
    return orion_x86_64_timer_read_ticks(ticks);
}

bool orion_clocksource_ticks_to_nanoseconds(uint64_t ticks,
                                             uint64_t numerator,
                                             uint64_t denominator,
                                             uint64_t *nanoseconds)
{
    if (nanoseconds == NULL || numerator == 0 || denominator == 0 ||
        numerator > UINT64_MAX / NANOSECONDS_PER_SECOND) {
        return false;
    }

    const uint64_t scale = numerator * NANOSECONDS_PER_SECOND;
    const uint64_t whole_units = ticks / denominator;
    const uint64_t remainder = ticks % denominator;
    if (whole_units > UINT64_MAX / scale ||
        (remainder != 0 && scale > UINT64_MAX / remainder)) {
        return false;
    }

    const uint64_t whole_nanoseconds = whole_units * scale;
    const uint64_t fractional_nanoseconds =
        (remainder * scale) / denominator;
    if (whole_nanoseconds > UINT64_MAX - fractional_nanoseconds) {
        return false;
    }

    *nanoseconds = whole_nanoseconds + fractional_nanoseconds;
    return true;
}

bool orion_clocksource_init(orion_clocksource_reader reader, void *context,
                            uint64_t tick_numerator,
                            uint64_t tick_denominator)
{
    if (source_initialized || reader == NULL || tick_numerator == 0 ||
        tick_denominator == 0) {
        return false;
    }

    uint64_t ticks = 0;
    uint64_t initial_nanoseconds = 0;
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    const bool source_valid =
        reader(context, &ticks) &&
        orion_clocksource_ticks_to_nanoseconds(
            ticks, tick_numerator, tick_denominator, &initial_nanoseconds);
    orion_x86_64_interrupts_restore(was_enabled);
    if (!source_valid) {
        return false;
    }

    source_reader = reader;
    source_context = context;
    source_tick_numerator = tick_numerator;
    source_tick_denominator = tick_denominator;
    last_nanoseconds = initial_nanoseconds;
    read_count = 0;
    monotonic_clamp_count = 0;
    source_initialized = true;
    return true;
}

bool orion_clocksource_init_pit(void)
{
    return orion_clocksource_init(
        pit_tick_reader, NULL, ORION_X86_64_PIT_DIVISOR,
        ORION_X86_64_PIT_INPUT_HZ);
}

bool orion_clocksource_now_nanoseconds(uint64_t *nanoseconds)
{
    if (!source_initialized || nanoseconds == NULL) {
        return false;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    uint64_t ticks = 0;
    uint64_t converted = 0;
    bool result = source_reader(source_context, &ticks) &&
                  orion_clocksource_ticks_to_nanoseconds(
                      ticks, source_tick_numerator, source_tick_denominator,
                      &converted);
    if (result && read_count != UINT64_MAX) {
        ++read_count;
        if (converted < last_nanoseconds) {
            converted = last_nanoseconds;
            if (monotonic_clamp_count == UINT64_MAX) {
                result = false;
            } else {
                ++monotonic_clamp_count;
            }
        } else {
            last_nanoseconds = converted;
        }
    } else {
        result = false;
    }

    if (result) {
        *nanoseconds = converted;
    }
    orion_x86_64_interrupts_restore(was_enabled);
    return result;
}

bool orion_clocksource_get_stats(orion_clocksource_stats *stats)
{
    if (stats == NULL || !source_initialized) {
        return false;
    }

    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    *stats = (orion_clocksource_stats){
        .reads = read_count,
        .monotonic_clamps = monotonic_clamp_count,
        .last_nanoseconds = last_nanoseconds
    };
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

bool orion_clocksource_is_available(void)
{
    return source_initialized;
}
