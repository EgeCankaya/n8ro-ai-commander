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

    HOW THE COMMANDER GETS TURNED ON (changed at the Phase 1b close-out, PRD v1.7.2). At the Phase
    1b gate this script could not run: the headless host applies no per-plugin config, so a
    data/config/plugins/ai-commander.cfg carrying commander.enabled=true still produced
    "backend=stub enabled=false" in the startup log, and both this smoke and the H1 pair were
    recorded as unmet gate items. The plugin now reads that file itself at initialize()
    (AIC-API-2), so this script writes it, asserts it took effect, and removes it in the finally.

    The "commander is ON" assertion below is still an ASSERTION rather than an assumption. A green
    smoke over the stub backend's canned orders is worse than a red one, because it looks like
    evidence.

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

# The deployed config now takes effect on this host (PRD v1.7.2), so it is release-tree state this
# script is responsible for putting back exactly as it found it.
$deployedCfg = Join-Path $ReleaseRoot "data\config\plugins\ai-commander.cfg"
$cfgBackup   = Join-Path $workDir "ai-commander-cfg-backup-$stamp"
$hadCfg      = Test-Path $deployedCfg
if ($hadCfg) {
    Copy-Item $deployedCfg $cfgBackup -Force
    Write-Host "  pre-existing ai-commander.cfg backed up to $cfgBackup"
}

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

    # -- 2. turn the commander on -----------------------------------------------------------------
    # This is what PRD v1.7.2 bought. commander.enabled is a positive act, and this script is
    # taking it explicitly and reversibly rather than depending on a file someone left behind.
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $deployedCfg) | Out-Null
    @"
# Written by run-live-scenario.ps1 for the Phase 1b live gate. Removed in its finally block.
commander.enabled=true
commander.backend=local
commander.cadenceS=20
local.model=qwen2.5:7b-instruct-q8_0
local.grammarEnabled=true
"@ | Set-Content -Path $deployedCfg -Encoding ascii
    Assert-That (Test-Path $deployedCfg) "deployed config written to data/config/plugins/ai-commander.cfg"

    # -- 3. commander-on run ---------------------------------------------------------------------
    Write-Host "`n-- commander-on run --"
    $log = Invoke-Scenario -Name "Mariana Shield" -Seconds $RunSeconds -Tag "commander-on"

    Assert-That ($log -match 'ai-commander: registered the aiCommander namespace') `
        "plugin loaded and registered its namespace"
    Assert-That ($log -match 'runtime-column probe pass') "AIC-ARCH-4 probe passed"

    # If this fails the run measured NOTHING - it measured the stub backend. Asserted rather than
    # assumed, because a green smoke over canned orders is worse than a red one.
    Assert-That ($log -match 'backend=local enabled=true') `
        ("commander is ON with the local backend - if this fails the plugin did not apply the " +
         "deployed config (AIC-API-2, PRD v1.7.2) and the run is void")

    Assert-That ($log -match 'applied 5 field\(s\) from the deployed config') `
        "the deployed config was read and applied in full, by path and field count"

    Assert-That ($log -match 'doctrine loaded from') `
        "doctrine block was loaded (a missing one degrades order quality silently)"

    # NOTE ON WHERE THE RUN-END STATS COME FROM. They are read out of the ORDER LOG, not stdout.
    # The engine's stdout is block-buffered when redirected to a file, so the last buffer of a run
    # is routinely lost - observed here: a clean 90 s run whose log ends at simulationTime 80.05 s
    # with no shutdown lines at all, while the order log carries a t=90.1 s run-end record. Any
    # assertion keyed to the TAIL of stdout is therefore unreliable by construction. The order log
    # is the PRD's own record of a run (AIC-DET-1) and is written through the recorder, which is
    # why every other assertion below already reads it.

    # Order-log assertions. The log is the PRD's own record of what happened (AIC-DET-1), so the
    # smoke reads it rather than re-deriving from stdout.
    if (Test-Path $orderLog) {
        $records   = Get-Content $orderLog | ForEach-Object { $_ | ConvertFrom-Json }
        $accepted  = @($records | Where-Object { $_.event -eq 'order.accepted' })
        $rejected  = @($records | Where-Object { $_.event -eq 'order.rejected' })
        $requested = @($records | Where-Object { $_.event -eq 'order.requested' })

        Assert-That ($accepted.Count -gt 0) "at least one order was accepted ($($accepted.Count))"

        # REPORTED, NOT BARRED (PRD v1.7.5). A <=10-minute engagement yields ~10 orders, and a rate
        # over 10 samples cannot tell a real regression from three unlucky draws. The >= 95 % bar
        # lives on the 200-order soak, where n is large enough to mean something. What this script
        # uniquely proves is that the pipeline works INSIDE THE ENGINE, and that needs no rate.
        $rate = if ($requested.Count) { 100.0 * $accepted.Count / $requested.Count } else { 0 }
        Write-Host ("  acceptance: {0} of {1} requested ({2} %) - reported, not barred; the bar is on the soak" -f `
            $accepted.Count, $requested.Count, [math]::Round($rate,1))

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

        # Every rejection accounted for by reason, and any Stage-B rejection carrying enough of the
        # offending order to diagnose it (PRD v1.7.5 / AIC-DET-1). The first run of this gate could
        # not name the cause of its own failure because rawBody was empty here.
        $byReason = $rejected | Group-Object reason | Sort-Object Count -Descending
        foreach ($group in $byReason) { Write-Host "  reject.$($group.Name): $($group.Count)" }
        foreach ($r in $rejected) {
            Write-Host "    [$($r.reason)] $($r.entityId) :: $($r.detail)"
            if ($r.rawBody) { Write-Host "      rawBody: $($r.rawBody)" }
        }
        $withoutBody = @($rejected | Where-Object { -not $_.rawBody })
        Assert-That ($withoutBody.Count -eq 0) `
            "every rejection carries its raw body for diagnosis ($($withoutBody.Count) without)"

        # Validation's live-smoke bar: no frame exceeding 5 ms of plugin cost. The run-end record
        # carries the whole-run MAXIMUM, not a percentile - a p95 inside budget with one 40 ms
        # outlier is still a dropped frame in a demo, so the maximum is the number that matters.
        $runEnd = @($records | Where-Object { $_.event -eq 'commander.disabled' }) | Select-Object -Last 1
        if ($runEnd) {
            $stats = $runEnd.detail | ConvertFrom-Json
            Write-Host ("  plugin frame cost: p50 {0} ms, p95 {1} ms, max {2} ms over {3} frames" -f `
                $stats.frame.p50Ms, $stats.frame.p95Ms, $stats.frame.maxMs, $stats.frame.frames)
            Assert-That ($stats.frame.maxMs -lt 5.0) `
                "no frame exceeded 5 ms of plugin cost (max $($stats.frame.maxMs) ms)"
            Assert-That ($stats.timeouts -eq 0) `
                "the plugin's own timeout counter is zero ($($stats.timeouts))"

            # Reported separately and NOT held to a bar, per the Phase 1b gate. The offline
            # harness measured 0.00 % against six hand-written situations; this is the first
            # measurement against situations nobody chose.
            $shape  = if ($stats.rejectByReason.shape)  { $stats.rejectByReason.shape }  else { 0 }
            $schema = if ($stats.rejectByReason.schema) { $stats.rejectByReason.schema } else { 0 }
            $shapeRate  = if ($stats.requested) { 100.0 * $shape  / $stats.requested } else { 0 }
            $schemaRate = if ($stats.requested) { 100.0 * $schema / $stats.requested } else { 0 }
            Write-Host ("  reject.shape : {0} of {1} requested ({2} %)" -f $shape, $stats.requested, [math]::Round($shapeRate,2))
            Write-Host ("  reject.schema: {0} of {1} requested ({2} %)" -f $schema, $stats.requested, [math]::Round($schemaRate,2))
            Assert-That ($schemaRate -lt 1.0) "reject.schema < 1 % (got $([math]::Round($schemaRate,2)) %)"
        } else {
            Assert-That $false "run-end stats record present in the order log"
        }
    } else {
        Assert-That $false "order log written to $orderLog"
    }

    Assert-That (-not ($log -match '\[ERROR\].*aiCommander')) "plugin emitted no ERROR-level line"

    # -- 4. commander-off control run --------------------------------------------------------------
    if (-not $SkipControl) {
        Write-Host "`n-- commander-off control run (H1 pair) --"
        # Restore the shipped script FIRST, so the control run is the scenario exactly as it ships.
        Copy-Item $backup $missionPath -Force
        # And take the config away, so the control is genuinely commander-OFF. Validation asks the
        # commander-off run to be indistinguishable from one with the plugin absent; leaving the
        # config in place would leave the commander enabled against the stock script and measure
        # something else entirely.
        if (Test-Path $deployedCfg) { Remove-Item $deployedCfg -Force }

        $control = Invoke-Scenario -Name "Mariana Shield" -Seconds $RunSeconds -Tag "commander-off"
        Assert-That ($control -match 'Interceptor committed: RedSu35_01') `
            "control run flies RedSu35_01 on stock Tier-1 logic"
        Assert-That ($control -match 'enabled=false') `
            "control run really is commander-off, not merely running the stock script"
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

    # The config this script wrote now has teeth on every host (PRD v1.7.2). Leaving it behind
    # would silently enable the commander on every later run of this tree, interactive ones
    # included - which is the residual exposure v1.7.2 records, so this script must not create it.
    if (Test-Path $deployedCfg) { Remove-Item $deployedCfg -Force }
    if ($hadCfg -and (Test-Path $cfgBackup)) {
        Copy-Item $cfgBackup $deployedCfg -Force
        Write-Host "  pre-existing ai-commander.cfg restored"
    } else {
        Write-Host "  ai-commander.cfg removed; no commander config left in the release tree"
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
