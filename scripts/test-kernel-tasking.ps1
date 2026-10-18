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
        '[ORION][TASK][INFO] Cooperative round-robin alternation passed'
    ) `
    -RequireRunning `
    -SuccessMessage 'QEMU cooperative tasking boot: PASS'

$serialPath = Join-Path $BuildDirectory 'qemu-serial.log'
$output = Get-Content -Raw -LiteralPath $serialPath
$events = [regex]::Matches(
    $output,
    '\[ORION\]\[TASK\]\[INFO\] thread run id: (?<id>\d+)\r?\n' +
    '\[ORION\]\[TASK\]\[INFO\] thread run iteration: (?<step>\d+)')
if ($events.Count -ne 8) {
    throw "Expected 8 cooperative tasking trace events; found $($events.Count)."
}

for ($index = 0; $index -lt $events.Count; $index++) {
    $expectedId = if (($index % 2) -eq 0) { '2' } else { '3' }
    $expectedStep = [string][math]::Floor($index / 2)
    if ($events[$index].Groups['id'].Value -ne $expectedId -or
        $events[$index].Groups['step'].Value -ne $expectedStep) {
        throw "Unexpected cooperative scheduling trace at event $index."
    }
}

Write-Output 'QEMU round-robin alternation trace: PASS (2,3 repeated for 4 rounds)'
