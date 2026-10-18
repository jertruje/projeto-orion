#ifndef ORION_X86_64_TIMER_H
#define ORION_X86_64_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_X86_64_PIT_INPUT_HZ UINT32_C(1193182)
#define ORION_X86_64_PIT_DIVISOR UINT16_C(11932)

typedef struct {
    uint64_t ticks;
    uint64_t handler_samples;
    uint64_t handler_tsc_total;
    uint64_t handler_tsc_max;
    uint16_t divisor;
} orion_x86_64_timer_stats;

bool orion_x86_64_timer_init(void);
bool orion_x86_64_timer_stop(void);
bool orion_x86_64_timer_get_stats(orion_x86_64_timer_stats *stats);
void orion_x86_64_timer_interrupt(void);

#endif
