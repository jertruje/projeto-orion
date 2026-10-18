param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [int]$ObservationSeconds = 15
)

$ErrorActionPreference = 'Stop'
$bootSource = Join-Path $BuildDirectory 'esp\EFI\BOOT\BOOTX64.EFI'
$kernelPath = Join-Path $BuildDirectory 'esp\kernel.elf'
$kernelSource = Join-Path $BuildDirectory 'panic-test-kernel.elf'

foreach ($source in @($bootSource, $kernelPath, $kernelSource)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Panic QEMU test input is missing: $source. Build the test target first."
    }
}

$originalKernel = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $kernelPath).Path)
$testBoot = Join-Path $PSScriptRoot 'test-boot.ps1'
try {
    Copy-Item -LiteralPath $kernelSource -Destination $kernelPath -Force
    & $testBoot `
        -ToolchainHome $ToolchainHome `
        -BuildDirectory $BuildDirectory `
        -ObservationSeconds $ObservationSeconds `
        -ExpectedMessages @(
            '[ORION][UEFI][INFO] kernel ELF validated',
            '[ORION][UEFI][INFO] kernel load segments reserved',
            '[ORION][PANIC][ERROR] Structured fault event ID 2',
            '[ORION][EXCEPTION][ERROR] CPU exception vector 3',
            '[ORION][EXCEPTION][INFO] Breakpoint handled; resuming interrupted code',
            '[ORION][TEST][INFO] Breakpoint handler returned to interrupted code',
            '[ORION][TEST][ERROR] QEMU panic path reached'
        ) `
        -SkipStandardHardwareChecks `
        -RequireRunning `
        -SuccessMessage 'QEMU panic-path verification: PASS'
} finally {
    [System.IO.File]::WriteAllBytes((Resolve-Path -LiteralPath $kernelPath).Path,
                                   $originalKernel)
}
