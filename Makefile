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
KERNEL_OBJECTS += $(BUILD)/net-network.o $(BUILD)/ne2k-pci.o
KERNEL_OBJECTS += $(BUILD)/user-mode.o $(BUILD)/user-space.o \
  /power/acpi.o \\
	$(BUILD)/syscall.o $(BUILD)/elf64.o $(BUILD)/user-program.o
KERNEL_TASKING_OBJECTS := $(BUILD)/context_switch.o $(BUILD)/process.o \
	$(BUILD)/thread.o $(BUILD)/scheduler.o $(BUILD)/timer.o \
	$(BUILD)/interrupt_state.o $(BUILD)/capability.o $(BUILD)/ipc-channel.o \
	$(BUILD)/fault-report.o $(BUILD)/syscall.o $(BUILD)/user-space.o \
	$(BUILD)/page_tables.o $(BUILD)/physical.o $(BUILD)/memory_map.o \
	$(BUILD)/serial.o $(BUILD)/user-mode.o
.PHONY: all check-tools build run test-boot test-storage-qemu prepare-storage-tool \
	test-network-qemu \
	test-tasking-qemu test-panic-qemu test-user-mode-qemu \
	test-exceptions-qemu test-preemption-qemu test-unit toolchain-test \
	validate-docs clean inspect
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
$(BUILD)/user-mode.o: kernel/arch/x86_64/user_mode.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/user-space.o: kernel/mm/user_space.c kernel/mm/user_space.h \
	kernel/mm/page_tables.h kernel/mm/physical.h \
	kernel/arch/x86_64/cpu_state.h kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/syscall.o: kernel/syscall/syscall.c kernel/syscall/syscall.h \
	kernel/mm/user_space.h kernel/console/serial.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/elf64.o: kernel/exec/elf64.c kernel/exec/elf64.h \
	kernel/mm/user_space.h kernel/mm/physical.h \
	kernel/arch/x86_64/cpu_state.h kernel/arch/x86_64/gdt.h \
	kernel/arch/x86_64/interrupt_state.h kernel/arch/x86_64/user_mode.h \
	kernel/syscall/syscall.h include/orion/boot_info.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/user-program.o: kernel/exec/user_program.c \
	kernel/exec/user_program.h kernel/exec/elf64.h \
	kernel/arch/x86_64/gdt.h kernel/arch/x86_64/interrupt_state.h \
	kernel/arch/x86_64/user_mode.h kernel/syscall/syscall.h \
	kernel/task/process.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/user-entry.o: apps/orion/entry.S | $(BUILD)/.dir
	$(CLANG) $(APP_CFLAGS) -c $< -o $@
$(BUILD)/user-test-app.o: apps/orion/test_app.c \
	lib/orion/syscall.h | $(BUILD)/.dir
	$(CLANG) $(APP_CFLAGS) -c $< -o $@
$(BUILD)/user-lib-syscall.o: lib/orion/syscall.c lib/orion/syscall.h | $(BUILD)/.dir
	$(CLANG) $(APP_CFLAGS) -c $< -o $@
$(BUILD)/application.elf: $(BUILD)/user-entry.o \
	$(BUILD)/user-test-app.o $(BUILD)/user-lib-syscall.o apps/orion/test_app.ld
	$(LLD) -m elf_x86_64 -T apps/orion/test_app.ld --no-undefined \
		--build-id=none -o $@ $(filter %.o,$^)
$(BUILD)/user-test-main.o: kernel/core/main.c include/orion/boot_info.h \
	include/orion/init_state.h kernel/exec/user_program.h \
	kernel/arch/x86_64/cpu_state.h kernel/arch/x86_64/gdt.h \
	kernel/arch/x86_64/idt.h kernel/arch/x86_64/interrupt_state.h \
	kernel/arch/x86_64/timer.h kernel/drivers/bus/pci.h \
	kernel/drivers/core/driver.h kernel/drivers/platform/uart16550.h \
	kernel/mm/active_page_tables.h kernel/mm/memory_map.h \
	kernel/mm/physical.h kernel/task/scheduler.h kernel/log/log.h \
	kernel/security/capability.h kernel/panic/panic.h \
	kernel/console/serial.h kernel/time/clocksource.h \
	kernel/time/timer.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_USER_QEMU_TEST -c $< -o $@
$(BUILD)/user-test-kernel.elf: $(filter-out $(BUILD)/main.o,$(KERNEL_OBJECTS)) \
	$(BUILD)/user-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/storage-test-main.o: kernel/core/main.c include/orion/boot_info.h \
	include/orion/init_state.h kernel/time/clocksource.h kernel/time/timer.h \
	kernel/vfs/vfs.h kernel/drivers/storage/ata_pio.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_STORAGE_QEMU_TEST -c $< -o $@
$(BUILD)/storage-test-kernel.elf: $(filter-out $(BUILD)/main.o,$(KERNEL_OBJECTS)) \
	$(BUILD)/storage-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/network-test-main.o: kernel/core/main.c include/orion/boot_info.h \
	include/orion/init_state.h kernel/time/clocksource.h kernel/time/timer.h \
	kernel/vfs/vfs.h kernel/drivers/storage/ata_pio.h \
	kernel/drivers/net/ne2k_pci.h kernel/net/network.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_NETWORK_QEMU_TEST -c $< -o $@
$(BUILD)/network-test-kernel.elf: $(filter-out $(BUILD)/main.o,$(KERNEL_OBJECTS)) \
	$(BUILD)/network-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/cpu_state.o: kernel/arch/x86_64/cpu_state.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/fatal.o: kernel/arch/x86_64/fatal.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/fault-report.o: kernel/fault/report.c kernel/fault/report.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
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
	kernel/arch/x86_64/timer.h \
	kernel/log/log.h kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/timer.o: kernel/arch/x86_64/timer.c \
	kernel/arch/x86_64/timer.h kernel/arch/x86_64/idt.h \
	kernel/arch/x86_64/interrupt_state.h kernel/task/scheduler.h \
	kernel/panic/panic.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/clocksource.o: kernel/time/clocksource.c kernel/time/clocksource.h \
	kernel/arch/x86_64/interrupt_state.h kernel/arch/x86_64/timer.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/kernel-timer.o: kernel/time/timer.c kernel/time/timer.h \
	kernel/time/clocksource.h kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/block.o: kernel/storage/block.c kernel/storage/block.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/ata-pio.o: kernel/drivers/storage/ata_pio.c \
	kernel/drivers/storage/ata_pio.h kernel/storage/block.h \
	kernel/drivers/core/driver.h kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/orionfs.o: kernel/fs/orionfs.c kernel/fs/orionfs.h \
	kernel/storage/block.h kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/vfs.o: kernel/vfs/vfs.c kernel/vfs/vfs.h kernel/fs/orionfs.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/net-network.o: kernel/net/network.c kernel/net/network.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/ne2k-pci.o: kernel/drivers/net/ne2k_pci.c \
	kernel/drivers/net/ne2k_pci.h kernel/net/network.h \
	kernel/drivers/bus/pci.h kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
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
$(BUILD)/active_page_tables.o: kernel/mm/active_page_tables.c \
	kernel/mm/active_page_tables.h kernel/mm/page_tables.h \
	kernel/mm/memory_map.h kernel/mm/physical.h \
	kernel/arch/x86_64/cpu_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/context_switch.o: kernel/arch/x86_64/context_switch.S | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/process.o: kernel/task/process.c kernel/task/process.h \
	kernel/task/thread.h kernel/security/capability.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/capability.o: kernel/security/capability.c \
	kernel/security/capability.h kernel/task/process.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/ipc-channel.o: kernel/ipc/channel.c kernel/ipc/channel.h \
	kernel/security/capability.h kernel/task/scheduler.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/thread.o: kernel/task/thread.c kernel/task/thread.h \
	kernel/task/process.h kernel/task/scheduler.h \
	kernel/arch/x86_64/context_switch.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/scheduler.o: kernel/task/scheduler.c kernel/task/scheduler.h \
	kernel/task/thread.h kernel/task/process.h \
	kernel/arch/x86_64/context_switch.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/nx-active-page-tables.o: kernel/mm/active_page_tables.c \
	kernel/mm/active_page_tables.h kernel/mm/page_tables.h \
	kernel/mm/memory_map.h kernel/mm/physical.h \
	kernel/arch/x86_64/cpu_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_TEST_NX_EXECUTION_FAULT -c $< -o $@
$(BUILD)/readonly-active-page-tables.o: kernel/mm/active_page_tables.c \
	kernel/mm/active_page_tables.h kernel/mm/page_tables.h \
	kernel/mm/memory_map.h kernel/mm/physical.h \
	kernel/arch/x86_64/cpu_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -DORION_TEST_READONLY_WRITE_FAULT -c $< -o $@
$(BUILD)/main.o: kernel/core/main.c include/orion/boot_info.h include/orion/init_state.h \
	kernel/arch/x86_64/cpu_state.h kernel/arch/x86_64/gdt.h \
	kernel/arch/x86_64/idt.h kernel/arch/x86_64/interrupt_state.h \
	kernel/arch/x86_64/timer.h kernel/drivers/bus/pci.h \
	kernel/drivers/core/driver.h kernel/drivers/platform/uart16550.h \
	kernel/mm/active_page_tables.h kernel/mm/memory_map.h \
	kernel/mm/physical.h kernel/task/scheduler.h kernel/log/log.h \
	kernel/security/capability.h kernel/panic/panic.h \
	kernel/console/serial.h kernel/time/clocksource.h \
	kernel/time/timer.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/driver-core.o: kernel/drivers/core/driver.c \
	kernel/drivers/core/driver.h kernel/arch/x86_64/interrupt_state.h \
	| $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/pci-bus.o: kernel/drivers/bus/pci.c kernel/drivers/bus/pci.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) $(KERNEL_CFLAGS) -c $< -o $@
$(BUILD)/uart16550.o: kernel/drivers/platform/uart16550.c \
	kernel/drivers/platform/uart16550.h kernel/drivers/core/driver.h \
	kernel/console/serial.h | $(BUILD)/.dir
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
test-user-mode-qemu: check-tools $(BOOT_DIR)/BOOTX64.EFI \
	$(BUILD)/user-test-kernel.elf $(BUILD)/application.elf
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-user-mode-qemu.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)" \
		-UserKernel "$(BUILD)/user-test-kernel.elf" \
		-BootLoader "$(BOOT_DIR)/BOOTX64.EFI" \
		-Application "$(BUILD)/application.elf"
test-tasking-qemu: build
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-kernel-tasking.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)"
test-preemption-qemu: build
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-kernel-preemption.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)"
$(BUILD)/boot-info-test.exe: tests/boot_info_test.c include/orion/boot_info.h \
	include/orion/init_state.h | $(BUILD)/.dir
	$(CLANG) -Iinclude -std=c11 -Wall -Wextra -Werror $< -o $@
$(BUILD)/kernel-log-test.exe: tests/kernel/log_test.c kernel/log/log.c \
	kernel/log/log.h kernel/console/serial.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel/log -Ikernel/console \
		tests/kernel/log_test.c kernel/log/log.c -o $@
$(BUILD)/kernel-panic-test.exe: tests/kernel/panic_test.c kernel/panic/panic.c \
	kernel/panic/panic.h kernel/arch/x86_64/fatal.h kernel/log/log.h \
	kernel/fault/report.c kernel/fault/report.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel/log -Ikernel/panic \
		-Ikernel/fault -Ikernel/arch/x86_64 tests/kernel/panic_test.c \
		kernel/panic/panic.c kernel/fault/report.c -o $@
$(BUILD)/kernel-fault-test.exe: tests/kernel/fault/fault_test.c \
	kernel/fault/report.c kernel/fault/report.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel/fault \
		-Ikernel/arch/x86_64 tests/kernel/fault/fault_test.c \
		kernel/fault/report.c -o $@
$(BUILD)/kernel-drivers-test.exe: tests/kernel/drivers/drivers_test.c \
	kernel/drivers/core/driver.c kernel/drivers/core/driver.h \
	kernel/drivers/bus/pci.c kernel/drivers/bus/pci.h \
	kernel/fault/report.c kernel/fault/report.h \
	kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel/drivers/core \
		-Ikernel/drivers/bus -Ikernel/arch/x86_64 \
		tests/kernel/drivers/drivers_test.c kernel/drivers/core/driver.c \
		kernel/drivers/bus/pci.c kernel/fault/report.c -o $@
$(BUILD)/kernel-time-test.exe: tests/kernel/time/time_test.c \
	kernel/time/clocksource.c kernel/time/clocksource.h \
	kernel/time/timer.c kernel/time/timer.h \
	kernel/arch/x86_64/timer.h kernel/arch/x86_64/interrupt_state.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel \
		tests/kernel/time/time_test.c kernel/time/clocksource.c \
		kernel/time/timer.c -o $@
$(BUILD)/kernel-storage-test.exe: tests/kernel/storage/storage_test.c \
	kernel/storage/block.c kernel/storage/block.h kernel/fs/orionfs.c \
	kernel/fs/orionfs.h kernel/vfs/vfs.c kernel/vfs/vfs.h \
	kernel/arch/x86_64/interrupt_state.h \
	tests/storage/host_interrupt_shim.c | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel \
		tests/kernel/storage/storage_test.c kernel/storage/block.c \
		kernel/fs/orionfs.c kernel/vfs/vfs.c \
		tests/storage/host_interrupt_shim.c -o $@
$(BUILD)/kernel-net-test.exe: tests/kernel/net/network_test.c \
	kernel/net/network.c kernel/net/network.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel \
		tests/kernel/net/network_test.c kernel/net/network.c -o $@
$(BUILD)/kernel-elf64-test.exe: tests/kernel/exec/elf64_test.c \
	kernel/exec/elf64.c kernel/exec/elf64.h kernel/mm/user_space.h \
	kernel/mm/page_tables.h include/orion/boot_info.h | $(BUILD)/.dir
	$(CLANG) -I. -Iinclude -Ikernel -std=c11 -Wall -Wextra -Werror \
		tests/kernel/exec/elf64_test.c kernel/exec/elf64.c -o $@
$(BUILD)/orionfs-image.exe: tools/orionfs_image.c \
	kernel/storage/block.c kernel/fs/orionfs.c \
	tests/storage/host_interrupt_shim.c | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel \
		tools/orionfs_image.c kernel/storage/block.c kernel/fs/orionfs.c \
		tests/storage/host_interrupt_shim.c -o $@
prepare-storage-tool: $(BUILD)/orionfs-image.exe
test-storage-qemu: check-tools $(BOOT_DIR)/BOOTX64.EFI \
	$(BUILD)/storage-test-kernel.elf $(BUILD)/orionfs-image.exe
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-storage-qemu.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)" \
		-StorageKernel "$(BUILD)/storage-test-kernel.elf" \
		-BootLoader "$(BOOT_DIR)/BOOTX64.EFI" -AllowWrite
test-network-qemu: check-tools $(BOOT_DIR)/BOOTX64.EFI \
	$(BUILD)/network-test-kernel.elf
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-network-qemu.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)" \
		-NetworkKernel "$(BUILD)/network-test-kernel.elf" \
		-BootLoader "$(BOOT_DIR)/BOOTX64.EFI"
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
$(BUILD)/kernel-task-test.exe: tests/kernel/task/task_test.c \
	kernel/task/process.c kernel/task/process.h kernel/task/thread.c \
	kernel/task/thread.h kernel/task/scheduler.c kernel/task/scheduler.h \
	kernel/security/capability.c kernel/security/capability.h \
	kernel/arch/x86_64/context_switch.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel/task -Ikernel/security \
		-Ikernel/log -Ikernel/panic -Ikernel/arch/x86_64 \
		tests/kernel/task/task_test.c kernel/task/process.c \
		kernel/task/thread.c kernel/task/scheduler.c \
		kernel/security/capability.c -o $@
$(BUILD)/kernel-ipc-test.exe: tests/kernel/ipc/ipc_test.c \
	kernel/ipc/channel.c kernel/ipc/channel.h \
	kernel/task/process.c kernel/task/process.h kernel/task/thread.c \
	kernel/task/thread.h kernel/task/scheduler.c kernel/task/scheduler.h \
	kernel/security/capability.c kernel/security/capability.h \
	kernel/arch/x86_64/context_switch.h | $(BUILD)/.dir
	$(CLANG) -std=c11 -Wall -Wextra -Werror -Ikernel/task -Ikernel/security \
		-Ikernel/ipc -Ikernel/log -Ikernel/panic -Ikernel/arch/x86_64 \
		tests/kernel/ipc/ipc_test.c kernel/ipc/channel.c \
		kernel/task/process.c kernel/task/thread.c kernel/task/scheduler.c \
		kernel/security/capability.c -o $@
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
	$(BUILD)/cpu_state.o $(KERNEL_TASKING_OBJECTS) \
	$(BUILD)/panic-test-main.o \
	kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/gp-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o $(KERNEL_TASKING_OBJECTS) \
	$(BUILD)/gp-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/ud-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o $(KERNEL_TASKING_OBJECTS) \
	$(BUILD)/ud-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/df-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o $(KERNEL_TASKING_OBJECTS) \
	$(BUILD)/df-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/pf-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o $(KERNEL_TASKING_OBJECTS) \
	$(BUILD)/pf-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/de-test-kernel.elf: $(BUILD)/entry.o $(BUILD)/serial.o \
	$(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/fatal.o $(BUILD)/gdt.o \
	$(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/exceptions.o \
	$(BUILD)/cpu_state.o $(KERNEL_TASKING_OBJECTS) \
	$(BUILD)/de-test-main.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/nx-test-kernel.elf: $(filter-out $(BUILD)/active_page_tables.o,$(KERNEL_OBJECTS)) \
	$(BUILD)/nx-active-page-tables.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
$(BUILD)/readonly-test-kernel.elf: $(filter-out $(BUILD)/active_page_tables.o,$(KERNEL_OBJECTS)) \
	$(BUILD)/readonly-active-page-tables.o kernel/linker.ld
	$(LLD) -m elf_x86_64 -T kernel/linker.ld --no-undefined --build-id=none \
		-o $@ $(filter %.o,$^)
test-unit: $(BUILD)/boot-info-test.exe $(BUILD)/kernel-log-test.exe \
	$(BUILD)/kernel-panic-test.exe $(BUILD)/kernel-memory-test.exe \
	$(BUILD)/page-tables-test.exe $(BUILD)/kernel-task-test.exe \
	$(BUILD)/kernel-ipc-test.exe $(BUILD)/kernel-fault-test.exe \
	$(BUILD)/kernel-drivers-test.exe $(BUILD)/kernel-time-test.exe \
	$(BUILD)/kernel-storage-test.exe $(BUILD)/kernel-net-test.exe \
	$(BUILD)/kernel-elf64-test.exe
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-kernel-unit.ps1 \
		-BuildDirectory "$(BUILD)"
test-panic-qemu: check-tools $(BOOT_DIR)/BOOTX64.EFI $(ESP)/kernel.elf \
	$(BUILD)/panic-test-kernel.elf
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-kernel-panic.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)"
test-exceptions-qemu: check-tools $(BOOT_DIR)/BOOTX64.EFI \
	$(ESP)/kernel.elf $(BUILD)/gp-test-kernel.elf $(BUILD)/ud-test-kernel.elf \
	$(BUILD)/df-test-kernel.elf $(BUILD)/pf-test-kernel.elf \
	$(BUILD)/de-test-kernel.elf $(BUILD)/nx-test-kernel.elf \
	$(BUILD)/readonly-test-kernel.elf
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-kernel-exceptions.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)" -BuildDirectory "$(BUILD)"
toolchain-test:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-toolchain.ps1 \
		-ToolchainHome "$(TOOLCHAIN_HOME)"
validate-docs:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/validate-docs.ps1
clean:
	@powershell.exe -NoProfile -Command "if (Test-Path -LiteralPath '$(BUILD)') { Remove-Item -LiteralPath '$(BUILD)' -Recurse -Force }"
