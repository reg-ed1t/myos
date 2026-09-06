TOOLCHAIN ?= clang

ifeq ($(TOOLCHAIN), clang)
    CC      = clang
    LD      = ld.lld
    
    CFLAGS_TOOLCHAIN  = --target=i386-pc-none-elf
    STRICT_EXTRA      = -Wgnu
    LDFLAGS_TOOLCHAIN = -m elf_i386
else ifeq ($(TOOLCHAIN), gcc)
    CROSS_PREFIX     ?= i386-elf-
    CC                = $(CROSS_PREFIX)gcc
    LD                = $(CROSS_PREFIX)gcc
    
    CFLAGS_TOOLCHAIN  =
    STRICT_EXTRA      =
    LDFLAGS_TOOLCHAIN = -ffreestanding -O2 -lgcc
else
    $(error Unknown TOOLCHAIN: $(TOOLCHAIN). Supported options are 'clang' and 'gcc')
endif

AS = nasm

COMMON_CFLAGS = -Iinclude -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
                -fno-stack-protector -fno-pie -fno-pic

CFLAGS        = $(CFLAGS_TOOLCHAIN) $(COMMON_CFLAGS)
STRICT_CFLAGS = $(CFLAGS) $(STRICT_EXTRA) -Werror -Wpedantic -Wshadow \
                -Wpointer-arith -Wcast-align -Wwrite-strings \
                -Wstrict-prototypes -Wmissing-prototypes -Og -g

ASFLAGS = -f elf32
LDFLAGS = $(LDFLAGS_TOOLCHAIN) -nostdlib -T linker.ld

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

C_SOURCES = src/kernel.c src/vga.c src/idt.c src/gdt.c src/drivers.c src/pmm.c src/vmm.c
C_OBJS = $(patsubst src/%.c, $(OBJ_DIR)/%.o, $(C_SOURCES))
BOOT_OBJ = $(OBJ_DIR)/boot.o

OBJS = $(BOOT_OBJ) $(C_OBJS)

BIN = $(BUILD_DIR)/myos.bin
ISO = $(BUILD_DIR)/myos.iso
BOOT = boot.asm

.PHONY: all strict clean

all: $(ISO)

strict: CFLAGS := $(STRICT_CFLAGS)
strict: clean $(ISO)

$(BIN): $(OBJS) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(ISO): $(BIN)
	cp $(BIN) iso/boot/
	grub-mkrescue -o $@ iso
	rm -f iso/boot/myos.bin

$(OBJ_DIR)/boot.o: $(BOOT) | $(OBJ_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)