section .multiboot
    align 4

    dd 0x1BADB002
    dd 0x00
    dd -(0x1BADB002 + 0x00)

section .text

global _start
extern kernel_main

_start:
    mov esp, stack_top
    cli

    ; Multiboot:
    ; EAX = bootloader magic
    ; EBX = pointer to Multiboot information structure
    ;
    ; kernel_main(magic, multiboot_info)

    push ebx
    push eax

    call kernel_main

    add esp, 8

.hang:
    hlt
    jmp .hang

; Keyboard
global keyboard_isr_asm
extern keyboard_handler

keyboard_isr_asm:
    pusha
    call keyboard_handler
    popa
    iret

; Timer
global timer_isr_asm
extern timer_handler

timer_isr_asm:
    pusha
    call timer_handler
    popa
    iret

; GDT
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


; CPU EXCEPTIONS
global exception_0
global exception_1
global exception_2
global exception_3
global exception_4
global exception_5
global exception_6
global exception_7
global exception_8
global exception_9
global exception_10
global exception_11
global exception_12
global exception_13
global exception_14
global exception_15
global exception_16
global exception_17
global exception_18
global exception_19
global exception_30

extern exception_handler


; Exceptions WITHOUT CPU-provided error codes
exception_0:
    push 0
    push 0
    jmp exception_common

exception_1:
    push 0
    push 1
    jmp exception_common

exception_2:
    push 0
    push 2
    jmp exception_common

exception_3:
    push 0
    push 3
    jmp exception_common

exception_4:
    push 0
    push 4
    jmp exception_common

exception_5:
    push 0
    push 5
    jmp exception_common

exception_6:
    push 0
    push 6
    jmp exception_common

exception_7:
    push 0
    push 7
    jmp exception_common


; Exception 8: Double Fault
exception_8:
    push 8
    jmp exception_common

; Exception 9: Coprocessor Segment Overrun
exception_9:
    push 0
    push 9
    jmp exception_common

; Exceptions with CPU-provided error codes
exception_10:
    push 10
    jmp exception_common

exception_11:
    push 11
    jmp exception_common

exception_12:
    push 12
    jmp exception_common

exception_13:
    push 13
    jmp exception_common

exception_14:
    push 14
    jmp exception_common

; Exception 15: Reserved
exception_15:
    push 0
    push 15
    jmp exception_common

; Exceptions without error codes
exception_16:
    push 0
    push 16
    jmp exception_common

; Exception 17: Alignment Check
; CPU provides an error code.
exception_17:
    push 17
    jmp exception_common

exception_18:
    push 0
    push 18
    jmp exception_common

exception_19:
    push 0
    push 19
    jmp exception_common

; Exception 30: Security Exception
; CPU provides an error code.
exception_30:
    push 30
    jmp exception_common

; After pusha:
;
;   esp + 0   = EDI
;   esp + 4   = ESI
;   esp + 8   = EBP
;   esp + 12  = original ESP
;   esp + 16  = EBX
;   esp + 20  = EDX
;   esp + 24  = ECX
;   esp + 28  = EAX
;   esp + 32  = exception number
;   esp + 36  = error code
;   esp + 40  = EIP
;   esp + 44  = CS
;   esp + 48  = EFLAGS
;   esp + 52  = user ESP
;   esp + 56  = user SS

exception_common:
    pusha

    push esp
    call exception_handler
    add esp, 4

    popa
    add esp, 8

    iret

; Page-directory / paging support
global load_page_directory_asm
global enable_paging_asm
global invalidate_tlb_asm

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

; Default IRQ handlers
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


; Mouse
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