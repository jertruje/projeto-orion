#ifndef ORION_X86_64_GDT_H
#define ORION_X86_64_GDT_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_X86_64_KERNEL_CODE_SELECTOR 0x08U
#define ORION_X86_64_KERNEL_DATA_SELECTOR 0x10U
#define ORION_X86_64_USER_CODE_SELECTOR 0x18U
#define ORION_X86_64_USER_DATA_SELECTOR 0x20U
#define ORION_X86_64_TSS_SELECTOR 0x28U

bool orion_x86_64_gdt_init(void);
bool orion_x86_64_tss_set_kernel_stack(uintptr_t stack_top);
void orion_x86_64_nmi_restore(void);

#endif
