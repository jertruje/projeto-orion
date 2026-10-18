param(
    [string]$BuildDirectory = 'build'
)

$ErrorActionPreference = 'Stop'
$tests = @(
    @{ Path = (Join-Path $BuildDirectory 'boot-info-test.exe'); Arguments = @() },
    @{ Path = (Join-Path $BuildDirectory 'kernel-memory-test.exe'); Arguments = @() },
    @{ Path = (Join-Path $BuildDirectory 'page-tables-test.exe'); Arguments = @() },
    @{ Path = (Join-Path $BuildDirectory 'kernel-task-test.exe'); Arguments = @() },
    @{ Path = (Join-Path $BuildDirectory 'kernel-ipc-test.exe'); Arguments = @() },
    @{ Path = (Join-Path $BuildDirectory 'kernel-fault-test.exe'); Arguments = @() },
    @{ Path = (Join-Path $BuildDirectory 'kernel-log-test.exe'); Arguments = @() },
    @{ Path = (Join-Path $BuildDirectory 'kernel-panic-test.exe'); Arguments = @('normal') },
    @{ Path = (Join-Path $BuildDirectory 'kernel-panic-test.exe'); Arguments = @('wrapper') },
    @{ Path = (Join-Path $BuildDirectory 'kernel-panic-test.exe'); Arguments = @('defaults') },
    @{ Path = (Join-Path $BuildDirectory 'kernel-panic-test.exe'); Arguments = @('recursive') },
    @{ Path = (Join-Path $BuildDirectory 'kernel-panic-test.exe'); Arguments = @('log-failure') }
)

foreach ($test in $tests) {
    if (-not (Test-Path -LiteralPath $test.Path -PathType Leaf)) {
        throw "Kernel unit test executable is missing: $($test.Path)"
    }
    if ($test.Arguments.Count -eq 0) {
        & $test.Path
    } else {
        & $test.Path $test.Arguments[0]
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Kernel unit test failed: $($test.Path) $($test.Arguments -join ' ')"
    }
}

Write-Output 'Kernel unit tests: PASS (boot-info/init-state, memory, tasking, IPC, fault reports, logging, panic)'
