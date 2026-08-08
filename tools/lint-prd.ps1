#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Structural lint for the PRD. Runs anywhere pwsh does - no SDK, no compiler, no release tree.

.DESCRIPTION
    The PRD is the contract. Most of the ways it decays are mechanical and therefore checkable:
    an FR loses its UAC, an Open Question loses its decision target, an Out-of-Scope row loses its
    target column, a requirement acquires a weasel word.

    This exists because those decays are exactly what a human review catches LATE. OQ-4 sat Open
    past its "Phase 1a end" decision target across three revisions and was only caught by a
    /prd-review pass after the milestone had already closed. That is a check a script can run on
    every push.

    What it deliberately does NOT do: judge content. It cannot tell you whether a rationale is
    good, only whether one exists. Rating the argument is the reviewer's job.

.PARAMETER Path
    The PRD to lint. Defaults to docs/prd.md relative to the repo root.

.PARAMETER WarnAsError
    Treat warnings as failures. Off by default so advisory findings do not block a PR.
#>
[CmdletBinding()]
param(
    [string]$Path,
    [switch]$WarnAsError
)

$ErrorActionPreference = "Stop"

if (-not $Path) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $Path = Join-Path $repoRoot "docs/prd.md"
}
if (-not (Test-Path $Path)) {
    Write-Host "::error::PRD not found at $Path"
    exit 1
}

$lines = Get-Content -Path $Path
$text = $lines -join "`n"
$errors = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]
$checks = 0

function Fail { param([string]$m, [int]$line = 0)
    $script:errors.Add($(if ($line) { "L${line}: $m" } else { $m })) }
function Warn { param([string]$m, [int]$line = 0)
    $script:warnings.Add($(if ($line) { "L${line}: $m" } else { $m })) }
function Check { param([string]$name) $script:checks++; Write-Host "  - $name" }

# All pattern matching goes through [regex] explicitly rather than through -match or Select-String.
# Those two behave differently across PowerShell editions on patterns this script depends on, and
# the script has to run identically under Windows PowerShell 5.1 locally and pwsh 7 on a Linux
# runner. A lint that silently matches nothing is worse than no lint: it reports PASS.
function Get-Captures {
    param([string[]]$Lines, [string]$Pattern, [int]$Group = 1)
    $out = @()
    foreach ($line in $Lines) {
        $m = [regex]::Match($line, $Pattern)
        if ($m.Success) { $out += $m.Groups[$Group].Value }
    }
    return $out
}

function Find-Lines {
    param([string[]]$Lines, [string]$Pattern)
    $out = @()
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $m = [regex]::Match($Lines[$i], $Pattern)
        if ($m.Success) {
            $out += [pscustomobject]@{ LineNumber = $i + 1; Line = $Lines[$i]; Match = $m }
        }
    }
    return $out
}

Write-Host "=== PRD lint: $Path ==="
Write-Host ""

# ---------------------------------------------------------------------------------------------
# 1. FR <-> Trace <-> UAC parity
# ---------------------------------------------------------------------------------------------
Write-Host "[FR/UAC parity]"

$frIds    = @(Get-Captures $lines '^#### ([A-Z]+-[A-Z]+-\d+):')
$traceIds = @(Get-Captures $lines '^\*\*Trace:\*\* UAC-([A-Z]+-[A-Z]+-\d+)')
$uacIds   = @(Get-Captures $lines '^### UAC-([A-Z]+-[A-Z]+-\d+[a-z]?):')

Check "FRs defined: $($frIds.Count)"
Check "Trace lines: $($traceIds.Count)"
Check "UACs defined: $($uacIds.Count)"

if ($frIds.Count -eq 0) { Fail "no FRs found - is the heading convention '#### AIC-AREA-N:' intact?" }

foreach ($fr in $frIds) {
    if ($traceIds -notcontains $fr) { Fail "FR $fr has no '**Trace:** UAC-$fr' line" }
    # An FR may carry more than one UAC (e.g. UAC-AIC-API-1 and UAC-AIC-API-1b) - the base must exist.
    if ($uacIds -notcontains $fr) { Fail "FR $fr has no UAC-$fr entry in Appendix B" }
}
# Orphan UACs: a UAC whose base FR id no longer exists. Suffixed extras (1b) are legitimate and
# resolve to their base id, so they are only orphans if the base is gone too.
foreach ($uac in ($uacIds | Sort-Object -Unique)) {
    $base = $uac -replace '[a-z]$', ''
    if ($frIds -notcontains $base) { Fail "orphan UAC-$uac - no FR $base exists" }
}

# ---------------------------------------------------------------------------------------------
# 2. Every FR carries the customer-obsession fields
# ---------------------------------------------------------------------------------------------
Write-Host "[FR customer-obsession fields]"

foreach ($hit in (Find-Lines $lines '^#### ([A-Z]+-[A-Z]+-\d+):')) {
    $fr = $hit.Match.Groups[1].Value
    # Scan forward to the next FR heading or the end of the FR section.
    $j = $hit.LineNumber   # LineNumber is 1-based, so this is already the line after the heading.
    $block = @()
    while ($j -lt $lines.Count `
           -and -not [regex]::IsMatch($lines[$j], '^#### [A-Z]+-[A-Z]+-\d+:') `
           -and -not [regex]::IsMatch($lines[$j], '^## ')) {
        $block += $lines[$j]; $j++
    }
    $blockText = $block -join "`n"
    foreach ($field in '\*\*Customer scenario:\*\*', '\*\*Pain removed:\*\*', '\*\*Acceptance criteria:\*\*') {
        if (-not [regex]::IsMatch($blockText, $field)) {
            $pretty = $field -replace '\\', ''
            Fail "FR $fr is missing its $pretty field" $hit.LineNumber
        }
    }
}
Check "Customer scenario / Pain removed / Acceptance criteria present on every FR"

# ---------------------------------------------------------------------------------------------
# 3. Open Question hygiene - the check that would have caught OQ-4
# ---------------------------------------------------------------------------------------------
Write-Host "[Open Questions]"

$oqRows = @(Find-Lines $lines '^\| (OQ-\d+) \|')
Check "OQ rows: $($oqRows.Count)"
if ($oqRows.Count -eq 0) { Fail "no OQ table rows found" }

$openOqs = @()
foreach ($row in $oqRows) {
    $cols = $row.Line -split '\|'
    # | # | Question | Status | Decision target | Rationale |
    if ($cols.Count -lt 6) { Fail "OQ row is malformed (expected 5 columns)" $row.LineNumber; continue }
    $id = $cols[1].Trim(); $question = $cols[2].Trim()
    $status = $cols[3].Trim(); $target = $cols[4].Trim(); $rationale = $cols[5].Trim()

    if (-not $status)    { Fail "$id has an empty Status" $row.LineNumber }
    if (-not $rationale) { Fail "$id has an empty Rationale" $row.LineNumber }
    if (-not $question)  { Fail "$id has an empty Question" $row.LineNumber }

    $isResolved = [regex]::IsMatch($status, 'Resolved')
    if (-not $isResolved) {
        # An unresolved question with no decision target is an aspirational deferral - invisible debt.
        if (-not $target -or [regex]::IsMatch($target, '^[-—–\s]*$')) {
            Fail "$id is '$status' but has no Decision target - aspirational deferrals become invisible debt" $row.LineNumber
        }
        $openOqs += [pscustomobject]@{ Id = $id; Status = $status; Target = $target }
    }
}

# The OQ table must not grow an owner column - named owners become deferral crutches.
$oqHeader = @(Find-Lines $lines '^\| # \| Question \| Status \|') | Select-Object -First 1
if ($oqHeader -and [regex]::IsMatch($oqHeader.Line, '\|\s*Owner\s*\|')) {
    Fail "the OQ table has an Owner column - the executor of the next iteration drives resolution" $oqHeader.LineNumber
}

# Surface every unresolved OQ with its target on each run. This does not compute overdue-ness -
# "Phase 1a end" is not a date - but it puts the slip in front of a reviewer on every PR, which is
# what was missing when OQ-4 aged past its target unnoticed.
if ($openOqs.Count -gt 0) {
    Write-Host ""
    Write-Host "  Unresolved open questions (review these against current phase):"
    foreach ($oq in $openOqs) {
        Write-Host ("    {0,-6} target={1,-28} status={2}" -f $oq.Id, $oq.Target, $oq.Status)
    }
    Write-Host ""
}

# ---------------------------------------------------------------------------------------------
# 4. Out-of-Scope completeness
# ---------------------------------------------------------------------------------------------
Write-Host "[Out of scope]"

$inOoS = $false; $oosRows = 0
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ([regex]::IsMatch($lines[$i], '^## Out of scope')) { $inOoS = $true; continue }
    if ($inOoS -and [regex]::IsMatch($lines[$i], '^## ')) { break }
    if (-not $inOoS -or -not [regex]::IsMatch($lines[$i], '^\| ')) { continue }
    if ([regex]::IsMatch($lines[$i], '^\| Item \|') -or [regex]::IsMatch($lines[$i], '^\|[\s\-]+\|')) { continue }

    $cols = $lines[$i] -split '\|'
    # | Item | Status | Rationale | Target | Added |
    if ($cols.Count -lt 6) { Fail "Out-of-Scope row is malformed (expected 5 columns)" ($i + 1); continue }
    $oosRows++
    $item = $cols[1].Trim(); $status = $cols[2].Trim()
    $rationale = $cols[3].Trim(); $target = $cols[4].Trim(); $added = $cols[5].Trim()

    if (-not $item)      { Fail "Out-of-Scope row has an empty Item" ($i + 1) }
    if (-not $rationale) { Fail "Out-of-Scope '$item' has an empty Rationale" ($i + 1) }
    if (-not $target)    { Fail "Out-of-Scope '$item' has an empty Target - never blank, use 'N/A' with a reason" ($i + 1) }
    if (-not $added)     { Fail "Out-of-Scope '$item' has no Added date - revisions must be traceable" ($i + 1) }
    if (-not [regex]::IsMatch($status, '^(Deferred|Out of scope|TBD)')) {
        Fail "Out-of-Scope '$item' has status '$status' - expected Deferred / Out of scope / TBD" ($i + 1)
    }
    if ([regex]::IsMatch($lines[$i], '~~')) {
        Fail "Out-of-Scope '$item' uses strikethrough - revisions add dated rows, they do not hide content" ($i + 1)
    }
}
Check "Out-of-Scope rows: $oosRows (Item/Status/Rationale/Target/Added all populated)"

# ---------------------------------------------------------------------------------------------
# 5. Requirement smells
# ---------------------------------------------------------------------------------------------
Write-Host "[Requirement smells]"

$smells = @(
    @{ Pattern = '\b(if possible|as appropriate|where feasible|when practical)\b'; Name = 'loophole' },
    @{ Pattern = '\b(user-friendly|intuitive|easy to use|clean design)\b';          Name = 'untestable criterion' },
    @{ Pattern = '\b(fast response|high availability|low latency)\b';               Name = 'missing metric' },
    @{ Pattern = '\b(better than current|improved performance)\b';                  Name = 'comparative without baseline' },
    @{ Pattern = '\b(and so on|etc\.)';                                             Name = 'open-ended list' }
)
foreach ($smell in $smells) {
    foreach ($hit in (Find-Lines $lines $smell.Pattern)) {
        Warn "$($smell.Name): '$($hit.Match.Value)'" $hit.LineNumber
    }
}

# SHALL discipline: a normative statement must commit, not hedge.
foreach ($hit in (Find-Lines $lines '^The system (should|may|will|is expected to)\b')) {
    Fail "normative statement hedges - use SHALL or remove: '$($hit.Line.Substring(0, [Math]::Min(80, $hit.Line.Length)))'" $hit.LineNumber
}
Check "smell scan across $($lines.Count) lines"

# ---------------------------------------------------------------------------------------------
# 6. Version coherence - Status must match the newest changelog entry
# ---------------------------------------------------------------------------------------------
Write-Host "[Version coherence]"

$statusLine      = @(Find-Lines $lines '^\*\*Status:\*\*\s*Draft (v[\d.]+)')   | Select-Object -First 1
$firstChangelog  = @(Find-Lines $lines '^### (v[\d.]+) [—-]')                 | Select-Object -First 1
$firstRevHistory = @(Find-Lines $lines '^- (v[\d.]+) [—-]')                   | Select-Object -First 1

if (-not $statusLine) {
    Fail "no '**Status:** Draft vN.N' line in the header"
} else {
    $statusVersion = $statusLine.Match.Groups[1].Value
    Check "Status: $statusVersion"
    if ($firstChangelog) {
        $changelogVersion = $firstChangelog.Match.Groups[1].Value
        if ($statusVersion -ne $changelogVersion) {
            Fail "Status is $statusVersion but the newest changelog entry is $changelogVersion"
        }
    } else { Warn "no changelog section found" }
    if ($firstRevHistory) {
        $revVersion = $firstRevHistory.Match.Groups[1].Value
        if ($statusVersion -ne $revVersion) {
            Fail "Status is $statusVersion but the newest revision-history entry is $revVersion"
        }
    }
}

# ---------------------------------------------------------------------------------------------
# 7. The summary artifact must not go stale
# ---------------------------------------------------------------------------------------------
# docs/summary.md is the two-page front door to a 4,000-line document (PRD v1.8.30, §Corrections
# item 46). A summary that drifts is worse than none, and this project has demonstrated twice that
# continuously-maintained duplicate figures drift: §Success metrics' headline in-engine acceptance
# figure was ONE RUN STALE when v1.8.30 recomputed it, sitting under a subsection about why that
# number needed watching.
#
# So the summary carries a version stamp and this check pins it to the PRD's. The failure mode it
# catches is not a typo - it is a PRD revision that changed a verdict while the summary kept quoting
# the old one, which is exactly the failure a reader consults a summary to avoid.
Write-Host "[Summary artifact]"

$summaryPath = Join-Path (Split-Path -Parent $Path) "summary.md"
if (-not (Test-Path $summaryPath)) {
    Warn "docs/summary.md is missing - the PRD has no short front door (PRD v1.8.30, item 46)"
    Check "summary artifact: absent"
} else {
    $summaryLines = Get-Content -Path $summaryPath
    $stamp = @(Find-Lines $summaryLines '^\*\*PRD version:\*\*\s*(v[\d.]+)') | Select-Object -First 1
    if (-not $stamp) {
        Fail "docs/summary.md has no '**PRD version:** vN.N' stamp - without it nothing can tell whether it is current"
        Check "summary artifact: unstamped"
    } else {
        $summaryVersion = $stamp.Match.Groups[1].Value
        Check "summary artifact: $summaryVersion"
        if ($statusLine -and $summaryVersion -ne $statusLine.Match.Groups[1].Value) {
            Fail ("docs/summary.md is stamped $summaryVersion but the PRD is " `
                + "$($statusLine.Match.Groups[1].Value) - regenerate the summary, or the front door " `
                + "quotes a verdict the document no longer holds") $stamp.LineNumber
        }
    }
}

# ---------------------------------------------------------------------------------------------
# 8. The in-engine acceptance figure must agree across all three documents
# ---------------------------------------------------------------------------------------------
# C17's real cost is staleness, not disagreement. The in-engine acceptance figure has gone stale
# FOUR times - 86.2 % -> 84.3 % -> 79.8 % -> 69.6 % - and every time the same way: a run was
# archived, nobody recomputed, and the old number was requoted until a reader caught it
# (PRD §Corrections items 46(f), 48, 50, 51). It is quoted in three documents that drift apart.
#
# This check cannot recompute the figure: the archive lives OUTSIDE the repository by design
# (order logs carry live scenario state) and this script must run on a bare checkout with no
# release tree. So `tools/acceptance-report.py` owns the number and prints a sentinel, and this
# check enforces that all three documents carry the SAME sentinel. Drift fails the build; a
# refresh is a three-line paste.
Write-Host "[In-engine acceptance sentinel]"

$sentinelPattern = '<!--\s*in-engine-acceptance:\s*([\d.]+)\s*\[([\d.]+),\s*([\d.]+)\]\s*n=(\d+)\s*runs=(\d+)\s*-->'
$repoRootForDocs = Split-Path -Parent $Path
$sentinelFiles = @(
    @{ Name = "docs/prd.md";     File = $Path },
    @{ Name = "docs/summary.md"; File = (Join-Path $repoRootForDocs "summary.md") },
    @{ Name = "README.md";       File = (Join-Path (Split-Path -Parent $repoRootForDocs) "README.md") }
)

$found = @()
foreach ($entry in $sentinelFiles) {
    if (-not (Test-Path $entry.File)) {
        Fail "$($entry.Name) not found - cannot check the acceptance sentinel"
        continue
    }
    $hit = @(Find-Lines (Get-Content -Path $entry.File) $sentinelPattern) | Select-Object -First 1
    if (-not $hit) {
        Fail ("$($entry.Name) carries no in-engine-acceptance sentinel - run " `
            + "``python tools/acceptance-report.py`` and paste the line it prints")
    } else {
        $found += [pscustomobject]@{
            Name = $entry.Name
            Value = $hit.Match.Groups[0].Value.Trim()
            Point = $hit.Match.Groups[1].Value
            N = $hit.Match.Groups[4].Value
        }
    }
}

if ($found.Count -eq $sentinelFiles.Count) {
    $distinct = @($found | Select-Object -ExpandProperty Value -Unique)
    if ($distinct.Count -ne 1) {
        Fail "the in-engine acceptance sentinel differs across documents - they have drifted:"
        foreach ($f in $found) { Fail "    $($f.Name): $($f.Value)" }
    } else {
        Check "acceptance sentinel agrees across 3 documents: $($found[0].Point) % (n = $($found[0].N))"
    }
}

# And the figure SHALL NOT appear as a bare point estimate anywhere (PRD v1.8.30). Every interval
# in play is 15-30 points wide; a bare decimal implies a precision the sample does not carry, and
# it is the form in which 86.2 % survived its own successor run.
if ($found.Count -gt 0) {
    $point = [regex]::Escape($found[0].Point)
    foreach ($entry in $sentinelFiles) {
        if (-not (Test-Path $entry.File)) { continue }
        foreach ($hit in (Find-Lines (Get-Content -Path $entry.File) "$point\s*%")) {
            if ([regex]::IsMatch($hit.Line, "$point\s*%\s*\[")) { continue }   # carries its interval
            if ([regex]::IsMatch($hit.Line, 'in-engine-acceptance:')) { continue }  # the sentinel
            Warn ("$($entry.Name): in-engine acceptance quoted without its interval - " `
                + "'$($found[0].Point) %' must be followed by [lo, hi]") $hit.LineNumber
        }
    }
}

# ---------------------------------------------------------------------------------------------
# 9. Required sections for the Comprehensive tier
# ---------------------------------------------------------------------------------------------
Write-Host "[Required sections]"

$required = @(
    '## Problem statement', '## Goals and success metrics', '## Out of scope',
    '## Key hypotheses', '## Tenets', '## Functional requirements', '## Scope authority',
    '## Risks and open decisions', '## Validation and test plan', '## Rollout and milestones',
    '## Appendix A', '## Appendix B', '## Appendix C'
)
foreach ($section in $required) {
    if (-not [regex]::IsMatch($text, [regex]::Escape($section))) { Fail "required section missing: $section" }
}
Check "all $($required.Count) required sections present"

# ---------------------------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------------------------
Write-Host ""
Write-Host "=== summary ==="
Write-Host "checks   : $checks"
Write-Host "errors   : $($errors.Count)"
Write-Host "warnings : $($warnings.Count)"

foreach ($w in $warnings) { Write-Host "::warning::PRD lint: $w" }
foreach ($e in $errors)   { Write-Host "::error::PRD lint: $e" }

if ($errors.Count -gt 0) { Write-Host ""; Write-Host "PRD LINT FAILED"; exit 1 }
if ($WarnAsError -and $warnings.Count -gt 0) { Write-Host ""; Write-Host "PRD LINT FAILED (warnings as errors)"; exit 1 }
Write-Host ""
Write-Host "PRD LINT PASS"
exit 0
