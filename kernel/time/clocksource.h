#ifndef ORION_KERNEL_TIME_CLOCKSOURCE_H
#define ORION_KERNEL_TIME_CLOCKSOURCE_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*orion_clocksource_reader)(void *context, uint64_t *ticks);

typedef struct {
    uint64_t reads;
    uint64_t monotonic_clamps;
    uint64_t last_nanoseconds;
} orion_clocksource_stats;

bool orion_clocksource_init(orion_clocksource_reader reader, void *context,
                            uint64_t tick_numerator,
                            uint64_t tick_denominator);
bool orion_clocksource_init_pit(void);
bool orion_clocksource_ticks_to_nanoseconds(uint64_t ticks,
                                             uint64_t numerator,
                                             uint64_t denominator,
                                             uint64_t *nanoseconds);
bool orion_clocksource_now_nanoseconds(uint64_t *nanoseconds);
bool orion_clocksource_get_stats(orion_clocksource_stats *stats);
bool orion_clocksource_is_available(void);

#endif
