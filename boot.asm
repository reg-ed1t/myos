section .multiboot
    align 4
    dd 0x1BADB002
    dd 0x00
    dd -(0x1BADB002 + 0x00)

section .text
global _start
extern kernel_main
extern keyboard_handler

_start:
    mov esp, stack_top
    cli

    ; Multiboot:
    ; EAX = bootloader magic
    ; EBX = pointer to multiboot information structure
    ;
    ; kernel_main(uint32_t magic, uint32_t multiboot_info)

    push ebx
    push eax
    call kernel_main
    add esp, 8

.hang:
    hlt
    jmp .hang

; keyboard implementation
global keyboard_isr_asm
keyboard_isr_asm:
    pusha
    call keyboard_handler
    popa
    iret

; timer implementation
global timer_isr_asm
extern timer_handler

timer_isr_asm:
    pusha
    call timer_handler
    popa
    iret

; gdt
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush

.flush:
    ret

; ISRs for CPU Exceptions
global exception_0
global exception_13
global exception_14

extern exception_handler

exception_0:
    push 0
    push 0
    jmp exception_common

exception_13:
    ; CPU automatically pushes error code here
    push 13
    jmp exception_common

global load_page_directory_asm
global enable_paging_asm
global invalidate_tlb_asm

extern page_fault_handler_c

load_page_directory_asm:
    mov eax, [esp + 4]
    mov cr3, eax
    ret

enable_paging_asm:
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    ret

invalidate_tlb_asm:
    mov eax, [esp + 4]
    invlpg [eax]
    ret

; ISR for Page Fault (Interrupt 14)
exception_14:
    pusha

    mov eax, cr2
    mov edx, [esp + 32]    ; error code
    mov ecx, [esp + 36]    ; EIP
    mov ebx, [esp + 40]    ; CS

    push ebx               ; CS
    push ecx               ; EIP
    push edx               ; error code
    push eax               ; faulting address

    call page_fault_handler_c

    add esp, 16
    popa

    ; CPU pushed the original page-fault error code.
    add esp, 4

    iret

exception_common:
    pusha

    push esp
    call exception_handler
    add esp, 4

    popa
    add esp, 8
    iret

global dummy_isr
global default_master_irq
global default_slave_irq

dummy_isr:
    pusha
    popa
    iret

default_master_irq:
    pusha
    mov al, 0x20
    out 0x20, al
    popa
    iret

default_slave_irq:
    pusha
    mov al, 0x20
    out 0xA0, al
    out 0x20, al
    popa
    iret

; mouse
global mouse_isr_asm
extern mouse_handler

mouse_isr_asm:
    pusha
    call mouse_handler
    popa
    iret

section .bss
align 16

stack_bottom:
    resb 16384

stack_top: