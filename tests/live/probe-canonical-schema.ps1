# Tests PRD Corrections item 19 against the live API. THE ONLY THING THAT CAN.
#
# THIS SCRIPT MAKES LIVE REQUESTS TO api.anthropic.com. Like everything under tests/live/ it is a
# manual gate and must never enter CI.
#
# WHY IT EXISTS, and why `--mode schema` is not sufficient.
#
# Corrections item 19 records, explicitly as A PREDICTION FROM DOCUMENTATION AND NOT A MEASUREMENT,
# that the hosted structured-output path rejects `minimum`/`maximum`/`minLength`/`maxLength` and
# does not list `oneOf`. The adapter therefore sends orderJsonSchemaForStructuredOutputs(), a
# mechanical projection with exactly those keywords stripped.
#
# So driving the adapter tests the PROJECTION. It cannot test the PREDICTION - the keywords the
# prediction is about are not in the request the adapter builds. Watching the adapter succeed and
# concluding "the prediction is refuted" is a category error, and it is one this loop made for about
# ten minutes before catching it.
#
# The only way to settle item 19 is to post the CANONICAL document and see what the API says. This
# script posts both, in order, and reports each verbatim.
#
# WHAT IT TRANSMITS: two JSON schema documents and a fixed placeholder user message. The schemas are
# compiled into the DLL and contain no scenario state; the message is a literal string written here.
# NO snapshot, no position, no velocity, no track, no loadout. This probe is strictly narrower than
# the v1.8.3 grant it runs under.

[CmdletBinding()]
param(
    # Resolved through N8RO_RELEASE_ROOT rather than hardcoded - see count-prefix-tokens.ps1.
    [string]$CanonicalFile  = (Join-Path $(if ($env:N8RO_RELEASE_ROOT) { $env:N8RO_RELEASE_ROOT } else { "C:\N8RO" }) "schema-canonical.json"),
    [string]$ProjectionFile = (Join-Path $(if ($env:N8RO_RELEASE_ROOT) { $env:N8RO_RELEASE_ROOT } else { "C:\N8RO" }) "schema-projection.json"),
    [string]$Model          = "claude-haiku-4-5",
    [string]$KeyEnvVar      = "ANTHROPIC_API_KEY",
    [string]$BaseUrl        = "https://api.anthropic.com"
)

$ErrorActionPreference = "Stop"

$key = [Environment]::GetEnvironmentVariable($KeyEnvVar)
if ([string]::IsNullOrWhiteSpace($key)) {
    Write-Error "environment variable '$KeyEnvVar' is unset or empty - no API key available"
}

$headers = @{
    "x-api-key"         = $key
    "anthropic-version" = "2023-06-01"
    "content-type"      = "application/json"
}

# Deliberately trivial and fixed. The probe is about whether the SCHEMA is accepted, which the API
# decides before it infers anything, so the message only has to exist.
$userMessage = "Emit one order."

function Invoke-SchemaProbe {
    param([string]$Label, [string]$SchemaPath)

    Write-Host ""
    Write-Host ("=== {0} ===" -f $Label)
    if (-not (Test-Path $SchemaPath)) {
        Write-Host ("  SKIPPED - {0} not found. Run: ai-commander-live-tests.exe --mode dump-schemas" -f $SchemaPath)
        return
    }

    $schemaText = [System.IO.File]::ReadAllText($SchemaPath)
    $schema     = $schemaText | ConvertFrom-Json

    # Report which of the predicted keywords are actually present, so the result below is read
    # against what was sent rather than against an assumption about what was sent.
    $present = @()
    foreach ($keyword in @("minimum", "maximum", "multipleOf", "minLength", "maxLength", "oneOf", "anyOf")) {
        if ($schemaText -match ('"{0}"' -f $keyword)) { $present += $keyword }
    }
    Write-Host ("  file            {0} ({1:N0} bytes)" -f $SchemaPath, $schemaText.Length)
    Write-Host ("  keywords present {0}" -f ($present -join ", "))

    $body = @{
        model         = $Model
        max_tokens    = 512
        messages      = @(@{ role = "user"; content = $userMessage })
        output_config = @{ format = @{ type = "json_schema"; schema = $schema } }
    } | ConvertTo-Json -Depth 40 -Compress

    try {
        $response = Invoke-RestMethod -Method Post -Uri "$BaseUrl/v1/messages" `
                                      -Headers $headers -Body $body -TimeoutSec 90
        Write-Host "  RESULT          ACCEPTED (HTTP 200)"
        Write-Host ("  stop_reason     {0}" -f $response.stop_reason)
        Write-Host ("  usage           in {0}, out {1}" -f $response.usage.input_tokens, $response.usage.output_tokens)
    } catch {
        # Read the response STREAM, not $_.ErrorDetails.Message - that property is routinely blank
        # on Windows PowerShell 5.1 for a non-2xx and turns a legible API error into a bare status
        # code. Corrections item 20 records this costing a round trip already.
        $resp = $_.Exception.Response
        if ($resp) {
            Write-Host ("  RESULT          REJECTED (HTTP {0})" -f $resp.StatusCode.value__)
            try {
                $reader = New-Object System.IO.StreamReader($resp.GetResponseStream())
                $errorBody = $reader.ReadToEnd()
                Write-Host  "  body, verbatim:"
                Write-Host ("    {0}" -f $errorBody)
            } catch {
                Write-Host "  body            (could not be read)"
            }
        } else {
            Write-Host ("  RESULT          TRANSPORT FAILURE - {0}" -f $_.Exception.Message)
        }
    }
}

Write-Host "=== Corrections item 19: does the hosted path reject the canonical schema's keywords? ==="
Write-Host ("  model         {0}" -f $Model)
Write-Host  "  transmitted   two schema documents + a fixed placeholder message. No scenario state."
Write-Host  ""
Write-Host  "  PREDICTION (PRD v1.8, recorded as a prediction and not a measurement):"
Write-Host  "    the hosted structured-output path REJECTS minimum/maximum/minLength/maxLength,"
Write-Host  "    and oneOf is not on its supported list."

Invoke-SchemaProbe -Label "CANONICAL document (the one the prediction is about)" -SchemaPath $CanonicalFile
Invoke-SchemaProbe -Label "PROJECTION (what the adapter actually sends)"        -SchemaPath $ProjectionFile

# -- the third document, and the reason it exists ------------------------------------------------
#
# The prediction has TWO halves: that the bound keywords are rejected, and that `oneOf` is not
# supported. The canonical document carries both, so a rejection naming `oneOf` settles the second
# half and leaves the first UNTESTED - the request never got far enough to be judged on `minimum`.
#
# The projection changes both things at once (oneOf -> anyOf, and bounds stripped), so comparing it
# against the canonical cannot separate them either. Isolating the bounds needs a document that
# differs from the canonical in exactly one way: `oneOf` renamed to `anyOf`, every bound left in
# place. If THAT is accepted, the bounds half of the prediction is refuted and only the `oneOf`
# half was ever true.
$hybridPath = Join-Path ([System.IO.Path]::GetDirectoryName($CanonicalFile)) "schema-hybrid.json"
if (Test-Path $CanonicalFile) {
    $hybrid = [System.IO.File]::ReadAllText($CanonicalFile).Replace('"oneOf"', '"anyOf"')
    [System.IO.File]::WriteAllText($hybridPath, $hybrid)
    Invoke-SchemaProbe -Label "HYBRID (canonical with oneOf->anyOf, ALL bounds retained)" -SchemaPath $hybridPath
}

Write-Host ""
Write-Host "=== how to read this ==="
Write-Host "  canonical REJECTED + projection ACCEPTED -> prediction CONFIRMED; the projection is"
Write-Host "                                              load-bearing and AIC-BE-1's restatement"
Write-Host "                                              was necessary."
Write-Host "  canonical ACCEPTED + projection ACCEPTED -> prediction REFUTED; the projection is"
Write-Host "                                              insurance, not a fix. Correct item 19."
Write-Host "                                              Do NOT delete the projection on this"
Write-Host "                                              alone - it costs nothing and the"
Write-Host "                                              documented support list still omits the"
Write-Host "                                              keywords, so acceptance today is not a"
Write-Host "                                              contract for tomorrow."
Write-Host "  canonical REJECTED for some OTHER reason -> its own finding. Report the body."
