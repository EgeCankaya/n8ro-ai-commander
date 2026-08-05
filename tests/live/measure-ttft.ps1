# Measures TIME TO FIRST TOKEN, separately from total round trip, for C2.
#
# THIS SCRIPT MAKES LIVE REQUESTS TO api.anthropic.com. Like everything else under tests/live/ it is
# a manual gate and must never enter CI - the PRD's CI requirement is explicit that the unit,
# stubbed-integration and replay suites run with no inference server and no network.
#
# WHAT IT TRANSMITS: the request bodies written by `ai-commander-live-tests.exe --mode ttft-dump`,
# unmodified except for one added field. Those bodies carry the volatile suffix - position,
# velocity, heading, team, reported tracks, loadout. Authorized by the fifth egress grant
# (PRD v1.8.11); the fixtures are the six synthetic LiveMain situations.
#
# WHY IT EXISTS. C2 is a p95 miss (4,615 ms against a 2.5 s target) that two rounds of measurement
# have failed to decompose. PRD §Corrections item 28(f) shows why more of the same will not work:
# the total-latency estimator has a residual SD of 1,482 ms, so the INTERCEPT - the fixed
# per-request term the doctrine comparison is actually about - has a 95 % interval of +/- 776 ms at
# n=48. Two arms would need an intercept gap above ~1,100 ms to resolve, and reaching that by
# sample size costs ~$10 against a $5 budget.
#
# TTFT is not a bigger sample. It is a LOWER-VARIANCE ESTIMATOR: it stops the clock before
# generation instead of averaging generation away, and generation is where the 1,482 ms of scatter
# lives (57 % of the mean round trip). The fixed term is what TTFT measures directly.
#
# WHY THE BODY IS NOT BUILT HERE, which is the whole design. §Corrections item 22 records what
# happens when a probe measures something adjacent to the real request: OQ-8 counted the prefix
# correctly, against the right tokenizer, and got a number for the wrong object, because the
# adapter sends an entire second copy of the schema after the renderer stops. A TTFT probe that
# composed its own JSON would repeat that error exactly. So the bodies come from the adapter's own
# construction path, captured at the transport boundary by --mode ttft-dump, and THE ONLY EDIT
# MADE HERE IS ADDING "stream": true. That edit is asserted below rather than assumed.
#
# STREAMING AND SCOPE. §Out of scope's streaming row is unchanged and still governs the product:
# an order is atomic and the adapter does not stream. This is an instrument, and it has to live
# here because it cannot live there - n8ro::core::IHttpClient::send() is blocking and returns a
# whole body, with no chunk callback and no headers-received hook.
#
# A NULL RESULT IS A REAL RESULT. The fifth grant records in advance that the fixed term may still
# sit below the noise floor even in TTFT. If the arms do not separate, that is the finding, and it
# is not grounds for a third pair of arms.

[CmdletBinding()]
param(
    # Directory holding ttft-request-NN.json, written by --mode ttft-dump.
    [Parameter(Mandatory = $true)]
    [string]$RequestDir,

    # Free-text label for this arm, written into every CSV row. The comparison is between two runs
    # of this script over two dumps that differ ONLY in doctrine size, so the arm has to be
    # recorded per row or the two CSVs cannot be pooled.
    [Parameter(Mandatory = $true)]
    [string]$Arm,

    [string]$KeyEnvVar = "ANTHROPIC_API_KEY",
    [string]$BaseUrl   = "https://api.anthropic.com",

    # Repeats per fixture. 8 x 6 fixtures = 48 orders, the same shape as every prior hosted run,
    # which keeps this arm poolable with them.
    [int]$Repeats = 8,

    [string]$Csv = "ttft.csv",

    # Seconds. Matches commander.requestTimeoutS.
    [int]$TimeoutS = 90
)

$ErrorActionPreference = "Stop"

# -- inputs -----------------------------------------------------------------------------------

$requests = @(Get-ChildItem -Path $RequestDir -Filter "ttft-request-*.json" | Sort-Object Name)
if ($requests.Count -eq 0) {
    Write-Error "no ttft-request-*.json in $RequestDir. Run: ai-commander-live-tests.exe --mode ttft-dump --backend claude"
}

$key = [Environment]::GetEnvironmentVariable($KeyEnvVar)
if ([string]::IsNullOrWhiteSpace($key)) {
    Write-Error "environment variable '$KeyEnvVar' is unset or empty - no API key available"
}

Add-Type -AssemblyName System.Net.Http

# Two .NET Framework defaults that would otherwise be measured as if they were the service.
#
# Expect100Continue makes the stack send headers, WAIT for a 100-continue, and only then send the
# body. On a 27 KB body against a TLS endpoint that inserts a round trip into the exact interval
# this script is trying to measure - and it lands in the FIXED term, which is the term C2 is about.
# The adapter does not do this: n8ro's IHttpClient implementation is a different stack entirely, so
# leaving it on would measure a property of PowerShell.
#
# DefaultConnectionLimit is 2 by default. Requests here are sequential so it should not bind, but a
# connection that is being recycled under a limit is a fixed stall that looks exactly like a slow
# service, and the first smoke run showed two samples pinned at ~30,500 ms.
[System.Net.ServicePointManager]::Expect100Continue = $false
[System.Net.ServicePointManager]::DefaultConnectionLimit = 10

Write-Host "=== C2: time to first token, separated from generation ==="
Write-Host ("  arm           {0}" -f $Arm)
Write-Host ("  requests      {0} fixtures x {1} repeats = {2} orders" -f $requests.Count, $Repeats, ($requests.Count * $Repeats))
Write-Host ("  base url      {0}" -f $BaseUrl)
Write-Host ("  key from      `${0}  (name only; value never logged)" -f $KeyEnvVar)
Write-Host  "  EGRESS        LIVE. Volatile suffix: position, velocity, heading, team, tracks,"
Write-Host  "                loadout, from the six synthetic LiveMain fixtures. PRD v1.8.11."
Write-Host ""

# -- prepare the bodies -------------------------------------------------------------------------
#
# Read as raw bytes then decode, so what is sent is what --mode ttft-dump wrote, with no encoding
# round-trip in between. The stream flag is inserted textually at the top-level opening brace
# rather than by ConvertTo-Json, because a parse/re-serialize cycle would silently renormalise
# escaping, key order and number formatting - and then the body measured here would not be the
# body the adapter builds, which is the one thing this probe must not get wrong.

$prepared = @()
foreach ($file in $requests) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $text  = [System.Text.Encoding]::UTF8.GetString($bytes)

    if ($text.Substring(0, 1) -ne "{") {
        Write-Error "$($file.Name) does not begin with '{' - not a request body"
    }
    if ($text -match '"stream"') {
        Write-Error "$($file.Name) already carries a stream field - refusing to double-edit it"
    }

    $streamed = '{"stream":true,' + $text.Substring(1)

    # Assert the edit did exactly one thing. Everything else about the body must be byte-identical
    # to what the adapter built, and this is the check that says so rather than trusting it.
    $expectedDelta = '{"stream":true,'.Length - 1
    if (($streamed.Length - $text.Length) -ne $expectedDelta) {
        Write-Error "$($file.Name): the stream edit changed $($streamed.Length - $text.Length) bytes, expected $expectedDelta"
    }

    $probe = ConvertFrom-Json $text
    $prepared += [pscustomobject]@{
        Name      = $file.Name
        Body      = $streamed
        Model     = $probe.model
        MaxTokens = $probe.max_tokens
        Bytes     = $bytes.Length
    }
}

Write-Host ("  model         {0}   max_tokens {1}" -f $prepared[0].Model, $prepared[0].MaxTokens)
Write-Host ("  body bytes    {0:N0} - {1:N0}  (adapter-built; only `"stream`":true added)" -f `
            ($prepared | Measure-Object Bytes -Minimum).Minimum, ($prepared | Measure-Object Bytes -Maximum).Maximum)
Write-Host ""

# -- the measurement ----------------------------------------------------------------------------

$handler = New-Object System.Net.Http.HttpClientHandler
$client  = New-Object System.Net.Http.HttpClient($handler)
$client.Timeout = [TimeSpan]::FromSeconds($TimeoutS)
$client.DefaultRequestHeaders.Add("x-api-key", $key)
$client.DefaultRequestHeaders.Add("anthropic-version", "2023-06-01")

$rows = @()
$failures = 0

try {
    for ($r = 1; $r -le $Repeats; $r++) {
        foreach ($p in $prepared) {
            $content = New-Object System.Net.Http.StringContent($p.Body, [System.Text.Encoding]::UTF8, "application/json")

            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            $ttftMs = -1
            $headersMs = -1
            $tokensOut = 0
            $tokensIn = 0
            $cacheRead = 0
            $stopReason = ""
            $deltas = 0
            $errText = ""

            try {
                # SendAsync, not PostAsync, and this is not a style choice. ResponseHeadersRead is
                # the load-bearing flag here - without it HttpClient buffers the ENTIRE response
                # before returning, TTFT would come back identical to total, and the instrument
                # would report a clean-looking null while measuring nothing. PostAsync has no
                # HttpCompletionOption overload at all; only SendAsync does.
                $req = New-Object System.Net.Http.HttpRequestMessage(
                    [System.Net.Http.HttpMethod]::Post, "$BaseUrl/v1/messages")
                $req.Content = $content

                $resp = $client.SendAsync($req,
                                          [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
                $headersMs = $sw.Elapsed.TotalMilliseconds

                if (-not $resp.IsSuccessStatusCode) {
                    $errText = "HTTP " + [int]$resp.StatusCode + " " + `
                               $resp.Content.ReadAsStringAsync().GetAwaiter().GetResult()
                    Write-Host ("  [WARN] {0} rep {1}: {2}" -f $p.Name, $r, $errText)
                    $failures++
                    $resp.Dispose()
                    $req.Dispose()
                    continue
                }

                $stream = $resp.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
                $reader = New-Object System.IO.StreamReader($stream)

                while (-not $reader.EndOfStream) {
                    $line = $reader.ReadLine()
                    if (-not $line.StartsWith("data: ")) { continue }
                    $payload = $line.Substring(6)
                    if ($payload -eq "[DONE]") { break }

                    $evt = ConvertFrom-Json $payload

                    # THE MEASUREMENT. content_block_delta is the first event carrying generated
                    # text. message_start arrives earlier and carries usage, but it is emitted
                    # before the model has produced a token - stopping the clock there would
                    # measure the service's acknowledgement, not its first token.
                    #
                    # READ THIS BEFORE READING ANY NUMBER THIS SCRIPT PRODUCES. Under structured
                    # outputs the service does NOT emit one delta per token: a ~560-token response
                    # was observed arriving in SIX deltas. So `ttftMs` is time-to-first-CHUNK, and
                    # the first chunk already contains many tokens' worth of generation. It is
                    # therefore an UPPER BOUND on the true fixed term, not the fixed term - and
                    # `deltas` below is recorded so a reader can see how coarse the granularity
                    # was rather than having to trust that it was fine.
                    if ($evt.type -eq "content_block_delta") {
                        $deltas++
                        if ($ttftMs -lt 0) { $ttftMs = $sw.Elapsed.TotalMilliseconds }
                    }
                    if ($evt.type -eq "message_start") {
                        $tokensIn  = [int]$evt.message.usage.input_tokens
                        $cacheRead = [int]$evt.message.usage.cache_read_input_tokens
                    }
                    if ($evt.type -eq "message_delta") {
                        $tokensOut  = [int]$evt.usage.output_tokens
                        $stopReason = [string]$evt.delta.stop_reason
                    }
                }

                $reader.Dispose()
                $resp.Dispose()
                $req.Dispose()
            }
            catch {
                $errText = $_.Exception.GetBaseException().Message
                Write-Host ("  [WARN] {0} rep {1}: {2}" -f $p.Name, $r, $errText)
                $failures++
                continue
            }
            finally {
                $sw.Stop()
                $content.Dispose()
            }

            if ($ttftMs -lt 0) {
                # A completed stream that never emitted a content delta. Recorded as a failure
                # rather than as a zero, because a zero would pool into the mean as a very fast
                # request and quietly bias the estimator this whole exercise exists to sharpen.
                $failures++
                Write-Host ("  [WARN] {0} rep {1}: stream carried no content_block_delta" -f $p.Name, $r)
                continue
            }

            $rows += [pscustomobject]@{
                arm            = $Arm
                fixture        = $p.Name
                repeat         = $r
                ttftMs         = [int][Math]::Round($ttftMs)
                headersMs      = [int][Math]::Round($headersMs)
                totalMs        = [int][Math]::Round($sw.Elapsed.TotalMilliseconds)
                afterFirstMs   = [int][Math]::Round($sw.Elapsed.TotalMilliseconds - $ttftMs)
                deltas         = $deltas
                tokensIn       = $tokensIn
                tokensOut      = $tokensOut
                cacheReadTokens= $cacheRead
                stopReason     = $stopReason
            }
        }
        Write-Host ("  repeat {0}/{1} done  ({2} rows)" -f $r, $Repeats, $rows.Count)
    }
}
finally {
    $client.Dispose()
    $handler.Dispose()
}

if ($rows.Count -eq 0) {
    Write-Error "no rows measured ($failures failures) - nothing to report"
}

$rows | Export-Csv -Path $Csv -NoTypeInformation -Encoding utf8

# -- report -------------------------------------------------------------------------------------
#
# Reported per arm rather than pooled: the comparison this instrument exists for is BETWEEN two
# runs of this script, and pooling here would hide the quantity being compared.

function Pct($values, $p) {
    $sorted = @($values | Sort-Object)
    if ($sorted.Count -eq 0) { return 0 }
    $idx = [int][Math]::Ceiling($sorted.Count * $p / 100.0) - 1
    if ($idx -lt 0) { $idx = 0 }
    if ($idx -ge $sorted.Count) { $idx = $sorted.Count - 1 }
    return $sorted[$idx]
}

function Stats($name, $values) {
    $m  = ($values | Measure-Object -Average -Minimum -Maximum)
    $mean = $m.Average
    $sd = 0.0
    if ($values.Count -gt 1) {
        $ss = 0.0
        foreach ($v in $values) { $ss += [Math]::Pow($v - $mean, 2) }
        $sd = [Math]::Sqrt($ss / ($values.Count - 1))
    }
    Write-Host ("  {0,-14} mean {1,7:N0}  SD {2,6:N0}  p50 {3,7:N0}  p95 {4,7:N0}  min {5,6:N0}  max {6,6:N0}" -f `
                $name, $mean, $sd, (Pct $values 50), (Pct $values 95), $m.Minimum, $m.Maximum)
}

Write-Host ""
Write-Host "=== arm '$Arm' - $($rows.Count) orders, $failures failures ==="
Stats "TTFT"        @($rows | ForEach-Object { $_.ttftMs })
Stats "headers"     @($rows | ForEach-Object { $_.headersMs })
Stats "afterFirst"  @($rows | ForEach-Object { $_.afterFirstMs })
Stats "deltas"      @($rows | ForEach-Object { $_.deltas })
Stats "total"       @($rows | ForEach-Object { $_.totalMs })
Stats "tokensOut"   @($rows | ForEach-Object { $_.tokensOut })

$cacheHits = @($rows | Where-Object { $_.cacheReadTokens -gt 0 }).Count
Write-Host ""
Write-Host ("  cache reads   {0}/{1} orders at {2:N0} tokens" -f `
            $cacheHits, $rows.Count, (@($rows | Where-Object { $_.cacheReadTokens -gt 0 } | ForEach-Object { $_.cacheReadTokens }) | Select-Object -First 1))
Write-Host ("  csv           {0}" -f (Resolve-Path $Csv))
Write-Host ""
Write-Host "  TTFT is the fixed-term estimator. Compare it ACROSS arms, not against the total"
Write-Host "  within one - and read the SD column before reading the mean difference."
