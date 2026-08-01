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
Write-Host "`n-- headless engine run ($RunSeconds s) --"
$workDir = Join-Path ([System.IO.Path]::GetTempPath()) "aic-smoke"
New-Item -ItemType Directory -Force -Path $workDir | Out-Null
$stamp   = (Get-Date -Format "yyyyMMddTHHmmssZ")
$outPath = Join-Path $workDir "smoke-$stamp.log"
$errPath = Join-Path $workDir "smoke-$stamp.err"

# Stub regeneration is only meaningful if we can see it happen this run.
if (Test-Path $stubPath) { Remove-Item $stubPath -Force }

$runner = Join-Path $workDir "run.cmd"
@"
@echo off
call "$ReleaseRoot\setup.cmd" >nul 2>&1 || exit /b 1
cd /d "%N8RO_RELEASE%"
"%N8RO_RELEASE%\bin\n8ro-sim-local.exe"
"@ | Set-Content -Path $runner -Encoding ascii

$proc = Start-Process -FilePath $runner -RedirectStandardOutput $outPath -RedirectStandardError $errPath -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds($RunSeconds)
while ((Get-Date) -lt $deadline -and -not $proc.HasExited) { Start-Sleep -Seconds 1 }
Get-Process -Name 'n8ro-sim-local' -ErrorAction SilentlyContinue | Stop-Process -Force
if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 2

$log = if (Test-Path $outPath) { Get-Content $outPath -Raw } else { "" }
Write-Host "  log: $outPath"

Assert-That ($log -match 'userPlugins\\sim\\ai-commander\.dll.*Loading plugin') `
    "engine loads the plugin from userPlugins/sim"
Assert-That ($log -match 'registered the aiCommander namespace \(14 functions\)') `
    "registers the aiCommander namespace with 14 functions"
Assert-That ($log -match 'runtime-column probe pass') `
    "AIC-ARCH-4 runtime-column probe passes"
Assert-That ($log -match 'enabled=false') `
    "fail-closed: commander.enabled is false on a default deploy"

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
