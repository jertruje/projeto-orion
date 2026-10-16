param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ToolchainHome)) {
    $ToolchainHome = Join-Path $env:LOCALAPPDATA 'orion-msys64\msys64'
}

$required = @(
    'ucrt64\bin\clang.exe',
    'ucrt64\bin\lld-link.exe',
    'ucrt64\bin\ld.lld.exe',
    'ucrt64\bin\nasm.exe',
    'ucrt64\bin\llvm-readobj.exe',
    'ucrt64\bin\llvm-nm.exe',
    'ucrt64\bin\qemu-system-x86_64.exe',
    'ucrt64\share\qemu\edk2-x86_64-code.fd',
    'ucrt64\share\qemu\edk2-i386-vars.fd'
)

$missing = @($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $ToolchainHome $_)) })
if ($missing.Count -gt 0) {
    Write-Error "Required toolchain files are missing under '$ToolchainHome': $($missing -join ', ')"
    exit 1
}

$bin = Join-Path $ToolchainHome 'ucrt64\bin'
$env:Path = "$bin;$env:Path"
$versions = @(
    @('clang', '--version'),
    @('lld-link', '--version'),
    @('ld.lld', '--version'),
    @('nasm', '-v'),
    @('qemu-system-x86_64', '--version')
)

foreach ($item in $versions) {
    $output = & (Join-Path $bin ($item[0] + '.exe')) $item[1] 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Tool version check failed: $($item[0])"
        exit 1
    }
    Write-Output (($output | Select-Object -First 1) -join '')
}
