#include "gdt.h"

#include <stddef.h>

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} x86_64_tss;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} x86_64_descriptor_table_pointer;

_Static_assert(sizeof(x86_64_tss) == 104, "x86-64 TSS layout");
_Static_assert(offsetof(x86_64_tss, rsp0) == 4, "TSS RSP0 offset");
_Static_assert(offsetof(x86_64_tss, ist1) == 36, "TSS IST1 offset");
_Static_assert(offsetof(x86_64_tss, io_map_base) == 102,
               "TSS I/O map offset");
_Static_assert(sizeof(x86_64_descriptor_table_pointer) == 10,
               "GDTR layout");

static uint64_t gdt[5] __attribute__((aligned(8)));
static x86_64_tss tss __attribute__((aligned(16)));
static uint8_t double_fault_stack[16384] __attribute__((aligned(16)));
_Static_assert(sizeof(gdt) == 40, "GDT entry count and size");
static uint8_t saved_nmi_port_state;
static bool nmi_mask_active;
static bool gdt_initialized;
extern char kernel_stack_top[];

extern void orion_x86_64_load_gdt(
    const x86_64_descriptor_table_pointer *gdtr);

static uint64_t tss_descriptor(uintptr_t base, uint32_t limit)
{
    return (uint64_t)(limit & 0xffffU) |
           ((uint64_t)(base & 0xffffffU) << 16) |
           (UINT64_C(0x89) << 40) |
           ((uint64_t)((limit >> 16) & 0x0fU) << 48) |
           ((uint64_t)((base >> 24) & 0xffU) << 56);
}

bool orion_x86_64_gdt_init(void)
{
    x86_64_descriptor_table_pointer gdtr;
    x86_64_descriptor_table_pointer loaded_gdtr;
    uint16_t code_selector;
    uint16_t data_selector;
    uint16_t task_selector;
    uintptr_t tss_base = (uintptr_t)&tss;
    uintptr_t double_fault_top =
        (uintptr_t)(double_fault_stack + sizeof(double_fault_stack));

    if (gdt_initialized || nmi_mask_active ||
        (tss_base & 0xfU) != 0 || (double_fault_top & 0xfU) != 0 ||
        (uintptr_t)kernel_stack_top == 0) {
        return false;
    }

    __asm__ volatile("inb $0x70, %0"
                     : "=a"(saved_nmi_port_state)
                     :
                     : "memory");
    {
        uint8_t masked_state = saved_nmi_port_state | 0x80U;
        __asm__ volatile("outb %0, $0x70"
                         :
                         : "a"(masked_state)
                         : "memory");
    }
    nmi_mask_active = true;

    tss = (x86_64_tss){0};
    tss.rsp0 = (uintptr_t)kernel_stack_top;
    tss.ist1 = double_fault_top;
    tss.io_map_base = sizeof(tss);

    gdt[0] = 0;
    gdt[1] = UINT64_C(0x00af9a000000ffff);
    gdt[2] = UINT64_C(0x00cf92000000ffff);
    gdt[3] = tss_descriptor(tss_base, sizeof(tss) - 1U);
    gdt[4] = (uint64_t)(tss_base >> 32);

    gdtr.limit = sizeof(gdt) - 1U;
    gdtr.base = (uintptr_t)gdt;
    orion_x86_64_load_gdt(&gdtr);
    __asm__ volatile("sgdt %0" : "=m"(loaded_gdtr));
    __asm__ volatile("mov %%cs, %0" : "=r"(code_selector));
    __asm__ volatile("mov %%ds, %0" : "=r"(data_selector));
    __asm__ volatile("str %0" : "=r"(task_selector));
    if (loaded_gdtr.limit != gdtr.limit || loaded_gdtr.base != gdtr.base ||
        code_selector != ORION_X86_64_KERNEL_CODE_SELECTOR ||
        data_selector != ORION_X86_64_KERNEL_DATA_SELECTOR ||
        task_selector != ORION_X86_64_TSS_SELECTOR) {
        return false;
    }
    gdt_initialized = true;
    return true;
}

void orion_x86_64_nmi_restore(void)
{
    if (!nmi_mask_active) {
        return;
    }
    __asm__ volatile("outb %0, $0x70"
                     :
                     : "a"(saved_nmi_port_state)
                     : "memory");
    nmi_mask_active = false;
}
