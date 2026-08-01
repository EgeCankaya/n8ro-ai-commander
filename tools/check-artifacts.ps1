#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Asserts that no classified or licensed artifact has been committed.

.DESCRIPTION
    Two of this repository's ignore rules are security-relevant rather than hygienic, and .gitignore
    alone does not enforce them: a file already tracked stays tracked, and `git add -f` bypasses the
    ignore entirely.

      * Order logs (*.jsonl) carry live scenario state - positions, ORBAT, team assignments,
        loadouts - which inherits the release tree's proprietary classification. Recording is ON BY
        DEFAULT and record.path is operator-configurable, so a developer will eventually point it
        inside the repository. The PRD calls this "the rule most likely to be missed".
      * SDK headers, import libraries and platform binaries are Arkheon's shipped code. The
        repository is intended to be unbuildable without a licensed install; committing any of them
        breaks that property silently.

    This runs over the tracked file list, so it catches what .gitignore cannot.
#>
[CmdletBinding()]
param([string]$RepoRoot)

$ErrorActionPreference = "Stop"
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }

Push-Location $RepoRoot
try {
    $tracked = @(& git ls-files)
} finally {
    Pop-Location
}

$violations = New-Object System.Collections.Generic.List[string]

$rules = @(
    @{ Pattern = '\.jsonl$';                        Why = 'order log - carries live scenario state (positions, ORBAT, teams, loadouts)' },
    @{ Pattern = '(^|/)\.env($|\.)';                Why = 'environment file - may carry credentials' },
    @{ Pattern = '\.(key|pem|pfx|p12)$';            Why = 'private key material' },
    @{ Pattern = '\.(gguf|safetensors|bin)$';       Why = 'model weights - large, licensed, and live in the release tree' },
    @{ Pattern = '(^|/)n8ro-[a-z]+\.(lib|dll|exp)$';Why = 'SDK import library or platform binary - Arkheon shipped code' },
    @{ Pattern = '(^|/)include/n8ro-';              Why = 'SDK header - Arkheon shipped code, must never be vendored' },
    @{ Pattern = 'schema-reference\.json$';         Why = 'SDK schema export - proprietary, resolved at runtime via N8RO_RELEASE' },
    @{ Pattern = '(^|/)(bin|x64|Win32)/';           Why = 'build output' },
    @{ Pattern = '\.(pdb|obj|ilk|idb|tlog)$';       Why = 'build intermediate' }
)

foreach ($file in $tracked) {
    foreach ($rule in $rules) {
        if ([regex]::IsMatch($file, $rule.Pattern, 'IgnoreCase')) {
            $violations.Add("$file  --  $($rule.Why)")
        }
    }
}

Write-Host "=== tracked-artifact guard ==="
Write-Host "tracked files : $($tracked.Count)"
Write-Host "rules         : $($rules.Count)"

if ($violations.Count -gt 0) {
    Write-Host ""
    foreach ($v in $violations) { Write-Host "::error::committed artifact: $v" }
    Write-Host ""
    Write-Host "ARTIFACT GUARD FAILED - $($violations.Count) violation(s)"
    Write-Host "A matching .gitignore rule is not enough: an already-tracked file stays tracked."
    Write-Host "Use 'git rm --cached <file>' to untrack, then verify the ignore rule covers it."
    exit 1
}

Write-Host ""
Write-Host "ARTIFACT GUARD PASS"
exit 0
