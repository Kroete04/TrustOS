
#include "interrupts.h"
#include "../../drivers/video/graphics.h"

#include <stdint.h>
#include <stddef.h>

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

_Static_assert(sizeof(struct idt_entry) == 16, "Invalid IDT entry");
_Static_assert(sizeof(struct idt_pointer) == 10, "Invalid IDTR");

static struct idt_entry idt[256];

extern void (*isr_stub_table[32])(void);

extern void irq1_stub(void);

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ volatile(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static void pic_init(void) {
    // Mask all IRQs during configuration
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    // Start initialization sequence
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    // Master PIC: vectors 32-39
    outb(0x21, 0x20);
    io_wait();

    // Slave PIC: vectors 40-47
    outb(0xA1, 0x28);
    io_wait();

    // Cascade wiring
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();

    // 8086 mode
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    // Only unmask IRQ1 (keyboard)
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);

}

void interrupts_enable(void) {
    __asm__ volatile("sti" ::: "memory");
}

void interrupts_disable(void) {
    __asm__ volatile("cli" ::: "memory");
}

static uint16_t read_cs(void) {
    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static void idt_set_gate(uint8_t vector, void (*handler)(void)) {
    uint64_t address = (uint64_t)handler;

    idt[vector] = (struct idt_entry) {
        .offset_low = (uint16_t)address,
        .selector = read_cs(),
        .ist = 0,
        .attributes = 0x8E,
        .offset_mid = (uint16_t)(address >> 16),
        .offset_high = (uint32_t)(address >> 32),
        .reserved = 0
    };
}

void interrupts_init(void) {
    __asm__ volatile("cli" ::: "memory");

    for (size_t i = 0; i < 256; i++) {
        idt[i] = (struct idt_entry){0};
    }

    // Install CPU-Exceptions 0 to 31
    for (size_t i = 0; i < 32; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i]);
    }

    // PS/2 Keyboard: IRQ1 -> Vector 33
    idt_set_gate(33, irq1_stub);

    struct idt_pointer pointer = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)idt
    };

    __asm__ volatile("lidt %0" : : "m"(pointer) : "memory");

    pic_init();

}

static const char *exception_name(uint64_t vector) {
    static const char *names[32] = {
        "DIVIDE ERROR",              // 0
        "DEBUG",                     // 1
        "NON-MASKABLE INTERRUPT",    // 2
        "BREAKPOINT",                // 3
        "OVERFLOW",                  // 4
        "BOUND RANGE EXCEEDED",      // 5
        "INVALID OPCODE",            // 6
        "DEVICE NOT AVAILABLE",      // 7
        "DOUBLE FAULT",              // 8
        "COPROCESSOR OVERRUN",       // 9
        "INVALID TSS",               // 10
        "SEGMENT NOT PRESENT",       // 11
        "STACK SEGMENT FAULT",       // 12
        "GENERAL PROTECTION",        // 13
        "PAGE FAULT",                // 14
        "RESERVED",                  // 15
        "X87 FLOATING POINT",        // 16
        "ALIGNMENT CHECK",           // 17
        "MACHINE CHECK",             // 18
        "SIMD FLOATING POINT",       // 19
        "VIRTUALIZATION",            // 20
        "CONTROL PROTECTION",        // 21
        "RESERVED",                  // 22
        "RESERVED",                  // 23
        "RESERVED",                  // 24
        "RESERVED",                  // 25
        "RESERVED",                  // 26
        "RESERVED",                  // 27
        "HYPERVISOR INJECTION",      // 28
        "VMM COMMUNICATION",         // 29
        "SECURITY EXCEPTION",        // 30
        "RESERVED"                   // 31
    };

    return vector < 32 ? names[vector] : "UNKNOWN";
}

static void print_hex(uint64_t value) {
    char text[19] = "0x0000000000000000";
    const char *hex = "0123456789ABCDEF";

    for (int i = 17; i >= 2; i--) {
        text[i] = hex[value & 0xF];
        value >>= 4;
    }

    graphics_print(text, 255, 180, 60);
}

__attribute__((noreturn))
void exception_handler(const struct interrupt_frame *frame) {
    graphics_print("\n!!! KERNEL PANIC !!!\n", 255, 60, 60);

    graphics_print("EXCEPTION: ", 255, 255, 255);
    graphics_print(exception_name(frame->vector), 255, 180, 60);
    graphics_print("\n", 255, 255, 255);

    graphics_print("VECTOR: ", 255, 255, 255);
    print_hex(frame->vector);
    graphics_print("\n", 255, 255, 255);

    graphics_print("ERROR CODE: ", 255, 255, 255);
    print_hex(frame->error_code);
    graphics_print("\n", 255, 255, 255);

    graphics_print("RIP: ", 255, 255, 255);
    print_hex(frame->rip);
    graphics_print("\n", 255, 255, 255);

    if (frame->vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

        graphics_print("FAULT ADDRESS: ", 255, 255, 255);
        print_hex(cr2);
        graphics_print("\n", 255, 255, 255);
    }

    graphics_print("\nCPU HALTED.\n", 255, 60, 60);

    for (;;) {
        __asm__ volatile("cli; hlt" ::: "memory");
    }

}

uint64_t interrupts_get_apic_base(void) {
    uint32_t low;
    uint32_t high;

    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1B));
    return ((uint64_t)high << 32) | low;
}

bool interrupts_unmask_apic_lint0(uint64_t virtual_apic_base) {
    volatile uint32_t *lint0 =
        (volatile uint32_t *)(uintptr_t)(virtual_apic_base + 0x350);

    uint32_t value = *lint0;

    // Preserve the firmware-provided delivery mode; clear only the mask bit.
    *lint0 = value & ~(1U << 16);
    return ((*lint0 & (1U << 16)) == 0);
}
