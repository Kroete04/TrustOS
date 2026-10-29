
#include "interrupts.h"
#include "graphics.h"
#include <stdint.h>
#include <stddef.h>

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

_Static_assert(sizeof(struct idt_entry) == 16,
               "Invalid IDT entry size");
_Static_assert(sizeof(struct idt_pointer) == 10,
               "Invalid IDTR size");

static struct idt_entry idt[256];

extern void isr_breakpoint(void);
extern void isr_default(void);

static uint16_t read_cs(void) {
    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static void idt_set_gate(uint8_t vector, void (*handler)(void)) {
    uint64_t address = (uint64_t)handler;
    struct idt_entry *entry = &idt[vector];

    entry->offset_low = address & 0xffff;
    entry->selector = read_cs();
    entry->ist = 0;
    entry->type_attributes = 0x8E;
    entry->offset_mid = (address >> 16) & 0xffff;
    entry->offset_high = (address >> 32) & 0xffffffff;
    entry->reserved = 0;
}

void interrupts_init(void) {
    __asm__ volatile("cli" ::: "memory");

    for (size_t i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, isr_default);
    }

    idt_set_gate(3, isr_breakpoint);

    struct idt_pointer pointer = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)idt
    };

    __asm__ volatile("lidt %0" : : "m"(pointer) : "memory");
}

void exception_handler(uint64_t vector) {
    graphics_print("\n", 255, 255, 255);
    graphics_print("TRUSTOS KERNEL PANIC!\n", 255, 60, 60);

    if (vector == 3) {
        graphics_print("EXCEPTION: BREAKPOINT\n", 255, 180, 60);
        graphics_print("VECTOR: 3\n", 255, 180, 60);
    } else {
        graphics_print("UNEXPECTED EXCEPTION\n", 255, 60, 60);
    }

    graphics_print("CPU HALTED.\n", 255, 60, 60);

    for (;;) {
        __asm__ volatile("cli; hlt" ::: "memory");
    }
}
