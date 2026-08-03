<#
.SYNOPSIS
    End-to-end smoke for the AI Entity Commander.

.DESCRIPTION
    Unit tests prove the pipeline is correct in isolation. This proves the DEPLOYED ARTIFACT
    behaves correctly in the real engine, which is a different question and the one that catches
    install-time regressions: a plugin that fails to export its factory functions, fails to resolve
    the scripting service, registers under the wrong namespace, or trips the runtime-column probe
    will pass every unit test and fail here.

    Asserts, against a live headless run:
      1. the DLL exports create_plugin / destroy_plugin / get_plugin_signature
      2. the engine loads it from userPlugins/sim
      3. it coexists with any other user plugin already deployed there
      4. it registers the aiCommander namespace with all 14 functions
      5. the AIC-ARCH-4 runtime-column probe passes
      6. it is fail-closed: commander.enabled is false on a default deploy
      7. the engine regenerates data/resources/missions/stubs/aiCommander.lua with 14 functions
      8. no ERROR-level line is emitted by the plugin
      9. (v1.7.2) an ABSENT data/config/plugins/ai-commander.cfg is reported by path rather than
         passed over silently
     10. (v1.7.2) a PRESENT one is applied on the headless host, before the backend is built

    Needs no inference server and no network: both runs use the stub backend, which constructs no
    IHttpClient. That is a PRD Validation requirement of anything CI executes, and 2b is written
    the way it is to keep it true - it proves the config path, not the backend.

    Any pre-existing ai-commander.cfg is moved aside for the duration and restored in a finally,
    so a developer's leftover config can neither turn the fail-closed assertion green nor be
    destroyed by this script.

.PARAMETER ReleaseRoot
    The N8RO release tree. Defaults to N8RO_RELEASE_ROOT, then C:\N8RO.

.PARAMETER RunSeconds
    How long to let the headless sim run. 30s is enough for the probe to fire once a scenario
    spawns entities.
#>
[CmdletBinding()]
param(
    [string]$ReleaseRoot = $(if ($env:N8RO_RELEASE_ROOT) { $env:N8RO_RELEASE_ROOT } else { "C:\N8RO" }),
    [int]$RunSeconds = 30
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$failures = New-Object System.Collections.Generic.List[string]
$checks = 0

function Assert-That {
    param([bool]$Condition, [string]$Description)
    $script:checks++
    if ($Condition) {
        Write-Host "  [PASS] $Description"
    } else {
        Write-Host "  [FAIL] $Description" -ForegroundColor Red
        $script:failures.Add($Description)
    }
}

Write-Host "=== AI Entity Commander smoke ==="
Write-Host "release root : $ReleaseRoot"
Write-Host "repo root    : $repoRoot"

$deployedDll = Join-Path $ReleaseRoot "userPlugins\sim\ai-commander.dll"
$stubPath    = Join-Path $ReleaseRoot "data\resources\missions\stubs\aiCommander.lua"

# -- 1. exports --------------------------------------------------------------------------------
Write-Host "`n-- exports --"
if (-not (Test-Path $deployedDll)) {
    Write-Host "  [FAIL] deployed DLL not found at $deployedDll - build first" -ForegroundColor Red
    exit 1
}
$dumpbin = Get-ChildItem "$env:VCToolsInstallDir\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue
if (-not $dumpbin) {
    $dumpbin = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\18\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
}
if ($dumpbin) {
    $exports = & $dumpbin.FullName /nologo /exports $deployedDll 2>&1 | Out-String
    foreach ($symbol in 'create_plugin', 'destroy_plugin', 'get_plugin_signature') {
        Assert-That ($exports -match [regex]::Escape($symbol)) "exports $symbol"
    }
} else {
    Write-Host "  [SKIP] dumpbin not found; export check needs a VS developer environment"
}

# -- 2. headless run ---------------------------------------------------------------------------
$workDir = Join-Path ([System.IO.Path]::GetTempPath()) "aic-smoke"
New-Item -ItemType Directory -Force -Path $workDir | Out-Null
$stamp   = (Get-Date -Format "yyyyMMddTHHmmssZ")

function Invoke-Headless {
    param([string]$Tag)
    $out    = Join-Path $workDir "smoke-$Tag-$stamp.log"
    $err    = Join-Path $workDir "smoke-$Tag-$stamp.err"
    $runner = Join-Path $workDir "run-$Tag.cmd"
    @"
@echo off
call "$ReleaseRoot\setup.cmd" >nul 2>&1 || exit /b 1
cd /d "%N8RO_RELEASE%"
"%N8RO_RELEASE%\bin\n8ro-sim-local.exe"
"@ | Set-Content -Path $runner -Encoding ascii

    $proc = Start-Process -FilePath $runner -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -WindowStyle Hidden
    $deadline = (Get-Date).AddSeconds($RunSeconds)
    while ((Get-Date) -lt $deadline -and -not $proc.HasExited) { Start-Sleep -Seconds 1 }
    Get-Process -Name 'n8ro-sim-local' -ErrorAction SilentlyContinue | Stop-Process -Force
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 2
    Write-Host "  log: $out"
    return $(if (Test-Path $out) { Get-Content $out -Raw } else { "" })
}

# AIC-API-2 (v1.7.2): the plugin now reads this file, so the fail-closed run below is only
# meaningful if it is genuinely absent. Any pre-existing copy is moved aside and restored in the
# finally, because a developer's leftover config must not be able to turn this assertion green.
$deployedCfg = Join-Path $ReleaseRoot "data\config\plugins\ai-commander.cfg"
$cfgBackup   = Join-Path $workDir "ai-commander-cfg-backup-$stamp"
$hadCfg      = Test-Path $deployedCfg
if ($hadCfg) {
    Move-Item $deployedCfg $cfgBackup -Force
    Write-Host "  pre-existing ai-commander.cfg moved aside to $cfgBackup"
}

try {
    Write-Host "`n-- headless engine run, no deployed config ($RunSeconds s) --"

    # Stub regeneration is only meaningful if we can see it happen this run.
    if (Test-Path $stubPath) { Remove-Item $stubPath -Force }

    $log = Invoke-Headless -Tag "default"

    Assert-That ($log -match 'userPlugins\\sim\\ai-commander\.dll.*Loading plugin') `
        "engine loads the plugin from userPlugins/sim"
    Assert-That ($log -match 'registered the aiCommander namespace \(14 functions\)') `
        "registers the aiCommander namespace with 14 functions"
    Assert-That ($log -match 'runtime-column probe pass') `
        "AIC-ARCH-4 runtime-column probe passes"
    Assert-That ($log -match 'enabled=false') `
        "fail-closed: commander.enabled is false on a default deploy"

    # v1.7.2: the absent-file path must SAY it took the absent-file path. Corrections item 16 is
    # this project's own record of what an unlogged, silently-unresolved path costs - it hid for
    # two phases. A silent default is indistinguishable from a typo'd path.
    Assert-That ($log -match 'no deployed config at .*ai-commander\.cfg') `
        "absent deployed config is reported by path, not passed over silently"

    # Coexistence: at least one other user plugin loaded alongside ours, if any is deployed.
    $otherPlugins = Get-ChildItem (Join-Path $ReleaseRoot "userPlugins\sim\*.dll") -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ne 'ai-commander.dll' }
    if ($otherPlugins) {
        Assert-That ($log -match 'Plugins loaded from .*userPlugins\\sim \(loaded: [2-9]') `
            "coexists with $($otherPlugins.Count) other user plugin(s)"
    } else {
        Write-Host "  [SKIP] no other user plugin deployed; coexistence not exercised"
    }

    # The plugin must emit no ERROR-level line of its own.
    $pluginErrors = ($log -split "`n") | Where-Object { $_ -match '\[ERROR\].*aiCommander' }
    Assert-That ($pluginErrors.Count -eq 0) `
        "plugin emits no ERROR-level log line (found $($pluginErrors.Count))"

    # -- 2b. the deployed config file is honoured on the HEADLESS host ---------------------------
    # This is the claim v1.7.2 makes, and the reason two Phase 1b gate items were unreachable
    # before it. It is asserted against the real n8ro-sim-local.exe rather than a fake context,
    # because "the headless host now honours this file" is a statement about that binary.
    #
    # Deliberately backend=stub. Flipping commander.enabled from its compiled default proves the
    # file was read and applied; selecting `local` would prove the same thing while making this
    # suite reach for an inference server, and PRD Validation forbids that of anything CI runs.
    Write-Host "`n-- headless engine run, deployed config present ($RunSeconds s) --"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $deployedCfg) | Out-Null
    @"
# Written by run-smoke.ps1. Removed in its finally block.
commander.enabled=true
commander.backend=stub
commander.cadenceS=5
"@ | Set-Content -Path $deployedCfg -Encoding ascii

    $cfgLog = Invoke-Headless -Tag "configured"

    Assert-That ($cfgLog -match 'applied 3 field\(s\) from the deployed config') `
        "headless host applies data/config/plugins/ai-commander.cfg, naming the field count"
    Assert-That ($cfgLog -match 'enabled=true') `
        "commander.enabled=true from the file takes effect on a headless run (AIC-API-2 v1.7.2)"

    # The read must land BEFORE the backend is constructed, or the startup line reports a state
    # that has already been superseded. cadenceS=5 is in the same line and proves the whole file
    # reached the runtime, not just the master switch.
    Assert-That ($cfgLog -match 'backend=stub enabled=true cadenceS=5') `
        "the config was applied before the backend was built - one startup line, all three values"

    $cfgErrors = ($cfgLog -split "`n") | Where-Object { $_ -match '\[ERROR\].*aiCommander' }
    Assert-That ($cfgErrors.Count -eq 0) `
        "plugin emits no ERROR-level line with a config applied (found $($cfgErrors.Count))"
}
finally {
    # Unconditional. A leftover ai-commander.cfg would now silently enable the commander on every
    # later run of this tree, including interactive ones - which is exactly the residual exposure
    # PRD v1.7.2 records, so this script must not be the thing that creates it.
    if (Test-Path $deployedCfg) { Remove-Item $deployedCfg -Force }
    if ($hadCfg -and (Test-Path $cfgBackup)) {
        Move-Item $cfgBackup $deployedCfg -Force
        Write-Host "`n  pre-existing ai-commander.cfg restored"
    } else {
        Write-Host "`n  release tree left clean: no ai-commander.cfg"
    }
}

# -- 3. stub regeneration -----------------------------------------------------------------------
Write-Host "`n-- generated Lua stub --"
Assert-That (Test-Path $stubPath) "engine regenerates aiCommander.lua"
if (Test-Path $stubPath) {
    $stubFunctions = (Select-String -Path $stubPath -Pattern '^function aiCommander\.').Count
    Assert-That ($stubFunctions -eq 14) "stub documents 14 functions (found $stubFunctions)"
    foreach ($fn in 'requestCommand','releaseCommand','isValid','getPosture','getWaypoint',
                    'getOrbitRadiusM','getRoe','getOrderSerial','getOrderAgeS','getOrder',
                    'setSituationNote','reportTrack','reportLoadout','getStats') {
        Assert-That ((Select-String -Path $stubPath -Pattern "^function aiCommander\.$fn\(").Count -eq 1) `
            "stub documents aiCommander.$fn"
    }
}

# -- summary ------------------------------------------------------------------------------------
Write-Host "`n=== smoke summary ==="
Write-Host "checks : $checks"
Write-Host "failed : $($failures.Count)"
if ($failures.Count -gt 0) {
    Write-Host "`nFailures:" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "SMOKE PASS" -ForegroundColor Green
exit 0
