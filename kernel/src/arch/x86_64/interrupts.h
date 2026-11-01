
#ifndef TRUSTOS_INTERRUPTS_H
#define TRUSTOS_INTERRUPTS_H

#include <stdint.h>
#include <stdbool.h>

struct interrupt_frame {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx;
    uint64_t rcx, rbx, rax;

    uint64_t vector;
    uint64_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
bool interrupts_unmask_apic_lint0(uint64_t virtual_apic_base);

uint64_t interrupts_get_apic_base(void);

__attribute__((noreturn))
void exception_handler(const struct interrupt_frame *frame);

#endif

