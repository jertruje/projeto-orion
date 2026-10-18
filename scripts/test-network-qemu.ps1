param(
    [string]$ToolchainHome = $env:ORION_TOOLCHAIN_HOME,
    [string]$BuildDirectory = 'build',
    [string]$NetworkKernel = 'build\network-test-kernel.elf',
    [string]$BootLoader = 'build\esp\EFI\BOOT\BOOTX64.EFI',
    [int]$StartupTimeoutSeconds = 20,
    [int]$ResponseTimeoutMilliseconds = 5000
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ToolchainHome)) {
    $ToolchainHome = Join-Path $env:LOCALAPPDATA 'orion-msys64\msys64'
}
$qemu = Join-Path $ToolchainHome 'ucrt64\bin\qemu-system-x86_64.exe'
$code = Join-Path $ToolchainHome 'ucrt64\share\qemu\edk2-x86_64-code.fd'
$varsTemplate = Join-Path $ToolchainHome 'ucrt64\share\qemu\edk2-i386-vars.fd'
$resolvedKernel = [System.IO.Path]::GetFullPath($NetworkKernel)
$resolvedLoader = [System.IO.Path]::GetFullPath($BootLoader)
foreach ($path in @($qemu, $code, $varsTemplate, $resolvedKernel,
                    $resolvedLoader)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required network test input is missing: $path"
    }
}

$resolvedBuild = [System.IO.Path]::GetFullPath($BuildDirectory)
$esp = Join-Path $resolvedBuild 'network-esp'
$bootDirectory = Join-Path $esp 'EFI\BOOT'
$vars = Join-Path $resolvedBuild 'OVMF_NETWORK_TEST_VARS.fd'
$serial = Join-Path $resolvedBuild 'qemu-network-serial.log'
$stderr = Join-Path $resolvedBuild 'qemu-network-stderr.log'
New-Item -ItemType Directory -Force -Path $bootDirectory | Out-Null
Copy-Item -LiteralPath $resolvedLoader `
    -Destination (Join-Path $bootDirectory 'BOOTX64.EFI') -Force
Copy-Item -LiteralPath $resolvedKernel `
    -Destination (Join-Path $esp 'kernel.elf') -Force
Copy-Item -LiteralPath $varsTemplate -Destination $vars -Force
Remove-Item -LiteralPath $serial, $stderr -Force -ErrorAction SilentlyContinue

$driveCode = "if=pflash,format=raw,unit=0,readonly=on,file=`"$code`""
$driveVars = "if=pflash,format=raw,unit=1,file=`"$vars`""
$driveEsp = "format=raw,file=fat:rw:`"$esp`""
$netdev = 'user,id=net0,net=10.0.2.0/24,host=10.0.2.2,dhcpstart=10.0.2.15,hostfwd=udp:127.0.0.1:40170-10.0.2.15:40171'
$arguments = @(
    '-machine q35',
    '-m 256M',
    '-smp 1',
    "-drive $driveCode",
    "-drive $driveVars",
    "-drive $driveEsp",
    "-netdev $netdev",
    '-device ne2k_pci,netdev=net0,mac=52:54:00:12:34:56',
    '-display none',
    "-serial file:`"$serial`"",
    '-monitor none',
    '-no-reboot',
    '-no-shutdown'
) -join ' '

$process = Start-Process -FilePath $qemu -ArgumentList $arguments -PassThru `
    -NoNewWindow -RedirectStandardError $stderr
try {
    $readyDeadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    $ready = $false
    while ([DateTime]::UtcNow -lt $readyDeadline) {
        if ($process.HasExited) {
            throw "QEMU exited during network startup with code $($process.ExitCode)."
        }
        if (Test-Path -LiteralPath $serial) {
            $bootOutput = Get-Content -Raw -LiteralPath $serial
            if ($bootOutput -match [regex]::Escape(
                    '[ORION][NET][INFO] IPv4 10.0.2.15/24 configured; UDP echo port 40171 ready')) {
                $ready = $true
                break
            }
            if ($bootOutput -match '\[ORION\]\[PANIC\]') {
                throw "Kernel panic during network startup: $bootOutput"
            }
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) {
        $captured = if (Test-Path -LiteralPath $serial) {
            Get-Content -Raw -LiteralPath $serial
        } else {
            ''
        }
        throw "Network test kernel did not become ready within $StartupTimeoutSeconds seconds. Output: $captured"
    }

    $payload = [System.Text.Encoding]::UTF8.GetBytes('ORION_PHASE17_UDP_ECHO')
    $udp = [System.Net.Sockets.UdpClient]::new()
    try {
        $udp.Client.ReceiveTimeout = $ResponseTimeoutMilliseconds
        $null = $udp.Send($payload, $payload.Length, '127.0.0.1', 40170)
        $remote = [System.Net.IPEndPoint]::new(
            [System.Net.IPAddress]::Any, 0)
        $response = $udp.Receive([ref]$remote)
    } finally {
        $udp.Dispose()
    }
    if ($response.Length -ne $payload.Length) {
        throw "UDP echo response length mismatch: expected $($payload.Length), received $($response.Length)."
    }
    for ($index = 0; $index -lt $payload.Length; ++$index) {
        if ($response[$index] -ne $payload[$index]) {
            throw "UDP echo response differs at byte $index."
        }
    }

    $output = Get-Content -Raw -LiteralPath $serial
    foreach ($expected in @(
        '[ORION][NET][INFO] NE2000 PCI interface online; MAC available',
        '[ORION][NET][INFO] IPv4 10.0.2.15/24 configured; UDP echo port 40171 ready',
        '[ORION][NET][INFO] UDP echo request received',
        '[ORION][NET][INFO] UDP echo response transmitted',
        '[ORION][NET][INFO] Ping requests sent:',
        '[ORION][NET][INFO] Ping replies received:',
        '[ORION][NET][INFO] TX frames:',
        '[ORION][NET][INFO] RX frames:'
    )) {
        if ($output -notmatch [regex]::Escape($expected)) {
            throw "QEMU serial log is missing required network evidence: $expected"
        }
        if ($output -notmatch '\[ORION\]\[NET\]\[(INFO|WARN)\] ICMP ping to 10\.0\.2\.2 (succeeded|timed out|failed)') {
            throw 'QEMU serial log is missing the bounded ICMP ping diagnostic.'
        }
    }
    Write-Output 'QEMU NE2000 PCI + slirp UDP echo round trip: PASS'
    Write-Output "External host sent $($payload.Length) bytes to 127.0.0.1:40170 and received the byte-identical response."
    Write-Output $output
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
}
