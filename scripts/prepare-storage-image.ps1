param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [string]$ImagePath = 'build\orionfs-test.raw',
    [UInt64]$SizeMiB = 16,
    [switch]$Format,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
if (-not $Format) {
    throw 'Preparing this image formats it as ORIONFS. Pass -Format explicitly.'
}
if ($SizeMiB -lt 1 -or $SizeMiB -gt 2048) {
    throw 'Image size must be between 1 and 2048 MiB.'
}
if ([string]::IsNullOrWhiteSpace($ToolchainHome)) {
    $ToolchainHome = Join-Path $env:LOCALAPPDATA 'orion-msys64\msys64'
}
$qemuImg = Join-Path $ToolchainHome 'ucrt64\bin\qemu-img.exe'
$formatter = Join-Path $BuildDirectory 'orionfs-image.exe'
if (-not (Test-Path -LiteralPath $qemuImg -PathType Leaf) -or
    -not (Test-Path -LiteralPath $formatter -PathType Leaf)) {
    throw 'Required qemu-img or ORIONFS formatter is missing; build prepare-storage-tool first.'
}
$resolvedImage = [System.IO.Path]::GetFullPath($ImagePath)
if (Test-Path -LiteralPath $resolvedImage) {
    if ((Get-Item -LiteralPath $resolvedImage).PSIsContainer) {
        throw "Image path is a directory: $resolvedImage"
    }
    if (-not $Force) {
        throw "Image already exists and will not be overwritten without -Force: $resolvedImage"
    }
    Write-Warning "Explicitly replacing and formatting image: $resolvedImage"
    Remove-Item -LiteralPath $resolvedImage -Force
} else {
    Write-Output "Creating a new disposable raw test image: $resolvedImage"
}

$parent = Split-Path -Parent $resolvedImage
if (-not [string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}
& $qemuImg create -f raw $resolvedImage "$($SizeMiB)M"
if ($LASTEXITCODE -ne 0) {
    throw "qemu-img failed with exit code $LASTEXITCODE."
}
Write-Warning "Formatting $resolvedImage as ORIONFS v1; existing contents are not preserved."
& $formatter format $resolvedImage
if ($LASTEXITCODE -ne 0) {
    throw "ORIONFS formatter failed with exit code $LASTEXITCODE."
}
