param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [int]$ObservationSeconds = 15,
    [string]$EspDirectory = '',
    [string]$DataDiskPath = '',
    [string]$StorageExpectedMessage = '',
    [string[]]$ExpectedMessages = @(
        '[ORION][UEFI][INFO] kernel ELF validated',
        '[ORION][UEFI][INFO] kernel load segments reserved',
        '[ORION][BOOT][INFO] Kernel entry reached',
        '[ORION][CORE][INFO] UEFI boot services have exited',
        '[ORION][MEMORY][INFO] Validated UEFI memory descriptors:',
        '[ORION][MEMORY][INFO] Physical allocate/release smoke test passed',
        '[ORION][MEMORY][INFO] Active CR3 map/write/lookup/unmap smoke test passed',
        '[ORION][DRIVER][INFO] 16550-compatible COM1 driver online',
        '[ORION][PCI][INFO] Device vendor:device 2156276160',
        '[ORION][PCI][INFO] Device vendor:device 305402129',
        '[ORION][PCI][INFO] Device vendor:device 2156269779',
        '[ORION][PCI][INFO] Device vendor:device 2156275992',
        '[ORION][PCI][INFO] Device vendor:device 2156276002',
        '[ORION][PCI][INFO] Device vendor:device 2156276016',
        '[ORION][DRIVER][WARN] PCI functions rejected by bounded inventory: 0',
        '[ORION][DRIVER][INFO] 16550-compatible COM1 lifecycle passed',
        '[ORION][TASK][INFO] Cooperative round-robin alternation passed',
        '[ORION][TIME][INFO] Hardware-backed PIT deadline timer test passed',
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
$dataDisk = ''
if (-not [string]::IsNullOrWhiteSpace($DataDiskPath)) {
    $dataDisk = [System.IO.Path]::GetFullPath($DataDiskPath)
    if (-not (Test-Path -LiteralPath $dataDisk -PathType Leaf)) {
        throw "Data disk image does not exist: $dataDisk"
    }
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
$argumentsList = @(
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
)
if (-not [string]::IsNullOrWhiteSpace($dataDisk)) {
    $driveData = "if=none,id=orion-data,format=raw,file=`"$dataDisk`""
    $argumentsList += @(
        '-device piix3-ide,id=orion-ide',
        "-drive $driveData",
        '-device ide-hd,drive=orion-data,bus=orion-ide.0'
    )
}
$arguments = $argumentsList -join ' '

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

$pciCountMatch = [regex]::Match(
    $output,
    '\[ORION\]\[DRIVER\]\[INFO\] PCI functions discovered on bus 0: (?<value>\d+)'
)
if (-not $pciCountMatch.Success) {
    throw 'Boot evidence is missing the PCI inventory count.'
}
$pciCount = [UInt32]::Parse($pciCountMatch.Groups['value'].Value)
if ([string]::IsNullOrWhiteSpace($dataDisk)) {
    if ($pciCount -ne 6 -or
        $output -notmatch [regex]::Escape(
            '[ORION][STORAGE][WARN] No supported primary-master ATA disk detected')) {
        throw "Storage-free q35 boot did not report the expected optional disk absence (PCI functions=$pciCount)."
    }
} else {
    if ($pciCount -le 6 -or
        $output -notmatch [regex]::Escape(
            '[ORION][STORAGE][INFO] ATA PIO block device online')) {
        throw "QEMU data disk was not discovered and initialized (PCI functions=$pciCount)."
    }
    $capacityMatch = [regex]::Match(
        $output,
        '\[ORION\]\[STORAGE\]\[INFO\] ATA primary-master sectors: (?<value>\d+)'
    )
    if (-not $capacityMatch.Success -or
        [UInt64]::Parse($capacityMatch.Groups['value'].Value) -eq 0) {
        throw 'ATA IDENTIFY did not report a non-zero disk capacity.'
    }
    if (-not [string]::IsNullOrWhiteSpace($StorageExpectedMessage) -and
        $output -notmatch [regex]::Escape($StorageExpectedMessage)) {
        throw "Storage boot evidence missing expected message: $StorageExpectedMessage"
    }
}

if ($ExpectedMessages -contains
    '[ORION][TIME][INFO] Hardware-backed PIT deadline timer test passed') {
    $latenessMatch = [regex]::Match(
        $output,
        '\[ORION\]\[TIME\]\[INFO\] One-shot timer maximum lateness ns: (?<value>\d+)'
    )
    $quantumMatch = [regex]::Match(
        $output,
        '\[ORION\]\[TIME\]\[INFO\] PIT nominal quantum ns: (?<value>\d+)'
    )
    $preemptionTicksMatch = [regex]::Match(
        $output,
        '\[ORION\]\[SCHED\]\[INFO\] PIT timer interrupts: (?<value>\d+)'
    )
    $timekeepingTicksMatch = [regex]::Match(
        $output,
        '\[ORION\]\[TIME\]\[INFO\] PIT IRQ0 ticks in timekeeping test: (?<value>\d+)'
    )
    if (-not $latenessMatch.Success -or -not $quantumMatch.Success -or
        -not $preemptionTicksMatch.Success -or -not $timekeepingTicksMatch.Success) {
        throw 'QEMU timekeeping evidence is missing numeric timing counters.'
    }
    $lateness = [UInt64]::Parse($latenessMatch.Groups['value'].Value)
    $quantum = [UInt64]::Parse($quantumMatch.Groups['value'].Value)
    $preemptionTicks = [UInt64]::Parse($preemptionTicksMatch.Groups['value'].Value)
    $timekeepingTicks = [UInt64]::Parse($timekeepingTicksMatch.Groups['value'].Value)
    if ($lateness -gt 200000000 -or $quantum -lt 9000000 -or
        $quantum -gt 11000000 -or $timekeepingTicks -le $preemptionTicks) {
        throw "QEMU time metrics outside documented limits: lateness=${lateness}ns, quantum=${quantum}ns, ticks=$preemptionTicks->$timekeepingTicks."
    }
    Write-Output "PIT nominal tick quantum: $quantum ns; measured one-shot lateness: $lateness ns."
    Write-Output "PIT ticks continued after scheduler preemption testing: $preemptionTicks -> $timekeepingTicks."
}

Write-Output $SuccessMessage
