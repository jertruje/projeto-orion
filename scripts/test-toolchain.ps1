param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($ToolchainHome)) {
    $ToolchainHome = Join-Path $env:LOCALAPPDATA 'orion-msys64\msys64'
}

$bin = Join-Path $ToolchainHome 'ucrt64\bin'
$clang = Join-Path $bin 'clang.exe'
$linker = Join-Path $bin 'ld.lld.exe'
$readobj = Join-Path $bin 'llvm-readobj.exe'
$nm = Join-Path $bin 'llvm-nm.exe'
$required = @($clang, $linker, $readobj, $nm)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missing.Count -gt 0) {
    throw "Toolchain test cannot run; missing required executables: $($missing -join ', ')"
}

$source = Join-Path $root 'tests\toolchain\hello.c'
$linkerScript = Join-Path $root 'tests\toolchain\linker.ld'
$outputRoot = Join-Path $root 'build\toolchain-test'
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "Toolchain probe source is missing: $source"
}
if (-not (Test-Path -LiteralPath $linkerScript -PathType Leaf)) {
    throw "Toolchain probe linker script is missing: $linkerScript"
}

function Invoke-CheckedTool {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$Description
    )

    $output = & $Executable @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($output) {
        $output | ForEach-Object { Write-Output $_ }
    }
    if ($exitCode -ne 0) {
        throw "$Description failed with exit code $exitCode."
    }
    return $output
}

$version = Invoke-CheckedTool -Executable $clang -Arguments @('--version') -Description 'Clang version check'
Write-Output "Compiler: $(($version | Select-Object -First 1) -join '')"

$compileFlags = @(
    '--target=x86_64-elf',
    '-std=c11',
    '-O2',
    '-Wall',
    '-Wextra',
    '-Werror',
    '-ffreestanding',
    '-fno-stack-protector',
    '-fno-pic',
    '-fno-builtin',
    '-mno-red-zone',
    '-mcmodel=small',
    '-fno-asynchronous-unwind-tables',
    '-fno-unwind-tables'
)
$hashes = @()

foreach ($label in @('first', 'repeat')) {
    $directory = Join-Path $outputRoot $label
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $object = Join-Path $directory 'hello.o'
    $executable = Join-Path $directory 'hello.elf'

    $compileArguments = $compileFlags + @('-c', $source, '-o', $object)
    Invoke-CheckedTool -Executable $clang -Arguments $compileArguments -Description "Clang $label compilation" | Out-Null

    $linkArguments = @(
        '-m', 'elf_x86_64',
        '-T', $linkerScript,
        '--no-undefined',
        '--build-id=none',
        '-o', $executable,
        $object
    )
    Invoke-CheckedTool -Executable $linker -Arguments $linkArguments -Description "LLD $label link" | Out-Null

    $inspection = Invoke-CheckedTool -Executable $readobj -Arguments @('--file-headers', '--program-headers', '--symbols', $executable) -Description "ELF $label inspection"
    $inspectionText = $inspection -join "`n"
    if ($inspectionText -notmatch 'Format:\s+elf64-x86-64' -or
        $inspectionText -notmatch 'Arch:\s+x86_64' -or
        $inspectionText -notmatch 'AddressSize:\s+64bit' -or
        $inspectionText -notmatch 'Type:\s+Executable' -or
        $inspectionText -notmatch 'Name:\s+orion_toolchain_probe') {
        throw "ELF $label inspection did not confirm ELF64 x86-64 executable and probe symbol."
    }

    $undefined = Invoke-CheckedTool -Executable $nm -Arguments @('--undefined-only', $executable) -Description "Undefined-symbol check $label"
    if (($undefined -join '').Trim().Length -ne 0) {
        throw "ELF $label unexpectedly contains undefined symbols: $($undefined -join ', ')"
    }

    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $executable).Hash.ToLowerInvariant()
    $hashes += $hash
    Write-Output "$label ELF: $executable"
    Write-Output "$label SHA-256: $hash"
    Write-Output "$label undefined symbols: none"
}

if ($hashes[0] -ne $hashes[1]) {
    throw "Repeated builds differ: $($hashes[0]) != $($hashes[1])"
}

Write-Output 'Toolchain probe: PASS (ELF64 x86-64, no undefined symbols, repeated SHA-256 matches)'
