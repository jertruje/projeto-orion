$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$requiredFiles = @(
    'docs\requirements.md',
    'docs\architecture-options.md',
    'docs\threat-model.md',
    'docs\success-metrics.md',
    'docs\recovery-compatibility.md',
    'docs\linux-comparison.md',
    'docs\decision-log.md',
    'docs\risk-register.md',
    'docs\traceability.md',
    'docs\research\phase-2-evidence.md',
    'docs\research\evidence-register.md',
    'docs\research\linux-limitations.md',
    'docs\research\architecture-comparison.md',
    'docs\research\security-analysis.md',
    'docs\research\performance-analysis.md',
    'docs\research\driver-and-hardware-compatibility.md',
    'docs\research\failure-recovery.md',
    'docs\research\benchmark-plan.md',
    'docs\research\research-gaps.md',
    'docs\research\phase2-conclusions.md',
    'docs\boot-contract.md',
    'docs\kernel-core.md',
    'docs\kernel\core-structure.md',
    'docs\kernel\initialization-sequence.md',
    'docs\kernel\logging.md',
    'docs\kernel\panic-handling.md',
    'docs\kernel\module-contracts.md',
    'docs\kernel\invariants.md',
    'docs\kernel\testing.md',
    'docs\kernel\x86_64-cpu-state.md',
    'docs\kernel\gdt.md',
    'docs\kernel\idt.md',
    'docs\kernel\exception-handling.md',
    'docs\kernel\interrupt-frame.md',
    'docs\kernel\interrupt-state.md',
    'docs\kernel\interrupt-controller.md',
    'docs\kernel\interrupt-debugging.md',
    'docs\memory\memory-model.md',
    'docs\memory\boot-memory-map.md',
    'docs\memory\physical-memory-manager.md',
    'docs\memory\virtual-memory-manager.md',
    'docs\memory\page-tables.md',
    'docs\memory\virtual-address-layout.md',
    'docs\memory\page-faults.md',
    'docs\memory\concurrency-and-safety.md',
    'docs\memory\testing.md',
    'docs\tasking\process-model.md',
    'docs\tasking\thread-model.md',
    'docs\tasking\thread-lifecycle.md',
    'docs\tasking\context-switch.md',
    'docs\tasking\scheduler.md',
    'docs\tasking\stack-management.md',
    'docs\tasking\concurrency-model.md',
    'docs\tasking\testing.md',
    'docs\tasking\scheduling-policy.md',
    'docs\tasking\preemption.md',
    'docs\tasking\timer.md',
    'docs\tasking\synchronization.md',
    'docs\tasking\multicore-architecture.md',
    'docs\tasking\cpu-affinity.md',
    'docs\tasking\scheduler-metrics.md',
    'docs\tasking\scheduler-testing.md',
    'docs\security\threat-model.md',
    'docs\security\capability-model.md',
    'docs\security\object-types-and-rights.md',
    'docs\security\capability-lifecycle.md',
    'docs\security\privilege-boundaries.md',
    'docs\security\address-space-isolation.md',
    'docs\security\security-testing.md',
    'docs\ipc\architecture.md',
    'docs\ipc\message-format.md',
    'docs\ipc\channel-lifecycle.md',
    'docs\ipc\security-and-capabilities.md',
    'docs\ipc\synchronization.md',
    'docs\ipc\resource-limits.md',
    'docs\ipc\testing.md',
    'docs\faults\fault-model.md',
    'docs\faults\exception-handling.md',
    'docs\faults\panic-policy.md',
    'docs\faults\process-recovery.md',
    'docs\faults\service-supervision.md',
    'docs\faults\diagnostics.md',
    'docs\faults\testing.md',
    'docs\drivers\architecture.md',
    'docs\drivers\driver-lifecycle.md',
    'docs\drivers\pci-enumeration.md',
    'docs\drivers\hardware-access.md',
    'docs\drivers\interrupts.md',
    'docs\drivers\dma-policy.md',
    'docs\drivers\supported-devices.md',
    'docs\drivers\testing.md',
    'docs\time\clocksource.md',
    'docs\time\timer-api.md',
    'docs\time\scheduler-integration.md',
    'docs\time\precision-and-jitter.md',
    'docs\power\acpi-support.md',
    'docs\power\power-states.md',
    'docs\power\failure-handling.md',
    'docs\power\testing.md',
    'docs\time\clocksource.md',
    'docs\time\timer-api.md',
    'docs\time\scheduler-integration.md',
    'docs\time\precision-and-jitter.md',
    'docs\power\acpi-support.md',
    'docs\power\power-states.md',
    'docs\power\failure-handling.md',
    'docs\power\testing.md',
    'kernel\security\capability.h',
    'kernel\security\capability.c',
    'kernel\ipc\channel.h',
    'kernel\ipc\channel.c',
    'kernel\fault\report.h',
    'kernel\fault\report.c',
    'kernel\drivers\core\driver.h',
    'kernel\drivers\core\driver.c',
    'kernel\drivers\bus\pci.h',
    'kernel\drivers\bus\pci.c',
    'kernel\drivers\platform\uart16550.h',
    'kernel\drivers\platform\uart16550.c',
    'kernel\time\clocksource.h',
    'kernel\time\clocksource.c',
    'kernel\time\timer.h',
    'kernel\time\timer.c',
    'kernel\time\clocksource.h',
    'kernel\time\clocksource.c',
    'kernel\time\timer.h',
    'kernel\time\timer.c',
    'tests\kernel\task\task_test.c',
    'tests\kernel\ipc\ipc_test.c',
    'tests\kernel\fault\fault_test.c',
    'tests\kernel\drivers\drivers_test.c',
    'tests\kernel\time\time_test.c',
    'tests\kernel\time\time_test.c',
    'kernel\net\network.h',
    'kernel\net\network.c',
    'kernel\drivers\net\ne2k_pci.h',
    'kernel\drivers\net\ne2k_pci.c',
    'tests\kernel\net\network_test.c',
    'scripts\test-network-qemu.ps1',
    'scripts\test-kernel-preemption.ps1',
    'docs\phase-status.md',
    'docs\phase-status-11-20.md',
    'docs\phase-status-21-26.md',
    'docs\phase-gates.md',
    'docs\storage\filesystem.md',
    'docs\network\networking.md',
    'docs\toolchain.md',
    'docs\development-environment.md',
    'docs\language-decisions.md',
    'tests\toolchain\README.md',
    'tests\toolchain\hello.c',
    'tests\toolchain\linker.ld',
    'scripts\test-toolchain.ps1',
    'tests\kernel\log_test.c',
    'tests\kernel\panic_test.c',
    'tests\kernel\panic_qemu_main.c',
    'scripts\test-kernel-unit.ps1',
    'scripts\test-kernel-panic.ps1',
    'scripts\test-kernel-exceptions.ps1',
    'scripts\test-kernel-tasking.ps1',
    'kernel\arch\x86_64\interrupt_state.h',
    'kernel\arch\x86_64\interrupt_state.S',
    'docs\architecture\system-overview.md',
    'docs\architecture\kernel-boundaries.md',
    'docs\architecture\component-model.md',
    'docs\architecture\security-architecture.md',
    'docs\architecture\ipc-design.md',
    'docs\architecture\memory-and-concurrency.md',
    'docs\architecture\scheduling-design.md',
    'docs\architecture\driver-model.md',
    'docs\architecture\failure-recovery.md',
    'docs\architecture\boot-sequence.md',
    'docs\architecture\interfaces-and-contracts.md',
    'docs\architecture\architecture-risks.md',
    'docs\architecture\phase-dependencies.md'
)

foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required document is missing: $relativePath"
    }
    if ((Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Required document is empty: $relativePath"
    }
}

$requirements = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\requirements.md')
$traceability = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\traceability.md')
$requirementIds = [regex]::Matches($requirements, 'OR-[A-Z]+-\d+') |
    ForEach-Object { $_.Value } |
    Select-Object -Unique
foreach ($id in $requirementIds) {
    if ($traceability -notmatch [regex]::Escape($id)) {
        throw "Requirement has no traceability row: $id"
    }
}

$phases = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\phase-status.md')
foreach ($phase in 1..10) {
    if ($phases -notmatch "(?m)^\| $phase\s+") {
        throw "Phase status row is missing for phase $phase"
    }
}

$laterPhases = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\phase-status-11-20.md')
foreach ($phase in 11..20) {
    if ($laterPhases -notmatch "(?m)^\| $phase\s+") {
        throw "Phase status row is missing for phase $phase"
    }

    if ($laterPhases -notmatch '(?m)^\| 13\s+— Supervisor de falhas \| PARCIAL: REGISTRO E POLÍTICA FAIL-STOP') {
        throw 'Phase 13 must remain explicitly partial until service recovery exists.'
    }
    if ($laterPhases -notmatch '(?m)^\| 14\s+— Framework de drivers \| PARCIAL: REGISTRO, PCI E UART') {
        throw 'Phase 14 must remain explicitly partial until broader device support exists.'
    }
    if ($laterPhases -notmatch '(?m)^\| 15\s+— Temporizadores e energia \| PARCIAL: PIT CLOCKSOURCE E DEADLINE QUEUE') {
        throw 'Phase 15 must remain explicitly partial until scheduler and power support exist.'
    }
    if ($laterPhases -notmatch '(?m)^\| 16\s+— Armazenamento e filesystem \| PARCIAL: ATA PIO, ORIONFS E VFS INTERNO') {
        throw 'Phase 16 must remain scoped to the tested ATA/ORIONFS implementation.'
    }
    if ($laterPhases -notmatch '(?m)^\| 17\s+— Rede \| PARCIAL: NE2000/PIO, IPv4, ICMP E UDP') {
        throw 'Phase 17 must remain scoped to tested NE2000/IPv4/ICMP/UDP support.'
    }
}

$finalPhases = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\phase-status-21-26.md')
foreach ($phase in 21..26) {
    if ($finalPhases -notmatch "(?m)^\| $phase\s+") {
        throw "Final-batch status row is missing for phase $phase"
    }
}

$decisionLog = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\decision-log.md')
$adrDefinitions = [regex]::Matches($decisionLog, '(?m)^### (ADR-\d{4})\b') |
    ForEach-Object { $_.Groups[1].Value }
if ($adrDefinitions.Count -eq 0) {
    throw 'No formal ADR definitions found.'
}
if (($adrDefinitions | Select-Object -Unique).Count -ne $adrDefinitions.Count) {
    throw 'Duplicate formal ADR identifiers found in decision-log.md.'
}

$traceability = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\traceability.md')
foreach ($adr in $adrDefinitions) {
    if ($traceability -notmatch [regex]::Escape($adr)) {
        throw "ADR is missing from traceability.md: $adr"
    }
}

$researchRoot = Join-Path $root 'docs\research'
$evidenceRegister = Get-Content -Raw -LiteralPath (Join-Path $researchRoot 'evidence-register.md')
$evidenceIds = [regex]::Matches($evidenceRegister, '(?m)^### (EVD-\d{3})\b') |
    ForEach-Object { $_.Groups[1].Value }
if ($evidenceIds.Count -lt 20) {
    throw "Phase 2 evidence register has fewer than 20 entries: $($evidenceIds.Count)"
}
if (($evidenceIds | Select-Object -Unique).Count -ne $evidenceIds.Count) {
    throw 'Duplicate evidence identifiers found in evidence-register.md.'
}

$researchFiles = Get-ChildItem -LiteralPath $researchRoot -Recurse -File -Filter '*.md'
foreach ($researchFile in $researchFiles) {
    $researchContent = Get-Content -Raw -LiteralPath $researchFile.FullName
    $referencedEvidence = [regex]::Matches($researchContent, '\bEVD-\d{3}\b') |
        ForEach-Object { $_.Value } |
        Select-Object -Unique
    foreach ($evidenceId in $referencedEvidence) {
        if ($evidenceIds -notcontains $evidenceId) {
            throw "Evidence reference has no register entry in $($researchFile.FullName): $evidenceId"
        }
    }
}

$comparison = Get-Content -Raw -LiteralPath (Join-Path $researchRoot 'architecture-comparison.md')
$comparisonRows = [regex]::Matches($comparison, '(?m)^\| (?:[1-9]|1\d|20)\. ').Count
if ($comparisonRows -ne 20) {
    throw "Architecture comparison must contain 20 evidence rows; found $comparisonRows."
}

$markdownFiles = Get-ChildItem -LiteralPath $root -Recurse -File -Filter '*.md' |
    Where-Object { $_.FullName -notmatch '[\\/](build|\.git)[\\/]' }
$mermaidCount = 0
$localLinkCount = 0
foreach ($markdownFile in $markdownFiles) {
    $content = Get-Content -Raw -LiteralPath $markdownFile.FullName
    $fences = [regex]::Matches($content, '(?m)^```')
    if (($fences.Count % 2) -ne 0) {
        throw "Unclosed Markdown code fence: $($markdownFile.FullName)"
    }
    $mermaidCount += [regex]::Matches($content, '(?m)^```mermaid\s*$').Count

    $links = [regex]::Matches($content, '(?<!!)\[[^\]]+\]\((?<target>[^)]+)\)')
    foreach ($link in $links) {
        $target = $link.Groups['target'].Value.Trim()
        if ($target -match '^(https?:|mailto:|#|//)') {
            continue
        }
        $target = ($target -split '#', 2)[0]
        if ([string]::IsNullOrWhiteSpace($target)) {
            continue
        }
        if ($target.StartsWith('<') -and $target.EndsWith('>')) {
            $target = $target.Substring(1, $target.Length - 2)
        }
        $target = [Uri]::UnescapeDataString($target)
        $linkedPath = Join-Path $markdownFile.DirectoryName $target
        if (-not (Test-Path -LiteralPath $linkedPath)) {
            throw "Broken local Markdown link in $($markdownFile.FullName): $target"
        }
        $localLinkCount++
    }
}
if ($mermaidCount -lt 4) {
    throw "Expected at least four architecture Mermaid diagrams; found $mermaidCount."
}

Write-Output "Document validation: PASS ($($requiredFiles.Count) files, $($requirementIds.Count) requirements, $($adrDefinitions.Count) ADR definitions, $($evidenceIds.Count) evidence entries, $comparisonRows comparison rows, $localLinkCount local links, $mermaidCount Mermaid diagrams, phases 1-26 present)"
