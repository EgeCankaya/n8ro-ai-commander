<#
    Arkheon Technologies
    Proprietary and Confidential.
    Unauthorized copying of this file, via any medium, is strictly prohibited.
    (c) Arkheon Technologies. All rights reserved.

.SYNOPSIS
    Exercises AIC-ORD-2 clause 8 in a COMMANDED scenario (PRD v1.8.43, Corrections item 59).

.DESCRIPTION
    Clause 8 has never fired with the product running. Across the two post-fix three-arm runs
    `navigation.requestHoldPosition` was called ZERO times and clause 8 fired ZERO times, because
    clause 7 took all 33 `hold` orders and prevented the onset (item 55(c)). That is the designed
    relationship between the two clauses -- and it is why no number of further runs closes the gap:
    clause 8 is DOWNSTREAM of clause 7, nothing else in this scenario stalls an aircraft, and clause
    7 working correctly is exactly what keeps clause 8 unexercised.

    So the below-floor state is INJECTED, the way tools/c23-hold-probe.lua injected the distant hold
    the model never orders (item 51(f)).

    HOW THIS DIFFERS FROM run-c23-probe.ps1, AND IT IS THE WHOLE POINT. That probe asserts the
    commander is OFF and refuses to start otherwise, because a result that could be an artifact of a
    published order would answer nothing. THIS probe requires the commander to be ON: the question is
    whether clause 8 works while a live commander is publishing orders into the same script.

    AND THE PROBE IS THE SHIPPED SCRIPT. tools/c8-floor-probe.lua is
    lua/ai_commander_interceptor.lua byte-for-byte, plus an appendix. This script ASSERTS that
    prefix on every run and refuses to start if it has drifted -- so "clauses 7 and 8 are the
    shipped implementation" is checked rather than claimed. c23-hold-probe.lua could not say this:
    it was a bare instrument implementing no AIC-ORD-2 row at all.

    WHAT IT PROVES:     clause 8 fires, takes navigation, and returns the aircraft above
                        safety.minSpeedMps within one cadence window, with a live commander in the
                        loop and a within-run control aircraft that is never injected.
    WHAT IT DOES NOT:   prove that any ordinary order path reaches clause 8. No such path is known
                        in this scenario. THE STALL IS INJECTED and the result must say so.

    NO HOSTED EGRESS, NO GRANT, NO COST. The local backend only; -Backend claude does not exist here
    on purpose.

    EVERYTHING IT TOUCHES IN THE RELEASE TREE IS RESTORED IN A FINALLY BLOCK, including on failure:
    the mission script, the deployed commander config, and the doctrine. A tree left carrying a probe
    script would silently drive every later run of this scenario, interactive ones included.

    THE LOG NEVER ENTERS THE REPOSITORY. Order logs and engine logs carry live scenario state, which
    is a security-relevant ignore rule enforced by tools/check-artifacts.ps1.

.PARAMETER ReleaseRoot
    The N8RO release tree. Defaults to N8RO_RELEASE_ROOT, then C:\N8RO.

.PARAMETER RunSeconds
    Sim seconds. The injection window opens at 90 s and abandons at 300 s; the default leaves margin
    to observe the recovery and a stretch of post-recovery flight.

.PARAMETER ArchiveRoot
    Where the run evidence goes. Outside the repository, beside every other run.
#>
[CmdletBinding()]
param(
    [string]$ReleaseRoot = $(if ($env:N8RO_RELEASE_ROOT) { $env:N8RO_RELEASE_ROOT } else { "C:\N8RO" }),
    [int]$RunSeconds = 400,
    [string]$LocalModel = "qwen2.5:7b-instruct-q8_0",
    [string]$ArchiveRoot = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) "N8RO AI Commander logs")
)

$ErrorActionPreference = "Stop"

$repoRoot     = Split-Path -Parent $PSScriptRoot
$probeSrc     = Join-Path $repoRoot "tools\c8-floor-probe.lua"
$referenceSrc = Join-Path $repoRoot "lua\ai_commander_interceptor.lua"
$doctrineSrc  = Join-Path $repoRoot "data\doctrine.txt"
$missionDst   = Join-Path $ReleaseRoot "data\resources\missions\oppint_red_interceptor.lua"
$doctrineDst  = Join-Path $ReleaseRoot "data\doctrine.txt"
$commanderCfg = Join-Path $ReleaseRoot "data\config\plugins\ai-commander.cfg"
$orderLog     = Join-Path $ReleaseRoot "logs\ai-commander\orders.jsonl"

$stamp   = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$outDir  = Join-Path $ArchiveRoot "$stamp-c8-probe"
$logPath = Join-Path $outDir "c8-probe-$stamp.log"

$failures = New-Object System.Collections.Generic.List[string]
$checks = 0
function Assert-That {
    param([bool]$Condition, [string]$Description)
    $script:checks++
    if ($Condition) { Write-Host "  [PASS] $Description" }
    else { Write-Host "  [FAIL] $Description" -ForegroundColor Red; $script:failures.Add($Description) }
}
function Fail([string]$m) { Write-Host "  [FAIL] $m" -ForegroundColor Red; throw $m }
function Ok([string]$m)   { Write-Host "  [ok]   $m" -ForegroundColor Green }

Write-Host "=== AIC-ORD-2 clause 8 -- commanded-scenario probe ==="
Write-Host "  release tree : $ReleaseRoot"
Write-Host "  scenario     : Mariana Shield (shipped, unmodified)"
Write-Host "  injected     : RedSu35_02      control (never injected): RedSu35_01"
Write-Host "  backend      : local / $LocalModel   -- no hosted egress, no grant, no cost"
Write-Host "  archive      : $outDir"

# -- preflight ------------------------------------------------------------------------------------
Write-Host "`n-- preflight --"
if (-not (Test-Path $probeSrc))     { Fail "probe not found at $probeSrc" }
if (-not (Test-Path $referenceSrc)) { Fail "reference script not found at $referenceSrc" }
if (-not (Test-Path $missionDst))   { Fail "mission script not found at $missionDst" }

# THE CLAIM THIS SCRIPT EXISTS TO CHECK. "The probe IS the shipped script" is load-bearing -- it is
# what makes the clause 8 under test the shipped clause 8 -- so it is asserted against the bytes
# rather than trusted. A reference-script edit that did not reach the probe would otherwise leave
# this probe silently measuring an older implementation, which is exactly the class of defect
# Corrections item 56 records (an instrument three revisions stale, with nothing red).
$refBytes   = [System.IO.File]::ReadAllBytes($referenceSrc)
$probeBytes = [System.IO.File]::ReadAllBytes($probeSrc)
if ($probeBytes.Length -le $refBytes.Length) {
    Fail "probe is not longer than the reference script - it cannot carry the appendix"
}
$prefixMatches = $true
for ($i = 0; $i -lt $refBytes.Length; $i++) {
    if ($probeBytes[$i] -ne $refBytes[$i]) { $prefixMatches = $false; break }
}
if (-not $prefixMatches) {
    Fail ("tools/c8-floor-probe.lua has DRIFTED from lua/ai_commander_interceptor.lua at byte $i. " +
          "Regenerate the probe: the reference script must be its exact prefix, or clauses 7 and 8 " +
          "under test are not the shipped ones.")
}
Ok ("probe carries the reference script byte-for-byte as its prefix " +
    "($($refBytes.Length) of $($probeBytes.Length) bytes)")

if (Get-Process -Name 'n8ro-sim-local' -ErrorAction SilentlyContinue) {
    Fail "n8ro-sim-local is already running. Never start a run over a live one."
}
Ok "no engine process running"

try {
    $tags = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -TimeoutSec 8
    Ok "inference server is serving $($tags.models.Count) models"
} catch {
    Fail "inference server unreachable at http://localhost:11434 - this probe needs the commander ON"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# -- back up every piece of release-tree state this script touches ---------------------------------
$missionBackup  = Join-Path $outDir "oppint_red_interceptor-backup-$stamp.lua"
Copy-Item $missionDst $missionBackup -Force
Ok "shipped mission script backed up ($((Get-Item $missionBackup).Length) bytes)"

$cfgBackup = Join-Path $outDir "ai-commander-cfg-backup-$stamp"
$hadCfg    = Test-Path $commanderCfg
if ($hadCfg) { Copy-Item $commanderCfg $cfgBackup -Force; Ok "pre-existing ai-commander.cfg backed up" }

$doctrineBackup = Join-Path $outDir "doctrine-backup-$stamp.txt"
$hadDoctrine    = Test-Path $doctrineDst
if ($hadDoctrine) { Copy-Item $doctrineDst $doctrineBackup -Force; Ok "pre-existing doctrine.txt backed up" }
$seededDoctrine = $false

$restored = $false
try {
    Copy-Item $probeSrc $missionDst -Force
    if ((Get-Item $missionDst).Length -ne (Get-Item $probeSrc).Length) { Fail "probe did not install cleanly" }
    Ok "probe installed as oppint_red_interceptor.lua"

    if (-not $hadDoctrine) { $seededDoctrine = $true }
    Copy-Item $doctrineSrc $doctrineDst -Force
    if ((Get-Item $doctrineDst).Length -ne (Get-Item $doctrineSrc).Length) {
        Fail "deployed doctrine does not match the repository's (C15: the run would measure the wrong prompt)"
    }
    Ok "doctrine installed and byte-size verified against the repository's"

    # The commander goes ON. That is the difference between this probe and run-c23-probe.ps1.
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $commanderCfg) | Out-Null
    @"
# Written by run-c8-floor-probe.ps1 for the clause 8 commanded-scenario probe.
# Removed in its finally block.
commander.enabled=true
commander.backend=local
commander.cadenceS=20
local.model=$LocalModel
local.grammarEnabled=true
"@ | Set-Content -Path $commanderCfg -Encoding ascii
    Ok "deployed config written - commander ON, local backend"

    # Start from an empty order log, WITHOUT destroying the predecessor (Corrections item 35(f)).
    if (Test-Path $orderLog) {
        Move-Item -Path $orderLog -Destination (Join-Path $outDir "orders-previous-run-$stamp.jsonl") -Force
        Ok "previous order log preserved"
    }

    $runner = Join-Path $outDir "run-probe.cmd"
    @"
@echo off
call "$ReleaseRoot\setup.cmd" >nul 2>&1 || exit /b 1
cd /d "%N8RO_RELEASE%"
"%N8RO_RELEASE%\bin\n8ro-sim-local.exe" --scenario "Mariana Shield" --run-ms $($RunSeconds * 1000)
"@ | Set-Content -Path $runner -Encoding ascii

    Write-Host "`n-- running --"
    Write-Host "  'Mariana Shield' for $RunSeconds sim-seconds -> $logPath"
    Write-Host "  injection window opens at t=90 s, abandons at t=300 s"
    $proc = Start-Process -FilePath $runner -RedirectStandardOutput $logPath `
        -RedirectStandardError "$logPath.err" -PassThru -WindowStyle Hidden
    $deadline = (Get-Date).AddSeconds($RunSeconds + 150)
    while ((Get-Date) -lt $deadline -and -not $proc.HasExited) { Start-Sleep -Seconds 5 }
    Get-Process -Name 'n8ro-sim-local' -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    Ok "run complete"
}
finally {
    # Runs on success, on failure and on Ctrl-C. See run-c23-probe.ps1's finally for what a
    # half-restored tree costs.
    Write-Host "`n-- restoring the release tree --"
    if (Test-Path $missionBackup) {
        Copy-Item $missionBackup $missionDst -Force
        if ((Get-Item $missionDst).Length -eq (Get-Item $missionBackup).Length) {
            $restored = $true
            Write-Host "  [ok]   shipped mission script RESTORED byte-for-byte" -ForegroundColor Green
        } else {
            Write-Host "  [FAIL] RESTORE MISMATCH - restore $missionBackup by hand before any other run" -ForegroundColor Red
        }
    }

    if (Test-Path $commanderCfg) { Remove-Item $commanderCfg -Force }
    if ($hadCfg -and (Test-Path $cfgBackup)) {
        Copy-Item $cfgBackup $commanderCfg -Force
        Write-Host "  [ok]   pre-existing ai-commander.cfg restored" -ForegroundColor Green
    } else {
        Write-Host "  [ok]   ai-commander.cfg removed; no commander config left in the tree" -ForegroundColor Green
    }

    if ($hadDoctrine -and (Test-Path $doctrineBackup)) {
        Copy-Item $doctrineBackup $doctrineDst -Force
        Write-Host "  [ok]   pre-existing doctrine.txt restored" -ForegroundColor Green
    } elseif ($seededDoctrine -and (Test-Path $doctrineDst)) {
        Remove-Item $doctrineDst -Force
        Write-Host "  [ok]   removed the seeded doctrine" -ForegroundColor Green
    }

    if (Test-Path $orderLog) {
        Copy-Item $orderLog (Join-Path $outDir "orders.jsonl") -Force
        Write-Host "  [ok]   order log archived" -ForegroundColor Green
    }
}

if (-not $restored) { throw "release tree not restored - stopping" }
if (-not (Test-Path $logPath)) { Fail "no log produced" }

# -- results ---------------------------------------------------------------------------------------
Write-Host "`n=== what the run says ==="
$log = Get-Content $logPath

function Find-Log([string]$pattern) { return @($log | Where-Object { $_ -match $pattern }) }

# The run has to have been a COMMANDED one, asserted from the startup log rather than assumed.
# A green probe over the stub backend would be worse than a red one.
Assert-That ((Find-Log 'backend=local enabled=true').Count -gt 0) `
    "commander was ON with the local backend (if this fails the probe measured nothing)"

$injectOn  = Find-Log 'C8PROBE INJECTION-ON'
$injectOff = Find-Log 'C8PROBE INJECTION-OFF'
$abandoned = Find-Log 'C8PROBE INJECTION-ABANDONED'
$recovered = Find-Log 'C8PROBE RECOVERED'

# The shipped script's own clause 8 log lines. These are emitted by
# lua/ai_commander_interceptor.lua, not by the appendix - so they are the product speaking.
$clause8Took     = Find-Log 'below flying speed at .* Tier 1 takes navigation'
$clause8Returned = Find-Log 'recovered to .* navigation returns to the posture'

Assert-That ($injectOn.Count -gt 0)  "the injection window opened"
Assert-That ($abandoned.Count -eq 0) "the injection drove the aircraft below the floor (not abandoned on the deadline)"
Assert-That ($injectOff.Count -gt 0) "the injector latched OFF at the crossing - everything after it is the shipped clause 8"

Assert-That ($clause8Took.Count -gt 0) `
    "CLAUSE 8 FIRED IN A COMMANDED SCENARIO - the shipped script logged 'Tier 1 takes navigation'"
Assert-That ($clause8Returned.Count -gt 0) `
    "clause 8 released navigation back to the posture after recovering"
Assert-That ($recovered.Count -gt 0) "the aircraft returned above the floor"

# One cadence window is 20 s (commander.cadenceS above). Clause 8's criterion is that the aircraft
# does not stay below the floor for longer than that.
$recoveryS = $null
if ($recovered.Count -gt 0 -and $recovered[0] -match 'elapsedS=([0-9.]+)') {
    $recoveryS = [double]$Matches[1]
    Write-Host ("  recovery took {0} s against a {1} s cadence window" -f $recoveryS, 20.0)
    Assert-That ($recoveryS -le 20.0) `
        "recovery completed within one cadence window ($recoveryS s of 20 s)"
}

# THE WITHIN-RUN CONTROL. RedSu35_01 flies the shipped script untouched for the whole run. If it
# also went below the floor, the injection is not what put the other aircraft there.
$controlBelow = Find-Log 'C8PROBE t=.*RedSu35_01 CONTROL.*below=True'
Assert-That ($controlBelow.Count -eq 0) `
    "the un-injected control aircraft never went below the floor ($($controlBelow.Count) samples)"

Write-Host "`n  THE STALL WAS INJECTED. This probe shows clause 8 works with a live commander in the"
Write-Host "  loop; it does NOT show that any ordinary order path reaches clause 8, and no such path"
Write-Host "  is known in this scenario (PRD v1.8.43, Corrections item 59(d))."
Write-Host "`n  log: $logPath"

Write-Host "`n=== clause 8 commanded-scenario probe summary ==="
Write-Host "checks : $checks"
Write-Host "failed : $($failures.Count)"
if ($failures.Count -gt 0) {
    Write-Host "`nFailures:" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "CLAUSE 8 COMMANDED-SCENARIO PROBE PASS" -ForegroundColor Green
exit 0
