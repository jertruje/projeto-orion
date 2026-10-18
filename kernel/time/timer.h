#ifndef ORION_KERNEL_TIME_TIMER_H
#define ORION_KERNEL_TIME_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_TIMER_CAPACITY 32U

typedef struct {
    uint16_t slot;
    uint32_t generation;
} orion_timer_handle;

typedef bool (*orion_timer_callback)(orion_timer_handle handle,
                                     void *context);

typedef enum {
    ORION_TIMER_OK,
    ORION_TIMER_INVALID,
    ORION_TIMER_UNAVAILABLE,
    ORION_TIMER_FULL,
    ORION_TIMER_BUSY
} orion_timer_status;

typedef struct {
    uint64_t armed;
    uint64_t cancelled;
    uint64_t expirations;
    uint64_t callback_failures;
    uint64_t poll_scans;
    uint64_t max_lateness_nanoseconds;
    uint32_t active;
    uint32_t peak_active;
} orion_timer_stats;

bool orion_timer_system_init(void);
orion_timer_status orion_timer_arm_at(uint64_t deadline_nanoseconds,
                                      uint64_t period_nanoseconds,
                                      orion_timer_callback callback,
                                      void *context,
                                      orion_timer_handle *handle);
orion_timer_status orion_timer_arm_after(uint64_t delay_nanoseconds,
                                         uint64_t period_nanoseconds,
                                         orion_timer_callback callback,
                                         void *context,
                                         orion_timer_handle *handle);
orion_timer_status orion_timer_cancel(orion_timer_handle handle);
bool orion_timer_run_expired(uint32_t callback_limit, uint32_t *callbacks_run);
bool orion_timer_get_stats(orion_timer_stats *stats);
bool orion_timer_check_invariants(void);

/**
 * Blocks the current thread for the specified duration.
 * @param duration_nanoseconds The time to sleep, in nanoseconds.
 * @return ORION_TIMER_OK if successful, or an error code.
 */
orion_timer_status orion_timer_sleep(uint64_t duration_nanoseconds);

#endif
