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
    [switch]$SkipControl,

    # THE THIRD ARM, AND THE ONE THAT HAS NEVER EXISTED (PRD v1.8.30, C22).
    #
    # Two arms cannot answer the question this project is asking. The control arm restores the
    # SHIPPED mission script AND removes the commander config, so commander-on and commander-off
    # differ in TWO variables - script and commander - and §Corrections item 45(b) records that the
    # first version of that comparison was published before anyone noticed.
    #
    # This arm holds the script fixed and moves only the commander: the commander-AWARE script, run
    # with no config and no inference server. It separates "the reference script is now as good as
    # stock" from "the commander adds or subtracts on top of a competent script", and until it
    # passes the commanded arm measures nothing about the model.
    #
    # It is the SUCCESSOR to §Corrections item 45's arm C rather than a repeat of it. Arm C was run
    # against the BROKEN script and established a mechanism; this one is run against the fixed
    # script and establishes whether the fix worked. It needs no inference server and costs nothing
    # but wall clock.
    [switch]$SkipScriptOnlyArm,

    # `claude` REACHES THE NETWORK, and it is the one parameter here that needs an owner grant.
    #
    # This is C1. Every hosted measurement in this project before 2026-08-05 ran against the six
    # synthetic fixtures in tests/live/LiveMain.cpp - the same FIELD SET as a real snapshot, filled
    # with values invented for a test. `-Backend claude` transmits the same fields drawn from REAL
    # SCENARIO STATE in a shipped mission. That distinction is what four consecutive egress grants
    # declined to blur, and it was released by an owner decision recorded in PRD v1.8.11 before this
    # parameter existed. See docs/egress.md, §Where those values come from.
    [ValidateSet("local", "claude")]
    [string]$Backend = "local",

    [string]$ClaudeModel = "claude-haiku-4-5",

    # The local model, parameterised at v1.8.22 so the H1 model-dependence question can be asked
    # at more than one capability. The default is the value this script hardcoded through Phase 1b
    # and Phase 3, so every prior run reproduces unchanged.
    [string]$LocalModel = "qwen2.5:7b-instruct-q8_0",
    [string]$KeyEnvVar = "ANTHROPIC_API_KEY",

    # WHERE THIS RUN'S EVIDENCE GOES, and why it is outside the repository (PRD v1.8.18,
    # §Corrections item 35(f); §Source control, "Where run evidence lives").
    #
    # Order logs carry live scenario state - positions, ORBAT, team assignments, loadouts - which
    # inherits the release tree's proprietary classification. `logs/` and `*.jsonl` are therefore
    # security-relevant ignore rules, and tools/check-artifacts.ps1 fails the build on a tracked
    # .jsonl. In-tree was never available.
    #
    # But "not in the repository" is not a location, and that gap cost this project real evidence:
    # C11 - the largest open item in the PRD - rests on paired commander-on / commander-off logs
    # described only as "retained for the domain review", which sat in %TEMP% for three days. This
    # parameter is that location, written down once.
    [string]$ArchiveRoot = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) "N8RO AI Commander logs")
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

# C26 (PRD v1.8.53, §Corrections item 69(d)). This read `(Get-Date -Format "yyyyMMddTHHmmssZ")`,
# which formats LOCAL time and then appends a literal `Z` claiming it is UTC. Every probe under
# `tools/` has always used `.ToUniversalTime()`, so the archive carries TWO time bases under one
# suffix, and `tools/outcome-campaign.py --since` is a LEXICAL comparison over the folder name.
# On this machine local is UTC+3, so a scenario folder sorts three hours LATE against a probe
# folder, and a `--since` boundary can include or exclude a run for no reason a reader can see.
#
# THE FIX IS ONLY FORWARD-LOOKING. Folder names already written are permanently ambiguous - the
# offset is not recorded anywhere in them, and nothing in the archive can recover it. What makes
# that survivable is that the one boundary it was ever used for was checked in both bases and the
# ordering holds either way (§Corrections item 68, review finding F11).
$stamp   = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$workDir = Join-Path ([System.IO.Path]::GetTempPath()) "aic-live"
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

# The durable home for this run's evidence. %TEMP% stays as the working directory - it is where the
# engine's stdout is redirected while the run is in flight - but nothing is left there to be found
# later by luck.
$archiveDir = Join-Path $ArchiveRoot "$stamp-$Backend"
New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null

$failures = New-Object System.Collections.Generic.List[string]
$checks = 0
function Assert-That {
    param([bool]$Condition, [string]$Description)
    $script:checks++
    if ($Condition) { Write-Host "  [PASS] $Description" }
    else { Write-Host "  [FAIL] $Description" -ForegroundColor Red; $script:failures.Add($Description) }
}

# EVERY NUMBER THIS HARNESS PRINTS GOES THROUGH HERE, AND IT IS NOT COSMETIC.
#
# PowerShell's -f operator formats with the CURRENT culture. On a machine whose decimal separator is
# a comma - which is most of Europe, and is the machine this was written on - the frame-cost line
# printed "p50 0,0054 ms" while the maximum on the same line printed with a period, because the two
# reached -f by different paths.
#
# The figures this harness prints are transcribed into docs/prd.md, docs/summary.md and README.md,
# where lint-prd.ps1 pins them and every existing copy uses a period. A published gate figure whose
# punctuation depends on the operator's Windows locale is a figure two readers can legitimately
# disagree about, and the disagreement would be invisible to every check in this repository.
#
# Precision is deliberately NOT changed here - this formats the same values, it does not round them.
function Fmt {
    param($Value)
    if ($null -eq $Value) { return "" }
    try { return [string]::Format([cultureinfo]::InvariantCulture, "{0}", [double]$Value) }
    catch { return [string]$Value }
}

Write-Host "=== AI Entity Commander - Phase 1b live scenario smoke ==="
Write-Host "release root : $ReleaseRoot"
Write-Host "scenario     : Mariana Shield (shipped, unmodified)"
Write-Host "entities     : RedSu35_01 and RedSu35_02, both on the swapped Tier-1 script"
Write-Host "run seconds  : $RunSeconds per run"
Write-Host "backend      : $Backend"
if ($Backend -eq "local") { Write-Host "model        : $LocalModel" }
if ($Backend -eq "claude") {
    if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($KeyEnvVar))) {
        Write-Error "environment variable '$KeyEnvVar' is unset or empty - no API key available"
    }
    Write-Host ""
    Write-Host "  *** THIS RUN REACHES THE NETWORK, AND WITH REAL SCENARIO STATE. ***" -ForegroundColor Yellow
    Write-Host "  model      : $ClaudeModel   key from `$$KeyEnvVar (name only; value never logged)"
    Write-Host "  transmits  : position, velocity, heading, team, reported tracks, loadout -"
    Write-Host "               the same field set as every hosted run since v1.8.3, but drawn from"
    Write-Host "               RedSu35_01 in the shipped 'Mariana Shield' scenario rather than from"
    Write-Host "               the six synthetic LiveMain fixtures."
    # WHICH GRANT IS IN FORCE, not which grant came first. This cited v1.8.11 - the FIFTH grant,
    # which authorized one named measurement (C1) and whose boundary does not extend to a later run.
    # A later grant does not inherit an earlier one's boundary and an earlier one does not authorize
    # a later run; naming the superseded grant tells an operator the run is covered by something
    # that does not cover it. The standing grant is the seventh, v1.8.48, and it is held by the
    # RELEASE TREE rather than by a person - which is what makes this script runnable at all.
    Write-Host "  authorized : PRD v1.8.48, the seventh grant and the STANDING one - it authorizes"
    Write-Host "               running the hosted backend as a product, and is held by the release"
    Write-Host "               tree rather than by a person. No PRD revision is needed before this"
    Write-Host "               run. (v1.8.11, the fifth grant, authorized C1 - the first hosted run"
    Write-Host "               on real scenario state - and is history, not this run's authority.)"
    Write-Host "  ceiling    : the config written below sets no claude.maxSpendUsd, so the adapter's"
    Write-Host "               default of 1.00 USD PER ENGINE PROCESS applies, fail-closed. It does"
    Write-Host "               not bound running this script repeatedly."
    Write-Host "  see        : docs/egress.md, 'Where those values come from'"
    Write-Host ""
}

if (-not (Test-Path $missionPath)) { throw "No shipped mission script at $missionPath" }
if (-not (Test-Path $scriptSrc))   { throw "No reference Tier-1 script at $scriptSrc" }

Write-Host "`n-- preflight --"
# ONLY THE LOCAL BACKEND NEEDS THIS. Asserted unconditionally until 2026-08-14, which meant a
# `-Backend claude` run could never report `failed: 0` on a machine with no Ollama installed - the
# hosted arm contacts api.anthropic.com and never touches localhost:11434. The assertion is real
# where it is a prerequisite and absent where it is not, rather than being softened for both.
if ($Backend -eq "local") {
    try {
        $tags = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -TimeoutSec 8
        Assert-That ($tags.models.Count -gt 0) "inference server is serving $($tags.models.Count) models"
    } catch {
        Assert-That $false "inference server reachable at http://localhost:11434 ($($_.Exception.Message))"
    }
} else {
    # The hosted path's prerequisite is the key, and it fails at request time with a configuration
    # error rather than a transport one - which reads like a backend outage in an order log.
    $keyPresent = -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($KeyEnvVar))
    Assert-That $keyPresent "`$$KeyEnvVar is set (the hosted backend's prerequisite; value never read here)"
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

# The doctrine is release-tree state this script is responsible for too, and it is backed up on the
# same terms as the mission script and the config rather than merely seeded-if-absent.
#
# WHY THIS CHANGED (v1.8.25, C15). Both this script and the .vcxproj post-build step used to copy
# data/doctrine.txt ONLY when it was absent, on the reasoning that an operator's own doctrine should
# not be overwritten. The consequence is that a doctrine EDIT never reached the release tree: C15's
# fix rewrote the CRUISE SPEED block from 6,932 to 7,824 bytes, the unit suite pinned the new prefix,
# and a live run would still have sent the old one. The measurement and the artifact would have
# disagreed by 892 bytes with nothing red - which is the same class of error as the one-byte prefix
# divergence PRD §Corrections item 31(f) records catching by hand, three orders of magnitude larger
# and inside the run that was supposed to be the evidence.
#
# So: back up whatever is there, install the repository's copy, and put the original back in the
# finally. An operator's doctrine survives the run; the run measures what this repository ships.
$doctrineBackup = Join-Path $workDir "doctrine-backup-$stamp.txt"
$hadDoctrine    = Test-Path $doctrineDst
if ($hadDoctrine) {
    Copy-Item $doctrineDst $doctrineBackup -Force
    Write-Host "  pre-existing doctrine.txt backed up to $doctrineBackup"
}
$seededDoctrine = $false

try {
    # -- 1. swap in the commander-aware Tier-1 script --------------------------------------------
    Write-Host "`n-- wiring --"
    Copy-Item $scriptSrc $missionPath -Force
    Assert-That ((Get-Item $missionPath).Length -eq (Get-Item $scriptSrc).Length) `
        "commander-aware Tier-1 script swapped in for oppint_red_interceptor.lua"

    if (-not $hadDoctrine) { $seededDoctrine = $true }
    Copy-Item $doctrineSrc $doctrineDst -Force
    Assert-That (Test-Path $doctrineDst) "doctrine present at data/doctrine.txt (prompt.doctrinePath default)"
    # Asserted by SIZE, not merely by presence. "A doctrine file exists" was the old check and it is
    # what let a stale block through; what the run needs to be true is that the block it sends is the
    # block this repository ships and the unit suite pins.
    Assert-That ((Get-Item $doctrineDst).Length -eq (Get-Item $doctrineSrc).Length) `
        "deployed doctrine matches the repository's byte-for-byte (C15: a stale block would make the run measure the wrong prompt)"

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

    # PRESERVED, NOT DELETED (PRD §Corrections item 35(f)).
    #
    # This line used to read:
    #
    #     if (Test-Path $orderLog) { Remove-Item $orderLog -Force -ErrorAction SilentlyContinue }
    #
    # It destroyed the previous run's order log at the START of every run, and it is why the
    # 2026-08-03 local run's log does not exist to be reviewed. The run still needs to begin from
    # an empty log - every order-count assertion below reads the whole file and would otherwise
    # count a predecessor's orders - but "start empty" and "destroy the predecessor" are different
    # requirements, and only the first one was ever needed.
    #
    # A run that overwrites its own predecessor's evidence makes every paired comparison a
    # one-shot, which is precisely what H1 needs and cannot get.
    if (Test-Path $orderLog) {
        $rotated = Join-Path $archiveDir ("orders-previous-run-{0}.jsonl" -f $stamp)
        Move-Item -Path $orderLog -Destination $rotated -Force
        Write-Host "  previous order log preserved -> $rotated"
    }

    # -- 2. turn the commander on -----------------------------------------------------------------
    # This is what PRD v1.7.2 bought. commander.enabled is a positive act, and this script is
    # taking it explicitly and reversibly rather than depending on a file someone left behind.
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $deployedCfg) | Out-Null
    if ($Backend -eq "claude") {
        @"
# Written by run-live-scenario.ps1 for the C1 hosted live-scenario run. Removed in its finally block.
commander.enabled=true
commander.backend=claude
commander.cadenceS=20
claude.enabled=true
claude.model=$ClaudeModel
claude.apiKeyEnvVar=$KeyEnvVar
"@ | Set-Content -Path $deployedCfg -Encoding ascii
        $expectedFields = 6
    } else {
        @"
# Written by run-live-scenario.ps1 for the Phase 1b live gate. Removed in its finally block.
commander.enabled=true
commander.backend=local
commander.cadenceS=20
local.model=$LocalModel
local.grammarEnabled=true
"@ | Set-Content -Path $deployedCfg -Encoding ascii
        $expectedFields = 5
    }
    Assert-That (Test-Path $deployedCfg) "deployed config written to data/config/plugins/ai-commander.cfg"

    # -- 3. commander-on run ---------------------------------------------------------------------
    Write-Host "`n-- commander-on run --"
    $log = Invoke-Scenario -Name "Mariana Shield" -Seconds $RunSeconds -Tag "commander-on"

    Assert-That ($log -match 'ai-commander: registered the aiCommander namespace') `
        "plugin loaded and registered its namespace"
    Assert-That ($log -match 'runtime-column probe pass') "AIC-ARCH-4 probe passed"

    # If this fails the run measured NOTHING - it measured the stub backend. Asserted rather than
    # assumed, because a green smoke over canned orders is worse than a red one.
    Assert-That ($log -match "backend=$Backend enabled=true") `
        ("commander is ON with the $Backend backend - if this fails the plugin did not apply the " +
         "deployed config (AIC-API-2, PRD v1.7.2) and the run is void")

    Assert-That ($log -match "applied $expectedFields field\(s\) from the deployed config") `
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
        # THE DENOMINATOR IS RESOLVED ORDERS, NOT REQUESTED ONES (v1.8.15). A request issued in the
        # last few seconds of a fixed-length run is still in flight when the engine stops: it has no
        # accepted and no rejected record, and counting it against acceptance charges the backend for
        # a verdict the run did not wait for. The C1 run printed "10 of 13 (76.9 %)" where two of the
        # thirteen were unresolved - a number that reads exactly like an order-quality result and is
        # not one, which is the same shape as the 91.7 % configuration artifact in §Corrections 24(b).
        $resolved = $accepted.Count + $rejected.Count
        $inFlight = $requested.Count - $resolved
        $rate = if ($resolved) { 100.0 * $accepted.Count / $resolved } else { 0 }
        Write-Host ("  acceptance: {0} of {1} RESOLVED ({2} %) - {3} requested, {4} still in flight at shutdown" -f `
            $accepted.Count, $resolved, (Fmt ([math]::Round($rate,1))), $requested.Count, $inFlight)
        Write-Host ("              REPORTED, NOT BARRED, and at n={0} it is not a rate at all - the >= 95 %" -f $resolved)
        Write-Host  "              bar lives on the 200-order soak. Do not quote this figure as one."

        $postures = @($accepted | ForEach-Object { $_.order.posture } | Sort-Object -Unique)
        Assert-That ($postures.Count -ge 3) `
            "at least three distinct postures observed ($($postures -join ', '))"

        # NOT A p95, AND IT STOPPED CLAIMING TO BE ONE (v1.8.15). At n~10 the 95th percentile index
        # lands on the last or second-to-last sample, so this printed the MAXIMUM under a percentile's
        # name. PRD v1.7.4 Finding 3 recorded exactly this - "the reported p95 is not a p95", 10,363 ms
        # being the second-highest of five - and this script went on printing one for four revisions.
        # A percentile needs a sample; a ten-minute engagement does not produce one. The range is what
        # this run can honestly say, so the range is what it says.
        $latencies = @($accepted | ForEach-Object { $_.latencyMs } | Sort-Object)
        if ($latencies.Count) {
            $median = $latencies[[int]([math]::Floor($latencies.Count / 2))]
            Write-Host ("  order latency: min {0} ms, median {1} ms, max {2} ms over n={3}" -f `
                $latencies[0], $median, $latencies[-1], $latencies.Count)
            Write-Host ("              NO PERCENTILE IS REPORTED. At n={0} a p95 is the maximum wearing a" -f $latencies.Count)
            Write-Host  "              percentile's name (PRD v1.7.4, Finding 3). The p95 lives on the soak."
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
                (Fmt $stats.frame.p50Ms), (Fmt $stats.frame.p95Ms), (Fmt $stats.frame.maxMs), $stats.frame.frames)
            Assert-That ($stats.frame.maxMs -lt 5.0) `
                "no frame exceeded 5 ms of plugin cost (max $(Fmt $stats.frame.maxMs) ms)"
            Assert-That ($stats.timeouts -eq 0) `
                "the plugin's own timeout counter is zero ($($stats.timeouts))"

            # Reported separately and NOT held to a bar, per the Phase 1b gate. The offline
            # harness measured 0.00 % against six hand-written situations; this is the first
            # measurement against situations nobody chose.
            $shape  = if ($stats.rejectByReason.shape)  { $stats.rejectByReason.shape }  else { 0 }
            $schema = if ($stats.rejectByReason.schema) { $stats.rejectByReason.schema } else { 0 }
            $shapeRate  = if ($stats.requested) { 100.0 * $shape  / $stats.requested } else { 0 }
            $schemaRate = if ($stats.requested) { 100.0 * $schema / $stats.requested } else { 0 }
            Write-Host ("  reject.shape : {0} of {1} requested ({2} %)" -f $shape, $stats.requested, (Fmt ([math]::Round($shapeRate,2))))
            Write-Host ("  reject.schema: {0} of {1} requested ({2} %)" -f $schema, $stats.requested, (Fmt ([math]::Round($schemaRate,2))))
            Assert-That ($schemaRate -lt 1.0) "reject.schema < 1 % (got $(Fmt ([math]::Round($schemaRate,2))) %)"
        } else {
            Assert-That $false "run-end stats record present in the order log"
        }
    } else {
        Assert-That $false "order log written to $orderLog"
    }

    Assert-That (-not ($log -match '\[ERROR\].*aiCommander')) "plugin emitted no ERROR-level line"

    # -- 4. script-only arm: the commander-AWARE script with the commander OFF (C22) ----------------
    #
    # RUN ORDER MATTERS AND IS DELIBERATE. This must run BEFORE the control arm below, because the
    # control arm restores the shipped mission script and cannot be undone without another copy.
    # Here the swapped script is still in place; only the config goes away.
    #
    # What this arm answers: does the reference Tier-1 script fight as well as the stock one when
    # nothing is commanding it? Before PRD v1.8.30 the answer was no, and the cost was 22 of 24
    # commanded aircraft - §Corrections item 45's arm C measured exactly this configuration against
    # the broken script and got 0 launches and 2 of 2 lost.
    if (-not $SkipScriptOnlyArm) {
        Write-Host "`n-- script-only arm: commander-aware script, commander OFF (C22) --"
        if (Test-Path $deployedCfg) { Remove-Item $deployedCfg -Force }

        $scriptOnly = Invoke-Scenario -Name "Mariana Shield" -Seconds $RunSeconds -Tag "script-only"
        Assert-That ($scriptOnly -notmatch 'enabled=true') `
            "script-only arm really is commander-off (no config, no orders)"
        Write-Host "  This arm holds the SCRIPT fixed and moves only the commander, which the"
        Write-Host "  commander-on/commander-off pair below cannot do: that pair swaps the mission"
        Write-Host "  script as well (PRD Corrections item 45(b))."
    }

    # -- 5. commander-off control run --------------------------------------------------------------
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
        Write-Host "  transitions in the two logs. Both are archived to $archiveDir for that"
        Write-Host "  review - a named, durable location rather than the %TEMP% directory the"
        Write-Host "  v1.7.4 pair survived in by luck for three days (PRD Corrections item 35(f))."
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

    if ($hadDoctrine -and (Test-Path $doctrineBackup)) {
        # The run installed this repository's doctrine over whatever was there; put the original
        # back. Restoring is now the normal path rather than the exceptional one, because the run
        # OVERWRITES rather than seeds (see the backup block above).
        Copy-Item $doctrineBackup $doctrineDst -Force
        Write-Host "  pre-existing doctrine.txt restored"
    }
    elseif ($seededDoctrine -and (Test-Path $doctrineDst)) {
        Remove-Item $doctrineDst -Force; Write-Host "  removed the seeded doctrine"
    }

    # -- archive the run's evidence --------------------------------------------------------------
    # In the FINALLY block deliberately. A run that failed halfway is the run whose logs someone
    # most wants to read, and archiving only on the success path would discard exactly those. This
    # is also the H1 pair's home: the commander-on and commander-off logs land side by side, which
    # is the comparison C11 needs and the reason the convention exists at all.
    Write-Host "`n-- archiving run evidence --"
    $archived = 0
    if (Test-Path $orderLog) {
        Copy-Item $orderLog (Join-Path $archiveDir "orders.jsonl") -Force
        $archived++
    }
    foreach ($tag in @("commander-on", "commander-off", "script-only")) {
        foreach ($suffix in @("", ".err")) {
            $src = Join-Path $workDir "$tag-$stamp.log$suffix"
            if (Test-Path $src) {
                Copy-Item $src (Join-Path $archiveDir (Split-Path -Leaf $src)) -Force
                $archived++
            }
        }
    }
    if ($archived -gt 0) {
        Write-Host "  $archived file(s) -> $archiveDir"
        Write-Host "  This is the H1 evidence location (PRD Corrections item 35(f)). It is OUTSIDE"
        Write-Host "  the repository on purpose: order logs carry live scenario state, and both the"
        Write-Host "  *.jsonl ignore rule and check-artifacts.ps1 exist to keep them out of it."
    } else {
        Write-Host "  nothing to archive (no order log and no engine log produced)"
    }

    # -- the outcome comparison (PRD v1.8.30, §Success metrics) ------------------------------------
    #
    # WHY THIS BLOCK EXISTS. For twenty-nine revisions this harness reported acceptance, postures,
    # frame cost, timeouts and reject rates - and NOT ONE OF THEM IS AN OUTCOME. So a commander
    # could score green on every published metric while making the entity it commands shoot 87 %
    # less, hit nothing, and die, which is what §Corrections item 45 eventually measured from logs
    # that had been on disk since Phase 1b. The instrument already existed; nobody had run it.
    #
    # WHY IT IS AN ASSERTION ON THE COMPUTATION AND NOT ON THE RESULT. §Success metrics puts this
    # row on "report, don't bar" footing, following v1.7.5's precedent and for its reason: at twelve
    # pairs, one scenario and one force laydown, a bar on KILLS would bar a quantity that is zero in
    # BOTH arms, and a bar on losses would fail every run. Both would train readers to ignore the
    # row. What IS asserted is that the comparison RAN - the same category of harness-integrity
    # check as "the commander is really ON", and for the same reason: a comparison that silently
    # fails to run looks exactly like a comparison that found nothing wrong.
    #
    # It runs in the FINALLY, after archiving, because the analyser reads the archived arm logs.
    Write-Host "`n-- outcome comparison (launches / detonations / kills / losses) --"
    $analyser = Join-Path $repoRoot "tools\analyse-outcomes.py"
    $python = $null
    foreach ($candidate in @("python", "py")) {
        $found = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($found) { $python = $found.Source; break }
    }
    if (-not (Test-Path $analyser)) {
        Assert-That $false "tools/analyse-outcomes.py present (the outcome comparison has no instrument)"
    }
    elseif (-not $python) {
        # NOT a silent skip. An outcome row nobody can compute is the gap this whole block closes,
        # and reporting the run green while the comparison never ran would reproduce it exactly.
        Assert-That $false "python available to run the outcome comparison (install it or pass -SkipControl)"
    }
    elseif ($SkipControl) {
        Write-Host "  skipped: -SkipControl leaves no arm to compare against"
    }
    else {
        # `--since` is the run's own stamp, so this compares THIS run's arms and not the archive's
        # history. The pooled figures across the whole archive are a separate question and are read
        # by running the analyser over $ArchiveRoot by hand.
        $outcome = & $python $analyser $ArchiveRoot --since $stamp 2>&1 | Out-String
        Write-Host $outcome
        Assert-That ($outcome -match 'POOLED') `
            "outcome comparison was computed for this run's arms (reported, not barred)"
        Write-Host "  REPORTED, NOT BARRED. A commanded arm launching or surviving materially less"
        Write-Host "  than its control is a finding to be EXPLAINED before the next gate - not a"
        Write-Host "  threshold that fails this run. And a detonation is not a kill: the four"
        Write-Host "  columns are separate because PRD item 40 conflated two of them."
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
