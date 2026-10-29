bits 64

section .text

extern exception_handler

%macro ISR_NOERR 1
global isr_%1
isr_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr_%1
isr_%1:
    push qword %1
    jmp isr_common
%endmacro

%assign i 0
%rep 32
    %if i = 8 || i = 10 || i = 11 || i = 12 || i = 13 || i = 14 || i = 17 || i = 21 || i = 29 || i = 30
        ISR_ERR i
    %else
        ISR_NOERR i
    %endif
    %assign i i+1
%endrep

isr_common:
    cld

    ; save register (hi Benji lol)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; pointer on Interrupt-Frame
    mov rdi, rsp

    ; adjust stack on 16 Bytes
    and rsp, -16

    call exception_handler

.hang:
    cli
    hlt
    jmp .hang

; table for all Exception-Stubs for C.
section .rodata

global isr_stub_table

isr_stub_table:
%assign i 0
%rep 32
    dq isr_%+i
    %assign i i+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
