# Measures the deployed stable prefix against Anthropic's tokenizer, for OQ-8.
#
# THIS SCRIPT MAKES A LIVE REQUEST TO api.anthropic.com. Like everything else under tests/live/ it
# is a manual gate and must never enter CI - the PRD's CI requirement is explicit that the unit,
# stubbed-integration, and replay suites run with no inference server and no network.
#
# WHAT IT TRANSMITS: the stable prefix and nothing else. That file is produced by
# `ai-commander-live-tests.exe --mode prefix`, which writes renderer.prefix() alone - the system
# prompt, the posture and ROE vocabulary, the order schema, and data/doctrine.txt. It carries NO
# volatile suffix and therefore no position, velocity, heading, team, track, or loadout. The owner's
# authorization (PRD v1.8.1, recorded in the Phase 2 gate) is scoped to exactly this.
#
# WHY IT EXISTS: OQ-8 asks whether the deployed prefix clears the configured model's prompt-cache
# minimum. There is no offline Claude tokenizer, and a byte-to-token ratio taken from a local model
# is the estimate the question already refuses. count_tokens is the only authority.
#
# The count_tokens endpoint is a utility endpoint rather than an inference call, so it is expected to
# be unbilled - which is why this can run on a zero-balance key. If that assumption is wrong the
# request fails with a billing error rather than quietly charging, and the failure is reported here.

[CmdletBinding()]
param(
    # Resolved through N8RO_RELEASE_ROOT like every other script here, rather than hardcoding a
    # release path. These two probes were the last files in the repository naming C:\N8RO with no
    # environment fallback (v1.8.57, item 73(k)).
    [string]$PrefixFile = (Join-Path $(if ($env:N8RO_RELEASE_ROOT) { $env:N8RO_RELEASE_ROOT } else { "C:\N8RO" }) "prefix.txt"),
    [string]$Model = "claude-haiku-4-5",
    [string]$KeyEnvVar = "ANTHROPIC_API_KEY",
    [string]$BaseUrl = "https://api.anthropic.com",

    # The prompt-cache minimum for the model above, in tokens. Haiku 4.5 is 4096. This is the
    # threshold OQ-8 is asked against; a prefix below it silently does not cache - no error, no
    # counter, just a bill that never gets the discount.
    [int]$CacheMinimumTokens = 4096
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $PrefixFile)) {
    Write-Error "prefix file not found: $PrefixFile. Run: ai-commander-live-tests.exe --mode prefix"
}

# Read as raw bytes then decode, so the byte count reported here is the byte count the engine logged
# as prefixBytes, with no encoding round-trip in between.
$bytes  = [System.IO.File]::ReadAllBytes($PrefixFile)
$prefix = [System.Text.Encoding]::UTF8.GetString($bytes)

$key = [Environment]::GetEnvironmentVariable($KeyEnvVar)
if ([string]::IsNullOrWhiteSpace($key)) {
    Write-Error "environment variable '$KeyEnvVar' is unset or empty - no API key available"
}

Write-Host "=== OQ-8: deployed prefix vs the model's prompt-cache minimum ==="
Write-Host ("  prefix file   {0}" -f $PrefixFile)
Write-Host ("  prefix bytes  {0:N0}" -f $bytes.Length)
Write-Host ("  model         {0}" -f $Model)
Write-Host  "  transmitted   stable prefix only - no snapshot, no scenario state"
Write-Host ""

$body = @{
    model    = $Model
    messages = @(@{ role = "user"; content = $prefix })
} | ConvertTo-Json -Depth 6 -Compress

# The key goes on x-api-key and nowhere else. It is never written to a file, never echoed, and never
# included in any output this script produces.
$headers = @{
    "x-api-key"         = $key
    "anthropic-version" = "2023-06-01"
    "content-type"      = "application/json"
}

try {
    $response = Invoke-RestMethod -Method Post -Uri "$BaseUrl/v1/messages/count_tokens" `
                                  -Headers $headers -Body $body -TimeoutSec 60
} catch {
    # Read the response STREAM, not $_.ErrorDetails.Message. On Windows PowerShell 5.1 that property
    # is frequently empty for a non-2xx from Invoke-RestMethod, which turns a perfectly legible API
    # error into a bare status code. The first run of this script hit exactly that and cost a
    # round trip to diagnose - the body said "credit balance is too low" and the script printed
    # nothing.
    Write-Host "REQUEST FAILED"
    $resp = $_.Exception.Response
    if ($resp) {
        Write-Host ("  HTTP status : {0}" -f $resp.StatusCode.value__)
        try {
            $reader = New-Object System.IO.StreamReader($resp.GetResponseStream())
            Write-Host ("  body        : {0}" -f $reader.ReadToEnd())
        } catch {
            Write-Host "  body        : (could not be read)"
        }
    } else {
        Write-Host ("  transport   : {0}" -f $_.Exception.Message)
    }
    Write-Host ""
    Write-Host "A 400 naming the credit balance means count_tokens is NOT reachable on a zero-balance"
    Write-Host "account. The endpoint may well be unbilled, but the API gateway refuses access before"
    Write-Host "that matters. OQ-8 needs a funded account. Report it; do not retry."
    exit 1
}

$tokens = $response.input_tokens
$ratio  = [math]::Round($bytes.Length / $tokens, 3)

Write-Host ("  MEASURED      {0:N0} input tokens" -f $tokens)
Write-Host ("  bytes/token   {0}" -f $ratio)
Write-Host ""
Write-Host ("  cache minimum {0:N0} tokens ({1})" -f $CacheMinimumTokens, $Model)

if ($tokens -ge $CacheMinimumTokens) {
    Write-Host ("  RESULT        CLEARS the minimum by {0:N0} tokens" -f ($tokens - $CacheMinimumTokens))
    Write-Host  "                The prefix caches AS WRITTEN. Padding delta is zero."
} else {
    Write-Host ("  RESULT        SHORT of the minimum by {0:N0} tokens" -f ($CacheMinimumTokens - $tokens))
    Write-Host  "                The prefix does NOT cache. The padding judgement is live again."
}

# -- the cost arithmetic OQ-8 exists to settle ---------------------------------------------------
# Haiku 4.5: 1 USD/MTok in, 5 USD/MTok out. Cache reads bill at 0.1x base, writes at 1.25x.
# An order is the prefix + a ~200-token volatile suffix + ~80 output tokens.
# A four-ship scenario-hour at a 20 s cadence is 4 x 180 = 720 orders.
$suffixTokens = 200
$outputTokens = 80
$ordersPerHour = 720

$uncachedPerOrder = (($tokens + $suffixTokens) * 1.0 / 1e6) + ($outputTokens * 5.0 / 1e6)
$cachedPerOrder   = ($tokens * 0.10 / 1e6) + ($suffixTokens * 1.0 / 1e6) + ($outputTokens * 5.0 / 1e6)

Write-Host ""
Write-Host "=== cost, recomputed off the MEASURED count ==="
Write-Host ("  uncached      {0} per order    {1} per four-ship-hour" -f `
    [math]::Round($uncachedPerOrder, 5), [math]::Round($uncachedPerOrder * $ordersPerHour, 2))
Write-Host ("  cached        {0} per order    {1} per four-ship-hour" -f `
    [math]::Round($cachedPerOrder, 5), [math]::Round($cachedPerOrder * $ordersPerHour, 2))
Write-Host  "  target        1.10 per four-ship-hour (PRD Success metrics)"

$applicable = if ($tokens -ge $CacheMinimumTokens) { $cachedPerOrder } else { $uncachedPerOrder }
$hourly = [math]::Round($applicable * $ordersPerHour, 2)
if ($hourly -le 1.10) {
    Write-Host ("  VERDICT       MET - {0} per four-ship-hour" -f $hourly)
} else {
    Write-Host ("  VERDICT       MISSED - {0} per four-ship-hour" -f $hourly)
}
