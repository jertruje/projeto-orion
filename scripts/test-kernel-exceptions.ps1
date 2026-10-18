param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [int]$ObservationSeconds = 15
)

$ErrorActionPreference = 'Stop'
$kernelPath = Join-Path $BuildDirectory 'esp\kernel.elf'
$normalKernel = Join-Path $BuildDirectory 'kernel.elf'
$testBoot = Join-Path $PSScriptRoot 'test-boot.ps1'
$cases = @(
    @{
        Image = (Join-Path $BuildDirectory 'gp-test-kernel.elf')
        Expected = @(
            '[ORION][UEFI][INFO] kernel ELF validated',
            '[ORION][EXCEPTION][ERROR] Structured fault event ID 1',
            '[ORION][EXCEPTION][ERROR] CPU exception vector 13',
            '[ORION][EXCEPTION][ERROR] CPU error code 1034',
            '[ORION][EXCEPTION][ERROR] fatal CPU exception'
        )
    },
    @{
        Image = (Join-Path $BuildDirectory 'ud-test-kernel.elf')
        Expected = @(
            '[ORION][UEFI][INFO] kernel ELF validated',
            '[ORION][EXCEPTION][ERROR] Structured fault event ID 1',
            '[ORION][EXCEPTION][ERROR] CPU exception vector 6',
            '[ORION][EXCEPTION][ERROR] CPU error code 0',
            '[ORION][EXCEPTION][ERROR] fatal CPU exception'
        )
    },
    @{
        Image = (Join-Path $BuildDirectory 'df-test-kernel.elf')
        Expected = @(
            '[ORION][UEFI][INFO] kernel ELF validated',
            '[ORION][EXCEPTION][ERROR] Structured fault event ID 1',
            '[ORION][EXCEPTION][ERROR] CPU exception vector 8',
            '[ORION][EXCEPTION][ERROR] CPU error code 0',
            '[ORION][EXCEPTION][ERROR] fatal CPU exception'
        )
    },
    @{
        Image = (Join-Path $BuildDirectory 'pf-test-kernel.elf')
        Expected = @(
            '[ORION][UEFI][INFO] kernel ELF validated',
            '[ORION][EXCEPTION][ERROR] Structured fault event ID 1',
            '[ORION][EXCEPTION][ERROR] CPU exception vector 14',
            '[ORION][EXCEPTION][ERROR] CPU error code 0',
            '[ORION][EXCEPTION][ERROR] Page fault CR2 140737488351232',
            '[ORION][EXCEPTION][ERROR] Page fault: protection violation 0',
            '[ORION][EXCEPTION][ERROR] Page fault: write access 0',
            '[ORION][EXCEPTION][ERROR] Page fault: user access 0',
            '[ORION][EXCEPTION][ERROR] Page fault: reserved-bit violation 0',
            '[ORION][EXCEPTION][ERROR] Page fault: instruction fetch 0',
            '[ORION][EXCEPTION][ERROR] fatal CPU exception'
        )
    },
    @{
        Image = (Join-Path $BuildDirectory 'de-test-kernel.elf')
        Expected = @(
            '[ORION][UEFI][INFO] kernel ELF validated',
            '[ORION][EXCEPTION][ERROR] Structured fault event ID 1',
            '[ORION][EXCEPTION][ERROR] CPU exception vector 0',
            '[ORION][EXCEPTION][ERROR] CPU error code 0',
            '[ORION][EXCEPTION][ERROR] fatal CPU exception'
        )
    },
    @{
        Image = (Join-Path $BuildDirectory 'readonly-test-kernel.elf')
        Expected = @(
            '[ORION][UEFI][INFO] kernel ELF validated',
            '[ORION][EXCEPTION][ERROR] Structured fault event ID 1',
            '[ORION][EXCEPTION][ERROR] CPU exception vector 14',
            '[ORION][EXCEPTION][ERROR] CPU error code 3',
            '[ORION][EXCEPTION][ERROR] Page fault: protection violation 1',
            '[ORION][EXCEPTION][ERROR] Page fault: write access 1',
            '[ORION][EXCEPTION][ERROR] Page fault: user access 0',
            '[ORION][EXCEPTION][ERROR] Page fault: instruction fetch 0',
            '[ORION][EXCEPTION][ERROR] fatal CPU exception'
        )
    },
    @{
        Image = (Join-Path $BuildDirectory 'nx-test-kernel.elf')
        Expected = @(
            '[ORION][UEFI][INFO] kernel ELF validated',
            '[ORION][EXCEPTION][ERROR] Structured fault event ID 1',
            '[ORION][EXCEPTION][ERROR] CPU exception vector 14',
            '[ORION][EXCEPTION][ERROR] CPU error code 17',
            '[ORION][EXCEPTION][ERROR] Page fault: protection violation 1',
            '[ORION][EXCEPTION][ERROR] Page fault: write access 0',
            '[ORION][EXCEPTION][ERROR] Page fault: user access 0',
            '[ORION][EXCEPTION][ERROR] Page fault: instruction fetch 1',
            '[ORION][EXCEPTION][ERROR] fatal CPU exception'
        )
    }
)

foreach ($path in @($kernelPath, $normalKernel) + @($cases | ForEach-Object { $_.Image })) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Exception QEMU test input is missing: $path. Build the test target first."
    }
}

$originalKernel = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $normalKernel).Path)
try {
    foreach ($case in $cases) {
        Copy-Item -LiteralPath $case.Image -Destination $kernelPath -Force
        & $testBoot `
            -ToolchainHome $ToolchainHome `
            -BuildDirectory $BuildDirectory `
            -ObservationSeconds $ObservationSeconds `
            -ExpectedMessages $case.Expected `
            -SkipStandardHardwareChecks `
            -RequireRunning `
            -SuccessMessage "QEMU exception test passed: $($case.Image)"
    }
} finally {
    [System.IO.File]::WriteAllBytes((Resolve-Path -LiteralPath $normalKernel).Path,
                                   $originalKernel)
    Copy-Item -LiteralPath $normalKernel -Destination $kernelPath -Force
}

Write-Output 'QEMU controlled exception verification: PASS (DE, UD, GP, PF, DF, RO, NX)'
