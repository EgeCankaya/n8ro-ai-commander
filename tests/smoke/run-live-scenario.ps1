<#
.SYNOPSIS
    Phase 1b in-engine live smoke on the OQ-6 scenario, with a paired commander-off control run.

.DESCRIPTION
    The offline harness (tests/live) proves the adapter, the envelope, and the validator against a
    live server. This proves the same thing INSIDE the engine, which is a different question: the
    snapshot comes from real component reads, Stage B runs against real entity state, and the Lua
    tier actually consumes the published order.

    OQ-6 resolved to oppint_red_interceptor / RedSu35_01, which lives in the shipped seed scenario
    "Mariana Shield". This script does NOT modify that scenario. It APPENDS a duplicate entry named
    "Mariana Shield AI" whose RedSu35_01 points at the commander-aware Tier-1 script, so:

      * no shipped record is edited, and the change is reversible by removing one array element;
      * the H1 pair is fair by construction - "Mariana Shield" is the commander-off control and
        "Mariana Shield AI" the commander-on run, from byte-identical initial conditions;
      * RedSu35_02 stays on stock Tier-1 logic in BOTH runs, giving a within-run control as well.

    Everything it touches in the release tree is backed up first and restored in a finally block,
    including on failure. The tree is left as it was found.

    KNOWN PRECONDITION: the headless host does not apply per-plugin config. Observed at the Phase 1b
    gate - a data/config/plugins/ai-commander.cfg carrying commander.enabled=true and
    commander.backend=local still produced "backend=stub enabled=false" in the startup log. Until
    that is resolved, the commander-on assertions below FAIL LOUDLY rather than silently measuring
    the stub backend, and the run must be driven from the UI host instead.

.PARAMETER ReleaseRoot
    The N8RO release tree. Defaults to N8RO_RELEASE_ROOT, then C:\N8RO.

.PARAMETER RunSeconds
    Sim seconds per run. The PRD's live smoke is a 10-minute run; 600 is the gate value. Shorter
    runs are useful for wiring checks but do not satisfy the gate.

.PARAMETER SkipControl
    Skip the commander-off control run (H1 then cannot be assessed).
#>
[CmdletBinding()]
param(
    [string]$ReleaseRoot = $(if ($env:N8RO_RELEASE_ROOT) { $env:N8RO_RELEASE_ROOT } else { "C:\N8RO" }),
    [int]$RunSeconds = 600,
    [switch]$SkipControl
)

$ErrorActionPreference = "Stop"
$repoRoot    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$seedPath    = Join-Path $ReleaseRoot "data\resources\seed\realistic_scenario_seed_data.json"
$missionDir  = Join-Path $ReleaseRoot "data\resources\missions"
$scriptSrc   = Join-Path $repoRoot "lua\ai_commander_interceptor.lua"
$scriptDst   = Join-Path $missionDir "ai_commander_interceptor.lua"
$doctrineSrc = Join-Path $repoRoot "data\doctrine.txt"
$doctrineDst = Join-Path $ReleaseRoot "data\doctrine.txt"
$orderLog    = Join-Path $ReleaseRoot "logs\ai-commander\orders.jsonl"

$stamp   = (Get-Date -Format "yyyyMMddTHHmmssZ")
$workDir = Join-Path ([System.IO.Path]::GetTempPath()) "aic-live"
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

$failures = New-Object System.Collections.Generic.List[string]
$checks = 0
function Assert-That {
    param([bool]$Condition, [string]$Description)
    $script:checks++
    if ($Condition) { Write-Host "  [PASS] $Description" }
    else { Write-Host "  [FAIL] $Description" -ForegroundColor Red; $script:failures.Add($Description) }
}

Write-Host "=== AI Entity Commander - Phase 1b live scenario smoke ==="
Write-Host "release root : $ReleaseRoot"
Write-Host "scenario     : Mariana Shield AI (additive duplicate of Mariana Shield)"
Write-Host "entity       : RedSu35_01  (RedSu35_02 stays on stock Tier-1 logic)"
Write-Host "run seconds  : $RunSeconds"

if (-not (Test-Path $seedPath))  { throw "No scenario seed at $seedPath" }
if (-not (Test-Path $scriptSrc)) { throw "No reference Tier-1 script at $scriptSrc" }

Write-Host "`n-- preflight --"
try {
    $tags = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -TimeoutSec 8
    Assert-That ($tags.models.Count -gt 0) "inference server is serving $($tags.models.Count) models"
} catch {
    Assert-That $false "inference server reachable at http://localhost:11434 ($($_.Exception.Message))"
}

$backup = Join-Path $workDir "seed-backup-$stamp.json"
Copy-Item $seedPath $backup -Force
Write-Host "  seed backed up to $backup"

$seededDoctrine = $false
$installedScript = $false

try {
    # -- 1. install the commander-aware Tier-1 script -------------------------------------------
    Write-Host "`n-- wiring --"
    if (-not (Test-Path $scriptDst)) { $installedScript = $true }
    Copy-Item $scriptSrc $scriptDst -Force
    Assert-That (Test-Path $scriptDst) "reference Tier-1 script installed into data/resources/missions"

    if (-not (Test-Path $doctrineDst)) {
        Copy-Item $doctrineSrc $doctrineDst -Force
        $seededDoctrine = $true
    }
    Assert-That (Test-Path $doctrineDst) "doctrine present at data/doctrine.txt (prompt.doctrinePath default)"

    # -- 2. append the AI scenario, without touching the shipped one ----------------------------
    $seed = Get-Content $seedPath -Raw | ConvertFrom-Json
    $source = $seed.entries | Where-Object { $_.name -eq "Mariana Shield" }
    if (-not $source) { throw "seed carries no 'Mariana Shield' scenario" }

    if ($seed.entries | Where-Object { $_.name -eq "Mariana Shield AI" }) {
        Write-Host "  'Mariana Shield AI' already present; replacing it"
        $seed.entries = @($seed.entries | Where-Object { $_.name -ne "Mariana Shield AI" })
    }

    # Deep copy through JSON so the duplicate shares no object with the shipped entry.
    $clone = $source | ConvertTo-Json -Depth 40 | ConvertFrom-Json
    $clone.name = "Mariana Shield AI"
    $clone.description = "Phase 1b commander-on variant of Mariana Shield. RedSu35_01 runs the " +
                         "commander-aware Tier-1 script; every other entity is unchanged."
    $commanded = $clone.entities | Where-Object { $_.entityName -eq "RedSu35_01" }
    if (-not $commanded) { throw "'Mariana Shield' carries no RedSu35_01" }
    $commanded.missionScriptPath = "resources/missions/ai_commander_interceptor.lua"

    $seed.entries = @($seed.entries) + @($clone)
    $seed | ConvertTo-Json -Depth 40 | Set-Content -Path $seedPath -Encoding utf8
    Assert-That $true "appended 'Mariana Shield AI' (shipped 'Mariana Shield' untouched)"

    function Invoke-Scenario {
        param([string]$Name, [int]$Seconds, [string]$Tag)
        $out = Join-Path $workDir "$Tag-$stamp.log"
        $runner = Join-Path $workDir "run-$Tag.cmd"
        @"
@echo off
call "$ReleaseRoot\setup.cmd" >nul 2>&1 || exit /b 1
cd /d "%N8RO_RELEASE%"
"%N8RO_RELEASE%\bin\n8ro-sim-local.exe" --scenario "$Name" --run-ms $($Seconds * 1000)
"@ | Set-Content -Path $runner -Encoding ascii
        Write-Host "  running '$Name' for $Seconds s -> $out"
        $proc = Start-Process -FilePath $runner -RedirectStandardOutput $out `
            -RedirectStandardError "$out.err" -PassThru -WindowStyle Hidden
        $deadline = (Get-Date).AddSeconds($Seconds + 90)
        while ((Get-Date) -lt $deadline -and -not $proc.HasExited) { Start-Sleep -Seconds 2 }
        Get-Process -Name 'n8ro-sim-local' -ErrorAction SilentlyContinue | Stop-Process -Force
        Start-Sleep -Seconds 2
        return (Get-Content $out -Raw)
    }

    if (Test-Path $orderLog) { Remove-Item $orderLog -Force -ErrorAction SilentlyContinue }

    # -- 3. commander-on run ---------------------------------------------------------------------
    Write-Host "`n-- commander-on run --"
    $log = Invoke-Scenario -Name "Mariana Shield AI" -Seconds $RunSeconds -Tag "commander-on"

    Assert-That ($log -match 'ai-commander: registered the aiCommander namespace') `
        "plugin loaded and registered its namespace"
    Assert-That ($log -match 'runtime-column probe pass') "AIC-ARCH-4 probe passed"

    # If this fails the run measured NOTHING - it measured the stub backend. Asserted rather than
    # assumed, because a green smoke over canned orders is worse than a red one.
    Assert-That ($log -match 'backend=local enabled=true') `
        ("commander is ON with the local backend - if this fails the headless host did not apply " +
         "per-plugin config and the run is void; drive it from the UI host instead")

    Assert-That ($log -match 'doctrine loaded from') `
        "doctrine block was loaded (a missing one degrades order quality silently)"

    # Order-log assertions. The log is the PRD's own record of what happened (AIC-DET-1), so the
    # smoke reads it rather than re-deriving from stdout.
    if (Test-Path $orderLog) {
        $records   = Get-Content $orderLog | ForEach-Object { $_ | ConvertFrom-Json }
        $accepted  = @($records | Where-Object { $_.event -eq 'order.accepted' })
        $rejected  = @($records | Where-Object { $_.event -eq 'order.rejected' })
        $requested = @($records | Where-Object { $_.event -eq 'order.requested' })

        Assert-That ($accepted.Count -gt 0) "at least one order was accepted ($($accepted.Count))"
        $rate = if ($requested.Count) { 100.0 * $accepted.Count / $requested.Count } else { 0 }
        Assert-That ($rate -ge 90) "acceptance rate >= 90 % (got $([math]::Round($rate,1)) %)"

        $postures = @($accepted | ForEach-Object { $_.order.posture } | Sort-Object -Unique)
        Assert-That ($postures.Count -ge 3) `
            "at least three distinct postures observed ($($postures -join ', '))"

        $latencies = @($accepted | ForEach-Object { $_.latencyMs } | Sort-Object)
        if ($latencies.Count) {
            $p95 = $latencies[[math]::Min($latencies.Count - 1, [int]($latencies.Count * 0.95))]
            Write-Host "  p95 order latency: $p95 ms"
        }

        # The first order of a run must complete rather than time out (PRD Phase 1b gate).
        $timeouts = @($records | Where-Object { $_.event -eq 'order.timeout' })
        Assert-That ($timeouts.Count -eq 0) `
            "no order timed out, including the first of the run ($($timeouts.Count) timeouts)"

        # Fratricide is a Stage-B reject reason. Rejections here are the control WORKING; what must
        # be zero is a fratricidal order that reached an entity, and none can, by construction.
        $fratricide = @($rejected | Where-Object { $_.reason -eq 'fratricide' })
        Write-Host "  Stage-B fratricide rejections: $($fratricide.Count) (rejections are the control working)"

        $byReason = $rejected | Group-Object reason | Sort-Object Count -Descending
        foreach ($group in $byReason) { Write-Host "  reject.$($group.Name): $($group.Count)" }
    } else {
        Assert-That $false "order log written to $orderLog"
    }

    Assert-That (-not ($log -match '\[ERROR\].*aiCommander')) "plugin emitted no ERROR-level line"

    # -- 4. commander-off control run --------------------------------------------------------------
    if (-not $SkipControl) {
        Write-Host "`n-- commander-off control run (H1 pair) --"
        $control = Invoke-Scenario -Name "Mariana Shield" -Seconds $RunSeconds -Tag "commander-off"
        Assert-That ($control -match 'Interceptor committed: RedSu35_01') `
            "control run flies RedSu35_01 on stock Tier-1 logic"
        Write-Host "  H1 is a JUDGEMENT, not an assertion: a domain reviewer compares the posture"
        Write-Host "  transitions in the two logs. Both are kept in $workDir for that review."
    }
}
finally {
    Write-Host "`n-- restoring the release tree --"
    Copy-Item $backup $seedPath -Force
    Write-Host "  seed restored from $backup"
    if ($installedScript -and (Test-Path $scriptDst)) {
        Remove-Item $scriptDst -Force; Write-Host "  removed the installed Tier-1 script"
    }
    if ($seededDoctrine -and (Test-Path $doctrineDst)) {
        Remove-Item $doctrineDst -Force; Write-Host "  removed the seeded doctrine"
    }
}

Write-Host "`n=== live scenario smoke summary ==="
Write-Host "checks : $checks"
Write-Host "failed : $($failures.Count)"
if ($failures.Count -gt 0) {
    Write-Host "`nFailures:" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "LIVE SCENARIO SMOKE PASS" -ForegroundColor Green
exit 0
