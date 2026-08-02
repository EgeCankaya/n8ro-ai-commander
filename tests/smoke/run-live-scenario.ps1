<#
.SYNOPSIS
    Phase 1b in-engine live smoke on the OQ-6 scenario, with a paired commander-off control run.

.DESCRIPTION
    The offline harness (tests/live) proves the adapter, the envelope, and the validator against a
    live server. This proves the same thing INSIDE the engine, which is a different question: the
    snapshot comes from real component reads, Stage B runs against real entity state, and the Lua
    tier actually consumes the published order.

    OQ-6 resolved to oppint_red_interceptor / RedSu35_01, in the shipped scenario "Mariana Shield".

    HOW THE COMMANDER GETS INTO THE SCENARIO. The first attempt appended a "Mariana Shield AI"
    entry to data/resources/seed/realistic_scenario_seed_data.json, on the belief that the seed JSON
    was the runtime source. It is not: the engine loads scenarios from binary DB records under
    data/db/N8roSimSchema/Profiles/Scenario/*.n8ro.instance (observed - the run failed with
    "cannot open file: .../Mariana Shield AI.n8ro.instance"). The seed JSON is an import source, and
    those records are compressed binary that no text edit can produce. Creating a scenario variant
    therefore needs the data-authoring tooling, which is a heavier dependency than this smoke should
    carry.

    So instead of a new scenario, this swaps the SCRIPT the existing one already points at:
    data/resources/missions/oppint_red_interceptor.lua is backed up, replaced with the
    commander-aware Tier-1 script, and restored in a finally block. "Mariana Shield" is then run
    twice - once with the swap in place (commander-on) and once without (commander-off) - which is
    the paired comparison the H1 gate item asks for, from identical initial conditions.

    What is lost against the scenario-variant approach: both RedSu35_01 and RedSu35_02 share that
    script, so there is no within-run control. The paired-run control remains, and it is the one the
    gate specifies.

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
$missionDir  = Join-Path $ReleaseRoot "data\resources\missions"
$scriptSrc   = Join-Path $repoRoot "lua\ai_commander_interceptor.lua"
# The script the shipped scenario already points at. Swapped, not added to.
$missionPath = Join-Path $missionDir "oppint_red_interceptor.lua"
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
Write-Host "scenario     : Mariana Shield (shipped, unmodified)"
Write-Host "entities     : RedSu35_01 and RedSu35_02, both on the swapped Tier-1 script"
Write-Host "run seconds  : $RunSeconds per run"

if (-not (Test-Path $missionPath)) { throw "No shipped mission script at $missionPath" }
if (-not (Test-Path $scriptSrc))   { throw "No reference Tier-1 script at $scriptSrc" }

Write-Host "`n-- preflight --"
try {
    $tags = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -TimeoutSec 8
    Assert-That ($tags.models.Count -gt 0) "inference server is serving $($tags.models.Count) models"
} catch {
    Assert-That $false "inference server reachable at http://localhost:11434 ($($_.Exception.Message))"
}

$backup = Join-Path $workDir "oppint_red_interceptor-backup-$stamp.lua"
Copy-Item $missionPath $backup -Force
Write-Host "  shipped mission script backed up to $backup"

$seededDoctrine = $false

try {
    # -- 1. swap in the commander-aware Tier-1 script --------------------------------------------
    Write-Host "`n-- wiring --"
    Copy-Item $scriptSrc $missionPath -Force
    Assert-That ((Get-Item $missionPath).Length -eq (Get-Item $scriptSrc).Length) `
        "commander-aware Tier-1 script swapped in for oppint_red_interceptor.lua"

    if (-not (Test-Path $doctrineDst)) {
        Copy-Item $doctrineSrc $doctrineDst -Force
        $seededDoctrine = $true
    }
    Assert-That (Test-Path $doctrineDst) "doctrine present at data/doctrine.txt (prompt.doctrinePath default)"

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
    $log = Invoke-Scenario -Name "Mariana Shield" -Seconds $RunSeconds -Tag "commander-on"

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
        # Restore the shipped script FIRST, so the control run is the scenario exactly as it ships.
        Copy-Item $backup $missionPath -Force
        $control = Invoke-Scenario -Name "Mariana Shield" -Seconds $RunSeconds -Tag "commander-off"
        Assert-That ($control -match 'Interceptor committed: RedSu35_01') `
            "control run flies RedSu35_01 on stock Tier-1 logic"
        Write-Host "  H1 is a JUDGEMENT, not an assertion: a domain reviewer compares the posture"
        Write-Host "  transitions in the two logs. Both are kept in $workDir for that review."
    }
}
finally {
    Write-Host "`n-- restoring the release tree --"
    # Unconditional, and idempotent if the control run already did it. The shipped mission script is
    # release-tree content; leaving a swapped copy behind would silently change every later run of
    # this scenario, including interactive ones.
    Copy-Item $backup $missionPath -Force
    Write-Host "  shipped oppint_red_interceptor.lua restored from $backup"
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
