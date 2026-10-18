param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [int]$ObservationSeconds = 15
)

$ErrorActionPreference = 'Stop'
$testBoot = Join-Path $PSScriptRoot 'test-boot.ps1'
& $testBoot `
    -ToolchainHome $ToolchainHome `
    -BuildDirectory $BuildDirectory `
    -ObservationSeconds $ObservationSeconds `
    -ExpectedMessages @(
        '[ORION][SCHED][INFO] Timer preemption test passed'
    ) `
    -RequireRunning `
    -SuccessMessage 'QEMU single-core timer preemption: PASS'

$output = Get-Content -Raw -LiteralPath (Join-Path $BuildDirectory 'qemu-serial.log')

function Get-LoggedCounter {
    param([string]$Label)

    $pattern = '\[ORION\]\[SCHED\]\[INFO\] ' +
        [regex]::Escape($Label) + ': (?<value>\d+)'
    $match = [regex]::Match($output, $pattern)
    if (-not $match.Success) {
        throw "Missing scheduler metric in QEMU serial trace: $Label"
    }
    return [UInt64]::Parse($match.Groups['value'].Value)
}

$timerTicks = Get-LoggedCounter 'PIT timer interrupts'
$preemptions = Get-LoggedCounter 'Timer-driven preemptions'
$workerZero = Get-LoggedCounter 'CPU-bound worker 0 iterations'
$workerOne = Get-LoggedCounter 'CPU-bound worker 1 iterations'
$cooperativeYields = Get-LoggedCounter 'Cooperative yields in preemption test'
$handlerSamples = Get-LoggedCounter 'No-switch timer handler samples'
$handlerCycles = Get-LoggedCounter 'Mean timer handler TSC cycles'
$contextSwitches = Get-LoggedCounter 'Context benchmark switches'
$handoffCycles = Get-LoggedCounter 'Mean scheduler handoff TSC cycles'

if ($timerTicks -eq 0 -or $preemptions -eq 0 -or
    $workerZero -lt 1000000 -or $workerOne -lt 1000000 -or
    $cooperativeYields -ne 0 -or $handlerSamples -eq 0 -or
    $handlerCycles -eq 0 -or $contextSwitches -eq 0 -or $handoffCycles -eq 0) {
    throw "Invalid preemption evidence: ticks=$timerTicks, preemptions=$preemptions, " +
        "worker0=$workerZero, worker1=$workerOne, yields=$cooperativeYields."
}

Write-Output "PIT IRQ0 delivered $timerTicks ticks; $preemptions timer-driven preemptions."
Write-Output 'Two CPU-bound workers each completed at least 1,000,000 iterations with zero yields.'
Write-Output "$handlerSamples no-switch timer handlers and $contextSwitches scheduler handoffs were measured with virtual TSC."
Write-Output 'QEMU CPU count: 1 (SMP intentionally not enabled).'
