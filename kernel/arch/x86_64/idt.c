#include "idt.h"

#include <stddef.h>

#include "gdt.h"

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} x86_64_idt_gate;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} x86_64_descriptor_table_pointer;

_Static_assert(sizeof(x86_64_idt_gate) == 16, "IDT gate layout");
_Static_assert(offsetof(x86_64_idt_gate, ist) == 4, "IDT IST offset");
_Static_assert(offsetof(x86_64_idt_gate, offset_high) == 8,
               "IDT high-offset position");
_Static_assert(sizeof(x86_64_descriptor_table_pointer) == 10,
               "IDTR layout");

static x86_64_idt_gate idt[ORION_X86_64_IDT_ENTRY_COUNT]
    __attribute__((aligned(16)));
_Static_assert(sizeof(idt) == 4096, "IDT entry count and size");
static bool idt_initialized;
extern const uintptr_t orion_x86_64_isr_stub_table[];
extern void orion_x86_64_load_idt(
    const x86_64_descriptor_table_pointer *idtr);

static void set_gate(uint32_t vector, uintptr_t handler, uint8_t ist)
{
    x86_64_idt_gate *gate = &idt[vector];
    gate->offset_low = (uint16_t)handler;
    gate->selector = ORION_X86_64_KERNEL_CODE_SELECTOR;
    gate->ist = ist & 0x7U;
    gate->attributes = 0x8eU;
    gate->offset_middle = (uint16_t)(handler >> 16);
    gate->offset_high = (uint32_t)(handler >> 32);
    gate->reserved = 0;
}

bool orion_x86_64_idt_init(void)
{
    x86_64_descriptor_table_pointer idtr;
    x86_64_descriptor_table_pointer loaded_idtr;

    if (idt_initialized) {
        return false;
    }
    for (uint32_t vector = 0; vector < ORION_X86_64_IDT_ENTRY_COUNT;
         ++vector) {
        idt[vector] = (x86_64_idt_gate){0};
    }
    for (uint32_t vector = 0; vector < ORION_X86_64_EXCEPTION_COUNT;
         ++vector) {
        set_gate(vector, orion_x86_64_isr_stub_table[vector],
                 vector == 8U ? 1U : 0U);
    }

    idtr.limit = sizeof(idt) - 1U;
    idtr.base = (uintptr_t)idt;
    orion_x86_64_load_idt(&idtr);
    __asm__ volatile("sidt %0" : "=m"(loaded_idtr));
    if (loaded_idtr.limit != idtr.limit || loaded_idtr.base != idtr.base) {
        return false;
    }

    for (uint32_t vector = 0; vector < ORION_X86_64_EXCEPTION_COUNT;
         ++vector) {
        if (idt[vector].selector != ORION_X86_64_KERNEL_CODE_SELECTOR ||
            idt[vector].attributes != 0x8eU ||
            (idt[vector].offset_low == 0 &&
             idt[vector].offset_middle == 0 &&
             idt[vector].offset_high == 0) ||
            (vector == 8U && idt[vector].ist != 1U) ||
            (vector != 8U && idt[vector].ist != 0U)) {
            return false;
        }
    }
    idt_initialized = true;
    orion_x86_64_nmi_restore();
    return true;
}
