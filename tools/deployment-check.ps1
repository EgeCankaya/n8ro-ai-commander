<#
    Arkheon Technologies
    Proprietary and Confidential.
    Unauthorized copying of this file, via any medium, is strictly prohibited.
    (c) Arkheon Technologies. All rights reserved.

.SYNOPSIS
    Runs the PRD's Deployment checklist against a release tree (PRD v1.8.49).

.DESCRIPTION
    §Deployment checklist has carried nine unticked boxes for the life of this document, and they
    were never project work: they are a PER-DEPLOYMENT runbook. Nobody can tick "replay determinism
    green on the target machine" in the abstract, because the target machine is whichever one
    somebody is installing on.

    So the boxes are not ticked. They are made RUNNABLE, on the precedent tools/lint-prd.ps1 and
    tools/check-artifacts.ps1 set: a rule someone has to remember is not a control. This script
    answers the eight checkable items against a named tree and prints a dated verdict; the ninth is
    a human review that no script can perform, and it is printed as such rather than faked green.

    WHAT IT DELIBERATELY DOES NOT DO. It does not modify the tree. It does not enable anything. It
    does not run the engine - the replay-determinism item is answered by the offline unit suite,
    which is where that test lives and which needs no scenario and no server.

    NO NETWORK except the local inference-server reachability probe, which is one of the items.
    No hosted egress, no API key read, no cost.

.PARAMETER ReleaseRoot
    The N8RO release tree to check. Defaults to N8RO_RELEASE_ROOT, then C:\N8RO.

.PARAMETER SkipUnitSuite
    Skip the replay-determinism item, which needs the unit test executable to have been built.
#>
[CmdletBinding()]
param(
    [string]$ReleaseRoot = $(if ($env:N8RO_RELEASE_ROOT) { $env:N8RO_RELEASE_ROOT } else { "C:\N8RO" }),
    [switch]$SkipUnitSuite
)

$ErrorActionPreference = "Continue"
$repoRoot = Split-Path -Parent $PSScriptRoot

$checks = 0
$failed = 0
$manual = 0

function Item {
    param([string]$Name, [bool]$Condition, [string]$Detail = "")
    $script:checks++
    if ($Condition) {
        Write-Host "  [PASS] $Name"
    } else {
        Write-Host "  [FAIL] $Name" -ForegroundColor Red
        $script:failed++
    }
    if ($Detail) { Write-Host "         $Detail" -ForegroundColor DarkGray }
}

function ManualItem {
    param([string]$Name, [string]$Detail)
    $script:manual++
    Write-Host "  [MANUAL] $Name" -ForegroundColor Yellow
    Write-Host "           $Detail" -ForegroundColor DarkGray
}

Write-Host "=== AI Entity Commander - deployment checklist ==="
Write-Host "  release tree : $ReleaseRoot"
Write-Host "  repository   : $repoRoot"
Write-Host "  checked      : $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))"
Write-Host ""

if (-not (Test-Path $ReleaseRoot)) {
    Write-Host "::error:: release tree not found at $ReleaseRoot"
    exit 1
}

# -- 1. the three required exports ------------------------------------------------------------------
# n8ro::core::IPlugin's contract. A DLL missing any of them loads and then does nothing, which is
# the failure mode worth catching before a demo rather than during one.
$deployedDll = Join-Path $ReleaseRoot "userPlugins\sim\ai-commander.dll"
$builtDll = Join-Path $repoRoot "bin\release\ai-commander.dll"
$dll = if (Test-Path $deployedDll) { $deployedDll } else { $builtDll }
if (Test-Path $dll) {
    $dumpbin = Get-Command dumpbin -ErrorAction SilentlyContinue
    if ($dumpbin) {
        $exports = & dumpbin /exports $dll 2>$null | Out-String
        $required = @("create_plugin", "destroy_plugin", "get_plugin_signature")
        $missing = @($required | Where-Object { $exports -notmatch [regex]::Escape($_) })
        Item "dumpbin /exports shows all three required exports" ($missing.Count -eq 0) `
            $(if ($missing.Count) { "missing: $($missing -join ', ')" } else { "$dll" })
    } else {
        ManualItem "dumpbin /exports shows all three required exports" `
            "dumpbin is not on PATH - run this from a developer prompt, or call dev\setup-dev.cmd first"
    }
} else {
    Item "the plugin DLL is present" $false "looked in $deployedDll and $builtDll"
}

# -- 2/3. the deployed config must not enable anything -----------------------------------------------
# THIS BECAME LOAD-BEARING AT v1.7.2. Before then the headless host applied no per-plugin config and
# a stray file was inert; the plugin now reads it itself at initialize(), so a file left behind by an
# interrupted run silently enables the commander on every later run, interactive ones included.
$deployedCfg = Join-Path $ReleaseRoot "data\config\plugins\ai-commander.cfg"
if (-not (Test-Path $deployedCfg)) {
    Item "data/config/plugins/ai-commander.cfg is absent, or disables the commander" $true `
        "absent - nothing enables the commander on this tree"
    Item "commander.enabled = false and claude.enabled = false in the deployed default config" $true `
        "no deployed config, so both default to false in code"
} else {
    $cfg = Get-Content $deployedCfg -Raw
    $commanderOn = $cfg -match '(?m)^\s*commander\.enabled\s*=\s*true'
    $claudeOn = $cfg -match '(?m)^\s*claude\.enabled\s*=\s*true'
    Item "data/config/plugins/ai-commander.cfg is absent, or disables the commander" (-not $commanderOn) `
        $(if ($commanderOn) { "PRESENT AND ENABLES THE COMMANDER: $deployedCfg" } else { $deployedCfg })
    Item "commander.enabled = false and claude.enabled = false in the deployed default config" `
        ((-not $commanderOn) -and (-not $claudeOn)) `
        $(if ($claudeOn) { "claude.enabled = true - this tree would reach the network" } else { "" })

    # The spend ceiling, when the hosted path is configured at all (AIC-BE-2, C24, v1.8.47).
    if ($claudeOn) {
        $ceiling = if ($cfg -match '(?m)^\s*claude\.maxSpendUsd\s*=\s*([0-9.]+)') { [double]$Matches[1] } else { $null }
        if ($null -eq $ceiling) {
            ManualItem "claude.maxSpendUsd is set on a tree with the hosted path enabled" `
                "not set - the adapter's default of 1.00 USD per process applies. Set it deliberately."
        } else {
            Item "claude.maxSpendUsd is a positive ceiling" ($ceiling -gt 0) `
                "$ceiling USD per engine process (at or below zero disables the hosted path - fail-closed)"
        }
    }
}

# -- 4. the Lua stub ---------------------------------------------------------------------------------
$stub = Join-Path $ReleaseRoot "data\resources\missions\stubs\aiCommander.lua"
Item "aiCommander.lua stub present (regenerate by running the engine once)" (Test-Path $stub) `
    $stub

# -- 5. order-log directory ---------------------------------------------------------------------------
$logDir = Join-Path $ReleaseRoot "logs\ai-commander"
$writable = $false
if (Test-Path $logDir) {
    try {
        $probe = Join-Path $logDir ".deployment-check-probe"
        Set-Content -Path $probe -Value "probe" -ErrorAction Stop
        Remove-Item $probe -Force -ErrorAction SilentlyContinue
        $writable = $true
    } catch { $writable = $false }
}
Item "order-log directory exists and is writable" $writable $logDir

# -- 6. inference server (local backend only) ---------------------------------------------------------
$serverUp = $false
$serverDetail = ""
try {
    $tags = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -TimeoutSec 8
    $serverUp = $true
    $serverDetail = "serving $($tags.models.Count) models"
} catch {
    $serverDetail = $_.Exception.Message
}
Item "inference server reachable from the sim host (local backend only)" $serverUp $serverDetail

# -- 7. safety clamps vs the target scenario ----------------------------------------------------------
# NOT AUTOMATABLE, and saying so is the point. `safety.minSpeedMps = 50` is a policy choice that
# CommanderConfig.h itself instructs an operator to lower for rotary-wing or loitering platforms.
# A script that guessed at a scenario's operating envelope would be inventing a verdict.
ManualItem "safety.* clamps reviewed against the target scenario's operating envelope" `
    "A JUDGEMENT, not a check. See CommanderConfig.h: minSpeedMps 50 and minAltitudeHaeM 100 are policy choices a rotary-wing or loitering deployment MUST lower."

# -- 8. replay determinism ------------------------------------------------------------------------------
if ($SkipUnitSuite) {
    ManualItem "replay determinism test green on the target machine" "skipped by -SkipUnitSuite"
} else {
    $testExe = Join-Path $repoRoot "tests\bin\release\ai-commander-tests.exe"
    if (-not (Test-Path $testExe)) {
        ManualItem "replay determinism test green on the target machine" `
            "test executable not built: $testExe"
    } else {
        # setup.cmd must be sourced or the exe exits -1073741515 (missing SDK DLLs on PATH).
        $runner = Join-Path ([System.IO.Path]::GetTempPath()) "aic-deployment-check-tests.cmd"
        @"
@echo off
call "$ReleaseRoot\setup.cmd" >nul 2>&1
"$testExe" %*
"@ | Set-Content -Path $runner -Encoding ascii
        $output = & cmd /c $runner 2>&1 | Out-String
        $replay = @($output -split "`n" | Where-Object { $_ -match 'Replay|Determinis' })
        $anyFail = $output -match '\[FAIL'
        Item "replay determinism test green on the target machine" `
            ((-not $anyFail) -and ($replay.Count -gt 0)) `
            "$($replay.Count) replay/determinism assertions in the offline suite; suite has failures: $anyFail"
        Remove-Item $runner -Force -ErrorAction SilentlyContinue
    }
}

# -- 9. Phase 2 / hosted path ---------------------------------------------------------------------------
# The authorization half is answered centrally as of PRD v1.8.48; the other two halves are properties
# of THIS host and THIS mission and cannot be.
Write-Host ""
Write-Host "  -- hosted path (only when claude.enabled = true) --"
Write-Host "  [PASS] owner authorization recorded - the STANDING grant, PRD v1.8.48, 2026-08-09"
$checks++
$keyName = "ANTHROPIC_API_KEY"
if (Test-Path $deployedCfg) {
    $cfgText = Get-Content $deployedCfg -Raw
    if ($cfgText -match '(?m)^\s*claude\.apiKeyEnvVar\s*=\s*(\S+)') { $keyName = $Matches[1] }
}
$keySet = -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($keyName))
Write-Host "  [INFO] API key in `$$keyName : $(if ($keySet) { 'present' } else { 'NOT SET - the hosted path would refuse with a configuration error' })"
ManualItem "the transmitted-field list reviewed against the mission ACTUALLY deployed" `
    "Condition (2) of the standing grant, and it cannot be discharged centrally: the field LIST is a property of the snapshot builder, but WHICH SCENARIO'S VALUES fill it is a property of whoever deploys. See docs/egress.md."

Write-Host ""
Write-Host "=== deployment checklist summary ==="
Write-Host "checked        : $checks"
Write-Host "failed         : $failed"
Write-Host "manual items   : $manual (printed, never auto-passed)"
if ($failed -gt 0) {
    Write-Host ""
    Write-Host "DEPLOYMENT CHECK FAILED" -ForegroundColor Red
    exit 1
}
Write-Host ""
Write-Host "DEPLOYMENT CHECK PASS - and the $manual manual item(s) above are still yours to answer." -ForegroundColor Green
exit 0
