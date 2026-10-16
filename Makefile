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
KERNEL_OBJECTS := $(BUILD)/entry.o $(BUILD)/main.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/cpu_state.o $(BUILD)/fatal.o \
	$(BUILD)/gdt.o $(BUILD)/idt.o $(BUILD)/interrupts.o \
	$(BUILD)/interrupt_state.o $(BUILD)/exceptions.o $(BUILD)/memory_map.o \
	$(BUILD)/physical.o $(BUILD)/page_tables.o

.PHONY: all check-tools build run test-boot test-panic-qemu test-exceptions-qemu test-unit toolchain-test validate-docs clean inspect

all: build

check-tools:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/check-toolchain.ps1

build: check-tools $(BOOT_DIR)/BOOTX64.EFI $(ESP)/kernel.elf

$(BUILD)/.dir:
	@powershell.exe -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD)' | Out-Null; New-Item -ItemType File -Force -Path '$(BUILD)/.dir' | Out-Null"

$(BOOT_DIR):
	@powershell.exe -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BOOT_DIR)' | Out-Null"

$(BUILD)/loader.obj: boot/uefi/loader.c include/orion/boot_info.h | $(BUILD)/.dir
	$(CLANG) $(EFI_CFLAGS) -c $< -o $@

$(BUILD)/io.obj: boot/uefi/io.asm | $(BUILD)/.dir
	$(NASM) -f win64 $< -o $@

$(BOOT_DIR)/BOOTX64.EFI: $(BUILD)/loader.obj $(BUILD)/io.obj | $(BOOT_DIR)
	$(LLD_LINK) /nologo /subsystem:efi_application /entry:efi_main /nodefaultlib \
		/machine:x64 /dynamicbase /nxcompat /out:$@ $^

$(BUILD)/entry.o: kernel/arch/x86_64/entry.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/cpu_state.o: kernel/arch/x86_64/cpu_state.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/fatal.o: kernel/arch/x86_64/fatal.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/gdt.o: kernel/arch/x86_64/gdt.c kernel/arch/x86_64/gdt.h \
	kernel/arch/x86_64/entry.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/idt.o: kernel/arch/x86_64/idt.c kernel/arch/x86_64/idt.h \
	kernel/arch/x86_64/gdt.h kernel/arch/x86_64/interrupts.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/interrupts.o: kernel/arch/x86_64/interrupts.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/interrupt_state.o: kernel/arch/x86_64/interrupt_state.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/exceptions.o: kernel/arch/x86_64/exceptions.c \
	kernel/arch/x86_64/idt.h kernel/arch/x86_64/cpu_state.h \
	kernel/log/log.h kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/memory_map.o: kernel/mm/memory_map.c kernel/mm/memory_map.h \
	include/orion/boot_info.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/physical.o: kernel/mm/physical.c kernel/mm/physical.h \
	kernel/mm/memory_map.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/page_tables.o: kernel/mm/page_tables.c kernel/mm/page_tables.h \
	| $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/main.o: kernel/core/main.c include/orion/boot_info.h include/orion/init_state.h \
	kernel/arch/x86_64/cpu_state.h kernel/arch/x86_64/gdt.h \
	kernel/arch/x86_64/idt.h kernel/arch/x86_64/interrupt_state.h \
	kernel/mm/memory_map.h kernel/mm/physical.h kernel/log/log.h \
	kernel/panic/panic.h kernel/console/serial.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/serial.o: kernel/console/serial.c kernel/console/serial.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/log.o: kernel/log/log.c kernel/log/log.h kernel/console/serial.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/panic.o: kernel/panic/panic.c kernel/panic/panic.h \
	kernel/log/log.h kernel/arch/x86_64/fatal.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_OBJECTS) kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(KERNEL_OBJECTS)

$(ESP)/kernel.elf: $(BUILD)/kernel.elf | $(BUILD)/.dir
	@powershell.exe -NoProfile -Command "Copy-Item -LiteralPath '$<' -Destination '$@' -Force"

inspect: build
	$(LLVM_READOBJ) --file-headers --program-headers --symbols $(BUILD)/kernel.elf
	$(LLVM_READOBJ) --file-headers --sections $(BOOT_DIR)/BOOTX64.EFI
	$(LLVM_NM) --undefined-only $(BUILD)/kernel.elf

run: build
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-qemu.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)"

test-boot: build
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-boot.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -RequireRunning

$(BUILD)/boot-info-test.exe: tests/boot_info_test.c include/orion/boot_info.h \
	include/orion/init_state.h | $(BUILD)/.dir
	$(CLANG) -Iinclude -std=c11 -Wall -Wextra -Werror $< -o $@

$(BUILD)/kernel-log-test.exe: tests/kernel/log_test.c kernel/log/log.c \
	kernel/log/log.h kernel/console/serial.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel/log -Ikernel/console \
		tests/kernel/log_test.c kernel/log/log.c -o $@

$(BUILD)/kernel-panic-test.exe: tests/kernel/panic_test.c kernel/panic/panic.c \
	kernel/panic/panic.h kernel/arch/x86_64/fatal.h kernel/log/log.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel/log -Ikernel/panic \
		tests/kernel/panic_test.c kernel/panic/panic.c -o $@

$(BUILD)/kernel-memory-test.exe: tests/kernel/memory/memory_test.c \
	kernel/mm/memory_map.c kernel/mm/memory_map.h kernel/mm/physical.c \
	kernel/mm/physical.h include/orion/boot_info.h | $(BUILD)/.dir
	$(CLANG) -Iinclude -Ikernel/mm -std=c11 -Wall -Wextra -Werror \
		tests/kernel/memory/memory_test.c kernel/mm/memory_map.c \
		kernel/mm/physical.c -o $@

$(BUILD)/page-tables-test.exe: tests/kernel/memory/page_tables_test.c \
	kernel/mm/page_tables.c kernel/mm/page_tables.h | $(BUILD)/.dir
	$(CLANG) -Ikernel/mm -std=c11 -Wall -Wextra -Werror \
		tests/kernel/memory/page_tables_test.c kernel/mm/page_tables.c -o $@

$(BUILD)/panic-test-main.o: tests/kernel/panic_qemu_main.c \
	include/orion/boot_info.h kernel/console/serial.h kernel/log/log.h \
	kernel/arch/x86_64/gdt.h kernel/arch/x86_64/idt.h \
	kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/gp-test-main.o: tests/kernel/panic_qemu_main.c \
	include/orion/boot_info.h kernel/console/serial.h kernel/log/log.h \
	kernel/arch/x86_64/gdt.h kernel/arch/x86_64/idt.h \
	kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_TEST_GENERAL_PROTECTION -c $< -o $@

$(BUILD)/ud-test-main.o: tests/kernel/panic_qemu_main.c \
	include/orion/boot_info.h kernel/console/serial.h kernel/log/log.h \
	kernel/arch/x86_64/gdt.h kernel/arch/x86_64/idt.h \
	kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_TEST_INVALID_OPCODE -c $< -o $@

$(BUILD)/df-test-main.o: tests/kernel/panic_qemu_main.c \
	include/orion/boot_info.h kernel/console/serial.h kernel/log/log.h \
	kernel/arch/x86_64/gdt.h kernel/arch/x86_64/idt.h \
	kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_TEST_DOUBLE_FAULT_IST -c $< -o $@

$(BUILD)/pf-test-main.o: tests/kernel/panic_qemu_main.c \
	include/orion/boot_info.h kernel/console/serial.h kernel/log/log.h \
	kernel/arch/x86_64/gdt.h kernel/arch/x86_64/idt.h \
	kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_TEST_PAGE_FAULT -c $< -o $@

$(BUILD)/de-test-main.o: tests/kernel/panic_qemu_main.c \
	include/orion/boot_info.h kernel/console/serial.h kernel/log/log.h \
	kernel/arch/x86_64/gdt.h kernel/arch/x86_64/idt.h \
	kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_TEST_DIVIDE_ERROR -c $< -o $@

$(BUILD)/panic-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o \
	$(BUILD)/panic-test-main.o \
	kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)

$(BUILD)/gp-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o \
	$(BUILD)/gp-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)

$(BUILD)/ud-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o \
	$(BUILD)/ud-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)

$(BUILD)/df-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o \
	$(BUILD)/df-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)

$(BUILD)/pf-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o \
	$(BUILD)/pf-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)

$(BUILD)/de-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o \
	$(BUILD)/de-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)

test-unit: $(BUILD)/boot-info-test.exe $(BUILD)/kernel-log-test.exe \
	$(BUILD)/kernel-panic-test.exe $(BUILD)/kernel-memory-test.exe \
	$(BUILD)/page-tables-test.exe
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-kernel-unit.ps1 \
		-BuildDirectory "$(BUILD)"

test-panic-qemu: check-tools $(BOOT_DIR)/BOOTX64.EFI $(ESP)/kernel.elf \
	$(BUILD)/panic-test-kernel.elf
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-kernel-panic.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)"

test-exceptions-qemu: check-tools $(BOOT_DIR)/BOOTX64.EFI \
	$(ESP)/kernel.elf $(BUILD)/gp-test-kernel.elf $(BUILD)/ud-test-kernel.elf \
	$(BUILD)/df-test-kernel.elf $(BUILD)/pf-test-kernel.elf \
	$(BUILD)/de-test-kernel.elf
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-kernel-exceptions.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)"

toolchain-test:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-toolchain.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)"

validate-docs:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/validate-docs.ps1

clean:
	@powershell.exe -NoProfile -Command "if (Test-Path -LiteralPath '$(BUILD)') { Remove-Item -LiteralPath '$(BUILD)' -Recurse -Force }"
