TOOLCHAIN_HOME ?= $(LOCALAPPDATA)/orion-msys64/msys64
BIN := $(TOOLCHAIN_HOME)/ucrt64/bin
CLANG := $(BIN)/clang
LLD_LINK := $(BIN)/lld-link
LLD := $(BIN)/ld.lld
NASM := $(BIN)/nasm
LLVM_READOBJ := $(BIN)/llvm-readobj
LLVM_NM := $(BIN)/llvm-nm
QEMU := $(BIN)/qemu-system-x86_64
BUILD := build
ESP := $(BUILD)/esp
BOOT_DIR := $(ESP)/EFI/BOOT
OVMF_CODE := $(TOOLCHAIN_HOME)/ucrt64/share/qemu/edk2-x86_64-code.fd
OVMF_VARS_TEMPLATE := $(TOOLCHAIN_HOME)/ucrt64/share/qemu/edk2-i386-vars.fd
OVMF_VARS := $(BUILD)/OVMF_VARS.fd
KERNEL_BASE := 0x200000
EFI_CFLAGS := --target=x86_64-pc-windows-msvc -Iinclude -std=c11 -O2 -Wall -Wextra \
	-Werror -ffreestanding -fno-stack-protector -fno-builtin -fno-exceptions
KERNEL_CFLAGS := --target=x86_64-elf -Iinclude -Ikernel/console -Ikernel/log \
	-Ikernel/panic -std=c11 -O2 -Wall -Wextra -Werror \
	-ffreestanding -fno-stack-protector -fno-pic -fno-builtin -mno-red-zone \
	-mcmodel=small -g
APP_CFLAGS := --target=x86_64-elf -Iinclude -Ilib -std=c11 -O2 \
	-Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pic \
	-fno-builtin -mno-red-zone -mcmodel=large -g
KERNEL_OBJECTS := $(BUILD)/entry.o $(BUILD)/main.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/cpu_state.o $(BUILD)/fatal.o \
	$(BUILD)/fault-report.o \
	$(BUILD)/gdt.o $(BUILD)/idt.o $(BUILD)/interrupts.o \
	$(BUILD)/interrupt_state.o $(BUILD)/exceptions.o $(BUILD)/memory_map.o \
	$(BUILD)/physical.o $(BUILD)/page_tables.o $(BUILD)/active_page_tables.o \
	$(BUILD)/driver-core.o $(BUILD)/pci-bus.o $(BUILD)/uart16550.o \
	$(BUILD)/context_switch.o $(BUILD)/process.o $(BUILD)/thread.o \
	$(BUILD)/scheduler.o $(BUILD)/timer.o $(BUILD)/capability.o \
	$(BUILD)/ipc-channel.o $(BUILD)/clocksource.o $(BUILD)/kernel-timer.o \
	$(BUILD)/block.o $(BUILD)/ata-pio.o $(BUILD)/orionfs.o $(BUILD)/vfs.o
KERNEL_OBJECTS += /net-network.o /ne2k-pci.o
KERNEL_OBJECTS += /user-mode.o /user-space.o
KERNEL_OBJECTS += /drivers/graphics/vga.o
KERNEL_OBJECTS += /syscall.o /elf64.o /user-program.o
