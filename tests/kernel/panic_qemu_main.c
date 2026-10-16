#include <orion/boot_info.h>

#include "../../kernel/console/serial.h"
#include "../../kernel/log/log.h"
#include "../../kernel/arch/x86_64/gdt.h"
#include "../../kernel/arch/x86_64/idt.h"
#include "../../kernel/panic/panic.h"

void kernel_main(const orion_boot_info *boot_info)
{
    (void)boot_info;
    orion_serial_init();
    if (!orion_x86_64_gdt_init() || !orion_x86_64_idt_init()) {
        orion_panic_at("TEST", "GDT/IDT initialization failed");
    }
#if defined(ORION_TEST_GENERAL_PROTECTION)
    __asm__ volatile("int $0x81");
    orion_panic_at("TEST", "general protection test unexpectedly returned");
#elif defined(ORION_TEST_INVALID_OPCODE)
    __asm__ volatile("ud2");
    orion_panic_at("TEST", "invalid opcode test unexpectedly returned");
#elif defined(ORION_TEST_PAGE_FAULT)
    {
        uintptr_t fault_address = UINT64_C(0x00007ffffffff000);
        uint64_t fault_value;
        __asm__ volatile("movq (%1), %0"
                         : "=r"(fault_value)
                         : "r"(fault_address)
                         : "memory");
        (void)fault_value;
        orion_panic_at("TEST", "page-fault test unexpectedly returned");
    }
#elif defined(ORION_TEST_DIVIDE_ERROR)
    {
        uint64_t low = 1;
        uint64_t high = 0;
        uint64_t divisor = 0;
        __asm__ volatile("divq %2"
                         : "+a"(low), "+d"(high)
                         : "r"(divisor)
                         : "cc");
        orion_panic_at("TEST", "divide-error test unexpectedly returned");
    }
#elif defined(ORION_TEST_DOUBLE_FAULT_IST)
    struct __attribute__((packed)) {
        uint16_t limit;
        uint64_t base;
    } idtr;
    uint8_t *idt_base;
    __asm__ volatile("sidt %0" : "=m"(idtr));
    idt_base = (uint8_t *)(uintptr_t)idtr.base;
    idt_base[6U * 16U + 5U] &= 0x7fU;
    idt_base[11U * 16U + 5U] &= 0x7fU;
    __asm__ volatile("ud2");
    orion_panic_at("TEST", "double-fault IST test unexpectedly returned");
#else
    __asm__ volatile("int3");
    if (!orion_log(ORION_LOG_INFO, "TEST",
                   "Breakpoint handler returned to interrupted code")) {
        orion_panic_at("TEST", "breakpoint test logging failed");
    }
    orion_panic_at("TEST", "QEMU panic path reached");
#endif
}
