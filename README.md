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

**The specification is [`docs/prd.md`](docs/prd.md).** It is the contract: 16 functional
requirements, each with acceptance criteria and a matching user-acceptance criterion.
Read it before changing anything here.

## Status

| Phase | Scope | State |
|---|---|---|
| 0 | Scaffold, repo, empty `aiCommander` namespace | in progress |
| 1a | Full pipeline on the `stub` / `replay` backends | not started |
| 1b | `local` adapter (Ollama / llama.cpp) | blocked on OQ-1, OQ-2 |
| 2 | `claude` adapter | needs owner authorization |

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
