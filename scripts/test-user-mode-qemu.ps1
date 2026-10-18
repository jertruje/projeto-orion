param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [string]$UserKernel = 'build\user-test-kernel.elf',
    [string]$BootLoader = 'build\esp\EFI\BOOT\BOOTX64.EFI',
    [string]$Application = 'build\application.elf',
    [int]$ObservationSeconds = 25
)

$ErrorActionPreference = 'Stop'
$resolvedBuild = [System.IO.Path]::GetFullPath($BuildDirectory)
$resolvedKernel = [System.IO.Path]::GetFullPath($UserKernel)
$resolvedLoader = [System.IO.Path]::GetFullPath($BootLoader)
$resolvedApplication = [System.IO.Path]::GetFullPath($Application)
foreach ($path in @($resolvedKernel, $resolvedLoader, $resolvedApplication)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required user-mode test input is missing: $path"
    }
}

$esp = Join-Path $resolvedBuild 'user-esp'
$bootDirectory = Join-Path $esp 'EFI\BOOT'
New-Item -ItemType Directory -Force -Path $bootDirectory | Out-Null
Copy-Item -LiteralPath $resolvedLoader `
    -Destination (Join-Path $bootDirectory 'BOOTX64.EFI') -Force
Copy-Item -LiteralPath $resolvedKernel `
    -Destination (Join-Path $esp 'kernel.elf') -Force
Copy-Item -LiteralPath $resolvedApplication `
    -Destination (Join-Path $esp 'application.elf') -Force

$testBoot = Join-Path $PSScriptRoot 'test-boot.ps1'
$expected = @(
    '[ORION][UEFI][INFO] kernel ELF validated',
    '[ORION][UEFI][INFO] kernel load segments reserved',
    '[ORION][UEFI][INFO] Optional application image loaded',
    '[ORION][BOOT][INFO] Kernel entry reached',
    '[ORION][CORE][INFO] UEFI boot services have exited',
    '[ORION][MEMORY][INFO] Physical allocate/release smoke test passed',
    '[ORION][MEMORY][INFO] Active CR3 map/write/lookup/unmap smoke test passed',
    '[ORION][DRIVER][INFO] 16550-compatible COM1 driver online',
    '[ORION][DRIVER][INFO] 16550-compatible COM1 lifecycle passed',
    '[ORION][TASK][INFO] Cooperative round-robin alternation passed',
    '[ORION][TIME][INFO] Hardware-backed PIT deadline timer test passed',
    'ORION independent CPL3 application',
    'ORION syscall ABI, info, and invalid-input checks passed',
    '[ORION][USER][INFO] CPL3 application syscalls and teardown passed',
    '[ORION][CORE][INFO] Minimal kernel is running'
)

& $testBoot -ToolchainHome $ToolchainHome `
    -BuildDirectory $resolvedBuild `
    -ObservationSeconds $ObservationSeconds `
    -EspDirectory $esp `
    -ExpectedMessages $expected `
    -RequireRunning `
    -SuccessMessage 'QEMU CPL3 syscall, ELF loader, and teardown integration: PASS'
