# n8ro-ai-commander

A `n8ro-sim` plugin that lets a language model issue tactical *intent* — posture,
target, waypoint, rules of engagement — to entities in a running scenario, while every
kinematic decision and every state mutation stays in the deterministic C++ and Lua
tiers that already exist.

The model is a commander on the radio, not a pilot in the seat. It emits one small
JSON order every ~20 simulation seconds; the existing Tier-1 Lua tactics decide whether
that intent is achievable and fly the aircraft. Inference never runs on the simulation
thread, and the order schema has no property for heading, velocity, or acceleration —
so "never produces raw kinematics" is enforced by the absence of vocabulary, not by a
check someone might forget to write.

## Start here

| If you want… | Read |
|---|---|
| **Two pages, and the honest state** | [`docs/summary.md`](docs/summary.md) |
| The contract — 17 functional requirements, each with acceptance criteria | [`docs/prd.md`](docs/prd.md) |
| Exactly what leaves the machine on the hosted path | [`docs/egress.md`](docs/egress.md) |
| Everything that is open, and what it would cost to close | [`docs/open-issues-review.md`](docs/open-issues-review.md) |

`docs/prd.md` is the specification and it is the contract. Read it before changing
anything here. `docs/summary.md` carries no number that is not also in the PRD, and
`tools/lint-prd.ps1` fails the build if its version stamp drifts from the PRD's.

## Status

| Phase | Scope | State |
|---|---|---|
| 0 | Scaffold, repo, empty `aiCommander` namespace | **closed** |
| 1a | Full pipeline on the `stub` / `replay` backends | **closed** |
| 1b | `local` adapter against Ollama | **closed** — every gate item has a result |
| 2 | `claude` adapter | **ran** — adapter complete, measured, and exercised in-engine. **Not formally closed**, but it may ship: the standing egress grant of 2026-08-09 authorizes the hosted backend as a product. See *Authorization* below |
| 3 | Diagnostics on what Phase 2 carried | **closed** — four results, of which three are negative, null, or a refusal |

**The open-item register** (PRD §Carried out of Phase 3) has three states, not two. A row that
nobody should touch and a live question used to look identical; a **deferred** row now carries a
revisit condition you can test without running anything.

| # | State | Item |
|---|---|---|
| ~~C27~~ | closed | *(v1.8.54 — the commander paid for four orders to an aircraft that had been `wrecked` for over a minute. **Opened and closed in one revision**; the guard is built and tested, and has not yet been observed firing in-engine)* |
| ~~C23~~ | closed | *(v1.8.41 — AIC-ORD-2 clauses 7 and 8 coded and pinned by six offline tests, demonstrated under a commander, residual named)* |
| ~~C17~~ | closed | *(v1.8.41 — closed as a stated position on C8's precedent; the bar stays on the fixtures, by owner decision)* |
| ~~C8~~ | closed | *(v1.8.36 — Haiku is and will remain the default, so `maxTokens = 512` is correct. Revisit condition kept as a tripwire)* |
| ~~C6~~ | cancelled | *(v1.8.36 — not answered; **nothing here is blocked on it**, and §Out of scope already specifies both branches)* |

**The register is empty. No open rows, no deferred rows.**

*(**Corrected v1.8.54.** This table said C23 and C17 were **open** and closed with *"Two live rows"*
— **thirteen revisions after the PRD closed both at v1.8.41.** Same failure as §Authorization below:
the README lagging the PRD on a load-bearing claim, §Corrections items 68(e) and 69(f).)*

**Gates as of 2026-08-14 (PRD v1.8.57). Which rows were re-run for this revision is stated per row,
because "all green as of" over a table where one row is eleven days old is the claim that went wrong
below:**

| Suite | Result |
|---|---|
| Unit | **186 / 186** — re-run 2026-08-14 |
| Deployed-artifact smoke | **30 / 30** — re-run 2026-08-14 |
| Live three-arm scenario smoke | **22 checks, 0 failed** (twice, 2026-08-09 — *not* re-run since; it needs a server and a ~35 min run) |
| `tools/lint-prd.ps1` · `tools/check-artifacts.ps1` | 16 checks, 0 errors, 0 warnings · 116 files, PASS — both re-run 2026-08-14 |
| `tools/deployment-check.ps1` | **6 checked, 0 failed**, 3 manual — re-run 2026-08-14 against a torn-down tree |

<!-- gate-figures: unit=186/186 artifact-smoke=30/30 live-smoke=22/0 tracked-files=116 -->

*(v1.8.53 — **this table was wrong and had been for some time.** It said `162 / 162` and
`105 files` under a heading asserting the figures were current; the real numbers were `169 / 169`
and `110`, and the file count was already false on the date the table stamped itself with. It is
the acceptance figure's staleness failure — four events, PRD §Corrections items 46(f), 48, 50, 51 —
recurring in the one document a reader meets first. **So it is now pinned rather than retyped:**
`tools/lint-prd.ps1` check 11 requires the sentinel above to match the PRD's copy, and it
**computes** `tracked-files` from `git ls-files` instead of trusting either. §Corrections item 69(f).)*

*(**v1.8.57 — and it went stale again anyway, in the half of the sentinel that was still pinned by
agreement.** Both copies said `unit=185/185` while the PRD's own v1.8.56 entry said **186/186** two
thousand lines away: check 11 compares the README's sentinel to the PRD's and passed, because the
two stale copies agreed with each other. **Agreement is not verification when both writers copy from
the same stale source.** `tools/lint-prd.ps1` check 16 now **computes** the unit figure the way
check 11 already computed `tracked-files` — counting `AIC_TEST(` across the .cpp files the unit
`.vcxproj` actually compiles — so the denominator is derived from the suite rather than typed.
§Corrections item 73.)*

**Measured against the PRD's success metrics:** cost **$1.05** per four-ship
scenario-hour against a ≤ $1.10 target (**met**); `reject.schema` **0.00 %** over 776
orders across two backends and three models (**met**); order acceptance **100 %** on the
synthetic-fixture soaks and **80.8 % [77.4, 83.8]** in-engine against real scenario
state — **90.9 % [88.2, 93.2]** once the two non-model-failure classes are removed
*(v1.8.46 — was 71.1 % and 85.8 %; **the outcome campaign's four three-arm runs plus the
clause 8 probe added 211 resolved orders, and the C23 stall-floor class did not grow by a
single sample — still 58, every one of them predating the fix**)*
(**two instruments, two named rows — see C17**); local 7B round-trip p95 **7,975 ms** against a
≤ 20 s target (**met**), hosted Haiku p95 **4,615 ms** over the 240-order soak against
≤ 2.5 s (**missed**, and not control-loop-binding — the 20 s cadence absorbs a p99 of
7,099 ms). Plugin frame cost p95 **0.0059 ms**, max **0.5334 ms** over 12,001 frames
against a 5 ms bar.

Total spend across six owner-authorized egress grants: **≈$2.57 of $5**.

## The finding that matters most

**For most of this project's life the commander made the aircraft it commanded worse, and
nobody had measured it.** §Success metrics carried six metrics and not one of them was an
outcome, so every published verdict was green while, across twelve paired runs, the
commanded aircraft launched **4 times against the shipped script's 32**, scored **0 kills
against 0**, and were **destroyed 22 times out of 24 against 0 of 24**.

**The cause was this project's own deterministic tier, not the language model.** The
reference Tier-1 script stopped fighting whenever no order was in force, its fire path was
reachable from one posture out of six, all three rungs of the fallback ladder were
non-fighting, and it had no automatic defensive reflex at all — while the shipped script's
first action every tick is a missile-defeat check.

**It is fixed, and the arm built to test it confirms the fix.** The confirming run holds
the commander off and changes only the script — the one comparison in this project that
moves a single variable:

| Arm | launches | detonations | kills | aircraft lost |
|---|---|---|---|---|
| **broken** script, no commander, no model | 0 | 0 | 0 | **2 of 2** |
| **fixed** script, no commander, no model | **3** | **2** | 0 | **0 of 2** |
| fixed script **+ commander** | 3 | 2 | 0 | 0 of 2 |
| shipped `oppint_red_interceptor.lua` | 2 | 1 | 0 | 0 of 2 |

Both jets end the fixed-script run in `engage … no order` — fighting, with nothing
commanding them.

**What that does not say.** It carries a mechanism — *does the uncommanded script ever
fire?* — and no rate.

**A second complete run has since landed, and it changes two of the sentences above.**

| run | build | ON | SCRIPT-ONLY | OFF |
|---|---|---|---|---|
| `095026` | old | 3 / 2 / 0 / 0 | 3 / 2 / 0 / 0 | 2 / 1 / 0 / 0 |
| `135722` | old | 3 / 2 / 0 / 0 | **2 / 2 / 1 / 0** | 4 / 2 / 0 / 0 |
| `185750` | new | 3 / 2 / 0 / 0 | **2 / 2 / 1 / 0** | 4 / 2 / 0 / 0 |

**The commanded/script-only identity does not replicate.** *"Identical on every column"* is
a universal claim and one counter-instance disproves it, so it is **refuted rather than
merely unsupported**.

**The obvious reading of the repeat is too strong, though.** The script-only arm carries **no
model** and is near-deterministic — its two kills are byte-identical, same munition id and
`pk` to six decimals — so those two runs are nearer one observation than two, and
**`095026`'s script-only row is the outlier that wants explaining**, on the same build and
script. The commanded arm was 3 / 2 / 0 / 0 in all three runs: **the variance in this
comparison sits in the arm without the model.** **Nothing about what the commander is worth
is established in either direction** *— on these four columns, which is all this section had
when it was written. See below.*

### And then it was measured, on one endpoint *(v1.8.46; this section corrected v1.8.52)*

**This README carried the paragraph above for six revisions after the answer existed** —
`docs/summary.md` and the PRD were updated and this file was not, which is
§Corrections item 68(e).

The four columns above **cannot answer the question at any n anyone will run** (17 to 156 runs
each; `losses` has been degenerate since the reference-script fix). The signal was in the graded
`pk` the engine records on every damage line. Under a protocol written **before** the first run,
four three-arm 600 s `local` runs at the shipped default give a paired commander-on minus
script-only difference in **damage absorbed** of:

> **95 % CI [−1.0210, −0.5272]** — mean −0.7741, sd 0.1552, t = −9.976 on 3 df, negative in
> 4 of 4 runs. **Quoted as an interval, never as a point.**

**Three limits travel with that number and are not optional.**

1. **The confound is not discharged.** This design **cannot separate** *"the commander makes the
   aircraft safer"* from *"the commander makes it fight less"* — the endpoint that would separate
   them needs ~156 runs. Damage dealt, kills, launches and losses stay **unmeasured and unclaimed
   in either direction**.
2. **One endpoint, one scenario, one model** — the shipped 7B default, in "Mariana Shield", with
   two commanded aircraft, on top of the reference Tier-1 script's own competence.
3. **The control arm is near-deterministic** — the same property this section already names above.
   Its damage-absorbed value takes **two values across all nine archived runs**, so the
   replication against the five earlier runs is of the commanded arm alone. The interval excludes
   zero under either control value (§Corrections item 68(b)).

*Source: PRD §Corrections items 62 and 68; §Validation, "The outcome campaign — a
pre-registration".*

**Those are the only two kills in eighteen archived runs**, both in the arm with no model in
the loop. **The qualification belongs with the number:** two SAM hits had already left
`BlueF16_02` `wrecked` (cumPk 0.903) before the Su-35's missile finished it. A kill by the
engine's definition, and not an unaided one.

## Why the validator is not optional

A live run once rejected this order, and every field in it is well-formed and in range:

```
posture: hold,  reason: "Maintain orbit over friendly positions."
waypoint: { latitudeDeg: -31.952876, longitudeDeg: 115.860450 }
```

**−31.952876, 115.860450 is Perth, Western Australia** — against an own-ship position near
Guam. The model substituted a memorised real-world coordinate for a waypoint whose correct
value was own-ship position, and it reproduced on a different entity to four decimal
places, on a value appearing nowhere in the prompt. Neither the schema nor the constrained
decoder can catch that; **Stage B's geofence is the only thing that does**, and widening
the bound would not have fixed it — the bound is what caught it.

*(**C5**, closed as a bounded negative. It is deliberately not over-claimed and it is deliberately
not the headline: **n = 11 waypoint-carrying orders support no rate**, and the detector looks for
Perth specifically, so it would not see a different memorised coordinate. What the record settles is
that the substitution **can** happen and that the geofence is the only layer that catches it. An
earlier version of this file led with this finding while the project's largest result — C21, above —
appeared nowhere in it.)*

## Known rough edges

Stated here rather than discovered during a demo.

- **C23 — the model orders every `hold` at the aircraft's own position, and the aircraft
  stops there.** One aircraft sat at 1.5 m/s for roughly 400 of 600 seconds, after which the
  model faithfully copied that speed into every order and **all 21 rejections in the run were
  the same speed-floor check**. *(v1.8.35: **all 19 archived holds were issued at 0.0 m from
  own position**, so there is no "arrival" — the aircraft is told to hold where it already is.
  **And the stall survives a full release to Tier 1**: twelve archived below-floor samples
  have no order in force at all, while the script calls `resumeWaypointFollowing`. The
  fallback ladder is not what sustains it.)* Open. **The question has two ends — the onset and
  the failure to recover — and they are different layers.** Still an owner decision.
- **In-engine acceptance is quoted as an interval, never as a point.** Every interval in
  play is 15–30 points wide. *(v1.8.36 — it had gone stale four times under that rule alone,
  so `tools/acceptance-report.py` now owns the number and `tools/lint-prd.ps1` fails the
  build if this file, the PRD and the summary disagree.)*

<!-- in-engine-acceptance: 80.8 [77.4, 83.8] n=609 runs=30 -->

<!-- outcome-damage-absorbed: -0.7741 [-1.0210, -0.5272] n=4 signs=4/4 SUPPORTED -->
- **Three kills have ever been scored**, and in **all three** the target was already `wrecked`
  by SAM hits when the Su-35's missile finished it. Two were 2026-08-08 in the script-only arm;
  the third is 2026-08-09 and is **the first in a commanded arm**. **A kill by the engine's
  definition; not an unaided one**, and no rate follows from three. *(The better result in that
  run is not a kill at all: `RedSu35_01` took `BlueF16_01` to `wrecked` with two of its own
  missiles and no SAM contribution — the first material damage a commanded aircraft has done
  on its own.)*
- **The commanded/script-only identity of the first three-arm run did not replicate.** It is
  refuted; nothing about the commander's value is established in either direction, and the
  script-only arm is near-deterministic so the repeat counts for less than it looks.

## Authorization — the hosted backend may ship, under a standing grant

`commander.backend = claude` reaches the network and transmits real scenario state.

**The standing hosted-egress authorization is in force — granted by the owner 2026-08-09 and
recorded in PRD v1.8.48.** It is the seventh egress authorization and the first that is not a
measurement grant: the six before it each authorize a named experiment, and a later grant does not
inherit an earlier one's boundary. This one authorizes **running the hosted backend as a product**,
and it answers three decisions separately so any one can be withdrawn without the others:

| Decision | Recorded as |
|---|---|
| May real scenario state from an arbitrary deployed mission leave the machine, standingly? | **Yes** |
| One authorization for an indefinite series, or a record per run? | **Standing** — no PRD revision before a hosted run |
| Who holds it — the owner personally, or anyone in possession of the release tree? | **The release tree.** `claude.enabled = true` in a deployed config is sufficient authorization |

**It was recorded after its enforcement was built, not before.** `claude.maxSpendUsd` latches the
hosted backend off at a configured ceiling (default **$1.00 per engine process**), fail-closed at or
below zero. Note the units: it is **per process**, so it does not bound running the engine a hundred
times.

**Two obligations travel with the grant and are the deployer's, not the owner's.** Re-review the
transmitted-field list against the mission **actually deployed** — the field list is a property of
the snapshot builder, but which scenario's values fill it is a property of whoever deploys. And
`tools/deployment-check.ps1` prints both as `[MANUAL]` rather than auto-passing them.

**What the grant does not cover.** It does not change a shipped default — `commander.enabled` and
`claude.enabled` are both `false` in the shipped config, and a standing authorization is permission
to turn something on rather than turning it on. It does not authorize spending past the ceiling. It
does not retroactively cover anything before 2026-08-09. And it does not authorize publication of
this repository — a **separate** decision of a different class, granted separately and recorded below.

### Publication — authorized 2026-08-14, PRD v1.8.56

**The owner authorized publication of this repository on 2026-08-14, recorded in PRD v1.8.56
(§Corrections item 72).** It is the eighth authorization this project holds and the first that is not
about egress. Keep the two apart: the seven egress grants govern **what leaves the machine at run
time**; this one governs **what is disclosed about the platform, permanently, to everyone.**

**What is disclosed.** This repository contains no Arkheon-shipped code — no SDK headers, no import
libraries, no platform binaries, no runtime data. It does contain component type strings, schema
field paths, Lua verb signatures, `data/doctrine.txt`, and a PRD documenting the platform's internals
at length. **Repository visibility was the sole control over that disclosure, and this grant releases
it. Nothing here becomes non-proprietary by being published.**

**What it does not authorize:**

- the release tree, any part of it, or any file carrying the Arkheon per-file banner — `NOTICE` is
  explicit that the banner convention governs files inside the release tree, which this is not;
- **order logs and run archives** — they carry live scenario state (positions, ORBAT, teams,
  loadouts). `*.jsonl` is gitignored and `tools/check-artifacts.ps1` enforces that over the tracked
  file list. **Both controls matter more after publication than they did before it;**
- any relicensing. The terms in `NOTICE` are unchanged.

**A reader without a licensed N8RO release has source they cannot build** — `bin/` is gitignored and
the build resolves the SDK through `N8RO_RELEASE`. An Anthropic API key is neither sufficient nor the
binding constraint.

*(**Corrected v1.8.54.** This section previously read "the hosted backend cannot ship" and "there is
therefore no authorization under which an operator other than the measurer may turn the hosted
backend on" — **six revisions after the standing grant made both sentences false.** A reader meeting
this project at its README was being told they may not do the thing v1.8.48's decision 3 explicitly
permits. It is the README-lags-PRD failure of §Corrections items 68(e) and 69(f), third occurrence,
so `tools/lint-prd.ps1` check 15 now fails the build if this section stops naming the grant's
revision.)*

*(**Updated v1.8.56.** This section previously ended *"it does not authorize publication of this
repository, which needs the same class of decision and has not been made"* — true when written and
retired by the grant above. Check 15 pinned **that exact sentence**, so it would have failed the
build the moment the sentence became false, and the obvious repair — weakening the check — would have
retired the guard for the one event it was built to survive. It now pins the **current** posture:
this section must name v1.8.56 and state the publication decision, whichever way it goes. A control
that only holds while a decision holds is a control with an expiry date nobody wrote down.)*

`docs/egress.md` enumerates every field that leaves the machine. Any change to that set is
a PRD revision **and** an `egress.md` revision, made before the next hosted request.

## Prerequisites

This repository contains no SDK code and **cannot be built without a licensed N8RO
release installed.** An Anthropic API key is neither sufficient nor the binding constraint. You need:

- An N8RO release tree (the folder containing `setup.cmd`)
- Visual Studio 2026 (v18.x) with the C++ workload
- `Release | x64`, C++17 (`stdcpp17`)

**Validated against `n8ro@2.1.144`** — `n8ro-core@0.1.90`, `n8ro-schema@1.0.43`, `n8ro-data@2.0.80`,
`n8ro-sim@2.0.140`.

**That is a pin, not a bound.** `get_plugin_signature()` returns `"N8RO_PLUGIN_V1"`, the only
compatibility gate the host applies, and it is far too coarse to catch a schema or ABI change. On any
other release expect to **rebuild** — the SDK interfaces pass `std::string`, `std::optional` and
`std::function` across the DLL boundary — then check two lines in the startup log:

- `runtime-column probe pass` — the AIC-ARCH-4 probe resolved all six fatal leaves the snapshot
  depends on. A **fail** names the leaf, and the commander stays disabled by design.
- no `probe.warning` — a warning means `componentLifecycle/health` stopped resolving, so the guard
  that stops the commander ordering a wrecked airframe is muted. **The commander still runs:** that
  is deliberate, not an oversight (PRD §Corrections item 71(b)).

`tools/deployment-check.ps1` reports drift from the pin against the tree's own `components.xml`.

## Build

```bat
open-solution.cmd
```

This chains the release tree's `setup.cmd` → `dev\setup-dev.cmd` → opens
`ai-commander.slnx` in Visual Studio. Then build `Release | x64`.

The tree is located via `N8RO_RELEASE_ROOT`, which defaults to `C:\N8RO`. Set it if
your release lives elsewhere:

```bat
set N8RO_RELEASE_ROOT=D:\path\to\release
open-solution.cmd
```

## Output

| Artifact | Path |
|---|---|
| Build output | `bin/release/ai-commander.dll` (gitignored) |
| Deployed DLL | `%N8RO_RELEASE_USER_SIM_PLUGINS%\ai-commander.dll` — copied by the post-build event |
| Generated Lua stub | `<release>/data/resources/missions/stubs/aiCommander.lua` — appears after one engine run |

Verify the plugin exports:

```bat
dumpbin /exports "%N8RO_RELEASE_USER_SIM_PLUGINS%\ai-commander.dll"
```

Expected: `create_plugin`, `destroy_plugin`, `get_plugin_signature`.

## Tests

| Suite | Command | Needs |
|---|---|---|
| **Unit (186)** | build `tests\ai-commander-tests.vcxproj`, run `tests\bin\release\ai-commander-tests.exe` **from the release root** | SDK only — **no server, no network** |
| ASan | same, with `/p:EnableASAN=true /p:IntDir=x64\asan\ /p:OutDir=bin\asan\`. Run it from a shell that has sourced `dev\setup-dev.cmd`, or the ASan runtime DLL will not resolve | SDK only |
| **Deployed-artifact smoke (30)** | `tests\smoke\run-smoke.ps1 -ReleaseRoot C:\N8RO` | a deployed DLL |
| **Live** gate harness | build `tests\live\ai-commander-live-tests.vcxproj`, run from the repo root: `--mode all --orders 200` | **a running inference server** |
| **Live** scenario smoke (22 checks, 3 arms) | `tests\smoke\run-live-scenario.ps1 -RunSeconds 600` | a server, and the commander enabled |

The first three are the CI gate and are required to run with no inference server and no
network. The two live suites are separate projects invoked by hand, deliberately — the
PRD requires that separation rather than merely recommending it.

**The unit suite exercises the reference Lua script in a real Lua VM**
(`tests/ReferenceScriptTests.cpp`), driving `onTick` against recording stubs for the six
global tables the script touches. Nothing checked that file until 2026-08-08, which is how
the defect above survived: it was validated by being run inside a 22-minute scenario and
read afterwards in an order log.

## Running a live scenario

Three arms, ~11 minutes each, ~35 minutes total:

```bat
tests\smoke\run-live-scenario.ps1 -RunSeconds 600 -Backend local -LocalModel "qwen2.5:7b-instruct-q8_0"
```

**Do not interrupt it.** Its `finally` block restores the shipped mission script, the
doctrine, and the deployed config. Killing it mid-run leaves the swapped script in place
and the commander silently enabled for every later run of that release tree, interactive
ones included.

Run evidence is archived **outside this repository**, to
`Documents\N8RO AI Commander logs\<stamp>-<backend>\`. Order logs carry live scenario
state — positions, ORBAT, team assignments, loadouts — which inherits the release tree's
proprietary classification. `*.jsonl` is a security-relevant ignore rule and
`tools/check-artifacts.ps1` enforces it over the tracked file list, because an
already-tracked file stays tracked and `git add -f` bypasses `.gitignore` entirely.

### The local backend

You need an [Ollama](https://ollama.com) server and a model **tag** — not a GGUF
filename, which is what the default used to be and what fails with model-not-found:

```bat
ollama serve
ollama list                       :: local.model must name one of these tags
```

Then point `local.baseUrl` at it and set `commander.backend=local`. Two things to check
on the first run, both of which the plugin logs:

- `doctrine loaded from '...' (N bytes)` — if instead you see a WARNING naming a path it
  could not find, the prompt is running without doctrine and order quality is degraded
  silently. The post-build event seeds `data/doctrine.txt` into the release tree if it is
  absent; it never overwrites an edited one.
- `backend=local enabled=true` — if this is missing the run measured the stub backend, and
  a green result against canned orders is worse than a red one because it looks like
  evidence.

## Conventions

- **Derive, don't guess.** Units, frames, component type strings, field paths, and Lua
  verb arities come from the release tree's `dev/ai-coding/schema-reference/schema-reference.json`,
  the generated stubs under `data/resources/missions/stubs/`, or `dev/samples/` — never
  from memory. See the PRD's Appendix A for the traceability table.
- **The plugin never mutates entity state.** No `entityControl.requestUpdate*`, no
  `writeComponentField*`. Every motion originates from a Tier-1 `navigation.*` or
  `weapon.*` call.
- **Scope Authority — the PRD changes before the code.** A new Lua function, posture, order
  field, config field, backend, record field, or transmitted field requires a PRD revision
  first, in its own commit. See the PRD's Scope Authority section.
- **A single run may open a question; it may not close one** — unless the question is a
  mechanism readable directly off recorded values. A mechanism is not a rate, and a
  mechanism must never be inferred from a distribution of outputs.
- Order logs are gitignored. They contain live scenario state.

## License

See [`NOTICE`](NOTICE). Private; publication requires owner authorization.
