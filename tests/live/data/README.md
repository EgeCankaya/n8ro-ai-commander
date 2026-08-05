# Archived per-order rows from the hosted runs

**What this is:** the raw per-order output of every live run this project has made against
`api.anthropic.com`, plus the script that derives PRD §Corrections item 28 from it.

**Why it is in the tree.** Items 25, 27(f) and 28 are all *re-analyses* — they make no new
request and derive their conclusions from rows written during runs already paid for. Until
v1.8.10 those rows existed only in scratch directories, which meant four measurements the owner
had authorized and paid for were one cleanup away from being unreproducible, and three §Corrections
items were citing a transcript rather than a file. **A re-analysis is worth no more than the
durability of the data under it.**

Nothing here reaches the network. `analyse-latency-vs-output.py` is stdlib-only Python and needs
no API key, no release tree, and no compiler.

```
python analyse-latency-vs-output.py
```

## Provenance

Every file was written by `ai-commander-live-tests.exe --csv <path>`. All four runs used the
**six synthetic `LiveMain` fixtures** — no real scenario state, no deployed mission file. That
distinction is the whole of carried item **C1**, and it is why no figure derived from these files
may be described as characterising the product rather than the fixtures.

| File | Run | Model | n | Grant | PRD |
|---|---|---|---|---|---|
| `c2-armA-haiku-full-doctrine.csv` | C2 decomposition, arm A — full doctrine, 17,756 B prefix / 7,608 cached tokens | `claude-haiku-4-5` | 60 | third (v1.8.3) | v1.8.5 |
| `c2-armB-haiku-short-doctrine.csv` | C2 decomposition, arm B — short doctrine, 10,976 B prefix / 5,991 cached tokens. Identical to arm A in every other respect | `claude-haiku-4-5` | 60 | third (v1.8.3) | v1.8.5 |
| `sonnet-maxtokens-512.csv` | First non-Haiku run, at the shipped ceiling. **44 accepted, 4 truncated at the cap** | `claude-sonnet-5` | 48 | third (v1.8.3) | v1.8.5, v1.8.6 |
| `sonnet-maxtokens-8192.csv` | The ceiling-raised re-run that closed C7. **48/48 accepted.** Paired with the row above: same model, same fixtures, ceiling the only difference | `claude-sonnet-5` | 48 | fourth (v1.8.8) | v1.8.9 |
| `c2-ttft-armA-full-doctrine.csv` | The TTFT run, arm A — 17,756 B prefix / 10,493 cached tokens. Different columns from the four above; see below | `claude-sonnet-5` | 48 | fifth (v1.8.11) | v1.8.12 |
| `c2-ttft-armB-short-doctrine.csv` | The TTFT run, arm B — 10,976 B prefix / 8,291 cached tokens. Identical to arm A in every other respect | `claude-sonnet-5` | 48 | fifth (v1.8.11) | v1.8.12 |
| `c3-quality-armA-prose-schema.csv` | C3's quality arm A — the prefix **as it shipped before v1.8.14**, 17,756 B. **120/120 accepted** | `claude-haiku-4-5` | 120 | fifth (v1.8.11) | v1.8.14 |
| `c3-quality-armB-no-prose-schema.csv` | C3's quality arm B — prose schema excised, 8,750 B. **120/120 accepted.** This prefix is what **now ships** | `claude-haiku-4-5` | 120 | fifth (v1.8.11) | v1.8.14 |

**The two C3 files are the evidence a shipped-code change rests on**, and they are the reason this
directory exists at all. The decision rule they were judged against was written into the PRD
(§Corrections item 30) and pushed to the remote **before** either arm ran — so the rule cannot have
been chosen to fit them. Arm A is the prefix as it shipped up to v1.8.14; **arm B is the prefix that
ships now**, byte-identical to it, including one blank line that was briefly removed as tidy-up and
restored so that the artifact under test and the artifact in production stayed the same object.

The two TTFT files come from `measure-ttft.ps1`, not from `--csv`, and carry a different schema:
`arm, fixture, repeat, ttftMs, headersMs, totalMs, afterFirstMs, deltas, tokensIn, tokensOut,
cacheReadTokens, stopReason`. Analysed by `analyse-ttft.py`.

**`ttftMs` in those two files does not mean what its name says, and §Corrections item 29 is mostly
about that.** Under structured outputs the service emits ~6 deltas per response rather than one per
token, and the first delta arrives *after* the generation — so `ttftMs` correlates with `totalMs` at
**0.995**, and `afterFirstMs` is a near-constant 2.2-second flush rather than generation time.
**`headersMs` is the column with no generation in it**, and it is the one worth reading.

## Columns

| Column | Meaning |
|---|---|
| `latencyMs` | Worker-side round trip, request sent → response parsed. **Includes generation.** That is the entire reason C2 needs a different instrument |
| `tokensIn` | `usage.input_tokens` — **uncached** input only. It does not contain the cached tokens (§Corrections item 23) |
| `tokensOut` | `usage.output_tokens` |
| `cacheReadTokens` | `usage.cache_read_input_tokens`. `0` on the first request of a run, then the full prefix on every subsequent one |
| `accepted` | `1` if the order passed Stage A and Stage B |
| `situation` | Which of the six fixtures produced the row |

## Two traps these files set, both already sprung once

**`tokensOut` is censored in `sonnet-maxtokens-512.csv`.** Four rows read exactly `512` because the
model was cut off there; their true lengths are `≥ 512` and were never observed (§Corrections
item 25(d)). Those four are a valid part of the acceptance count and **must be excluded from any
regression on `tokensOut`** — the value is the cap, not a measurement. Including them is what
produced item 27(f)'s inflated `r² 0.574 / slope 10.1`; the accepted-only fit is `n=44, r² 0.499,
slope 9.88`. The script filters them; anything else reading these rows must too.

**`r²` here is a property of the sampling.** Haiku's output spans ~50 tokens and Sonnet's ~550, so
the two are not comparable on `r²` at all, and neither is comparable to any future run with a
different spread. **The slope with its confidence interval is the statistic that transfers.**
Reading the `r²` column as though it described the system is the error §Corrections item 28
exists to correct, and it was made twice before it was caught.
