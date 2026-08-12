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
# 9. The engagement-outcome figure must agree across all three documents
# ---------------------------------------------------------------------------------------------
# Check 8 exists because the acceptance figure went stale four times before anyone pinned it.
# This one is pinned BEFORE the first time. The outcome interval is the most quotable number this
# project has produced - a single paired difference on n = 4 - and it is quoted in three
# documents that have already drifted once (README carried no outcome section at all until
# v1.8.52, three revisions after the result was published).
#
# `tools/outcome-campaign.py` owns the number, exactly as `acceptance-report.py` owns check 8's.
# This script cannot recompute it: the archive lives outside the repository by design.
Write-Host "[Engagement-outcome sentinel]"

$outcomePattern = '<!--\s*outcome-damage-absorbed:\s*([-+][\d.]+)\s*\[([-+][\d.]+),\s*([-+][\d.]+)\]\s*n=(\d+)\s*signs=(\d+)/(\d+)\s*(\w+)\s*-->'
$outcomeFound = @()
foreach ($entry in $sentinelFiles) {
    if (-not (Test-Path $entry.File)) { continue }
    $hit = @(Find-Lines (Get-Content -Path $entry.File) $outcomePattern) | Select-Object -First 1
    if (-not $hit) {
        Fail ("$($entry.Name) carries no outcome-damage-absorbed sentinel - run " `
            + "``python tools/outcome-campaign.py --since 20260809T190000Z`` and paste the line it prints")
    } else {
        $outcomeFound += [pscustomobject]@{
            Name = $entry.Name
            Value = $hit.Match.Groups[0].Value.Trim()
            Point = $hit.Match.Groups[1].Value
            Lo = $hit.Match.Groups[2].Value
            Hi = $hit.Match.Groups[3].Value
            N = $hit.Match.Groups[4].Value
        }
    }
}

if ($outcomeFound.Count -eq $sentinelFiles.Count) {
    $distinct = @($outcomeFound | Select-Object -ExpandProperty Value -Unique)
    if ($distinct.Count -ne 1) {
        Fail "the engagement-outcome sentinel differs across documents - they have drifted:"
        foreach ($f in $outcomeFound) { Fail "    $($f.Name): $($f.Value)" }
    } else {
        Check ("outcome sentinel agrees across 3 documents: $($outcomeFound[0].Point) " `
            + "[$($outcomeFound[0].Lo), $($outcomeFound[0].Hi)] (n = $($outcomeFound[0].N))")
    }
}

# And the point estimate SHALL NOT appear bare. This is check 8's interval rule applied to a
# result that needs it more: n = 4, and the control arm takes two distinct values in the entire
# archive (§Corrections item 68(b)). A bare -0.7741 reads as a precise quantity. It is not one.
if ($outcomeFound.Count -gt 0) {
    $pt = [regex]::Escape($outcomeFound[0].Point.TrimStart('+'))
    foreach ($entry in $sentinelFiles) {
        if (-not (Test-Path $entry.File)) { continue }
        foreach ($hit in (Find-Lines (Get-Content -Path $entry.File) $pt)) {
            if ([regex]::IsMatch($hit.Line, "$pt\s*\**\s*\[")) { continue }          # carries its interval
            if ([regex]::IsMatch($hit.Line, 'outcome-damage-absorbed:')) { continue } # the sentinel
            if ([regex]::IsMatch($hit.Line, '\[[-+]?[\d.]+,\s*[-+]?[\d.]+\]')) { continue } # interval on the line
            Warn ("$($entry.Name): the outcome point estimate is quoted without its interval - " `
                + "'$($outcomeFound[0].Point)' must carry [$($outcomeFound[0].Lo), $($outcomeFound[0].Hi)]") $hit.LineNumber
        }
    }
}

# ---------------------------------------------------------------------------------------------
# 10. Every cited NUMBERED RULE must actually exist
# ---------------------------------------------------------------------------------------------
# This check exists because "§Scope authority rule 4" was cited TWELVE times across the PRD, the
# register, a Lua probe and a Python analysis tool, as the sole authority for every "closed on the
# mechanism, explicitly not on the rate" decision - and it does not exist and never has.
# §Scope authority is three unnumbered paragraphs about the FR contract, byte-identical from this
# file's first commit to the revision that caught it (§Corrections item 68(a), review finding F1).
#
# Check 12 asserts that a required section HEADING is present. Nothing asserted that a
# cross-reference RESOLVES, so an invented authority could be cited indefinitely and each new
# citation made the last one look established. This check closes that: a citation of the form
# "§<Section> rule <n>" must find a section by that name AND a numbered item <n> inside it.
Write-Host "[Cited rules resolve]"

# USE versus MENTION. A citation inside a code span is a QUOTATION of a citation somebody made -
# `§Scope authority rule 4` in a dated history entry is the document reporting what it once said,
# not asserting it now. Those must survive verbatim: §Corrections item 66(b) is the ruling that a
# dated entry records what was believed on its date and is not edited afterwards. So the
# convention this check enforces is typographic and readable: BACKTICKS MEAN MENTIONED, bare text
# means asserted. Only bare text is required to resolve.
# BOTH SPELLINGS. Of the twelve citations of the rule that did not exist, nine read
# "§Scope authority rule 4" and three read "§Scope authority's fourth rule". A check that caught
# only the first spelling would have reported nine and left three standing, which is item 66's
# defect - a count asserted rather than counted - rebuilt into the control meant to prevent it.
$ordinals = @{ 'first' = 1; 'second' = 2; 'third' = 3; 'fourth' = 4; 'fifth' = 5;
               'sixth' = 6; 'seventh' = 7; 'eighth' = 8; 'ninth' = 9; 'tenth' = 10 }
$ruleCiteRe = '§([A-Z][A-Za-z]*(?: [a-z][A-Za-z]*)*) rule (\d+)'
$ordCiteRe = "§([A-Z][A-Za-z]*(?: [a-z][A-Za-z]*)*)'s ($($ordinals.Keys -join '|')) rule"
# The code spans are stripped FIRST and every match is taken from the stripped text, because a
# single line here routinely carries both a live citation and a quoted one - the in-place
# correction markers exist to put them side by side. Matching the raw line and testing the
# stripped one reports the wrong citation, which is a lint that names an innocent line.
$ruleCites = @()
for ($i = 0; $i -lt $lines.Count; $i++) {
    $stripped = [regex]::Replace($lines[$i], '`[^`]*`', '')
    foreach ($re in @($ruleCiteRe, $ordCiteRe)) {
        foreach ($m in [regex]::Matches($stripped, $re)) {
            $ruleCites += [pscustomobject]@{ LineNumber = $i + 1; Match = $m }
        }
    }
}
$ruleResolved = 0
foreach ($cite in $ruleCites) {
    $section = $cite.Match.Groups[1].Value
    $raw = $cite.Match.Groups[2].Value
    $number = if ($ordinals.ContainsKey($raw)) { $ordinals[$raw] } else { [int]$raw }

    # Find the heading, then the span of body text before the next heading of the same or higher
    # level. A heading may carry a dated suffix ("#### Closing a row, from v1.8.53") and still be
    # the section a bare "§Closing a row" names.
    $headingIdx = -1
    $headingLevel = 0
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $hm = [regex]::Match($lines[$i], '^(#{2,4}) ' + [regex]::Escape($section) + '(\s*[,–—-].*)?$')
        if ($hm.Success) { $headingIdx = $i; $headingLevel = $hm.Groups[1].Value.Length; break }
    }
    if ($headingIdx -lt 0) {
        Fail ("cites '§$section rule $number' but there is no section '$section' in this " `
            + "document - an unresolvable authority") $cite.LineNumber
        continue
    }

    $hasNumberedItem = $false
    for ($i = $headingIdx + 1; $i -lt $lines.Count; $i++) {
        $nh = [regex]::Match($lines[$i], '^(#{2,4}) ')
        if ($nh.Success -and $nh.Groups[1].Value.Length -le $headingLevel) { break }
        if ([regex]::IsMatch($lines[$i], '^\s*(\*\*)?' + $number + '\.')) { $hasNumberedItem = $true; break }
    }
    if (-not $hasNumberedItem) {
        Fail ("cites '§$section rule $number', and §$section exists but has no numbered item " `
            + "$number - the rule does not exist") $cite.LineNumber
    } else {
        $ruleResolved++
    }
}
Check "cited numbered rules resolve: $ruleResolved of $($ruleCites.Count)"

# ---------------------------------------------------------------------------------------------
# 11. The published gate figures must agree, and the file count must be true
# ---------------------------------------------------------------------------------------------
# README's gate table claimed "105 files" and "162 / 162" under a heading asserting the figures
# were current. The real numbers were 110 and 169/169 - the file count had been false since before
# the date the table stamps itself with (§Corrections item 69(f)). It is the same defect as the
# acceptance figure's four staleness events, in the one document a reader meets first.
#
# The tracked-file count is not pinned by agreement - it is COMPUTED here, so it cannot go stale.
Write-Host "[Gate figures]"

$gatePattern = '<!--\s*gate-figures:\s*unit=(\d+)/(\d+)\s+artifact-smoke=(\d+)/(\d+)\s+live-smoke=(\d+)/(\d+)\s+tracked-files=(\d+)\s*-->'
$gateFound = @()
foreach ($entry in @($sentinelFiles[0], $sentinelFiles[2])) {   # PRD is canonical, README quotes it
    if (-not (Test-Path $entry.File)) { continue }
    $hit = @(Find-Lines (Get-Content -Path $entry.File) $gatePattern) | Select-Object -First 1
    if (-not $hit) {
        Fail "$($entry.Name) carries no gate-figures sentinel"
    } else {
        $gateFound += [pscustomobject]@{
            Name = $entry.Name
            Value = $hit.Match.Groups[0].Value.Trim()
            Tracked = [int]$hit.Match.Groups[7].Value
            Unit = "$($hit.Match.Groups[1].Value)/$($hit.Match.Groups[2].Value)"
        }
    }
}

if ($gateFound.Count -eq 2) {
    $distinct = @($gateFound | Select-Object -ExpandProperty Value -Unique)
    if ($distinct.Count -ne 1) {
        Fail "the gate-figures sentinel differs between the PRD and README - they have drifted:"
        foreach ($f in $gateFound) { Fail "    $($f.Name): $($f.Value)" }
    } else {
        # The one figure that can be recomputed from a bare checkout, so it is.
        $repoDir = Split-Path -Parent (Split-Path -Parent $Path)
        $tracked = $null
        try {
            Push-Location $repoDir
            $tracked = @(git ls-files 2>$null).Count
        } catch { $tracked = $null } finally { Pop-Location }

        if ($null -eq $tracked -or $tracked -eq 0) {
            Warn "cannot run 'git ls-files' here - the tracked-file count went unverified"
            Check "gate figures agree across PRD and README: unit $($gateFound[0].Unit)"
        } elseif ($tracked -ne $gateFound[0].Tracked) {
            Fail ("the gate-figures sentinel claims tracked-files=$($gateFound[0].Tracked) but " `
                + "'git ls-files' counts $tracked - the published count is stale")
        } else {
            Check ("gate figures agree across PRD and README, and tracked-files=$tracked is " `
                + "confirmed against git")
        }
    }
}

# ---------------------------------------------------------------------------------------------
# 12. Required sections for the Comprehensive tier
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
# 15. README's authorization section must name the standing grant
# ---------------------------------------------------------------------------------------------
# For six revisions README's §Authorization read "the hosted backend cannot ship" and "there is
# therefore no authorization under which an operator other than the measurer may turn the hosted
# backend on" - both made false by the standing grant of 2026-08-09 (PRD v1.8.48). Over the same
# period the status table showed C23 and C17 open, thirteen revisions after both closed.
#
# This is the README-lags-PRD failure of §Corrections items 68(e) and 69(f), and v1.8.54 is its
# third occurrence in the one document a reader meets first - so it stops being a rule someone has
# to remember. The check is deliberately narrow: it does not validate the section's prose, only that
# it still names the revision that granted what it describes, that the specific falsified sentences
# have not come back, and that the one boundary the grant does NOT release is stated.
Write-Host "[README authorization currency]"

$readmePath = Join-Path (Split-Path -Parent $repoRootForDocs) "README.md"
if (-not (Test-Path $readmePath)) {
    Warn "README.md not found - the authorization-currency check went unrun"
    Check "README authorization: unchecked"
} else {
    $readmeText = Get-Content -Path $readmePath -Raw
    $authBlock = [regex]::Match($readmeText, '(?sm)^##\s+Authorization.*?(?=^##\s)')
    $authFailures = 0

    if (-not $authBlock.Success) {
        Fail "README.md has no '## Authorization' section - the egress posture is the one thing a reader must not have to infer"
        $authFailures++
    } else {
        # Dated correction markers QUOTE the sentences they retire - that is what a correction marker
        # is for, and this project keeps superseded text in place on purpose ("the original text is
        # kept because it is the argument that got the decision made"). Scanning them would make the
        # honest repair fail the check and reward deleting the history instead, so they are stripped
        # before the revoked-claim scan. Only italic parentheticals that announce themselves as
        # corrections are removed; ordinary prose is scanned in full.
        $block = [regex]::Replace(
            $authBlock.Value,
            '(?s)\*\(\s*\*\*Corrected.*?\)\*',
            '')

        # The grant actually in force. Bump both if a later grant supersedes v1.8.48.
        if ($block -notmatch 'v1\.8\.48') {
            Fail ("README's §Authorization does not cite the standing grant's PRD revision " `
                + "(v1.8.48) - it went stale for six revisions once already")
            $authFailures++
        }
        if ($block -notmatch '2026-08-09') {
            Fail "README's §Authorization does not carry the standing grant's date (2026-08-09)"
            $authFailures++
        }

        # The sentences the grant falsified, and the count it obsoleted.
        $revoked = @(
            'the hosted backend cannot ship',
            'no authorization under which an operator other than the measurer',
            'All six egress grants to date'
        )
        foreach ($s in $revoked) {
            if ($block -match [regex]::Escape($s)) {
                Fail "README's §Authorization still asserts a claim the standing grant falsified: `"$s`""
                $authFailures++
            }
        }

        # The one boundary the standing grant explicitly does NOT release. Anyone deciding whether to
        # share this repository must not have to reach the PRD to find that out.
        if ($block -notmatch 'does not authorize publication') {
            Fail ("README's §Authorization must state that the grant does NOT authorize publication " `
                + "of the repository - it is the boundary most likely to be assumed released")
            $authFailures++
        }
    }

    if ($authFailures -eq 0) {
        Check "README authorization cites the standing grant (v1.8.48, 2026-08-09) and its publication boundary"
    }
}

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
