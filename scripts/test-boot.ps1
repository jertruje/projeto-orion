param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [int]$ObservationSeconds = 15,
    [string]$EspDirectory = '',
    [string[]]$ExpectedMessages = @(
        '[ORION][UEFI][INFO] kernel ELF validated',
        '[ORION][UEFI][INFO] kernel load segments reserved',
        '[ORION][BOOT][INFO] Kernel entry reached',
        '[ORION][CORE][INFO] UEFI boot services have exited',
        '[ORION][MEMORY][INFO] Validated UEFI memory descriptors:',
        '[ORION][MEMORY][INFO] Physical allocate/release smoke test passed',
        '[ORION][MEMORY][INFO] Active CR3 map/write/lookup/unmap smoke test passed',
        '[ORION][TASK][INFO] Cooperative round-robin alternation passed',
        '[ORION][CORE][INFO] Minimal kernel is running'
    ),
    [switch]$RequireRunning,
    [string]$SuccessMessage = 'UEFI-to-kernel boot verification: PASS'
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ToolchainHome)) {
    $ToolchainHome = Join-Path $env:LOCALAPPDATA 'orion-msys64\msys64'
}

$qemu = Join-Path $ToolchainHome 'ucrt64\bin\qemu-system-x86_64.exe'
$code = Join-Path $ToolchainHome 'ucrt64\share\qemu\edk2-x86_64-code.fd'
$varsTemplate = Join-Path $ToolchainHome 'ucrt64\share\qemu\edk2-i386-vars.fd'
$vars = Join-Path $BuildDirectory 'OVMF_TEST_VARS.fd'
if ([string]::IsNullOrWhiteSpace($EspDirectory)) {
    $EspDirectory = Join-Path $BuildDirectory 'esp'
}
$esp = if ([System.IO.Path]::IsPathRooted($EspDirectory)) {
    $EspDirectory
} else {
    Join-Path (Get-Location).Path $EspDirectory
}
$serial = Join-Path $BuildDirectory 'qemu-serial.log'
$stderr = Join-Path $BuildDirectory 'qemu-stderr.log'

foreach ($path in @($qemu, $code, $varsTemplate, (Join-Path $esp 'EFI\BOOT\BOOTX64.EFI'), (Join-Path $esp 'kernel.elf'))) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required boot test file is missing: $path. Run mingw32-make build first."
    }
}

Copy-Item -LiteralPath $varsTemplate -Destination $vars -Force
Remove-Item -LiteralPath $serial, $stderr -Force -ErrorAction SilentlyContinue

$driveCode = "if=pflash,format=raw,unit=0,readonly=on,file=`"$code`""
$driveVars = "if=pflash,format=raw,unit=1,file=`"$vars`""
$driveEsp = "format=raw,file=fat:rw:`"$esp`""
$arguments = @(
    '-machine q35',
    '-m 256M',
    '-smp 1',
    "-drive $driveCode",
    "-drive $driveVars",
    "-drive $driveEsp",
    '-display none',
    "-serial file:`"$serial`"",
    '-monitor none',
    '-no-reboot',
    '-no-shutdown'
) -join ' '

$process = Start-Process -FilePath $qemu -ArgumentList $arguments -PassThru `
    -NoNewWindow -RedirectStandardError $stderr
$remainedRunning = $false
try {
    if (-not $process.WaitForExit($ObservationSeconds * 1000)) {
        $remainedRunning = $true
        Write-Output "QEMU remained running for $ObservationSeconds seconds; stopping this test instance."
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    } else {
        Write-Output "QEMU exited with code $($process.ExitCode)."
    }
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
}

if (Test-Path -LiteralPath $serial) {
    Get-Content -LiteralPath $serial
} else {
    Write-Error 'QEMU did not create the serial output log.'
}

if (Test-Path -LiteralPath $stderr) {
    $qemuErrors = Get-Content -LiteralPath $stderr
    if ($qemuErrors.Count -gt 0) {
        $qemuErrors
    }
}

if ($RequireRunning -and -not $remainedRunning) {
    throw 'QEMU exited before the configured observation interval.'
}

$output = if (Test-Path -LiteralPath $serial) { Get-Content -Raw -LiteralPath $serial } else { '' }
if ([string]::IsNullOrWhiteSpace($output)) {
    throw 'QEMU serial output log is empty; no boot or panic evidence was captured.'
}
if ($ExpectedMessages.Count -eq 0) {
    throw 'At least one expected serial message must be provided.'
}
foreach ($expected in $ExpectedMessages) {
    if ($output -notmatch [regex]::Escape($expected)) {
        throw "Boot evidence missing expected serial message: $expected"
    }
}

Write-Output $SuccessMessage
