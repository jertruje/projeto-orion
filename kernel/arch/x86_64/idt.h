#ifndef ORION_X86_64_IDT_H
#define ORION_X86_64_IDT_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_X86_64_EXCEPTION_COUNT 32U
#define ORION_X86_64_IDT_ENTRY_COUNT 256U
#define ORION_X86_64_EXCEPTION_FRAME_VECTOR_OFFSET 120U
#define ORION_X86_64_EXCEPTION_FRAME_ERROR_OFFSET 128U
#define ORION_X86_64_EXCEPTION_FRAME_RIP_OFFSET 136U

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
} orion_x86_64_exception_frame;

_Static_assert(sizeof(orion_x86_64_exception_frame) == 160,
               "exception register frame layout");
_Static_assert(__builtin_offsetof(orion_x86_64_exception_frame, vector) ==
                   ORION_X86_64_EXCEPTION_FRAME_VECTOR_OFFSET,
               "exception vector offset");
_Static_assert(__builtin_offsetof(orion_x86_64_exception_frame, error_code) ==
                   ORION_X86_64_EXCEPTION_FRAME_ERROR_OFFSET,
               "exception error code offset");
_Static_assert(__builtin_offsetof(orion_x86_64_exception_frame, rip) ==
                   ORION_X86_64_EXCEPTION_FRAME_RIP_OFFSET,
               "exception RIP offset");

bool orion_x86_64_idt_init(void);

#endif
