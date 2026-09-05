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
    call kernel_main

.hang:
    hlt
    jmp .hang

; keyboard implementation
global keyboard_isr_asm
keyboard_isr_asm:
    pusha                ; Push all general purpose registers
    call keyboard_handler
    popa                 ; Pop all general purpose registers
    iret                 ; INTERRUPT return (clears EFLAGS and pops CS/EIP)

; timer implementation
global timer_isr_asm
extern timer_handler

timer_isr_asm:
    pusha                ; Push all general purpose registers
    call timer_handler
    popa                 ; Pop all general purpose registers
    iret                 ; INTERRUPT return (clears EFLAGS and pops CS/EIP)

; gdt
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]  ; Get the pointer to the GDT passed from C
    lgdt [eax]          ; Load the new GDT pointer

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
    push 0              ; Dummy error code
    push 0              ; Exception number 0 (Divide by Zero)
    jmp exception_common

exception_13:
    ; CPU automatically pushes error code here
    push 13             ; Exception number 13 (General Protection Fault)
    jmp exception_common

global load_page_directory_asm
global enable_paging_asm
global invalidate_tlb_asm

extern page_fault_handler_c

load_page_directory_asm:
    mov eax, [esp + 4]
    mov cr3, eax          ; Load Page Directory Base Register (CR3)
    ret

enable_paging_asm:
    mov eax, cr0
    or eax, 0x80000000    ; Set bit 31 (PG) in CR0
    mov cr0, eax
    ret

invalidate_tlb_asm:
    mov eax, [esp + 4]
    invlpg [eax]          ; Invalidate single TLB entry
    ret

; ISR for Page Fault (Interrupt 14)
exception_14:
    pusha                 ; Push EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX (32 bytes)
    
    mov eax, cr2          ; Grab faulting address
    push eax              ; Push faulting address (2nd argument to C function)
    
    mov eax, [esp + 36]   ; Error code was pushed by CPU (32 bytes pusha + 4 bytes arg = 36)
    push eax              ; Push error code (1st argument to C function)
    
    call page_fault_handler_c
    
    add esp, 8            ; Clean up pushed args (error code + CR2)
    popa                  ; Restore general purpose registers
    add esp, 4            ; Clean up CPU error code
    iret                  ; Return from interrupt

exception_common:
    pusha               ; Push EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
    
    ; Pass the pointer to the stack (registers structure) to C
    push esp
    call exception_handler
    add esp, 4          ; Clean up the pushed ESP
    
    popa
    add esp, 8          ; Clean up error code and exception number
    iret

global dummy_isr

dummy_isr:
    pusha
    popa
    iret

; mouse
global mouse_isr_asm
extern mouse_handler

mouse_isr_asm:
    pusha                ; Push all general purpose registers
    call mouse_handler
    popa                 ; Pop all general purpose registers
    iret                 ; Interrupt return

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: