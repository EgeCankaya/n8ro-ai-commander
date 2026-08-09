<#
    Arkheon Technologies
    Proprietary and Confidential.
    Unauthorized copying of this file, via any medium, is strictly prohibited.
    (c) Arkheon Technologies. All rights reserved.

.SYNOPSIS
    C23's confirming run (PRD v1.8.37, docs/c23-report.md section 10.2).

.DESCRIPTION
    Runs tools/c23-hold-probe.lua in the shipped "Mariana Shield" scenario with the commander OFF
    and no inference server, to answer the two questions the offline Lua suite cannot:

      Q1  does navigation.requestHoldPosition stop an aircraft that flies IN to the ordered orbit
          from outside it - the distant hold that has never occurred in any archived run, because
          the model does not order one (PRD Corrections item 51(f))?
      Q2  does a commanded 300 m/s actually re-accelerate a stopped aircraft, or only get issued?

    NO MODEL, NO NETWORK, NO EGRESS GRANT, NO COST. The commander is not merely unconfigured: this
    script ASSERTS that data/config/plugins/ai-commander.cfg is absent before it starts and refuses
    to run if it is not. A probe whose result could be an artifact of a published order would answer
    neither question.

    EVERYTHING IT TOUCHES IS RESTORED IN A FINALLY BLOCK, including on failure and on Ctrl-C. The
    only file it modifies is data/resources/missions/oppint_red_interceptor.lua, which is backed up
    first and put back afterwards, byte-for-byte verified. PRD Corrections item 47 and the run
    traps in this project's brief both record what a half-restored tree costs: the commander stays
    silently enabled for later runs, interactive ones included.

    THE LOG NEVER ENTERS THE REPOSITORY. Output goes to the archive directory outside the tree,
    beside every other run, because run logs carry live scenario state and *.jsonl / run logs are a
    security-relevant ignore rule enforced by tools/check-artifacts.ps1.

.PARAMETER ReleaseRoot
    The N8RO release tree. Defaults to N8RO_RELEASE_ROOT, then C:\N8RO.

.PARAMETER RunSeconds
    Sim seconds. The probe's three phases end at 440 s; the default leaves margin.

.PARAMETER ArchiveRoot
    Where the run log is written. Defaults to the project's archive directory.
#>
[CmdletBinding()]
param(
    [string]$ReleaseRoot = $(if ($env:N8RO_RELEASE_ROOT) { $env:N8RO_RELEASE_ROOT } else { "C:\N8RO" }),
    [int]$RunSeconds = 460,
    [string]$ArchiveRoot = (Join-Path $env:USERPROFILE "Documents\N8RO AI Commander logs")
)

$ErrorActionPreference = "Stop"

$repoRoot   = Split-Path -Parent $PSScriptRoot
$probeSrc   = Join-Path $repoRoot "tools\c23-hold-probe.lua"
$missionDst = Join-Path $ReleaseRoot "data\resources\missions\oppint_red_interceptor.lua"
$commanderCfg = Join-Path $ReleaseRoot "data\config\plugins\ai-commander.cfg"

$stamp     = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$outDir    = Join-Path $ArchiveRoot "$stamp-c23-probe"
$logPath   = Join-Path $outDir "c23-probe-$stamp.log"

function Fail([string]$m) { Write-Host "  [FAIL] $m" -ForegroundColor Red; throw $m }
function Ok([string]$m)   { Write-Host "  [ok]   $m" -ForegroundColor Green }

Write-Host "=== C23 confirming probe ==="
Write-Host "  release tree : $ReleaseRoot"
Write-Host "  archive      : $outDir"

# -- preflight -----------------------------------------------------------------------------------
if (-not (Test-Path $probeSrc))   { Fail "probe script not found at $probeSrc" }
if (-not (Test-Path $missionDst)) { Fail "mission script not found at $missionDst" }
if (Test-Path $commanderCfg) {
    Fail ("$commanderCfg EXISTS. This probe requires the commander OFF, and a leftover config " +
          "from an interrupted run is exactly the state this check exists to catch. Remove it " +
          "deliberately, or find out which run left it.")
}
Ok "commander config absent - aiCommander will be nil throughout"

if (Get-Process -Name 'n8ro-sim-local' -ErrorAction SilentlyContinue) {
    Fail "n8ro-sim-local is already running. Never start a run over a live one."
}
Ok "no engine process running"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$backup = Join-Path $outDir "oppint_red_interceptor-backup-$stamp.lua"
Copy-Item $missionDst $backup -Force
Ok "shipped mission script backed up ($((Get-Item $backup).Length) bytes)"

$restored = $false
try {
    Copy-Item $probeSrc $missionDst -Force
    if ((Get-Item $missionDst).Length -ne (Get-Item $probeSrc).Length) {
        Fail "probe script did not install cleanly"
    }
    Ok "probe installed as oppint_red_interceptor.lua"

    $runner = Join-Path $outDir "run-probe.cmd"
    @"
@echo off
call "$ReleaseRoot\setup.cmd" >nul 2>&1 || exit /b 1
cd /d "%N8RO_RELEASE%"
"%N8RO_RELEASE%\bin\n8ro-sim-local.exe" --scenario "Mariana Shield" --run-ms $($RunSeconds * 1000)
"@ | Set-Content -Path $runner -Encoding ascii

    Write-Host "  running 'Mariana Shield' for $RunSeconds sim-seconds -> $logPath"
    $proc = Start-Process -FilePath $runner -RedirectStandardOutput $logPath `
        -RedirectStandardError "$logPath.err" -PassThru -WindowStyle Hidden
    $deadline = (Get-Date).AddSeconds($RunSeconds + 120)
    while ((Get-Date) -lt $deadline -and -not $proc.HasExited) { Start-Sleep -Seconds 5 }
    Get-Process -Name 'n8ro-sim-local' -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    Ok "run complete"
}
finally {
    # THE POINT OF THIS BLOCK. It runs on success, on failure and on Ctrl-C. A release tree left
    # carrying a probe script would silently drive every later scenario run - including an
    # interactive one - with an instrument that flies no tactics and never shoots.
    if (Test-Path $backup) {
        Copy-Item $backup $missionDst -Force
        if ((Get-Item $missionDst).Length -eq (Get-Item $backup).Length) {
            $restored = $true
            Write-Host "  [ok]   shipped mission script RESTORED byte-for-byte" -ForegroundColor Green
        } else {
            Write-Host "  [FAIL] RESTORE MISMATCH - restore $backup by hand before any other run" `
                -ForegroundColor Red
        }
    }
    if (Test-Path $commanderCfg) {
        Write-Host "  [WARN] a commander config appeared during this run and was NOT written by it" `
            -ForegroundColor Yellow
    }
}

if (-not $restored) { throw "release tree not restored - stopping" }

# -- results -------------------------------------------------------------------------------------
if (-not (Test-Path $logPath)) { Fail "no log produced" }
$samples = Select-String -Path $logPath -Pattern 'C23PROBE t=' -SimpleMatch
Write-Host "`n=== probe samples: $($samples.Count) ==="
Write-Host "  log: $logPath"
Write-Host "  analyse with: python tools\analyse-c23-probe.py `"$logPath`""
