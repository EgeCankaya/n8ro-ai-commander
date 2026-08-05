# Post-merge review — PRs #12 and #13 (`0e9fa30..ddb459e`)

**Audience:** whoever picks up the fix-forward work on `main`.
**Scope:** six non-merge commits, PRD v1.8.9 → v1.8.15, spanning PRs #12 and #13. Both were merged
without review. Everything here is already on `main`, so every finding is fix-forward — nothing
below is a blocking objection to a change that has not landed.
**Status:** written 2026-08-05 against `ddb459e`. Every gate was re-run at review time and all were
green: `lint-prd.ps1` 9 checks / 0 errors / 0 warnings; `check-artifacts.ps1` 92 files / 9 rules;
unit suite **116/116**; `run-smoke.ps1 -ReleaseRoot C:\N8RO` **30/30**. `tests/live/` is absent from
both `.github/workflows/*.yml` and `tests/ai-commander-tests.vcxproj` (the only `live` matches in the
workflows are the words "deliverable" and "lives elsewhere" in comments).

**Budget constraint governing this document.** Remaining spend is ≈$2.50 and any new request needs a
fresh grant recorded in the PRD *before* it is made. **No finding here requires a paid run to fix.**
Where a finding would need new measurement to *close* properly, it is written as a scoping fix plus a
carried item, not as a proposed run.

---

## A correction to the review brief this document answers

The brief asked for an attack on "§Corrections item 33", said to retire the `p95 ≤ 2.5 s`
success-metric target after measuring it unreachable, and described §Corrections as having 34 items.

**Neither is the case.** §Corrections (`docs/prd.md:73–424`) has **32** numbered items; there is no
item 33 or 34. And the target was **not** retired — the document refuses to move it in three places:

| Location | Text |
|---|---|
| `docs/prd.md:337` | *"§Success metrics' latency row is left unchanged and still reads MISSED. Moving a target to match a measurement is the one thing this document has refused since v1.3, and it is not going to start here."* |
| `docs/prd.md:1855` | *"The target row stays **MISSED**; it is not moved to fit the measurement"* |
| `docs/prd.md:470` | The metric table still reads `≤ 2.5 s` / `4,615 ms` / **MISSED** |

The argument item 29(f) *does* make has a real defect, and it is **R6** below. It is not the defect
the brief anticipated.

---

## Should fix forward

### R1 — `UAC-AIC-BE-3`'s cache-minimum guard does not exist in the code

**Where:** `src/AiCommanderPlugin.cpp:364` · `docs/prd.md:2025`
**Severity:** highest. This change spent 72 % of the margin the guard was meant to protect.

The PRD states in three places that a startup cache-minimum comparison exists and is load-bearing:

- `docs/prd.md:2025` (UAC-AIC-BE-3) — *"startup logged `prefixBytes` **and the derived token count
  against the configured model's cache minimum**"*
- `docs/prd.md:1036` — *"it is now **the only detector of a silent regression**"*
- `docs/prd.md:1560` — *"AIC-BE-3's startup comparison stays as the guard"*

The startup line logs `prefixBytes=` and stops there. There is **no** token estimate, no `4096`
constant, no comparison and no warning anywhere in `src/` — grepping the constant returns only
`FrameCostHistogram::kWindowSize` and `OrderRecorder::kMaxRecordedBodyBytes`. No test asserts it.

**Failure scenario.** An editor trims ~980 tokens of doctrine — which `docs/prd.md:170` still
explicitly tells them is safe (*"a page of doctrine can be deleted without consequence"*) — the
cached block drops below 4,096, prefix caching stops silently, and per-order cost goes from
$0.00122 to ≈$0.00583. Nothing fails, nothing warns, no test goes red. The only observable is
`cache_read_input_tokens` falling to zero in an order log nobody diffs. This is exactly the
"wrong loudly vs. wrong silently" distinction `docs/prd.md:1038` argues for, on the wrong side.

**Fix — two halves, both free, neither needing network.**

1. **Build-time.** A unit test pinning `PromptRenderer::build(...)` output to the measured **8,750
   bytes**, with the 3.955 B/tok ratio and the 4,096-token floor in the failure message.
   `tests/PromptRendererTests.cpp:174` already asserts that cache invalidation is a deliberate act
   for prefix *content*; this extends it to prefix *size*. It is also the assertion that would have
   caught the one-byte divergence item 31(f) records having caught by hand.
2. **Runtime.** Implement the comparison the UAC claims, against **`lastCacheReadTokens()`**, not
   `prefix().size()`. `docs/prd.md:1038(a)` already established that comparing the prefix alone is
   wrong by 69 % in the safe direction; after this change a naive `prefix()`-based check is wrong in
   the *other* direction — the prefix text alone is now ≈2,212 tokens and would emit a false
   shortfall warning on every run.

If the runtime half is deferred, say so in UAC-AIC-BE-3 rather than leaving the UAC describing
behaviour that does not exist.

---

### R2 — The `5.7×` cliff warning understates the hazard by ≈3.4×

**Where:** `docs/prd.md:383` and `docs/prd.md:2172`
**Text:** *"falling out of cache costs **5.7×** what the prose removal just saved"*

The multiplier is unsourced and does not reproduce. It reconstructs only as `$5.98 ÷ $1.05` — the
uncached-vs-cached four-ship-hour **ratio** from §Cost model's table (`docs/prd.md:1518`), computed
on the **pre-change 7,608-token prefix**. That is a ratio of two costs, not a multiple of the saving,
and the sentence attaches it to the saving.

**The arithmetic.** The saving was `$1.05 → $0.88` = **$0.17/four-ship-hour**. Falling out of cache
from the post-change state costs:

```
(179.7 + 5,118) tok x $1/MTok  +  106.2 tok x $5/MTok  =  $0.005829/order
$0.005829 x 720 orders/four-ship-hour                  =  $4.20/hour
$4.20 - $0.88                                          =  $3.32/hour  =  19.5x the $0.17 saved
```

Equivalently, ≈**4.8×** the current cached cost. Every reading reconstructible from the document's
own figures lands between 15× and 19×; none lands on 5.7.

**Failure scenario.** An editor reads "5.7×" against a "24 % margin", judges the downside modest, and
trims doctrine. Real exposure is a ≈19× reversal of the saving. **The error is in the direction that
makes the cliff look survivable**, in the one paragraph that exists to warn about it.

**Fix.** Replace with one of the two defensible statements and show the arithmetic inline —
*"falling out of cache costs ≈19× what this removal saved ($3.32/four-ship-hour against $0.17)"* or
*"≈4.8× the current cached cost"*. Same edit at `docs/prd.md:2172`.

---

### R3 — Cached-token figures are 120-row means presented as per-hit values

**Where:** `docs/prd.md:374` (item 31(a) table) · `docs/prd.md:383` · `docs/prd.md:409`

The archived CSVs carry a **constant** `cacheReadTokens` on every cache hit: **7,608** on arm A and
**5,118** on arm B, 119 rows each. The PRD's `7,544.6` and `5,075.4` are those constants averaged
over all **120** rows *including the first, uncached, request*:

```
7,608 x 119/120 = 7,544.6        5,118 x 119/120 = 5,075.4
```

`docs/prd.md:374` labels them *"Cache hits 119/120 @ 5,075.4 tok"*, which reads as the per-hit value
and is not one.

That average is the **correct** number for the cost rows at `docs/prd.md:1543-1544` — it is a
per-order mean. It is the **wrong** number everywhere the cache minimum is in view, because the
minimum applies to a single request. Two consequences:

- **`docs/prd.md:383` mixes conventions** — the true per-hit 7,608 for "before", the 120-row mean
  5,075 for "after". The real margin is `5,118 − 4,096` = **1,022 tokens (24.9 %)**, not 979 (23.9 %).
- **`docs/prd.md:409` is the one that actually misleads.** It reads: *"a real in-engine request
  reported `cache_read_input_tokens: 5,118` … one commit after it was measured on fixtures at
  5,075."* **The fixtures measured 5,118 too.** The engine and the fixtures agree *exactly*; the
  document presents a 43-token discrepancy that is an artifact of its own averaging. The claim
  *"the shipped artifact behaves as the arm that authorized it did"* is **stronger** than stated and
  is currently supported by a mis-derived number.

**Fix.** Report the per-hit constant (7,608 / 5,118) wherever the cache minimum is discussed; keep
the 120-row mean only in cost columns; label which is which. Correct `docs/prd.md:409` to record that
the two figures are identical — that is a better result than the one currently written.

---

### R4 — `measure-ttft.ps1`'s byte-delta assertion is a tautology, and the parse check runs on the wrong string

**Where:** `tests/live/measure-ttft.ps1:124-133`

The assertion cannot fail. Both sides are the same arithmetic over the same literal:

```powershell
$streamed = '{"stream":true,' + $text.Substring(1)   # length is ALWAYS $text.Length + 14
$expectedDelta = '{"stream":true,'.Length - 1        # ALWAYS 14
if (($streamed.Length - $text.Length) -ne $expectedDelta) { ... }   # always 14 -ne 14 -> false
```

The comment at `:126` says *"this is the check that says so rather than trusting it"*. It asserts
nothing about the body at all.

Compounding it, `:133` runs `ConvertFrom-Json $text` — validating the **original**, never the spliced
body that goes on the wire. The script has no check that what it transmits is well-formed JSON.

**Failure scenario.** Any future change to how `ttft-dump` serialises — a BOM, leading whitespace
before `{`, a degenerate `{}` body — produces malformed JSON that passes both guards and surfaces as
a **paid** HTTP 400, with `$failures` incrementing quietly while the run continues.

**The design intent is right and must be preserved:** do *not* round-trip the JSON, because
re-serialising would renormalise escaping and key order and stop it being the body the adapter
builds. Only the assertion needs to become real.

**Fix.** Keep the string splice; assert semantically afterwards:

```powershell
$after = ConvertFrom-Json $streamed   # throws under ErrorActionPreference=Stop if the splice broke it
if ($after.stream -ne $true) { Write-Error "$($file.Name): stream flag missing after splice" }
$before = ConvertFrom-Json $text
$added = @(Compare-Object $before.PSObject.Properties.Name $after.PSObject.Properties.Name |
           ForEach-Object { $_.InputObject })
if ($added.Count -ne 1 -or $added[0] -ne 'stream') {
    Write-Error "$($file.Name): the splice changed more than the stream field: $($added -join ', ')"
}
```

---

### R5 — The `Expect100Continue` fix is silently edition-dependent

**Where:** `tests/live/measure-ttft.ps1:92-93`

`[System.Net.ServicePointManager]::Expect100Continue = $false` affects `HttpClient` **only on .NET
Framework**, where `HttpClientHandler` wraps `HttpWebRequest`. Under PowerShell 7 (`pwsh`, .NET 5+)
`SocketsHttpHandler` ignores `ServicePointManager` entirely and both this line and
`DefaultConnectionLimit` become no-ops. Nothing in the script pins the edition.

**Failure scenario.** Someone re-runs the probe under `pwsh` and reintroduces precisely the artifact
item 29(g) paid to discover — *"two samples pinned at ~30,500 ms … a probe measuring its own client
stack and reporting it as latency"*. This time the lesson is already in the changelog, so the anomaly
reads as service variance rather than as a new finding.

**Fix.** Guard at the top of the script, beside the existing API-key check:

```powershell
if ($PSVersionTable.PSEdition -ne 'Desktop') {
    Write-Error ("run under Windows PowerShell 5.1: ServicePointManager::Expect100Continue is a " +
                 "no-op on .NET 5+ (SocketsHttpHandler), and it lands in the fixed term this " +
                 "probe measures - see PRD SS-Corrections item 29(g)")
}
```

---

### R6 — Item 29(f) closes a **Haiku** target with a **Sonnet** measurement

**Where:** `docs/prd.md:334` · `docs/prd.md:2206` · `docs/prd.md:1855`
**This is the weakest argument in the document** — not the target-retirement the brief anticipated
(which did not happen), but the reasoning that closes C2.

§Success metrics' latency row is model-specific: *"Claude Haiku 4.5: p95 ≤ 2.5 s"*
(`docs/prd.md:456`). The headers-time figures that close C2 — mean 3,157 ms, p95 6,917 ms — come from
`c2-ttft-armA-full-doctrine.csv` and `c2-ttft-armB-short-doctrine.csv`, **both `claude-sonnet-5`**
(`tests/live/data/README.md:38-39`). **Haiku's fixed term has never been measured.**

The claim is nonetheless generalised without a hedge:

- `docs/prd.md:334` — *"it was never achievable **on this path**"*
- `docs/prd.md:1855` — *"the target is unreachable by any prompt-side change and always was"*

The two models are not interchangeable here **by the document's own findings**: item 24 establishes
that tokenization is model-specific, item 28 is a sustained argument about not reading one model's
data as another's, and the totals differ materially (Sonnet mean total 7,393 ms against Haiku's
measured p95 of 4,615 ms). If Haiku's fixed term were ≈1,800 ms, the latency row would still read
**MISSED** — but the *cause* C2 closes on would be wrong, and "unreachable by any prompt-side change"
would be unsupported.

This is the same error class item 29(c) names three times within its own text, applied across models
rather than across statistics.

**Fix — no measurement required.** Scope the claim to what was measured: *"the fixed term on
`claude-sonnet-5` alone exceeds the target; Haiku's fixed term is unmeasured, and the ≤ 2.5 s row is
Haiku's."* Then file the Haiku headers-time measurement as a **carried item**. The archived request
bodies plus `measure-ttft.ps1` would answer it for ≈$0.10 whenever a grant next exists.
**Do not propose or make that run as part of this fix work.**

---

### R7 — Unused include *(already known before review)*

**Where:** `src/PromptRenderer.cpp:4`

`#include "OrderSchema.h"` is unused now that `orderJsonSchemaText()` is no longer rendered into the
prefix. Confirmed: no symbol from that header remains in the file. `orderJsonSchemaText()` itself
stays live for `LocalAdapter` (`tests/LocalAdapterTests.cpp:428`) and `tests/SchemaTests.cpp` — only
the include goes.

---

### R8 — `--no-prose-schema` can no longer succeed *(already known before review)*

**Where:** `tests/live/LiveMain.cpp:530` (search) · `tests/live/LiveMain.cpp:1030` (flag)

`exciseProseSchema` searches the prefix for `"ORDER SCHEMA (your reply must validate against this):\n"`,
which `PromptRenderer::build` no longer emits, so the flag hits `std::exit(1)` unconditionally and
the C3 experiment is no longer reproducible from the current binary.

**Recommendation: invert it, do not delete it.** Re-adding the prose block as a control arm keeps C3
reproducible and is the only way a future backend that does not constrain decoding can re-measure the
question item 31(c) explicitly leaves open. Deleting the flag makes the arm-A prefix unreconstructible
from the binary. If it is inverted, the new flag should assert the *absence* of the header before
inserting the block, mirroring the existing assert-don't-assume discipline.

---

## Verified sound — do not "fix" these

Confirmed correct during review. Changing them would be a regression.

- **`ttft-dump`'s first-write-wins is correct and load-bearing** (`tests/live/LiveMain.cpp:1180`).
  Traced through `ClaudeLlmClient::request`: `http_` is cached as a member
  (`src/ClaudeLlmClient.cpp:174`) so the factory runs once and **both** sends reach the same
  `CapturingHttpClient` and the same `&body`; send #1 is the real POST (`:207`); the retry send
  (`:211`) is gated on `response.has_value() && isRetryable(...)` and the fake returns `nullopt`, so
  it cannot fire; the failure path then calls `tlsLikelyUnavailable()` (`:225`) which issues
  `http_->send(control)` with `control` a `HttpMethod::Get` whose `body` is **never assigned**.
  Last-write-wins really would capture `""` on every fixture. Nothing else re-enters `send()`.
- **The rest of `measure-ttft.ps1`'s SSE reading is correct.** `SendAsync` + `ResponseHeadersRead` is
  required (`PostAsync` has no `HttpCompletionOption` overload); `$headersMs` is stamped before the
  body stream is touched; `content_block_delta` is the right first-token event (`message_start` would
  measure acknowledgement); the `finally` block stops the stopwatch before `totalMs` is read.
- **`run-live-scenario.ps1`'s two reporting fixes are correct.** `$rejected` is defined at `:246`
  before the `$resolved` computation at `:261`; the field counts (6 hosted / 5 local) match the
  emitted configs exactly; the median at `:281` reproduces item 32(d)'s stated 3,137 ms from the ten
  recorded samples.
- **Every figure spot-checked reproduces from the archived CSVs.**
  `analyse-latency-vs-output.py` regenerates item 28's slopes, CIs, the overlap test, the
  0.734 → 0.036 banding, the closed-form r² 0.019, the 396 ms intercept SE and the ≈$10.13 figure.
  `analyse-ttft.py` regenerates item 29's SDs, the 1.04/1.03 ratios, the three null arm deltas and the
  837/1,168 ms resolvable gaps. Independently verified: headers p95 6,917 / 7,357 ms;
  corr(TTFT, total) 0.995 / 0.991; cache reads 48 + 47 = 95/96; C3 arms 120/120 accepted with zero
  rejections. Cost per order recomputes to $0.001459 / $0.001218 against the stated
  $0.001464 / $0.001220.
- **The byte-identity discipline in `src/PromptRenderer.cpp:86-92`** — the deliberately retained bare
  `'\n'` — is correct and deliberate. **Do not remove it as tidy-up.** That is the exact edit item
  31(f) records catching.

---

## Worth noting — lower priority

- **N1.** `tests/live/measure-ttft.ps1:293-312` prints a p95 with no minimum-`n` guard — the same
  nearest-rank formula item 32(d) just removed from `run-live-scenario.ps1` for landing on the
  maximum. At the default `-Repeats 8` (n=48) it is legitimate; at `-Repeats 1` (n=6) `Pct 95`
  returns the max under a percentile's name. The lesson reached one instrument and not its sibling,
  which is the meta-complaint item 32(d) makes about itself.
- **N2.** `tests/live/LiveMain.cpp:1219` writes `ttft-request-NN.json` into the current working
  directory with no `--out` support, unlike every other mode. Two dumps taken from different
  directories are silently unrelated, and the arm-A/arm-B pairing depends on where you were standing.
- **N3.** `--no-prose-schema` is silently ignored outside `--mode soak` (`tests/live/LiveMain.cpp:1030`)
  — it only reaches `runSoak`. Worth an explicit rejection in other modes if the flag is kept.
- **N4.** `tests/PromptRendererTests.cpp:47` — `AIC_EXPECT_TRUE(reference.size() > 800, "the prefix
  should carry the schema and doctrine")`. Still passes at 8,750 bytes, but the message is now false
  and the bound is ≈10× below anything meaningful. **This is the natural home for R1's build-time
  assertion.**
- **N5.** `docs/prd.md:383` cites 7,608 for arm A while its own table nine lines above
  (`docs/prd.md:374`) says 7,544.6. Both are defensible numbers from different runs, but they appear
  in one item without distinction — the same conflation as R3.
- **N6.** `tests/live/measure-ttft.ps1:327` reports the cache-read token count via
  `Select-Object -First 1`, i.e. one sample presented as the run's value. It happens to be constant in
  both archived runs (which is R3's evidence), but nothing checks that it is.

---

## Overall assessment

**The shipped behaviour change is sound and unusually well evidenced** — pre-registered, paired,
decision rule pushed before the run, and the byte-identity discipline in `PromptRenderer.cpp` is
genuine practice rather than ceremony.

**The defects cluster in the guard rails around the change rather than in the change itself:** the
cache-minimum protection the PRD claims exists does not (R1), the warning that stands in for it
understates the hazard by 3.4× (R2), and the probe's headline "asserted rather than assumed" check
asserts nothing (R4).

**R1, R2 and R4 should be fixed before anyone edits doctrine again.**
