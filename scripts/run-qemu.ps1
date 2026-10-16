param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build'
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ToolchainHome)) {
    $ToolchainHome = Join-Path $env:LOCALAPPDATA 'orion-msys64\msys64'
}

$qemu = Join-Path $ToolchainHome 'ucrt64\bin\qemu-system-x86_64.exe'
$code = Join-Path $ToolchainHome 'ucrt64\share\qemu\edk2-x86_64-code.fd'
$varsTemplate = Join-Path $ToolchainHome 'ucrt64\share\qemu\edk2-i386-vars.fd'
$vars = Join-Path $BuildDirectory 'OVMF_VARS.fd'
$esp = Join-Path (Get-Location).Path (Join-Path $BuildDirectory 'esp')

foreach ($path in @($qemu, $code, $varsTemplate, (Join-Path $esp 'EFI\BOOT\BOOTX64.EFI'), (Join-Path $esp 'kernel.elf'))) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required QEMU boot file is missing: $path. Run mingw32-make build first."
    }
}

if (-not (Test-Path -LiteralPath $vars)) {
    Copy-Item -LiteralPath $varsTemplate -Destination $vars
}

& $qemu `
    -machine q35 `
    -m 256M `
    -smp 1 `
    -drive "if=pflash,format=raw,unit=0,readonly=on,file=$code" `
    -drive "if=pflash,format=raw,unit=1,file=$vars" `
    -drive "format=raw,file=fat:rw:$esp" `
    -display none `
    -serial stdio `
    -monitor none `
    -no-reboot `
    -no-shutdown

exit $LASTEXITCODE
