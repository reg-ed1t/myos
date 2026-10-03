TOOLCHAIN ?= clang

ifeq ($(TOOLCHAIN), clang)
    CC      = clang
    LD      = ld.lld
    
    CFLAGS_TOOLCHAIN  = --target=i386-pc-none-elf
    DEBUG_EXTRA       = -Wgnu
    LDFLAGS_TOOLCHAIN = -m elf_i386
else ifeq ($(TOOLCHAIN), gcc)
    CROSS_PREFIX     ?= i386-elf-
    CC                = $(CROSS_PREFIX)gcc
    LD                = $(CROSS_PREFIX)gcc

    CFLAGS_TOOLCHAIN  =
    DEBUG_EXTRA       =
    LDFLAGS_TOOLCHAIN = -ffreestanding -O2 -lgcc
else
    $(error Unknown TOOLCHAIN: $(TOOLCHAIN). Supported options are 'clang' and 'gcc')
endif

AS = nasm

# Базовые флаги компилятора
COMMON_CFLAGS = -Iinclude -std=gnu99 -ffreestanding \
                -fno-stack-protector -fno-pie -fno-pic

# Все предупреждения
WARN_CFLAGS   = -Wall -Wextra -Wpedantic -Wshadow \
                -Wpointer-arith -Wcast-align -Wwrite-strings \
                -Wstrict-prototypes -Wmissing-prototypes $(DEBUG_EXTRA)

# Настройка режима сборки (по умолчанию debug)
MODE ?= debug

ifeq ($(MODE), release)
    BUILD_SUBDIR = release
    MODE_CFLAGS  = -O2 -Werror
else
    BUILD_SUBDIR = debug
    MODE_CFLAGS  = -Og -g -Werror -DDEBUG
endif

CFLAGS  = $(CFLAGS_TOOLCHAIN) $(COMMON_CFLAGS) $(WARN_CFLAGS) $(MODE_CFLAGS)
ASFLAGS = -f elf32
LDFLAGS = $(LDFLAGS_TOOLCHAIN) -nostdlib -T linker.ld

BUILD_DIR = build/$(BUILD_SUBDIR)
OBJ_DIR   = $(BUILD_DIR)/obj

C_SOURCES = $(wildcard src/*.c)
C_OBJS    = $(patsubst src/%.c, $(OBJ_DIR)/%.o, $(C_SOURCES))
BOOT_OBJ  = $(OBJ_DIR)/boot.o

OBJS = $(BOOT_OBJ) $(C_OBJS)

BIN  = $(BUILD_DIR)/myos.bin
ISO  = $(BUILD_DIR)/myos.iso
BOOT = boot.asm

.PHONY: all debug release clean

all: debug

debug:
	@$(MAKE) MODE=debug build-target

release:
	@$(MAKE) MODE=release build-target

build-target: $(ISO)

$(BIN): $(OBJS) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(ISO): $(BIN)
	cp $(BIN) iso/boot/
	grub-mkrescue -o $@ iso
	rm -f iso/boot/myos.bin

$(OBJ_DIR)/boot.o: $(BOOT) | $(OBJ_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf build