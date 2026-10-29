
bits 64

section .text

global isr_breakpoint
global isr_default

extern exception_handler


isr_breakpoint:
    mov rdi, 3

    and rsp, -16

    call exception_handler

.halt:
    cli
    hlt
    jmp .halt

isr_default:
    cli

.default_halt:
    hlt
    jmp .default_halt
