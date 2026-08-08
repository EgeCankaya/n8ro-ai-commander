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
| 2 | `claude` adapter | **ran** — adapter complete, measured, and exercised in-engine. **Not formally closed**, and it cannot ship: see *Authorization* below |
| 3 | Diagnostics on what Phase 2 carried | **closed** — four results, of which three are negative, null, or a refusal |

**The open-item register** (PRD §Carried out of Phase 3) has three states, not two. A row that
nobody should touch and a live question used to look identical; a **deferred** row now carries a
revisit condition you can test without running anything.

| # | State | Item | Revisit when |
|---|---|---|---|
| **C23** | **open** | An uncommanded aircraft that runs out of authored route stops flying | — it is a design decision between three layers |
| **C17** | **open** | Acceptance is measured by two instruments and only one is gated | — owner-decided 2026-08-06; the decision does not make the instruments agree |
| **C8** | **deferred** | A `maxTokens` ceiling sized from one 48-order run is sized against noise | a **non-Haiku model is proposed** as `claude.model`'s default |
| **C6** | **deferred** *(dependency)* | Is `n8ro-llm` ever installed — someone else's roadmap, and **nothing here is blocked on it** | **`n8ro-llm` appears in the release tree** (`bin/`, `lib/` or `include/`) — absent as of 2026-08-08 |

Everything else in the register is closed. **Two open rows, and both are on the owner's desk rather
than an engineer's.**

**Gates, all green as of 2026-08-08:**

| Suite | Result |
|---|---|
| Unit | **156 / 156** |
| Deployed-artifact smoke | **30 / 30** |
| Live three-arm scenario smoke | **22 checks, 0 failed** |
| `tools/lint-prd.ps1` · `tools/check-artifacts.ps1` | 10 checks, 0 errors, 0 warnings · 101 files, PASS |

**Measured against the PRD's success metrics:** cost **$1.05** per four-ship
scenario-hour against a ≤ $1.10 target (**met**); `reject.schema` **0.00 %** over 776
orders across two backends and three models (**met**); order acceptance **100 %** on the
synthetic-fixture soaks and **79.8 % [71, 87]** in-engine against real scenario state
(**two instruments, one bar — see C17**); local 7B round-trip p95 **7,975 ms** against a
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

| run | ON | SCRIPT-ONLY | OFF |
|---|---|---|---|
| `095026` | 3 / 2 / 0 / 0 | 3 / 2 / 0 / 0 | 2 / 1 / 0 / 0 |
| `135722` | 3 / 2 / 0 / 0 | **2 / 2 / 1 / 0** | 4 / 2 / 0 / 0 |

**The commanded/script-only identity does not replicate.** *"Identical on every column"* is
a universal claim and one counter-instance disproves it, so it is **refuted rather than
merely unsupported**. **Nothing about what the commander is worth follows from that** — n = 2,
and the columns move in opposite directions: the commanded arm launched *more*, and the
script-only arm scored the kill.

**That `1` is the first kill in seventeen archived runs**, and it is in the arm with no model
in the loop at all. **The qualification belongs with the number:** two SAM hits had already
left `BlueF16_02` `wrecked` (cumPk 0.903) before the Su-35's missile finished it. A kill by
the engine's definition, and not an unaided one.

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

- **C23 — an aircraft that reaches an ordered `hold` point stops flying.** One aircraft sat
  at 1.5 m/s for roughly 400 of 600 seconds, after which the model faithfully copied that
  speed into every order and **all 21 rejections in the run were the same speed-floor
  check**. *(The mechanism was corrected in v1.8.34: **all 39 archived below-floor samples
  were under `hold`, none under the route-exhaustion path the row originally named**, which
  is first reached ~280 s after the collapse. Arrival is not sufficient either — of five
  arrivals, three collapsed and two recovered.)* Open; the field is narrowed and the layer
  is still an owner decision.
- **In-engine acceptance is quoted as an interval, never as a point.** Every interval in
  play is 15–30 points wide.
- **Exactly one kill has ever been scored** — 2026-08-08, script-only arm, and the target was
  already `wrecked` by two SAM hits when the Su-35's missile finished it. Seventeen runs, one
  kill, none by a commanded arm.
- **The commanded/script-only identity of the first three-arm run did not replicate**, and
  n is still 2. Nothing about the commander's value is established in either direction.

## Authorization — the hosted backend cannot ship

`commander.backend = claude` reaches the network and transmits real scenario state.
All six egress grants to date authorize **measurement**, each scoped to named experiments,
and a later grant does not inherit an earlier one's boundary. **There is therefore no
authorization under which an operator other than the measurer may turn the hosted backend
on.** `claude.enabled` defaults false and is independent of `commander.backend`.

`docs/egress.md` enumerates every field that leaves the machine. Any change to that set is
a PRD revision **and** an `egress.md` revision, made before the next hosted request.

## Prerequisites

This repository contains no SDK code and **cannot be built without a licensed N8RO
release installed.** You need:

- An N8RO release tree (the folder containing `setup.cmd`)
- Visual Studio 2026 (v18.x) with the C++ workload
- `Release | x64`, C++17 (`stdcpp17`)

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
| **Unit (156)** | build `tests\ai-commander-tests.vcxproj`, run `tests\bin\release\ai-commander-tests.exe` **from the release root** | SDK only — **no server, no network** |
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
