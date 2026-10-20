# Orion OS Kernel Build Script
# Compiles all kernel source files and links them into a kernel.elf executable

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

# Toolchain paths
$toolchainHome = Join-Path $env:LOCALAPPDATA 'orion-msys64\msys64'
$bin = Join-Path $toolchainHome 'ucrt64\bin'
$clang = Join-Path $bin 'clang.exe'
$linker = Join-Path $bin 'ld.lld.exe'

# Verify required tools exist
$required = @($clang, $linker)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missing.Count -gt 0) {
    throw "Build cannot proceed; missing required executables: $($missing -join ', ')'"  
}

# Output directory
$buildDir = Join-Path $root 'build'
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Compilation flags (from Makefile KERNEL_CFLAGS and test-toolchain.ps1)
$compileFlags = @(
    '--target=x86_64-elf',
    '-Iinclude',
    '-Ikernel',
    '-Ikernel/console',
    '-Ikernel/log',
    '-Ikernel/panic',
    '-std=c11',
    '-O2',
    '-Wall',
    '-Wextra',
    '-Werror',
    '-ffreestanding',
    '-fno-stack-protector',
    '-fno-pic',
    '-fno-builtin',
    '-mno-red-zone',
    '-mcmodel=small',
    '-g'
)

# Linking flags (from test-toolchain.ps1)
$linkerArgs = @(
    '-m', 'elf_x86_64',
    '-T', (Join-Path $root 'kernel/linker.ld'),
    '--no-undefined',
    '--build-id=none',
    '-o', (Join-Path $buildDir 'kernel.elf')
)

# List of source files to compile (based on KERNEL_OBJECTS in Makefile)
$sourceFiles = @(
    'kernel/arch/x86_64/entry.S',
    'kernel/core/main.c',
    'kernel/console/serial.c',
    'kernel/log/log.c',
    'kernel/panic/panic.c',
    'kernel/arch/x86_64/cpu_state.S',
    'kernel/arch/x86_64/fatal.S',
    'kernel/fault/report.c',
    'kernel/arch/x86_64/gdt.c',
    'kernel/arch/x86_64/idt.c',
    'kernel/arch/x86_64/interrupts.S',
    'kernel/arch/x86_64/interrupt_state.S',
    'kernel/arch/x86_64/exceptions.c',
    'kernel/mm/memory_map.c',
    'kernel/mm/physical.c',
    'kernel/mm/page_tables.c',
    'kernel/mm/active_page_tables.c',
    'kernel/mm/user_space.c',
    'kernel/drivers/core/driver.c',
    'kernel/drivers/bus/pci.c',
    'kernel/drivers/platform/uart16550.c',
    'kernel/arch/x86_64/context_switch.S',
    'kernel/task/process.c',
    'kernel/task/thread.c',
    'kernel/task/scheduler.c',
    'kernel/time/timer.c',
    'kernel/security/capability.c',
    'kernel/ipc/channel.c',
    'kernel/time/clocksource.c',
    'kernel/arch/x86_64/timer.c',
    'kernel/power/acpi.c',
    'kernel/storage/block.c',
    'kernel/drivers/storage/ata_pio.c',
    'kernel/fs/orionfs.c',
    'kernel/vfs/vfs.c',
    'kernel/net/network.c',
    'kernel/drivers/net/ne2k_pci.c'
)

# Compile each source file to an object file
Write-Host "Compiling source files..."
$objectFiles = @()
foreach ($sourceFile in $sourceFiles) {
    # Get the relative path from kernel/ to determine the object file name
    $relativePath = $sourceFile.Substring('kernel/'.Length)
    # Replace path separators with underscores and change extension to .o
    $objectFileName = $relativePath -replace '[\\/]', '_' -replace '\.[cS]$', '.o'
    $objectFile = Join-Path $buildDir $objectFileName
    
    Write-Host "Compiling $sourceFile -> $objectFile"
    
    $compileArguments = $compileFlags + @('-c', $sourceFile, '-o', $objectFile)
    & $clang @compileArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed for $sourceFile"
    }
    
    $objectFiles += $objectFile
}# Link all object files together
Write-Host "Linking object files into kernel.elf..."
$linkArguments = $linkerArgs + $objectFiles
& $linker @linkArguments
if ($LASTEXITCODE -ne 0) {
    throw "Linking failed"
}

Write-Host "Kernel build successful: $buildDir"
