#include "timer.h"

#include <stddef.h>

#include "idt.h"
#include "interrupt_state.h"
#include "../../panic/panic.h"
#include "../../task/scheduler.h"

#define PIC_MASTER_COMMAND UINT16_C(0x20)
#define PIC_MASTER_DATA UINT16_C(0x21)
#define PIC_SLAVE_COMMAND UINT16_C(0xa0)
#define PIC_SLAVE_DATA UINT16_C(0xa1)
#define PIT_CHANNEL_0 UINT16_C(0x40)
#define PIT_COMMAND UINT16_C(0x43)

static bool timer_initialized;
static bool timer_active;
static uint64_t timer_ticks;
static uint64_t handler_samples;
static uint64_t handler_tsc_total;
static uint64_t handler_tsc_max;

static uint64_t read_tsc(void)
{
    uint32_t low;
    uint32_t high;
    __asm__ volatile("lfence; rdtsc; lfence"
                     : "=a"(low), "=d"(high)
                     :
                     : "memory");
    return ((uint64_t)high << 32) | low;
}

static void write_port(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1"
                     :
                     : "a"(value), "Nd"(port)
                     : "memory");
}

static void pic_remap_and_mask(void)
{
    write_port(PIC_MASTER_DATA, UINT8_C(0xff));
    write_port(PIC_SLAVE_DATA, UINT8_C(0xff));
    write_port(PIC_MASTER_COMMAND, UINT8_C(0x11));
    write_port(PIC_SLAVE_COMMAND, UINT8_C(0x11));
    write_port(PIC_MASTER_DATA, UINT8_C(0x20));
    write_port(PIC_SLAVE_DATA, UINT8_C(0x28));
    write_port(PIC_MASTER_DATA, UINT8_C(0x04));
    write_port(PIC_SLAVE_DATA, UINT8_C(0x02));
    write_port(PIC_MASTER_DATA, UINT8_C(0x01));
    write_port(PIC_SLAVE_DATA, UINT8_C(0x01));
    write_port(PIC_MASTER_DATA, UINT8_C(0xff));
    write_port(PIC_SLAVE_DATA, UINT8_C(0xff));
}

static void program_pit(void)
{
    write_port(PIT_COMMAND, UINT8_C(0x34));
    write_port(PIT_CHANNEL_0, (uint8_t)ORION_X86_64_PIT_DIVISOR);
    write_port(PIT_CHANNEL_0,
               (uint8_t)(ORION_X86_64_PIT_DIVISOR >> 8));
}

bool orion_x86_64_timer_init(void)
{
    if (timer_initialized || orion_x86_64_interrupts_enabled() ||
        !orion_scheduler_check_invariants()) {
        return false;
    }

    pic_remap_and_mask();
    program_pit();
    timer_ticks = 0;
    handler_samples = 0;
    handler_tsc_total = 0;
    handler_tsc_max = 0;
    timer_active = true;
    if (!orion_scheduler_enable_preemption()) {
        timer_active = false;
        write_port(PIC_MASTER_DATA, UINT8_C(0xff));
        return false;
    }
    write_port(PIC_MASTER_DATA, UINT8_C(0xfe));
    timer_initialized = true;
    return true;
}

bool orion_x86_64_timer_stop(void)
{
    if (!timer_initialized || orion_x86_64_interrupts_enabled()) {
        return false;
    }
    write_port(PIC_MASTER_DATA, UINT8_C(0xff));
    write_port(PIC_SLAVE_DATA, UINT8_C(0xff));
    timer_active = false;
    return orion_scheduler_disable_preemption();
}

bool orion_x86_64_timer_start_timekeeping(void)
{
    if (!timer_initialized || timer_active ||
        orion_x86_64_interrupts_enabled() ||
        orion_scheduler_preemption_enabled()) {
        return false;
    }
    timer_active = true;
    write_port(PIC_MASTER_DATA, UINT8_C(0xfe));
    return true;
}

bool orion_x86_64_timer_read_ticks(uint64_t *ticks)
{
    if (!timer_initialized || !timer_active || ticks == NULL ||
        orion_x86_64_interrupts_enabled()) {
        return false;
    }
    *ticks = timer_ticks;
    return true;
}

bool orion_x86_64_timer_get_stats(orion_x86_64_timer_stats *stats)
{
    if (!timer_initialized || stats == NULL ||
        orion_x86_64_interrupts_enabled()) {
        return false;
    }
    *stats = (orion_x86_64_timer_stats){
        .ticks = timer_ticks,
        .handler_samples = handler_samples,
        .handler_tsc_total = handler_tsc_total,
        .handler_tsc_max = handler_tsc_max,
        .divisor = ORION_X86_64_PIT_DIVISOR
    };
    return true;
}

void orion_x86_64_timer_interrupt(void)
{
    if (!timer_active || timer_ticks == UINT64_MAX) {
        orion_panic_at("TIMER", "invalid or overflowing PIT interrupt");
    }
    ++timer_ticks;
    write_port(PIC_MASTER_COMMAND, UINT8_C(0x20));
    if (!orion_scheduler_preemption_enabled()) {
        return;
    }
    const uint64_t start_tsc = read_tsc();
    bool preempted = false;
    if (!orion_scheduler_timer_interrupt(&preempted)) {
        orion_panic_at("TIMER", "scheduler rejected PIT interrupt");
    }
    if (!preempted) {
        const uint64_t elapsed = read_tsc() - start_tsc;
        if (handler_samples == UINT64_MAX ||
            handler_tsc_total > UINT64_MAX - elapsed) {
            orion_panic_at("TIMER", "timer metric counter overflow");
        }
        ++handler_samples;
        handler_tsc_total += elapsed;
        if (elapsed > handler_tsc_max) {
            handler_tsc_max = elapsed;
        }
    }
}
