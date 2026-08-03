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

**The specification is [`docs/prd.md`](docs/prd.md).** It is the contract: 17 functional
requirements, each with acceptance criteria and a matching user-acceptance criterion.
Read it before changing anything here.

## Status

| Phase | Scope | State |
|---|---|---|
| 0 | Scaffold, repo, empty `aiCommander` namespace | **done** |
| 1a | Full pipeline on the `stub` / `replay` backends | **done** (PR #1) |
| 1b | `local` adapter against Ollama | **done bar one gate item** — see below |
| 2 | `claude` adapter | not started; needs owner authorization |

Phase 1b measured, through the shipping adapter against Ollama 0.32.5 /
`qwen2.5:7b-instruct-q8_0`: **100 % acceptance over a 200-order soak**, `reject.schema`
**0.00 %**, `reject.shape` **0.00 %**, p95 **2,163 ms**, and the first order of a run
completes from a cold model in 4,566 ms against a 90 s budget. Unit suite 98/98, also
98/98 under AddressSanitizer; deployed-artifact smoke 30/30.

The two gate items that were previously unreachable have now **run** (PRD v1.7.4). The
headless host applies `data/config/plugins/ai-commander.cfg` as of v1.7.2, so the
commander can be switched on for an automated run; the H1 paired logs exist for review.

**The in-engine live smoke rejects a low rate of orders, and the cause is now known.** Every
rejection is the Stage-B safety envelope working. With `rawBody` delivered on Stage-B
rejections (v1.7.5) the offending order names itself: `posture: hold` carrying
`waypoint: −31.952876, 115.860450` — **Perth, Western Australia**, against an own-ship
position near Guam. The model substitutes a memorised real-world coordinate for a waypoint
whose correct value was own-ship position. Every field is well-formed and in range, so
neither the schema nor the constrained decoder can catch it — **Stage B is the only thing
that does.** A separate order in the same run had the geography right and the speed wrong
(600 m/s against a 400 bound). Two independent low-rate lapses, not one systematic fault.

It is **not** resolved by widening the geofence. The bound is what caught it.

Two caveats on that run, both in the PRD: the commanded entities are **destroyed at
~85 s** by the scenario, so a "10-minute run" measures about 85 seconds of commanding on
~10 requests — the gate is re-specified in v1.7.5 to assert over the commanded window and
to report acceptance rather than bar it, with the ≥ 95 % bar staying on the 200-order soak
where the sample size supports it. `reject.shape` held at **0 %** against situations nobody
authored. Plugin frame cost over **12,001 frames**: p50 0.0018 ms, max 0.262 ms against a
5 ms bar.

**H2 was measured and is not supported** — a byte-stable prefix is worth 3.1 %, not the
predicted ≥ 30 %, on a GPU where prompt evaluation is a small share of the round trip.

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
| Unit (87) | build `tests\ai-commander-tests.vcxproj`, run `tests\bin\release\ai-commander-tests.exe` **from the release root** | SDK only — **no server, no network** |
| ASan | same, with `/p:EnableASAN=true /p:IntDir=x64\asan\ /p:OutDir=bin\asan\`. Run it from a shell that has sourced `dev\setup-dev.cmd`, or the ASan runtime DLL will not resolve | SDK only |
| Deployed-artifact smoke (25) | `tests\smoke\run-smoke.ps1 -ReleaseRoot C:\N8RO` | a deployed DLL |
| **Live** gate harness | build `tests\live\ai-commander-live-tests.vcxproj`, run from the repo root: `--mode all --orders 200` | **a running inference server** |
| **Live** scenario smoke | `tests\smoke\run-live-scenario.ps1 -RunSeconds 600` | a server, and the commander enabled |

The first three are the CI gate and are required to run with no inference server and no
network. The two live suites are separate projects invoked by hand, deliberately — the
PRD requires that separation rather than merely recommending it.

## Running the local backend

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
- `backend=local enabled=true` — the headless `n8ro-sim-local.exe` does **not** apply
  per-plugin config, so a `.cfg` file alone will not turn the commander on there.

## Conventions

- **Derive, don't guess.** Units, frames, component type strings, field paths, and Lua
  verb arities come from the release tree's `dev/ai-coding/schema-reference/schema-reference.json`,
  the generated stubs under `data/resources/missions/stubs/`, or `dev/samples/` — never
  from memory. See the PRD's Appendix A for the traceability table.
- **The plugin never mutates entity state.** No `entityControl.requestUpdate*`, no
  `writeComponentField*`. Every motion originates from a Tier-1 `navigation.*` or
  `weapon.*` call.
- **Scope Authority.** A new Lua function, posture, order field, config field, or
  backend requires a PRD revision first. See the PRD's Scope Authority section.
- Order logs are gitignored. They contain live scenario state.

## License

See [`NOTICE`](NOTICE). Default to private; publication requires owner authorization.
