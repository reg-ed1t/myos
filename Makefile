export PATH := $(HOME)/opt/cross/bin:$(PATH)
CC = i386-elf-gcc
AS = nasm
LD = i386-elf-gcc

#CFLAGS = -Iinclude -std=gnu99 -ffreestanding -O2 \
         -Wall -Wextra -Werror -Wpedantic \
         -Wshadow -Wpointer-arith -Wcast-align \
         -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
         -fno-stack-protector -fno-pie -fno-pic
CFLAGS = -Iinclude -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -fno-pie -fno-pic
ASFLAGS = -f elf32
LDFLAGS = -ffreestanding -O2 -nostdlib -T linker.ld -lgcc

OBJS = boot.o kernel.o vga.o idt.o gdt.o drivers.o pmm.o vmm.o
BIN = myos.bin
BOOT = boot.asm

all: $(BIN)

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	rm -f $(OBJS)
	cp $(BIN) iso/boot/
	grub-mkrescue -o myos.iso iso/

boot.o: $(BOOT)
	$(AS) $(ASFLAGS) $< -o $@

kernel.o: src/kernel.c
	$(CC) -c $< -o $@ $(CFLAGS)

vga.o: src/vga.c
	$(CC) -c $< -o $@ $(CFLAGS)

idt.o: src/idt.c
	$(CC) -c $< -o $@ $(CFLAGS)

gdt.o: src/gdt.c
	$(CC) -c $< -o $@ $(CFLAGS)

drivers.o: src/drivers.c
	$(CC) -c $< -o $@ $(CFLAGS)

pmm.o: src/pmm.c
	$(CC) -c $< -o $@ $(CFLAGS)
	
vmm.o: src/vmm.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJS) $(BIN)
