param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [string]$ImagePath = 'build\orionfs-test.raw',
    [string]$StorageKernel = 'build\storage-test-kernel.elf',
    [string]$BootLoader = 'build\esp\EFI\BOOT\BOOTX64.EFI',
    [int]$ObservationSeconds = 15,
    [switch]$AllowWrite
)

$ErrorActionPreference = 'Stop'
if (-not $AllowWrite) {
    throw 'The test writes a proof file to the disk image. Pass -AllowWrite explicitly.'
}
if ([string]::IsNullOrWhiteSpace($ToolchainHome)) {
    $ToolchainHome = Join-Path $env:LOCALAPPDATA 'orion-msys64\msys64'
}
$resolvedImage = [System.IO.Path]::GetFullPath($ImagePath)
$resolvedKernel = [System.IO.Path]::GetFullPath($StorageKernel)
$resolvedLoader = [System.IO.Path]::GetFullPath($BootLoader)
foreach ($path in @($resolvedImage, $resolvedKernel, $resolvedLoader)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required storage integration input is missing: $path"
    }
}
$storageEsp = Join-Path ([System.IO.Path]::GetFullPath($BuildDirectory)) 'storage-esp'
$bootDirectory = Join-Path $storageEsp 'EFI\BOOT'
New-Item -ItemType Directory -Force -Path $bootDirectory | Out-Null
Copy-Item -LiteralPath $resolvedLoader `
    -Destination (Join-Path $bootDirectory 'BOOTX64.EFI') -Force
Copy-Item -LiteralPath $resolvedKernel `
    -Destination (Join-Path $storageEsp 'kernel.elf') -Force

$testBoot = Join-Path $PSScriptRoot 'test-boot.ps1'
$commonArguments = @{
    ToolchainHome = $ToolchainHome
    BuildDirectory = $BuildDirectory
    ObservationSeconds = $ObservationSeconds
    EspDirectory = $storageEsp
    DataDiskPath = $resolvedImage
    ExpectedMessages = @(
        '[ORION][UEFI][INFO] kernel ELF validated',
        '[ORION][UEFI][INFO] kernel load segments reserved',
        '[ORION][BOOT][INFO] Kernel entry reached',
        '[ORION][CORE][INFO] UEFI boot services have exited',
        '[ORION][DRIVER][INFO] 16550-compatible COM1 driver online',
        '[ORION][DRIVER][WARN] PCI functions rejected by bounded inventory: 0',
        '[ORION][STORAGE][INFO] ATA PIO block device online',
        '[ORION][TIME][INFO] Hardware-backed PIT deadline timer test passed',
        '[ORION][CORE][INFO] Minimal kernel is running'
    )
}

Write-Output 'Storage pass 1: boot, create the proof file, flush, and unmount.'
& $testBoot @commonArguments `
    -StorageExpectedMessage '[ORION][STORAGE][INFO] Persistence proof committed; reboot to verify' `
    -RequireRunning `
    -SuccessMessage 'QEMU ATA PIO filesystem write: PASS'

Write-Output 'Storage pass 2: start a fresh QEMU process and verify disk contents.'
& $testBoot @commonArguments `
    -StorageExpectedMessage '[ORION][STORAGE][INFO] Persistence proof recovered across QEMU boots' `
    -RequireRunning `
    -SuccessMessage 'QEMU ATA PIO persistence across boots: PASS'
