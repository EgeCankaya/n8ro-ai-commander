<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# N8RO AI Entity Commander — Product Requirements Document

> **One-liner:** A `n8ro-sim` plugin that lets a language model issue tactical *intent* — posture, target, waypoint, rules of engagement — to entities in a running scenario, while every kinematic decision and every state mutation stays in the deterministic C++ and Lua tiers that already exist.

**Date:** 2026-07-31 (revised 2026-08-01)
**Status:** Draft v1.3
**Revision history:**
- v1.3 — reconciled the contract with two findings from the Phase 1a gate, both recorded in [PR #1](https://github.com/EgeCankaya/n8ro-ai-commander/pull/1). (a) **"TSAN clean" is unsatisfiable on the target platform** — no ThreadSanitizer runtime ships for Windows — so the Phase 1a gate item, the §Risks Threading mitigation, the integration-suite bullet, and UAC-AIC-ARCH-2 now name the evidence that was actually produced (ASan 65/65, a 20,000-publish exchange-slot stress test with torn-read detection, and a `static_assert`-enforced value-only capture), with the residual gap versus a real race detector stated rather than papered over. (b) **The prompt prefix was measured** at 4,738 bytes ≈ 1,200 tokens on a live engine run; recorded as evidence against OQ-8, which stays **open** — the padding call is a Phase 2 cost judgement for the owner — and the Cost model's arithmetic is recomputed off the measured figure instead of the ~800-token assumption.
- v1.2 — closed the snapshot-reachability gap found in design: sensor tracks and weapon loadout have no public C++ read seam, so Tier 1 now reports them into the commander (`aiCommander.reportTrack` / `reportLoadout` in AIC-API-1; Stage-B B3, §Exactly what is transmitted, and AIC-ORD-2 restated accordingly). Folded in four mechanism corrections verified against the shipped headers (transport failure is `std::nullopt`, not `statusCode == 0`; detections arrive as triples; transform velocity/orientation/acceleration are runtime columns on dot-joined paths that read back `0` silently; Stage-B B1 uses `IEntityManager::getEntity`). Added AIC-ARCH-4 (runtime-column startup probe) and OQ-9.
- v1.1 — added §Source control and repository (standalone repo, layout, ignore rules, the single `open-solution.cmd` relocation edit, CI split, visibility); recorded the owner's decision that plugin source files carry no Arkheon per-file header; added the `prompt.doctrinePath` config field, which §Scope Authority requires be authorized here before design.
**Owner:** Arkheon Technologies — N8RO platform
**Audience:** Platform owner (authorization + cost decisions), plugin implementer (build contract), QA
**Tier:** Comprehensive
**Classification:** Proprietary and Confidential

## Purpose and scope

This PRD specifies a single deliverable: a C++ plugin, built against the shipped N8RO SDK in the release tree at `C:\N8RO`, that adds a Tier-2 *commander* to the existing simulation stack. The commander observes a small, bounded snapshot of an entity's tactical situation, asks a language model for a structured order, validates that order hard, and publishes it where the entity's existing deterministic Lua behavior can read it. The plugin never moves an entity, never writes component state directly, and never blocks the simulation update thread.

The boundary of this document is the plugin and its contracts: the order schema, the validation pipeline, the Lua API surface, the configuration set, the record/replay format, and the two backend adapters. It does not specify platform changes — nothing in `bin/`, `include/`, or the shipped engine is modified. It exists as a separate document because the work introduces the first nondeterministic decision source into a platform whose own documentation treats reproducibility as a design goal; that tension needs a written contract before code is written.

### Source inputs

**Authoritative (binding):**
- `n8ro-ai-commander-prd-prompt.md` — owner's authoring brief: the decided three-tier architecture, the backend strategy, the measured latency budget, the verified SDK facts, and the enumerated risks and open questions. Its "Open questions" are carried into this PRD unresolved, by instruction.
- `docs/modules/n8ro-sim/dev/extension-points.md:140` — *"Scripting is a decision layer — authoritative state mutation stays in C++."* This single line fixes the tier split.
- `dev/ai-coding/schema-reference/schema-reference.json` — the sole authority for component type strings, **authored** field paths, units, and frames. Every unit in this PRD's order schema is read from a `unit` key in that file, not from memory. It does **not** cover runtime state: `componentTransform` exports only `positionGeodetic/{latitudeDeg,longitudeDeg,altitudeHaeM}`, `headingDeg`, and `speedMps` (v1.2).
- `include/n8ro-sim/entity/TransformRuntimeColumns.h` — the single source of truth for the transform's **runtime** pose and motion columns (`velocityNed.*`, `accelerationNed.*`, `orientationBodyToNedQuat.*`). Paths are **dot**-joined, not slash-joined as in the schema, and the header warns that resolving a name that does not exist yields a handle reading back `0` *without* an error (v1.2 — drives AIC-ARCH-4).
- `include/n8ro-core/plugin/IPlugin.h:36` — *"…is delivered on the host's single update thread, never concurrently with one another."*
- `include/n8ro-core/core/net/IHttpClient.h` — `send()` is blocking and returns `std::optional<HttpResponse>`; the class is documented single-thread-only; `https` requires the OpenSSL build.
- `include/n8ro-sim/component/ComponentFieldAccess.h` — the generic field-access seam. It exposes `…Real` / `…Int` / `…Text` readers only; there is no reader for a schema `list` node (v1.2 — why loadout is not reachable here).
- `data/resources/missions/stubs/{navigation,weapon,sensor,entityControl,mission,simulation}.lua` — the generated stubs are the authority on verb arities and return shapes.

**Contextual (informational, not binding):**
- `data/resources/missions/oppint_red_interceptor.lua` — the quality bar for a Tier-1 behavior and the source of this PRD's posture vocabulary.
- `docs/modules/n8ro-sim/dev/handbooks/mission-lua-scripting-handbook.md` — Lua runtime contract.
- `dev/samples/sim/sim-scripting/` — the clone source and the registration idiom.
- `docs/modules/n8ro-llm/` — describes a service that is **not installed**; read for intent, depended on for nothing.

### Corrections verified in-tree

Items 1–3 corrected the authoring brief against what is actually installed. Items 4–7 were found in
design (2026-08-01) by reading the shipped headers and generated stubs, and correct *this document*.
Items 8–9 were found at the **Phase 1a gate** (2026-08-01, PR #1) by running the toolchain and the
engine rather than by reading them, and correct requirements this document had written from
assumption. Each is load-bearing.

1. **HTTPS is available.** `IHttpClient.h` warns that `https` needs the OpenSSL build. `bin/libssl-3-x64.dll` and `bin/libcrypto-3-x64.dll` are both present, so the Phase 2 call to `api.anthropic.com` is not blocked on a missing TLS backend. This must still be asserted at runtime (see AIC-BE-4).
2. **A sanctioned async mechanism exists.** `IThreadRunner::submitBackgroundTask` is declared in the SDK. The plugin does not need a hand-rolled `std::thread` — but `PluginContext::threadRunner` is documented nullable, so a fallback is still required (OQ-7).
3. **The bot executables live in `bin/`, not `bin/ai/`.** `bin/ai/` contains only `.env`, `n8ro-mcp.exe`, and `run-full-stack.cmd`; `n8ro-data-bot.exe` and `n8ro-sim-bot.exe` are one level up in `bin/`, alongside the readiness flag `bin/n8ro-mcp-ready.tmp`. Relevant only to OQ-4.
4. **Sensor tracks and weapon loadout have no public C++ read seam.** *(v1.2)* There is no track component in `schema-reference.json` (225 records) or `ComponentTypeNames.h` (15 components); `IEntityManager` exposes no track accessor; `SensorScriptingApi.h` is an opaque factory returning `unique_ptr<IScriptingApiModule>`. `sensor.getDetectionList` is documented as returning *"runtime detections"* and exists only in Lua. Separately, `componentWeaponStoreManagement/loadout` is a schema `list`, and `ComponentFieldAccess` has no list reader; live remaining ammo is available only through `weapon.getWeaponLoadout`, also Lua-only. **Consequence:** the plugin cannot build the `tracks[]` and `own.loadout[]` rows of §Exactly what is transmitted by itself, and Stage-B check B3 cannot query the engine. Resolved by Tier-1 ingress — see AIC-API-1 `reportTrack` / `reportLoadout`.
5. **A transport failure is `std::nullopt`, not `statusCode == 0`.** *(v1.2)* `IHttpClient::send()` returns `std::optional<HttpResponse>` and yields `std::nullopt` on a transport / TLS failure or a malformed URL. A response that *is* returned always carries a real status-line code. The `HttpResponse::statusCode == 0` sentinel documented in the struct is therefore not what a caller observes on failure. Affects Stage-A check A1, AIC-BE-1, and AIC-BE-4.
6. **Detections arrive as repeating triples, not a list.** *(v1.2)* `sensor.getDetectionList(sensorEntityId)` returns `targetEntityId, range_m, snr_DB` repeated as Lua multiple return values, and returns *no* values when there is no detection. The countable enumeration idiom is `sensor.getTrackNr(id)` followed by `sensor.getTrackById(id, i)` with `i` **1-based**. Affects the reference Tier-1 script and the snapshot.
7. **Transform velocity, orientation, and acceleration are runtime columns, not schema leaves.** *(v1.2)* They are declared only in `TransformRuntimeColumns.h`, addressed by **dot**-joined paths (`velocityNed.x`), and — per that header — a path that does not resolve reads back `0` **silently, without an error**. That defeats this PRD's rule that a `std::nullopt` on a required snapshot field aborts the snapshot, because a mistyped path yields a plausible zero instead of a `nullopt`. Drives AIC-ARCH-4 and OQ-9. Stage-B B1's `entityControl.exists` is likewise a Lua verb; the C++ equivalent is `IEntityManager::getEntity(id) != nullptr`.
8. **ThreadSanitizer does not exist on this platform.** *(v1.3)* No `clang_rt.tsan` runtime ships anywhere in Visual Studio 2026 Insiders. `VC\Tools\MSVC\14.51.36231\include\sanitizer\` carries `tsan_interface.h` and `tsan_interface_atomic.h` — **headers only**, with no library to link — alongside the ASan, UBSan, and fuzzer runtimes, which do ship complete. Neither MSVC nor the bundled LLVM toolchain supports TSan on Windows. **Consequence:** the v1.2 Phase 1a gate item "TSAN clean" was permanently unsatisfiable, and a gate no build can pass is worse than no gate — it trains everyone to wave the checklist through. Replaced by the concurrency-evidence set in §Validation and test plan, with the residual gap recorded as a risk and race-detector coverage moved to §Out of scope. Affects the Threading risk row, the integration suite, the Phase 1a gate, the rollback triggers, and UAC-AIC-ARCH-2.
9. **The rendered prompt prefix is ~1,200 tokens, not ~800.** *(v1.3)* Measured on a live engine run: **4,738 bytes**, logged at startup as `prefixBytes`, ≈ 1,200 tokens. Haiku 4.5's prompt-cache minimum is 4,096 tokens, so the prefix as written silently does not cache — the direction v1.2 already assumed, but now with a real number rather than the "roughly 800–1500" range OQ-8 was written against. **Consequence:** the Cost model's per-order arithmetic was computed from a 1,000-token prompt and is understated; it is recomputed against ~1,400 input tokens below. This is **evidence into OQ-8, not a resolution of it** — whether to pad to the cache minimum remains a Phase 2 cost judgement for the owner.

## Problem statement

> **When** an operator wants an entity in a running N8RO scenario to behave adaptively — to change posture, re-prioritize targets, or break off — **they must** hand-author that judgment ahead of time as a deterministic Lua ladder, **which means** every scenario's tactical repertoire is frozen at authoring time and every new situation requires a scripting change by someone who knows both the tactics and the Lua API.

The tree ships 6.5 GB of quantized language models (`data/ai/model/`), a doctrine corpus (`data/ai/context/`), an MCP stack, and a documented `n8ro-llm` module — and no working path from any of it to entity behavior in a running scenario. Meanwhile `oppint_red_interceptor.lua` demonstrates how good the deterministic layer already is: two-ship target deconfliction, launch-range discipline at 0.8 of kinematic reach, shoot-look-shoot with a flight-time-derived assessment window, 40° crank geometry, defensive pump inside 15 km, winchester egress. Its 455 lines encode real judgment. What they cannot encode is *situational* judgment — the script's posture ladder at lines 346–439 (`defend` → `rtb` → `ingress` → `crank` → `engage`) is a fixed priority cascade, and changing it means editing Lua.

The commander's job is to replace exactly that cascade — the posture selection — with model-issued intent, and to leave the flying logic alone.

### Prior art and lessons learned

- **`n8ro-llm` was designed for this space and does not solve it.** `docs/modules/n8ro-llm/` describes a `SimLuaLLMHost` with a 7-step RAG pipeline on message-bus topics `n8ro-llm/generate` and `n8ro-llm/generate/result`. There is no `n8ro-llm.dll`, no import library, and no headers under `include/`. Beyond being absent, its pipeline generates *Lua scripts* — an authoring aid, not a control path. What we learn: the useful seam is a message-bus request/response boundary, and this plugin should be structured so it can adopt that boundary later without redesign (see OQ-3).
- **The MCP stack exists but its entity-control surface is unverified.** `bin/ai/run-full-stack.cmd` brings up `n8ro-data-bot.exe` → `n8ro-sim-bot.exe` → `bin/ai/n8ro-mcp.exe` over ZMQ IPC. If `n8ro-sim-bot` already exposes entity control as MCP tools, that is a shorter path than a new plugin. Nobody has confirmed it (OQ-4).
- **The Lua sandbox closes the obvious shortcut.** The scripting sandbox opens only `base`, `math`, `table`, `string`; `os`, `io`, `package`, `require`, `load`, and `dofile` are explicitly disabled, and every hook runs under a Lua instruction budget. There is no pure-Lua route to an HTTP call or an async wait. Whatever does the inference must be C++.
- **What is different this time:** the LLM is scoped out of the control loop from the first line of design, rather than being retrofitted out after a latency measurement disappoints.

## Goals and success metrics

### Goals

1. **An entity in a running scenario changes posture on model-issued intent, with no change to platform binaries.** Measured by: a live scenario run in which posture transitions in the entity's log correspond to accepted orders.
2. **The simulation update thread never waits on inference.** Measured by: per-frame plugin cost histogram (below).
3. **No invalid, hallucinated, or unsafe order ever reaches a `request*` verb.** Measured by: the validator's rejection counters plus an adversarial unit-test corpus.
4. **A run is reproducible from its order log.** Measured by: the replay determinism test.
5. **The hosted-API path is off unless an owner turns it on, and what it transmits is enumerated and asserted.** Measured by: default config state plus the transmitted-field allowlist test.

### Success metrics

| Metric | Baseline (current) | Target | How measured | Timeline |
|---|---|---|---|---|
| Plugin cost per `onTickFrame` | N/A — no implementation | p95 < 0.5 ms, p99 < 2.0 ms | Plugin-owned timing histogram, dumped via `aiCommander.getStats()` | Phase 1a gate |
| Order round-trip latency (request → accepted) | N/A | Local 3B CPU: p95 ≤ 8 s. Local 7B CPU: p95 ≤ 20 s. Claude Haiku 4.5: p95 ≤ 2.5 s | Worker-side timer recorded per order in the order log | Phase 1b / Phase 2 gates |
| Order acceptance rate (accepted ÷ requested) | N/A | ≥ 95 % over a 200-order soak, per backend | Rejection counters broken out by cause | Phase 1b gate |
| Parse/schema rejection rate | N/A | < 1 % with constrained decoding | Counter `reject.schema` ÷ requested | Phase 1b gate |
| Replay reproducibility | N/A | 100 % identical order sequence + content across two replays of one log | Replay determinism test (see Validation) | Phase 1a gate |
| Direct kinematic writes by the plugin | N/A | Exactly 0 | Call-site audit + a test asserting the plugin never invokes `entityControl.requestUpdatePosition/Velocity/Orientation` or `writeComponentField*` on `componentTransform` | Every gate |
| Cost per four-ship scenario-hour (Phase 2) | N/A | ≤ $1.10 with Haiku 4.5 | Token accounting in the order log × published rate | Phase 2 gate |

*(v1.3)* The cost target above was set against a ~1,000-token prompt. The measured prefix (§Corrections, item 9) puts the **uncached** Haiku figure at ~$1.30 per four-ship-hour, above the target; the cached-and-padded figure is ~$0.73, below it. The target is left unchanged because meeting it is exactly the question OQ-8 asks, and moving a target to match a measurement would erase the signal.

### Non-goals / deferred scope

- **Not a real-time LLM control loop.** The measured floor is 3–6 s per order for the 3B on CPU and 10–15 s for the 7B. Anything reactive — missile defeat, merge maneuvering, launch timing — stays in Tier 0/1 where it already works. The trade-off accepted: the commander cannot respond to events faster than its cadence, so Tier 1 must be able to act correctly on a stale order.
- **Not a replacement for deterministic tactics.** The commander parameterizes `oppint_red_interceptor.lua`'s behavior; it does not reimplement crank geometry, launch discipline, or defensive pump. The trade-off: order expressiveness is capped at what the existing verbs accept, which is precisely the point.
- **Not a scenario-authoring tool.** Generating scenarios, entity profiles, or Lua scripts is out. That is the space `n8ro-llm` was designed for, and it can have it.
- **Not multi-entity coordination in v1.** Orders are per-entity. Section-level intent ("two-ship pincer") is a genuinely different problem — it needs a shared world model, order consistency across entities, and conflict resolution. Deferred with a dated entry below.

## Out of scope

| Item | Status | Rationale | Target | Added |
|---|---|---|---|---|
| Section / flight-level coordinated orders | Deferred | Requires a shared multi-entity world model and inter-order consistency rules. Per-entity orders must be proven stable first; the schema reserves `sectionId` for the extension. | v1.1 | 2026-07-31 |
| RAG over `data/ai/context/` doctrine corpus | Deferred | Phase 1 uses a hand-authored 1–2 page doctrine block, which is byte-stable by construction. Retrieval makes the prefix volatile and destroys prompt caching unless the retrieval is itself cached. Bring in when order quality plateaus. | v1.1 | 2026-07-31 |
| Consuming `n8ro-llm/generate` over the message bus | Out of scope | The module is not installed — no DLL, no `.lib`, no headers. Cannot be depended on. Revisit if OQ-3 resolves to "yes". | N/A until installed | 2026-07-31 |
| Routing through the MCP stack (`n8ro-sim-bot.exe`) | Deferred | Its entity-control tool surface is unverified. If OQ-4 resolves to "sanctioned path", this PRD's Alternative 2 supersedes Alternative 1 and the plugin becomes a thin bridge. | Pending OQ-4 | 2026-07-31 |
| Fine-tuning or training any model | Out of scope | No training infrastructure ships; not the owner's brief. | N/A | 2026-07-31 |
| UI surface (`n8ro-ui` plugin) for order inspection | Deferred | Orders are observable via the order log and `aiCommander.getStats()`. A UI is presentation, not capability. | v1.1 | 2026-07-31 |
| Streaming / partial order consumption | Out of scope | An order is atomic; a half-parsed order is a rejected order. Streaming adds failure modes and buys nothing at an 80-token output. | N/A | 2026-07-31 |
| GPU provisioning or inference-server packaging | Out of scope | No inference server ships in this tree. Installing and running one on target machines is a deployment responsibility, recorded as a dependency and OQ-2. | N/A | 2026-07-31 |
| Plugin-side (C++) reading of sensor tracks or weapon loadout | Out of scope | No public read seam exists: no track component in the schema or `ComponentTypeNames.h`, no `IEntityManager` accessor, and `ComponentFieldAccess` has no reader for the `list`-typed loadout (§Corrections, item 4). Tier 1 reports both instead (AIC-API-1). Revisit only if the SDK later exposes a track surface — the ingress verbs would then become an optional override rather than the only path. | N/A until the SDK exposes one | 2026-08-01 |
| Plugin-side synthesis of a track list from the entity roster | Out of scope | Technically available via `IEntityManager::getAllEntities()`, and deliberately rejected: a roster is not a sensor picture. Substituting one would silently grant the model vision through terrain and beyond sensor range, and Stage-B B3 would then validate hallucinated targets against that fiction rather than catching them. | N/A — rejected, not deferred | 2026-08-01 |
| Dynamic race detection (ThreadSanitizer) over the sim-thread/worker boundary | Out of scope | No TSan runtime exists on the target platform: VS 2026 Insiders ships `tsan_interface.h` headers with no `clang_rt.tsan` library, and neither MSVC nor the bundled LLVM toolchain supports TSan on Windows (§Corrections, item 8). Substituted, not skipped — ASan over the full suite, a 20,000-publish exchange-slot stress test with torn-read detection, and a `static_assert`-enforced value-only worker capture (§Validation and test plan). The residual gap is recorded as a risk row rather than closed. | N/A until a race detector exists for this toolchain | 2026-08-01 |
| Free-text track attributes (`team`, `kind`, `domain`) in the prompt | Deferred | The v1.2 ingress verbs carry scalars only. Adding attributes means widening `reportTrack`, which is a PRD revision, and each added string is a new injection surface to charset-filter. Revisit if order quality shows the model cannot discriminate targets without them. | v1.1 | 2026-08-01 |

## Key hypotheses

- **H1: Posture-level intent at a 15–30 s cadence produces observably better behavior than a fixed posture cascade,** because the reference script's own posture transitions occur on tens-of-seconds timescales (assessment windows are clamped to 10–30 s; the defensive pump triggers at 15 km closure). *Signal:* count of posture transitions per engagement that a domain reviewer marks "appropriate given the situation", commander-on vs commander-off. *Validated by:* paired scenario runs on the same seed. *If wrong:* the cadence is too slow for any useful decision and the commander's value is limited to pre-engagement setup — in which case the honest response is to narrow the scope to mission-start intent rather than to speed up the loop.
- **H2: A byte-stable prompt prefix reduces p95 local latency by ≥ 30 %,** because both llama.cpp and Ollama cache the processed KV of an unchanged prefix and skip re-evaluating it. *Signal:* p95 latency with a byte-stable prefix vs. a prefix whose whitespace is perturbed each request. *Validated by:* A/B measurement over 100 orders on one scenario. *If wrong:* prefix discipline still costs nothing, but the cadence targets must be re-derived from raw prompt-eval throughput.
- **H3: Constrained decoding (GBNF grammar locally, `json_schema` structured output on Claude) drives the parse/schema rejection rate below 1 %,** versus double-digit rates from free-form prompting of a 3B model. *Signal:* the `reject.schema` counter over a 200-order soak, constrained vs unconstrained. *Validated by:* the same soak run twice. *If wrong:* the retry budget and the "retain last valid order" fallback carry more weight than planned, and the effective order cadence degrades by the retry factor.

## Tenets

Decision tie-breakers for implementation, in priority order — *unless you know better ones.*

1. **Determinism over intelligence.** When the deterministic tier and the model disagree, the deterministic tier wins. A stale valid order beats a fresh questionable one.
2. **The sim thread never waits.** No blocking call, no lock held across I/O, no unbounded work in `onTickFrame`. If the choice is between a fresher order and a stalled frame, the frame wins every time.
3. **Reject, don't repair.** An order that fails any check is discarded whole. Never clamp-and-accept a field that was out of range for a reason we don't understand; never patch missing fields with defaults. A repaired order is an order nobody specified.
4. **Explicit authorization over convenience.** Anything that moves proprietary scenario data off the machine is off by default and requires a positive act to enable, per run. Convenience never justifies a silent egress.
5. **Derive, don't guess.** Units, frames, field paths, and verb arities come from `schema-reference.json` and the generated stubs. A value that cannot be traced to one of those does not go in the code.

## Security posture and trust boundaries

*[Security]*

### Trust boundaries

- **Inside the boundary (Phase 1):** the N8RO process, the release tree, and a local inference server bound to loopback. `bin/ai/.env` already points at `http://localhost:11434`. No scenario data leaves the host.
- **Crossing the boundary (Phase 2):** every byte of the prompt's volatile suffix leaves the machine and reaches a third-party service. Every file in this tree carries an Arkheon proprietary/confidential header; scenario state — positions, ORBAT, team assignments, loadouts — inherits that classification.
- **Untrusted input inside the boundary:** `componentTrackIdentity` fields (`trackSource`, `callsign`, `originCountry`) are documented as free text ingested from an external track source such as a live ADS-B feed. Any such string that reaches a prompt is attacker-influenced text.

### Enforcement model

- **Fail-closed on the master switch.** `commander.enabled` defaults `false`. With it false the plugin loads, registers its namespace, and issues no orders. Every Lua getter returns the "no order" shape.
- **Fail-closed on the hosted backend.** `claude.enabled` is a *second, independent* switch defaulting `false`. Setting `commander.backend = "claude"` without `claude.enabled = true` is a configuration error that leaves the commander disabled with a logged reason — it does not silently fall back to local, because a silent fallback teaches operators that the switch does not matter.
- **Fail-closed on transport.** A `std::nullopt` from `IHttpClient::send()` (transport/TLS failure or malformed URL), a non-2xx status, a timeout, or an unparseable body all resolve to "no new order". The last valid order is retained.
- **Credentials are never persisted.** `claude.apiKeyEnvVar` stores the *name* of an environment variable. The value is read via `std::getenv` at request time, held for the duration of the request, never logged, never written to the order record, and never returned from `getConfigFields()`.

### Exactly what is transmitted

This list is the contract. A unit test asserts the serialized volatile suffix contains no field outside it (AIC-SEC-2).

**Stable prefix — contains no scenario data:**
- System prompt (role, constraints, refusal instruction)
- Posture and ROE vocabulary with plain-language definitions
- The Order JSON schema
- Hand-authored doctrine text (1–2 pages, generic tactical doctrine, no scenario or platform specifics)

**Volatile suffix — per commanded entity, per order:**

| Field | Source | Note |
|---|---|---|
| `simTimeS` | the `onTickFrame` simulation clock | |
| `own.entityId` | roster | Scenario-local runtime id string |
| `own.latitudeDeg`, `own.longitudeDeg`, `own.altitudeHaeM` | `componentTransform` schema leaves `positionGeodetic/*` | |
| `own.velN`, `own.velE`, `own.velD` | `componentTransform` **runtime columns** `velocityNed.x/.y/.z` | m/s, NED. Dot-joined paths per `TransformRuntimeColumns.h`; probed at startup (AIC-ARCH-4) |
| `own.headingDeg` | `componentTransform` schema leaf `headingDeg` | Degrees clockwise from true north |
| `own.team` | `IEntity::getTeam()` | |
| `own.loadout[]` | `aiCommander.reportLoadout`, called by Tier 1 from `weapon.getWeaponLoadout` | `hardpointName`, `weaponProfileName`, `ammoCount`, `ammoMax` |
| `tracks[]` (≤ `commander.maxTracksInPrompt`) | `aiCommander.reportTrack`, called by Tier 1 from `sensor.getTrackNr` + `getTrackById` | `targetEntityId`, `rangeM`, `snrDb` |
| `situationNote` | `aiCommander.setSituationNote` | ≤ 256 chars, sanitized |

**On the two reported rows** *(v1.2)*. Tracks and loadout are **pushed in by Tier 1**, not pulled by
the plugin, because neither has a public C++ read seam (§Corrections, item 4). This narrows the
transmitted set rather than widening it, and it *strengthens* AIC-SEC-2 in three ways: the plugin can
only transmit what a deterministic script explicitly handed it; `team`, `kind`, and `domain` are
dropped from the track row because the reporting verb does not carry them and the plugin will not
infer them; and the ingress verbs accept only scalars, so no free-text field can enter the prompt
through this path at all.

**Explicitly not transmitted:** file paths; scenario or mission file names; entity *profile* names; terrain or AI database contents; the `data/ai/context/` corpus; entitlement or license data; any config field value other than the model name and cadence; any `componentTrackIdentity` free-text field (see threat table).

### Threat model

| Threat | Impact | Mitigation |
|---|---|---|
| API key persisted in a `PluginConfigField` and written to disk in plaintext | Credential disclosure to anyone with filesystem access to the config | Config holds the env-var *name*; value read at request time; `getConfigFields()` never returns it; redaction test asserts the key string appears in no log or record |
| Proprietary scenario state egresses to a hosted API without authorization | Classification breach | `claude.enabled` defaults false and is independent of `commander.backend`; startup emits an explicit warning naming the destination host; the transmitted-field allowlist is asserted by test |
| Prompt injection via `componentTrackIdentity.callsign` or entity names sourced from an external feed | Model emits attacker-chosen posture, target, or waypoint | (a) `componentTrackIdentity` fields are excluded from the prompt entirely; (b) all remaining string fields are charset-filtered and length-capped before rendering; (c) the validator is the real defense — no order reaches a verb without passing the full pipeline regardless of what the model emits |
| Model targets a friendly entity | Fratricide in the scenario; invalid training/demo value | Target team is compared against own team on the sim thread; mismatch required. ROE gate applies independently. Tier 1 retains final fire authority |
| Plain-HTTP misconfiguration of the hosted backend | Key and scenario data in cleartext on the wire | `claude.baseUrl` must match `https://`; validated at `applyConfigFields`, rejected otherwise. TLS availability asserted at first request (AIC-BE-4) |
| Order log grows without bound during a long run | Disk exhaustion on the host machine | Size-capped rotation, default 64 MB across 4 files, configurable |
| A worker thread touches `IEntityManager` and races the sim thread | Undefined behavior, corrupted entity state, non-reproducible crashes | The worker receives a POD snapshot by value and holds no SDK pointer. Enforced structurally: the worker's callable captures only the snapshot and the client, never `ScriptingApiContext` |

## Functional requirements

### Naming and path conventions

These conventions govern every FR below. Drift between this table and the implementation is a review finding.

**Lua namespace authority.** The plugin registers exactly one new namespace, `aiCommander`, via `MissionRegistrar::registerFunctionWithMetadata`, following `dev/samples/sim/sim-scripting/src/EntityStateApiModule.cpp:145`. It extends no existing namespace. Rationale: built-in namespaces (`navigation`, `weapon`, `sensor`, `entityControl`, `mission`, `simulation`, `scenarioControl`, `animation`) are engine contracts; adding to them makes generated stubs ambiguous about ownership.

**Lua return-shape convention.** Follows the tree's existing idioms exactly: multi-valued reads return Lua multiple returns with a `nil` tuple on failure (`navigation.getPositionLatLonAlt`); structured reads return a JSON string with `"{}"` / `"[]"` on absence (`mission.getState`); commands return `boolean` (`navigation.requestGoTo`). No new convention is introduced.

**Component access convention.** All entity state the plugin reads for itself goes through `component/ComponentFieldAccess.h` — `readComponentFieldReal` / `Int` / `Text` — addressed by `(componentType, fieldPath)`, where the type string comes from `component/ComponentTypeNames.h`. Each reader returns `std::nullopt` on a missing entity, component, or field, or on a type mismatch; a `std::nullopt` on any required snapshot field aborts that entity's snapshot for the tick. The plugin calls **no** `writeComponentField*`.

There are **two distinct path forms**, and confusing them is silent *(v1.2)*:

| Kind | Path form | Authority | Failure mode |
|---|---|---|---|
| Authored schema leaf | slash-joined, the `schema-reference.json` `path` with the `/datablocks/<componentType>/` prefix dropped — e.g. `"positionGeodetic/altitudeHaeM"` | `schema-reference.json` | `std::nullopt` — loud, and honoured by the abort rule above |
| Transform runtime column | **dot**-joined — e.g. `"velocityNed.x"` | `entity/TransformRuntimeColumns.h` | reads back **`0` with no error** — silent, and *not* caught by the abort rule |

Because a mistyped runtime-column path yields a plausible zero rather than a `nullopt`, the runtime columns the snapshot depends on are probed once at startup against a known-moving entity and the commander refuses to enable if the probe fails (AIC-ARCH-4).

**State the plugin does not read.** Sensor tracks and weapon loadout have no public C++ read seam (§Corrections, item 4). Tier 1 reports them in through `aiCommander.reportTrack` / `reportLoadout` (AIC-API-1). The plugin SHALL NOT attempt to reconstruct a track list by other means — in particular, it SHALL NOT synthesize one by scanning `IEntityManager::getAllEntities()`, because an entity roster is not a sensor picture and substituting one for the other would silently give the model omniscient vision through terrain and beyond sensor range.

**Config field naming.** `PluginConfigField` is a flat list of typed name/value descriptors (`PluginConfigFieldType::{Int,Real,Bool,Text}`), values carried as canonical strings. Names are dotted, grouped by concern: `commander.*`, `local.*`, `claude.*`, `safety.*`, `record.*`, `replay.*`. Rationale: the type tag drives editor rendering and parsing, so every field must pick the narrowest type that fits — no JSON-in-a-Text-field.

**Module identity.** Each API module reports a `moduleId()` of the form `arkheon.aiCommander.<module>`, mirroring the sample's `"sample.entityState"`.

**Source file headers.** Files authored for this plugin — `.h`, `.cpp`, `.lua`, `.cmd`, and repository docs — **do not carry the Arkheon proprietary/confidential header**. Decided by the owner, 2026-08-01. The tree-wide instruction to preserve that header (`CLAUDE.md`) governs files *inside* the release tree; the plugin is a separate work product in its own repository and takes its own notice from that repository's `NOTICE` file instead of a per-file banner. This is a change of provenance marking only and does not weaken any data-classification requirement: the tree's classification still attaches to scenario *state*, which is what §Security posture governs (see AIC-SEC-2, and the threat table's egress row). Nothing in the plugin's own source is reclassified by this decision — the repository's visibility is what controls disclosure, and that is set in §Source control and repository.

**Where files live.** The plugin's authored files live in their own repository; its build and runtime artifacts land in the release tree. The full layout, the one build-wiring edit relocation requires, and the ignore rules are specified in §Source control and repository under Configuration and deployment.

### Architecture and tiering

#### AIC-ARCH-1: Three-tier decision split
The system SHALL confine the language model to Tier 2 (intent), leaving Tier 1 (deterministic tactics, 1–10 Hz Lua) and Tier 0 (per-tick C++ engine systems) unchanged in authority.

**Customer scenario:** A scenario author wants an interceptor to adapt its posture to an evolving air picture without rewriting `oppint_red_interceptor.lua`'s engagement logic.

**Pain removed:** Today the posture cascade at `oppint_red_interceptor.lua:346–439` is a fixed priority ladder; any change in tactical judgment requires editing Lua and re-running the scenario. There is no path from the shipped models to entity behavior at all.

**Acceptance criteria:**
- The plugin issues no call to `entityControl.requestUpdatePosition`, `requestUpdateVelocity`, or `requestUpdateOrientation`.
- The plugin issues no `writeComponentField*` call against `componentTransform`.
- A Tier-1 script with the commander disabled behaves byte-identically to the same script with the plugin absent.
- Every entity motion originates from a `navigation.*` or `weapon.*` verb invoked by Tier 1.

**Trace:** UAC-AIC-ARCH-1

#### AIC-ARCH-2: Snapshot → worker → order-slot threading
The system SHALL perform all inference on a worker context, exchanging only value-copied data with the simulation update thread through mutex-guarded latest-wins slots.

**Customer scenario:** An operator runs a scenario with the 7B model on CPU, where a single order takes 10–15 s, and expects the simulation to keep its frame rate.

**Pain removed:** `IHttpClient::send()` is blocking and single-thread-only; calling it from `onTickFrame` would stall the simulation for the full inference time. `ScriptingApiContext`'s collaborators are single-thread-only, so the naive fix — sharing the context with a thread — is undefined behavior.

**Acceptance criteria:**
- WHEN a request is in flight, the plugin's `onTickFrame` cost SHALL remain within the p95/p99 budget in Success Metrics.
- The worker callable SHALL capture only a POD snapshot and the `ILlmClient`; it SHALL NOT capture or dereference `IEntityManager`, `IScenarioManager`, `MessageBusPacked`, `ISimulationEngine`, or `ScriptingApiContext`.
- *(v1.3)* That capture constraint SHALL be enforced at compile time by `static_assert` over the captured types, and the snapshot's independence SHALL be covered by a deep-copy-outlives-original test. This criterion is load-bearing rather than belt-and-braces: with no race detector available on the target platform (§Corrections, item 8), a structural proof that the worker holds nothing shared is the strongest guarantee this design can obtain, and it is the one that does not depend on which interleavings a test run happened to hit.
- Snapshot and order slots SHALL be latest-wins: a newer snapshot overwrites an unconsumed older one; a newer completed order overwrites an unconsumed older one.
- WHILE `commander.enabled` is false, no worker SHALL be scheduled.
- IF `PluginContext::threadRunner` is null, THEN the plugin SHALL fall back to an owned worker thread; IF that also fails, THEN the commander SHALL remain disabled and log the reason once.

**Trace:** UAC-AIC-ARCH-2

#### AIC-ARCH-3: One `ILlmClient` seam, swappable adapters
The system SHALL express every backend behind a single `ILlmClient` interface selected at runtime by `commander.backend`, with adapters for `stub`, `replay`, `local`, and `claude`.

**Customer scenario:** A developer runs the full order pipeline in CI on a machine with no inference server, and the platform owner later switches a live run from the local model to Claude without a rebuild.

**Pain removed:** No inference server ships in this tree, so without a stub backend the pipeline is untestable anywhere. Without a runtime seam, the Phase 2 backend is a fork rather than a config change.

**Acceptance criteria:**
- All four adapters satisfy one interface taking a rendered prompt and returning either a raw response body or a transport failure.
- Switching `commander.backend` through `applyConfigFields` takes effect without restarting the process; an in-flight request from the previous backend is discarded on completion.
- The `stub` adapter performs no I/O and returns from a fixed order table.
- Validation, recording, and Lua publication are adapter-independent — identical code paths for all four.

**Trace:** UAC-AIC-ARCH-3

#### AIC-ARCH-4: Runtime-column startup probe
*(Added v1.2)* The system SHALL verify at startup that every `componentTransform` runtime column the snapshot depends on resolves to a real column, and SHALL refuse to enable the commander if any does not.

**Customer scenario:** An operator upgrades the release tree, a runtime column is renamed, and every subsequent order is computed from a snapshot whose velocity reads `0, 0, 0` — with no error anywhere.

**Pain removed:** `TransformRuntimeColumns.h` states that resolving a name that does not exist yields a handle that *"reads back 0 WITHOUT an error, so a typo here is silent — an entity simply never moves."* Every other required snapshot field fails loudly as `std::nullopt` and aborts the snapshot; these three do not. Without a probe, the single most likely snapshot defect is also the least visible one, and it degrades order quality rather than producing a diagnosable failure.

**Acceptance criteria:**
- The plugin SHALL probe `velocityNed.x`, `velocityNed.y`, and `velocityNed.z` on `componentTransform` once per scenario load, against any entity carrying a transform.
- The probe is a **resolve-check**: a column that returns `std::nullopt` is a fail; a column that resolves is a pass, including when it reads zero on a stationary entity. *(Simplified after OQ-9 resolved — `readComponentFieldReal` returns `std::nullopt` for an unresolvable runtime column, so the moving-entity heuristic the first draft required is unnecessary. It SHALL NOT be reintroduced: probing against a "should be moving" entity risks a false FAIL on the first frame, before the physics model has integrated a velocity, which would disable the commander on a healthy tree.)*
- The probe SHALL NOT read a deliberately invalid path to test the failure mode. Doing so emits `DynamicLayout` / `DynamicStore` errors into the host log on every run — diagnostics that were worth one experiment are not worth permanent log noise.
- IF the probe fails, THEN the commander SHALL remain disabled, log which path failed, and report the failure through `aiCommander.getStats()` — it SHALL NOT fall back to a zero velocity.
- The probe result SHALL be written to the startup log line alongside the backend, model, and cadence.
- Deferring the probe off `initialize()` is required, not optional: no scenario is loaded at `initialize()`, so the entity manager is empty and a probe there would always report "nothing to probe". A `NotRun` verdict SHALL be retried on later frames, and SHALL be re-armed on `onStop()` so a new scenario re-earns its verdict.

**Trace:** UAC-AIC-ARCH-4

### The order contract

#### AIC-ORD-1: Order document schema
The system SHALL accept from the model exactly one JSON object per request, conforming to the schema below, and SHALL reject any response that does not.

**Customer scenario:** A tactics author reads the order schema to understand precisely what the model is allowed to decide and what remains the script's job.

**Pain removed:** Without an enumerated contract, "what can the AI do" is answerable only by reading the prompt, and any schema drift between prompt, validator, and Lua consumer is silent.

```json
{
  "schemaVersion": 1,
  "entityId": "RedSu35_01",
  "posture": "engage",
  "targetEntityId": "BlueF18_02",
  "waypoint": {
    "latitudeDeg": 13.50,
    "longitudeDeg": 144.80,
    "altitudeHaeM": 10000.0
  },
  "cruiseSpeedMps": 300.0,
  "orbitRadiusM": 0.0,
  "roe": "weaponsFree",
  "reason": "Lead bandit inside 45 km with a full BVR rail; committing."
}
```

**Field contract.** Units and frames below are read from `dev/ai-coding/schema-reference/schema-reference.json` — the `unit` key on each record — not from memory.

| Field | Type | Required | Unit / frame | Valid range | Schema authority |
|---|---|---|---|---|---|
| `schemaVersion` | integer | yes | — | `1` exactly | this PRD |
| `entityId` | string | yes | — | must equal the requesting entity's runtime id | — |
| `posture` | enum string | yes | — | `ingress` \| `engage` \| `crank` \| `defend` \| `hold` \| `rtb` | vocabulary from `oppint_red_interceptor.lua:346–439` |
| `targetEntityId` | string | conditional | — | non-empty WHEN `posture ∈ {engage, crank}`; empty otherwise | — |
| `waypoint.latitudeDeg` | number | conditional | `Deg`, geodetic WGS-84 | [-90, 90] | `/datablocks/positionGeodetic/latitudeDeg` (`unit: Deg`) |
| `waypoint.longitudeDeg` | number | conditional | `Deg`, geodetic WGS-84 | [-180, 180] | `/datablocks/positionGeodetic/longitudeDeg` (`unit: Deg`) |
| `waypoint.altitudeHaeM` | number | conditional | `M`, **height above the WGS-84 ellipsoid** — not AGL, not MSL | [`safety.minAltitudeHaeM`, `safety.maxAltitudeHaeM`] | `/datablocks/positionGeodetic/altitudeHaeM` (`unit: M`) |
| `cruiseSpeedMps` | number | yes | `Mps`, ground speed | (0, `safety.maxSpeedMps`] | `/datablocks/waypoint/speed` (`unit: Mps`); `/datablocks/componentTransform/speedMps` (`unit: Mps`) |
| `orbitRadiusM` | number | conditional | `M` | (0, 50000] WHEN `posture == hold`; `0` otherwise | `/datablocks/componentNavigation/onWaypointReachedLoiterRadiusM` (`unit: M`) |
| `roe` | enum string | yes | — | `weaponsFree` \| `weaponsTight` \| `weaponsHold` | this PRD |
| `reason` | string | yes | — | 1–200 chars, sanitized; **advisory only, never parsed** | this PRD |

`waypoint` is required WHEN `posture ∈ {ingress, hold, rtb}` and SHALL be absent or ignored otherwise — for `engage`, `crank`, and `defend`, Tier 1 computes the geometry itself.

**Fields the model is structurally forbidden to emit.** The schema has no property for heading, pitch, roll, velocity components, acceleration, turn rate, load factor, hardpoint selection, or fire commands. A response carrying any additional property is rejected under `additionalProperties: false` (AIC-VAL-2). This is what "never produces raw kinematics" means concretely.

**Acceptance criteria:**
- A JSON Schema document matching this table is embedded in the plugin and is the single source used for (a) validation, (b) the Claude `output_config.format.schema`, and (c) generating the local GBNF grammar. One definition, three consumers.
- Every unit in the table is traceable to a `unit` key in `schema-reference.json`, verified by a test that re-reads the file.
- A response with an unknown top-level property is rejected.

**Trace:** UAC-AIC-ORD-1

#### AIC-ORD-2: Posture → verb mapping
The system SHALL define, for each posture, exactly which existing verbs Tier 1 invokes, and SHALL NOT introduce a new control verb.

**Customer scenario:** A script author adopting the commander needs to know what each posture obliges their `onTick` to do.

**Pain removed:** Without a fixed mapping, each script invents its own interpretation of `engage`, and orders are not portable between entities.

| Posture | Tier-1 obligation | Verb(s), with arity from the generated stubs |
|---|---|---|
| `ingress` | Fly to the ordered waypoint at the ordered speed | `navigation.requestGoTo(entityId, lat, lon, alt, cruiseSpeedMps) → boolean` |
| `engage` | Pursue the ordered target; fire only when the script's own launch-range discipline and ROE permit | `navigation.requestTrackTarget(entityId, targetEntityId, cruiseSpeedMps) → boolean`; `weapon.canFire(entityId) → boolean`; `weapon.requestFire(entityId, targetEntityId[, hardpointName]) → boolean` |
| `crank` | Fly the script's own offset geometry against the ordered target while a shot is assessed | `navigation.requestGoTo(...)` with a **script-computed** steer point — the model supplies the target, not the geometry |
| `defend` | Turn cold from the nearest munition track and descend, per the script's own pump logic | `navigation.requestGoTo(...)` with a script-computed point |
| `hold` | Orbit the ordered waypoint at the ordered radius and speed | `navigation.requestHoldPosition(entityId, lat, lon, alt, orbitRadiusM, cruiseSpeedMps) → boolean` |
| `rtb` | Egress to the ordered waypoint | `navigation.requestGoTo(...)`; `weapon.requestCeaseFire(entityId) → boolean` |

ROE is orthogonal to posture: `weaponsHold` obliges Tier 1 to call `weapon.requestCeaseFire` and suppress `requestFire` regardless of posture; `weaponsTight` permits fire only against the ordered `targetEntityId`; `weaponsFree` permits the script's own target selection within its ROE.

**The reporting obligation** *(v1.2)*. Because the plugin cannot see tracks or loadout (§Corrections, item 4), a script that wants situation-aware orders SHALL report what it sees before reading an order:

| Obligation | Verb(s), with arity from the generated stubs |
|---|---|
| Report the current track picture each cadence window | `sensor.getTrackNr(entityId) → number`, then for `i = 1..n` `sensor.getTrackById(entityId, i) → targetEntityId, range_m, snr_DB`, each forwarded as `aiCommander.reportTrack(entityId, targetEntityId, rangeM, snrDb) → boolean` |
| Report remaining stores each cadence window | `weapon.getWeaponLoadout(entityId) → table` (array of `{hardpointName, weaponProfileName, ammoCount, ammoMax}`), each row forwarded as `aiCommander.reportLoadout(entityId, hardpointName, weaponProfileName, ammoCount, ammoMax) → boolean` |

Reporting is **advisory, not required for correctness**: a script that reports nothing still receives orders, but the model sees no tracks, so Stage-B check B3 rejects any order naming a target and the entity is effectively limited to the waypoint postures. This is the intended degradation — an unreported track cannot be engaged on model initiative.

**Acceptance criteria:**
- The reference Tier-1 script shipped with the plugin implements every posture row and both reporting obligations.
- No verb outside `navigation.*`, `weapon.*`, `sensor.*`, `entityControl.get*`, `mission.*`, `simulation.*`, and `aiCommander.*` appears in the reference script.
- `scenarioControl.*` is not called by the reference script or the plugin.
- The reference script degrades correctly when `aiCommander` is `nil` — it falls back to `navigation.resumeWaypointFollowing` and calls no `aiCommander.*` verb.

**Trace:** UAC-AIC-ORD-2

### Validation and safety

#### AIC-VAL-1: Two-stage validation, split by thread affinity
The system SHALL validate every order in two stages — syntactic checks on the worker, semantic checks on the simulation thread — because semantic checks require SDK collaborators that are single-thread-only.

**Customer scenario:** An operator needs assurance that a model which hallucinates an entity id, or emits an order 40 s stale, cannot steer an entity.

**Pain removed:** A single-stage validator either blocks the sim thread with parsing work or performs liveness checks off-thread against `IEntityManager`, which is undefined behavior.

**Stage A — worker thread, no SDK access:**

| # | Check | Reject reason |
|---|---|---|
| A1 | `IHttpClient::send()` returned a value (not `std::nullopt`) and its `statusCode` is 2xx | `transport` |
| A2 | Backend-specific envelope parses and, for Claude, `stop_reason != "refusal"` | `refusal` / `envelope` |
| A3 | Body is a single well-formed JSON object | `parse` |
| A4 | Conforms to the AIC-ORD-1 schema, `additionalProperties: false` | `schema` |
| A5 | `schemaVersion == 1`; `posture` and `roe` are enum members | `version` / `enum` |
| A6 | Conditional-presence rules hold (`targetEntityId` for engage/crank; `waypoint` for ingress/hold/rtb; `orbitRadiusM` for hold) | `shape` |
| A7 | Numerics finite and within static range; strings within length caps and charset | `range` |

**Stage B — simulation thread, SDK access permitted:**

| # | Check | Reject reason |
|---|---|---|
| B1 | `entityId` is on the commander roster and `IEntityManager::getEntity(entityId) != nullptr` | `roster` |
| B2 | Order is not stale: `simTimeNow - snapshotSimTimeS <= commander.maxOrderAgeS` | `stale` |
| B3 | `targetEntityId` (when present) resolves via `getEntity` AND appears in the entity's **Tier-1-reported** track list for the current cadence window (AIC-API-1 `reportTrack`). An empty reported list rejects every targeted order | `track` |
| B4 | Target team differs from own team (`IEntity::getTeam()` on both) | `fratricide` |
| B5 | Waypoint (when present) is within `safety.geofenceRadiusM` of the entity's current position | `geofence` |
| B6 | Altitude within [`safety.minAltitudeHaeM`, `safety.maxAltitudeHaeM`]; speed within (0, `safety.maxSpeedMps`] | `clamp` |
| B7 | Order serial is monotonically newer than the currently published order | `superseded` |

**Acceptance criteria:**
- Every rejection increments a named counter and writes one `order.rejected` record carrying the reason and the raw body, truncated to 4 KB.
- Stage B runs entirely within `onTickFrame` and completes within the per-frame budget.
- A test corpus of at least 40 adversarial payloads — wrong types, out-of-range numbers, `NaN`, `Infinity`, extra properties, nested injection strings, missing conditionals, unknown enum members, hallucinated entity ids, friendly targets, stale timestamps, 10 MB bodies — is rejected 40/40 with the expected reason code.

**Trace:** UAC-AIC-VAL-1

#### AIC-VAL-2: Reject-and-retain fallback ladder
The system SHALL, on any rejection, leave the previously published order in force, and SHALL degrade through a defined ladder rather than to undefined behavior.

**Customer scenario:** The inference server dies mid-scenario and the operator expects entities to keep flying sensibly rather than freezing or reverting to spawn behavior.

**Pain removed:** Without a defined ladder, a backend outage produces per-script improvisation — some entities hold last order forever, others revert, none of it documented.

Ladder, in order:
1. **Retain** the last accepted order, until it exceeds `commander.orderValidityS` (default 120 s).
2. **Standing order.** On expiry, publish the configured standing order — default `posture=hold`, `roe=weaponsTight`, waypoint at the entity's position at expiry, `orbitRadiusM = safety.defaultOrbitRadiusM`.
3. **Release to Tier 1.** After `commander.releaseAfterS` (default 300 s) with no accepted order, `aiCommander.getOrder` returns `"{}"` and `aiCommander.isValid` returns false, which the reference script treats as "commander absent" and falls back to `navigation.resumeWaypointFollowing`.

**Acceptance criteria:**
- IF the backend is unreachable for the whole run, THEN entities SHALL complete the scenario under Tier-1 behavior with no error state and no stall.
- Each ladder transition writes one record and one log line.
- Never SHALL a rejected order be partially applied — the published order is replaced atomically or not at all.

**Trace:** UAC-AIC-VAL-2

#### AIC-SEC-2: Transmitted-field allowlist
The system SHALL serialize into the prompt only the fields enumerated in §"Exactly what is transmitted", and SHALL exclude all `componentTrackIdentity` free-text fields.

**Customer scenario:** The platform owner authorizing the hosted backend needs to state, to whoever asks, precisely what proprietary data leaves the machine.

**Pain removed:** "The prompt includes the entity state" is not an answer anyone can approve against; and free-text fields ingested from an external ADS-B feed are an unreviewed injection channel.

**Acceptance criteria:**
- A test renders a prompt from a snapshot whose every string field is set to a unique sentinel, then asserts that only allowlisted sentinels appear in the rendered bytes.
- `trackSource`, `callsign`, and `originCountry` sentinels are absent.
- The API key value is absent from the prompt, every log line, and every order record.
- *(v1.2)* The two Tier-1-reported rows carry only the scalars their ingress verbs accept: a `reportTrack` sentinel appears only as `targetEntityId`, and no `team`, `kind`, or `domain` field is rendered for a track. A test asserts the plugin derives no additional track attribute from any source.
- *(v1.2)* Strings arriving through `reportTrack` / `reportLoadout` / `setSituationNote` are charset-filtered and length-capped on ingress, before they can reach a prompt — Tier 1 is trusted to choose *which* tracks to report, not to have sanitized their ids.

**Trace:** UAC-AIC-SEC-2

### Plugin and Lua surface

#### AIC-API-1: The `aiCommander` Lua namespace
The system SHALL register the following functions under `aiCommander`, with the arities and return shapes below.

**Customer scenario:** A Tier-1 script author reads an order and acts on it using the same idioms as every other N8RO namespace.

**Pain removed:** Without a stable read API, scripts would have to parse JSON in a sandbox that has no JSON library and runs under an instruction budget.

| Function | Arity | Returns |
|---|---|---|
| `aiCommander.isValid(entityId)` | 1 | `boolean` — entity is on the roster and holds a currently valid order |
| `aiCommander.requestCommand(entityId)` | 1 | `boolean` — enroll on the roster; idempotent; false if the roster is at `commander.maxCommandedEntities` |
| `aiCommander.releaseCommand(entityId)` | 1 | `boolean` |
| `aiCommander.getPosture(entityId)` | 1 | `string, string, number` — posture, targetEntityId (empty when N/A), cruiseSpeedMps; `nil, nil, nil` on failure |
| `aiCommander.getWaypoint(entityId)` | 1 | `number, number, number` — latitudeDeg, longitudeDeg, altitudeHaeM; `nil, nil, nil` when the posture carries no waypoint |
| `aiCommander.getOrbitRadiusM(entityId)` | 1 | `number` — metres; `-1` when the posture is not `hold` |
| `aiCommander.getRoe(entityId)` | 1 | `string` — `weaponsFree` \| `weaponsTight` \| `weaponsHold`; empty string on failure |
| `aiCommander.getOrderSerial(entityId)` | 1 | `number` — monotonic per entity; `-1` when none. Lets a script detect a new order without diffing fields |
| `aiCommander.getOrderAgeS(entityId)` | 1 | `number` — simulation seconds since the order was accepted; `-1` when none |
| `aiCommander.getOrder(entityId)` | 1 | `string` — the full order document as JSON, or `"{}"`. For logging and debugging; scripts should prefer the typed getters |
| `aiCommander.setSituationNote(entityId, text)` | 2 | `boolean` — attach ≤ 256 chars of deterministic Tier-1 context to the next prompt (e.g. `"winchester"`, `"2 shots in air"`). Truncated and sanitized; false on invalid args |
| `aiCommander.reportTrack(entityId, targetEntityId, rangeM, snrDb)` | 4 | `boolean` — *(v1.2)* append one detected track to `entityId`'s reported picture for the current cadence window. `rangeM` metres, `snrDb` decibels, matching the sensor stub's `range_m` / `snr_DB`. Idempotent per `targetEntityId` — a repeat replaces the earlier row rather than duplicating it. False when the entity is not on the roster, on invalid args, or when the list already holds `commander.maxTracksInPrompt` entries |
| `aiCommander.reportLoadout(entityId, hardpointName, weaponProfileName, ammoCount, ammoMax)` | 5 | `boolean` — *(v1.2)* append one hardpoint's remaining stores for the current cadence window. Idempotent per `hardpointName`. False when the entity is not on the roster or on invalid args |
| `aiCommander.getStats()` | 0 | `string` — JSON: `requested`, `accepted`, `rejected` (by reason), `timeouts`, `lastLatencyMs`, `p95LatencyMs`, `backend`, `enabled`, `runtimeColumnProbe` *(v1.2, per AIC-ARCH-4)* |

Fourteen functions: eleven readers, one note setter, and two reporters *(v1.2 — `reportTrack` and `reportLoadout` added to close the gap in §Corrections, item 4)*. The namespace still extends no built-in namespace.

**Reported-list lifecycle** *(v1.2)*. Each entity's reported track and loadout lists are cleared by the plugin when a snapshot is taken for that entity, so a window's report reflects exactly one Tier-1 pass and a script that stops reporting stops contributing tracks rather than leaving a stale picture in place. Stage-B check B3 validates against the list that accompanied the snapshot the order was derived from, not the list current at publication time — otherwise a target legitimately reported at request time could be rejected merely because the window rolled over during inference.

**Acceptance criteria:**
- Every function is registered with `LuaApiFunctionMeta` carrying a description and signature, so the generated stub in `data/resources/missions/stubs/aiCommander.lua` documents it after one engine run.
- Every function returns the documented failure shape rather than raising, for: unknown entity id, wrong arity, wrong argument type, commander disabled.
- No function performs I/O, allocation-heavy work, or blocking. Each reader is O(1) against a pre-published order; each reporter is O(1) amortized against a list bounded by `commander.maxTracksInPrompt`.
- `ScriptingApiContext` is captured **by value** in every callback, per `plugin-authoring.md` and the sample at `EntityStateApiModule.cpp:145`.
- *(v1.2)* The reporters mutate only plugin-owned state. They call no `writeComponentField*` and no `entityControl.request*`, and they are the only functions in the namespace that are not pure reads.
- *(v1.2)* WHILE `commander.enabled` is false, the reporters return `false` and retain nothing — reporting into a disabled commander accumulates no state.

**Trace:** UAC-AIC-API-1

#### AIC-API-2: Plugin configuration surface
The system SHALL expose the following `PluginConfigField` set through `getConfigFields()` / `applyConfigFields()`.

**Customer scenario:** The platform owner switches backends, tunes cadence, and enables the hosted API from the plugin config UI without a rebuild.

**Pain removed:** Hard-coded endpoints and model names make every backend change a rebuild-and-redeploy, and make the authorization decision invisible.

| Name | Type | Default | Notes |
|---|---|---|---|
| `commander.enabled` | Bool | `false` | Master switch. Fail-closed |
| `commander.backend` | Text | `stub` | `stub` \| `replay` \| `local` \| `claude` |
| `commander.cadenceS` | Real | `20.0` | Minimum simulation seconds between orders per entity |
| `commander.maxCommandedEntities` | Int | `4` | Roster cap |
| `commander.maxConcurrentRequests` | Int | `1` | Worker fan-out; one `IHttpClient` per worker |
| `commander.requestTimeoutS` | Int | `30` | Written to `HttpRequest::timeoutS` (SDK default is 15) |
| `commander.maxOrderAgeS` | Real | `45.0` | Stage-B staleness bound |
| `commander.orderValidityS` | Real | `120.0` | Fallback ladder step 1 |
| `commander.releaseAfterS` | Real | `300.0` | Fallback ladder step 3 |
| `commander.maxTracksInPrompt` | Int | `8` | Bounds the volatile suffix |
| `prompt.doctrinePath` | Text | `data/doctrine.txt` | Repository-relative path to the doctrine block of the stable prefix (AIC-BE-3). A file, not a `Text` field: it is 1–2 pages, edited by whoever tunes tactics rather than whoever rebuilds the DLL, and its token count is what OQ-8 turns on. Read once at `initialize()`; a change mid-run does not take effect, preserving prefix byte-stability |
| `local.baseUrl` | Text | `http://localhost:11434` | Matches `bin/ai/.env` `OLLAMA_BASE_URL` |
| `local.model` | Text | `llama-3.2-3b-instruct-q4_k_m` | The 3B is the latency-viable CPU default |
| `local.temperature` | Real | `0.0` | Greedy decoding; lowest run-to-run variance |
| `local.grammarEnabled` | Bool | `true` | GBNF constrained decode where the server supports it (OQ-1) |
| `claude.enabled` | Bool | `false` | **Independent authorization gate.** Data egress |
| `claude.baseUrl` | Text | `https://api.anthropic.com` | Must be `https://` |
| `claude.model` | Text | `claude-haiku-4-5` | |
| `claude.maxTokens` | Int | `512` | Output is a constrained order, not prose |
| `claude.apiKeyEnvVar` | Text | `ANTHROPIC_API_KEY` | **Name only.** Never the value |
| `claude.effort` | Text | *(empty)* | Empty for Haiku 4.5, which does not accept `effort`; `low` for Sonnet 5 |
| `safety.maxSpeedMps` | Real | `400.0` | Clamp bound |
| `safety.minAltitudeHaeM` | Real | `100.0` | Clamp bound, HAE |
| `safety.maxAltitudeHaeM` | Real | `20000.0` | Clamp bound, HAE |
| `safety.geofenceRadiusM` | Real | `200000.0` | Waypoint bound relative to current position |
| `safety.defaultOrbitRadiusM` | Real | `8000.0` | Standing-order orbit |
| `record.enabled` | Bool | `true` | Order log; on by default — determinism depends on it |
| `record.path` | Text | `logs/ai-commander/` | Rotated, size-capped |
| `replay.path` | Text | *(empty)* | Required when `commander.backend = replay` |

**Acceptance criteria:**
- `applyConfigFields` validates every field and returns `false` on any invalid value, leaving all prior values unchanged — partial application is not permitted.
- `claude.baseUrl` not matching `https://` is rejected.
- `commander.backend = "claude"` with `claude.enabled = false` is rejected with a logged reason.
- `getConfigFields()` never returns an API key value.

**Trace:** UAC-AIC-API-2

### Backends

#### AIC-BE-1: Local adapter (Phase 1)
The system SHALL reach a local inference server over `IHttpClient` at `local.baseUrl`, requesting a schema-constrained JSON order.

**Customer scenario:** An operator runs the whole commander on an air-gapped host using a model already in `data/ai/model/`.

**Pain removed:** Phase 1 must work with no network egress at all, since scenario state is proprietary.

**Acceptance criteria:**
- The request carries `commander.requestTimeoutS` as `HttpRequest::timeoutS`.
- A `std::nullopt` return from `send()` is handled as a transport failure, distinct from a returned response carrying a non-2xx status. *(v1.2 — `send()` returns `std::optional<HttpResponse>`; the `statusCode == 0` sentinel in the struct is not what a caller observes on failure.)*
- One `IHttpClient` instance per worker thread — never shared, per the header's single-thread-only contract.
- The concrete endpoint path and payload shape are pinned once OQ-1 resolves; the adapter is written so that resolution is a single-file change.

**Trace:** UAC-AIC-BE-1

#### AIC-BE-2: Claude adapter (Phase 2)
The system SHALL, WHEN `claude.enabled` is true, issue `POST {claude.baseUrl}/v1/messages` over raw HTTPS with headers `x-api-key`, `anthropic-version: 2023-06-01`, and `content-type: application/json`, requesting a schema-guaranteed order via structured outputs.

**Customer scenario:** The owner wants order quality and sub-2-second latency for a demo, and holds $100 in API credit.

**Pain removed:** There is no official Anthropic C++ SDK; without a specified raw-HTTP contract, every implementer invents their own request shape and discovers the constraints by trial.

**Acceptance criteria:**
- The order schema from AIC-ORD-1 is sent as `output_config: {"format": {"type": "json_schema", "schema": {…}}}`.
- Assistant prefill is **not** used — it returns 400 on all current models.
- `stop_reason == "refusal"` is checked **before** reading `content`, and resolves to reject-reason `refusal`.
- `max_tokens` comes from `claude.maxTokens` (default 512).
- WHEN `claude.model` is a Sonnet 5 model, the request sets `thinking: {"type": "disabled"}` or `output_config.effort = "low"` on the latency path; WHEN it is Haiku 4.5, no `effort` parameter is sent.
- 429 and 5xx are retried at most once with backoff inside the same worker call; a second failure resolves to reject-reason `transport`.
- Input and output token counts from the response are written into the order record for cost accounting.

**Trace:** UAC-AIC-BE-2

#### AIC-BE-3: Prompt structure — stable prefix, volatile suffix
The system SHALL render every prompt as a byte-stable prefix followed by a volatile per-request suffix, and SHALL NOT vary the prefix between requests within a run.

**Customer scenario:** An operator on CPU inference needs every second of latency the design can recover.

**Pain removed:** Token generation is memory-bandwidth-bound; re-evaluating an unchanged ~1,200-token prefix on every request wastes the majority of the wall-clock budget, and on the hosted backend it forfeits the cache discount entirely.

**Structure:**

| Segment | Content | Volatility |
|---|---|---|
| Prefix (cacheable) | System prompt · posture and ROE vocabulary · the Order JSON schema · doctrine text | Byte-identical for the life of the run. Changes only when a config field that affects it changes, which invalidates the cache once, deliberately. **Measured: 4,738 bytes ≈ 1,200 tokens** *(v1.3, live engine run — §Corrections, item 9)* |
| Suffix (volatile) | The per-entity snapshot from §"Exactly what is transmitted" | New every request; ~150–250 tokens |

**Acceptance criteria:**
- A test renders the prefix 100 times across varying snapshots and asserts byte equality.
- The prefix contains no timestamp, no entity id, no counter, and no floating-point formatting of live state.
- The prefix's byte and token counts are recorded at startup (as `prefixBytes` and the derived token estimate) and compared against the configured model's cache minimum — 4096 tokens for Haiku 4.5, 1024 for Sonnet 5, 512 for Opus 5 — with a warning logged when it falls short, because a prefix under the minimum silently does not cache. *(v1.3)* This comparison now has a **real observed value on the local tree**: 4,738 bytes ≈ 1,200 tokens, i.e. below Haiku 4.5's 4096-token minimum and above both Sonnet 5's and Opus 5's. The criterion is therefore no longer a hypothetical guard — on the default `claude.model = claude-haiku-4-5` it is expected to *fire* on every run until OQ-8 is decided, and a run in which it does **not** fire on Haiku means the prefix changed and the measurement needs retaking.
- *(v1.2)* The suffix's Tier-1-reported lists render in a **deterministic order** — tracks ascending by `targetEntityId`, loadout ascending by `hardpointName` — not in Lua call order. Two snapshots holding the same set of reports SHALL render byte-identically, so `snapshotHash` (AIC-DET-1) is a function of the picture rather than of the order the script happened to iterate in.

**Trace:** UAC-AIC-BE-3

#### AIC-BE-4: TLS availability assertion
The system SHALL verify at first hosted request that the HTTPS transport is functional, and SHALL disable the hosted backend with a logged reason if it is not.

**Customer scenario:** An operator enables the Claude backend on a machine whose runtime is missing the OpenSSL DLLs and needs a clear diagnosis rather than a stream of unexplained transport failures.

**Pain removed:** `IHttpClient.h` documents that `https` needs the OpenSSL build, and a missing TLS backend is indistinguishable from a network outage at the `HttpResponse` level. (`bin/libssl-3-x64.dll` and `bin/libcrypto-3-x64.dll` are present in this tree, so the expected result is pass.)

**Acceptance criteria:**
- IF the first `https` request returns `std::nullopt` AND a plain-`http` control request to the same host returns a value, THEN the plugin SHALL log a TLS-availability diagnosis distinct from a generic transport failure — the `http` control succeeding while `https` does not is what isolates the missing TLS backend from a network outage. *(v1.2 — restated against the `std::optional` return; the previous phrasing compared two `statusCode == 0` results, which cannot distinguish the two causes.)*
- The commander falls back per AIC-VAL-2 rather than retrying in a tight loop.

**Trace:** UAC-AIC-BE-4

### Determinism and replay

#### AIC-DET-1: Order record format
The system SHALL append one JSON object per line (JSONL) to the order log for every lifecycle event, sufficient to replay the run without a model.

**Customer scenario:** An engineer investigating why an entity broke off at t=412 s reads the exact order, the snapshot that produced it, and the model's stated reason.

**Pain removed:** `docs/modules/n8ro-sim/dev/execution-models-and-timing.md` treats reproducibility as a design goal. An LLM in the loop breaks it outright. Without a record, a run is not merely nondeterministic — it is uninvestigable.

```jsonl
{"t":412.50,"frame":24750,"event":"order.requested","entityId":"RedSu35_01","serial":13,"backend":"local","model":"llama-3.2-3b-instruct-q4_k_m","snapshotHash":"sha256:9f2c…","promptHash":"sha256:41ab…"}
{"t":416.34,"frame":24980,"event":"order.accepted","entityId":"RedSu35_01","serial":13,"latencyMs":3840,"tokensIn":1004,"tokensOut":78,"order":{"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Lead bandit inside 45 km with a full BVR rail; committing."}}
{"t":436.34,"frame":26180,"event":"order.rejected","entityId":"RedSu35_01","serial":14,"reason":"track","detail":"targetEntityId 'BlueF18_09' not in current track list","rawBody":"{…truncated to 4096 bytes…}"}
```

Events: `commander.enabled`, `commander.disabled`, `order.requested`, `order.accepted`, `order.rejected`, `order.timeout`, `fallback.standing`, `fallback.released`.

**Acceptance criteria:**
- Records are written on the simulation thread from a bounded queue; the worker never touches the file.
- Every accepted order's `t` is the simulation time at which it was **published**, not requested — replay depends on publication time.
- `snapshotHash` and `promptHash` are stable hashes over canonical serializations, enabling prompt-drift detection across runs.
- Records contain no API key and no `componentTrackIdentity` field.
- Rotation caps total order-log size (default 64 MB across 4 files).

**Trace:** UAC-AIC-DET-1

#### AIC-DET-2: Replay mode
WHEN `commander.backend = "replay"`, the system SHALL source orders from `replay.path` instead of any model, publishing each recorded `order.accepted` at its recorded simulation time, and SHALL still run Stage-B validation against live state.

**Customer scenario:** A tester reproduces a reported behavior exactly, on a machine with no inference server, and a CI job runs the full pipeline deterministically.

**Pain removed:** Debugging a nondeterministic decision layer by re-running it is hopeless; and CI cannot depend on an inference server that does not ship.

**Reproducibility guarantees by mode:**

| Mode | Order sequence and content | Entity trajectory |
|---|---|---|
| `stub` | Deterministic by construction — fixed order table, no I/O | Reproducible under FixedTimestep with explicit pacing |
| `replay` | Identical to the recorded run by construction | Reproducible under FixedTimestep with explicit pacing **and** no other nondeterministic plugin loaded. A Stage-B rejection that did not occur in the original run (live state diverged) is itself logged and is the signal that the guarantee has broken |
| `local` | **None.** Varies with sampler state, server-side batching, and request timing | None |
| `claude` | **None.** Adds network variance and server-side model updates | None |

The honest statement: replay reproduces the *decision sequence*, not the *physics*. Trajectory reproducibility is inherited from the engine's own timing configuration, not provided by this plugin. Where the engine is not in FixedTimestep, replay still guarantees identical orders at identical simulation times, which is the part the commander owns.

**Acceptance criteria:**
- Replay performs no network I/O; the `IHttpClient` is never constructed.
- Orders are published at recorded simulation time, not wall time, and not frame number.
- Stage B still runs; a divergent rejection writes a `replay.divergence` record naming the check that failed.
- Replaying a log twice produces byte-identical published-order sequences.

**Trace:** UAC-AIC-DET-2

## Scope authority

The FR sections above are the **contract** for this PRD. The design document (path to be added when a design is created) realizes these FRs as components, sequences, and milestone tasks.

**The design must not introduce surface area beyond this PRD's FR set without a corresponding PRD revision.** If the design proposes a new Lua function, a new posture, a new order field, a new config field, or a new backend not authorized by an FR, this PRD must be updated first, through its revision flow. In particular: adding a posture means adding a row to AIC-ORD-2's mapping table, because a posture with no verb mapping is an order nobody can execute.

Conversely, **this PRD must not specify implementation detail beyond FR shape.** Class decomposition, the concrete mutex/slot data structures, the JSON library choice, header layout, and the internal snapshot struct belong in the design, not here. Where this document names a threading *pattern* (AIC-ARCH-2) it is stating a requirement about observable behavior — no blocking on the update thread, no SDK pointer on a worker — not prescribing an implementation.

## Performance requirements

### Latency targets

| Operation | p50 | p95 | p99 | Gating? |
|---|---|---|---|---|
| Plugin work per `onTickFrame` (snapshot build, Stage B, publish) | < 0.2 ms | < 0.5 ms | < 2.0 ms | **Yes** |
| Lua getter (`getPosture`, `getWaypoint`, …) | < 5 µs | < 20 µs | < 100 µs | Yes |
| Order round trip, local 3B Q4, CPU | 4 s | 8 s | 12 s | No — tracked |
| Order round trip, local 7B Q4, CPU | 12 s | 20 s | 30 s | No — tracked |
| Order round trip, either model on GPU | < 1 s | 2 s | 3 s | No — tracked |
| Order round trip, Claude Haiku 4.5 | 1.2 s | 2.5 s | 5 s | No — tracked |

The gating targets are the ones the plugin controls. Inference latency is a property of the host machine and the chosen model; the design's obligation is that it does not matter to frame time.

### Throughput

- Sustained: `commander.maxCommandedEntities` (default 4) entities at `commander.cadenceS` (default 20 s) = 0.2 orders/s aggregate.
- `commander.maxConcurrentRequests` defaults to 1 because CPU inference is memory-bandwidth-bound — concurrent requests to one CPU server do not increase throughput, they increase every request's latency. Raise it only for a GPU server or the hosted backend.
- Peak: one order per entity per cadence window; requests are never queued deeper than one per entity — a pending request suppresses new ones for that entity.

### Resource constraints

- Plugin resident memory: < 32 MB excluding the HTTP client's buffers.
- Model residency is the inference server's, not the plugin's: 1.9 GB for the 3B, 4.5 GB for the 7B (split across two files).
- Order log: capped at 64 MB by rotation.
- On CPU, the inference server competes with the simulation for memory bandwidth. Consequence worth stating: on a single-machine setup, running the 7B on CPU will measurably slow the simulation itself, independent of anything this plugin does. GPU inference, or a second machine, removes it.

### Optimization approach

- **Prompt-prefix stability** (AIC-BE-3) — the single highest-leverage optimization on both backends.
- **Latest-wins slots** rather than queues — a stale snapshot is worthless, so dropping it is correct, not lossy.
- **Per-entity request suppression** — never two in flight for one entity.
- **Constrained decoding** — a grammar-constrained 80-token order both parses reliably and stops promptly, versus a model that keeps writing prose past the JSON.
- **`max_tokens` at 512** — the output is a bounded order, and a small cap bounds the worst case.

## Cross-service impact

### Affected components

| Component | Impact | Changes required |
|---|---|---|
| `n8ro-sim` engine | None — no platform binary changes | None. The plugin loads from `%N8RO_RELEASE_USER_SIM_PLUGINS%` |
| Mission Lua scripts | New optional namespace available | A script opts in via `aiCommander.requestCommand`; scripts that don't call it are unaffected |
| Generated stubs (`data/resources/missions/stubs/`) | A new `aiCommander.lua` appears | Regenerated by running the engine once after deploy; the Lua language server is re-pointed at the folder |
| Inference server (external) | New runtime dependency | Must be installed and running on target machines. **Not shipped** |
| `n8ro-llm` | None | Deliberately not depended upon |
| MCP stack (`n8ro-mcp.exe`, `n8ro-sim-bot.exe`) | None in v1 | Pending OQ-4 |

### Interface changes

- **New:** the `aiCommander` Lua namespace, fourteen functions (AIC-API-1) — additive, no existing namespace modified.
- **New:** the Order JSON contract between the plugin and the model (AIC-ORD-1) — an external contract with a third party, versioned by `schemaVersion`.
- **New:** the JSONL order-record format (AIC-DET-1) — consumed by replay mode and by anything reading the logs.
- **New** *(v1.2)*: a Tier-1 → plugin data-flow direction. Before v1.2 the Lua surface was read-only from the script's side; `reportTrack` / `reportLoadout` make Tier 1 a *supplier* to the commander as well as a consumer of it. Additive and opt-in — a script that does not call them is unaffected, and no existing signature changes.
- **Unchanged:** every `navigation.*`, `weapon.*`, `sensor.*`, `entityControl.*`, `mission.*` signature.

### Deployment coordination

The inference server must be up before the scenario runs; the plugin degrades gracefully if it is not (AIC-VAL-2), so start order is a quality-of-run concern rather than a correctness one. Plugin deploy is a file copy into `%N8RO_RELEASE_USER_SIM_PLUGINS%` performed by the post-build event; the engine picks it up on next run. No platform component is version-coupled to the plugin.

## Configuration and deployment

### Build and deploy flow

1. **Scaffold — clone, never regenerate.**
   ```bat
   dev\samples\sim\new-sim-plugin.cmd ai-commander sim-scripting
   ```
   The script clones `sim-scripting`, assigns a fresh `ProjectGuid`, and rewrites the DLL name, plugin id, and file references so the new DLL builds and deploys alongside the original. `.vcxproj` / `.slnx` / `open-solution.cmd` are never hand-written from scratch — they carry the `setup.cmd` chain, the post-build deploy copy, and path validation.
2. **Environment.** `call setup.cmd` from the release root sets `N8RO_RELEASE`, `N8RO_RELEASE_USER_SIM_PLUGINS` (default `userPlugins/sim/`), `N8RO_RELEASE_TERRAIN_DB`, `N8RO_RELEASE_AI_DB` (default `data/ai`), prepends `bin/` to `PATH`, and configures Qt. `dev/setup-dev.cmd` resolves the VS 2026 install and MSBuild via `vswhere`.
3. **Build.** `open-solution.cmd` chains `setup.cmd` → `dev/setup-dev.cmd` → opens the `.slnx`. Build `Release | x64`, C++17 (`stdcpp17`), Visual Studio 2026 (v18.x).
   - Includes: `include/n8ro-core`, `include/n8ro-sim`, `include/n8ro-schema`, `include/n8ro-data`.
   - Link: `n8ro-core.lib`, `n8ro-sim.lib`, `n8ro-schema.lib`, `n8ro-data.lib`.
4. **Deploy.** Output lands at `<project>/bin/release/ai-commander.dll`; the post-build event copies it to `%N8RO_RELEASE_USER_SIM_PLUGINS%`.
5. **Verify exports.** `dumpbin /exports ai-commander.dll` must show `create_plugin`, `destroy_plugin`, `get_plugin_signature` (returning `"N8RO_PLUGIN_V1"`).
6. **Regenerate stubs.** Run the engine once; confirm `data/resources/missions/stubs/aiCommander.lua` appears, then point the Lua language server at that folder.

### Source control and repository

The plugin is version-controlled in a **standalone repository**, not inside the release tree. `C:\N8RO` is an installed dist — Qt Installer Framework artifacts, terrain and AI databases, models, and runtime logs — and must not become a working tree. It does not need to: every SDK path in the `.vcxproj` is `$(N8RO_RELEASE)`-rooted (`.vcxproj:68` includes, `:73` library dirs), so the project is location-independent once `setup.cmd` has run.

Scaffolding still happens in place — `new-sim-plugin.cmd` resolves its destination as a sibling of the source sample (`new-sim-plugin.ps1:17`) and its GUID/id rewriting depends on that — after which the directory is moved out to the repository root.

**Repository layout.**

```
n8ro-ai-commander/
  .gitignore  .gitattributes  README.md  NOTICE
  ai-commander.vcxproj        # root-level, exactly as the scaffold emits it
  ai-commander.slnx
  open-solution.cmd           # one edit; see below
  include/  src/
  lua/                        # Tier-1 reference scripts
  data/doctrine.txt           # the AIC-BE-3 stable prefix
  docs/prd.md  docs/adr/
  tests/
```

`README.md` states the prerequisites a clone cannot supply: a licensed N8RO release, `setup.cmd` run, Visual Studio 2026 (v18.x). `NOTICE` carries the provenance statement that replaces the per-file header (see §Naming and path conventions).

**Runtime and build artifacts remain in the release tree** and are never committed:

| Artifact | Path | Written by |
|---|---|---|
| Build output | `<repo>/bin/release/ai-commander.dll` | `<OutDir>` (`.vcxproj:32`) |
| Intermediates | `<repo>/x64/Release/` | `<IntDir>` (`.vcxproj:33`) |
| Deployed DLL | `%N8RO_RELEASE_USER_SIM_PLUGINS%\ai-commander.dll` (default `C:\N8RO\userPlugins\sim\`) | post-build `copy /Y` (`.vcxproj:84`; `mkdir`s the directory, which does not exist in a fresh tree) |
| Order log (JSONL) | `logs/ai-commander/` — `record.path` | AIC-DET-1 |
| Generated Lua stub | `data/resources/missions/stubs/aiCommander.lua` | engine, on next run |

**The one relocation edit.** `open-solution.cmd:19-20` locates the tree by walking four levels up (`%SCRIPT_DIR%\..\..\..\..\setup.cmd`), which is only correct while the project sits at `dev/samples/sim/<name>/`. Once relocated it resolves by environment variable with a documented fallback:

```bat
if not defined N8RO_RELEASE_ROOT set "N8RO_RELEASE_ROOT=C:\N8RO"
set "N8RO_RELEASE_SETUP=%N8RO_RELEASE_ROOT%\setup.cmd"
set "N8RO_RELEASE_DEV_SETUP=%N8RO_RELEASE_ROOT%\dev\setup-dev.cmd"
```

Everything downstream — the existence checks, the `N8RO_SETUP_DONE` guard, the MSBuild resolution via `vswhere` — is unchanged. This is the **only** permitted edit to the scaffolded build wiring; the standing rule against hand-rewriting `.vcxproj` / `.slnx` / `open-solution.cmd` otherwise holds in full.

**Ignore rules.** Beyond standard MSVC noise (`bin/`, `x64/`, `.vs/`, `*.user`, `*.suo`, `*.pdb`, `*.obj`, `*.ilk`, `*.exp`), two entries are security-relevant rather than hygienic:

```gitignore
# Order logs carry live scenario state — positions, ORBAT, team assignments —
# which inherits the tree's proprietary classification (see Security posture).
logs/
*.jsonl

# Secrets and models are never committed.
.env
*.key
*.gguf
```

The `*.jsonl` rule is the one most likely to be missed: AIC-DET-1 recording is **on by default**, and `record.path` is operator-configurable, so a developer will eventually point it inside the repository.

**No SDK header, import library, or platform binary is ever committed.** The repository is therefore unbuildable without a licensed install — which is the intended property, and what keeps Arkheon's shipped code out of it.

**Repository visibility.** Private by default. The plugin's sources necessarily quote proprietary SDK surface — component type strings, `schema-reference.json` leaf paths, Lua verb signatures and arities, scenario names — and `docs/prd.md` is dense with it. Publication requires the same explicit owner authorization as the hosted backend (§Operational readiness deployment checklist). The per-file header decision above is about provenance marking and does **not** imply publication clearance.

**Continuous integration is partial, by necessity.** Hosted runners have no SDK, no `n8ro-*.lib`, and no VS 2026, so they can run only: order-schema validity plus accept/reject fixture round-trips (AIC-ORD-1, AIC-VAL-1), `clang-format`, and docs linting. The `Release | x64` build, the `dumpbin /exports` check (AIC-API-1), and the Appendix A schema-conformance test — which re-reads `schema-reference.json`, a file that cannot be committed — require a **self-hosted runner with the release tree installed**. Any build badge must state which runner produced it.

*(v1.3)* The concurrency-evidence set that replaces the unavailable TSAN gate (§Validation and test plan) lands on the **self-hosted** side for the same reason: the AddressSanitizer build is a `/fsanitize=address` build of the same solution and needs the SDK, and the exchange-slot stress test links the plugin. Only the `static_assert` capture check is toolchain-portable, and it is a compile-time property of code the hosted runner cannot compile anyway. There is no configuration in which a hosted runner substantiates the threading claim.

### Inference-server prerequisites

| Backend | Prerequisite | Ships in tree? |
|---|---|---|
| `stub` | None | — |
| `replay` | A previously recorded order log | — |
| `local` | An HTTP inference server on `local.baseUrl` serving a model from `data/ai/model/`. Ollama or llama.cpp server (OQ-1). llama.cpp loads the shipped `.gguf` files directly and offers GBNF grammars for hard output guarantees; Ollama requires `ollama create` with a Modelfile to import them, and the 7B is split across two files | **No — unmet dependency** |
| `claude` | Network egress to `api.anthropic.com`, a valid API key in the environment variable named by `claude.apiKeyEnvVar`, OpenSSL runtime (present: `bin/libssl-3-x64.dll`, `bin/libcrypto-3-x64.dll`), and explicit owner authorization | Key and egress: no. TLS: yes |

## Observability

### Metrics

| Metric | What it measures | Alert threshold |
|---|---|---|
| `aicmd.frame.p95Ms` | Plugin cost per `onTickFrame` | > 0.5 ms sustained over 300 frames |
| `aicmd.orders.requested` / `.accepted` / `.rejected` | Order pipeline throughput | Acceptance rate < 90 % over 50 orders |
| `aicmd.reject.<reason>` | Per-reason rejection counters (13 reasons from AIC-VAL-1) | Any single reason > 25 % of rejections |
| `aicmd.latency.p50Ms` / `.p95Ms` | Round-trip inference latency | p95 > 2× the configured cadence — the commander cannot keep up |
| `aicmd.fallback.level` | 0 = live, 1 = retained, 2 = standing, 3 = released | Level ≥ 2 for > 60 s |
| `aicmd.tokens.in` / `.out` | Cost accounting (Phase 2) | Cumulative spend > 80 % of budget |
| `aicmd.tracks.reported` *(v1.2)* | Mean reported tracks per snapshot, per commanded entity | `0` for > 2 cadence windows on an entity whose script should be reporting — the Tier-1 reporting call has been dropped, and every targeted order will reject `track` |
| `aicmd.probe.runtimeColumns` *(v1.2)* | AIC-ARCH-4 result: `pass` / `fail` / `notRun` | Any value other than `pass` — the commander is disabled |

All are exposed through `aiCommander.getStats()` as JSON and written to the order log at run end.

### Logging

| Event | When | Fields |
|---|---|---|
| `commander.startup` | `initialize()` | backend, model, cadence, roster cap, `prefixBytes` and the derived prefix token count *(v1.3 — the field the 4,738-byte measurement was read from)*, cache-minimum comparison, TLS availability, runtime-column probe result *(v1.2)*, `services` / `threadRunner` nullability |
| `commander.egressWarning` | First hosted request in a run | destination host, model, field-count in the volatile suffix. **Always logged** — an egress must never be silent |
| `order.rejected` | Every rejection | entityId, serial, reason, detail, truncated raw body |
| `fallback.transition` | Ladder step change | entityId, from level, to level, seconds since last accepted order |
| `replay.divergence` | Stage-B failure during replay | entityId, serial, failing check |

### Health

`aiCommander.getStats()` is the health surface — there is no HTTP endpoint, because the plugin is in-process. A run is healthy when `enabled` is true, `fallback.level` is 0, and acceptance rate is above 90 %.

## Operational readiness

### Runbook

| Scenario | Detection | Response | Escalation |
|---|---|---|---|
| Inference server down | `aicmd.reject.transport` climbing; `fallback.level` ≥ 1 | Verify the server is listening on `local.baseUrl`; restart it. Entities continue under Tier 1 — no scenario abort needed | Owner, if a demo is running |
| Orders rejected for `schema` | `aicmd.reject.schema` > 1 % | Confirm `local.grammarEnabled`; check the model name matches a chat-tuned instruct model; inspect `rawBody` in the order log | Implementer |
| Orders rejected for `track` | `aicmd.reject.track` dominant | **First check `aicmd.tracks.reported`** *(v1.2)* — if it is 0, the Tier-1 script is not calling `aiCommander.reportTrack` and the model is being asked to pick targets it was never shown; this is a script bug, not a model failure. If it is non-zero, the model is hallucinating ids; verify `commander.maxTracksInPrompt` is not truncating the intended target | Implementer |
| Commander refuses to enable at startup | `aicmd.probe.runtimeColumns` = `fail` *(v1.2)* | A `componentTransform` runtime column no longer resolves — the release tree changed under the plugin. Read the startup log for the failing path and reconcile against `include/n8ro-sim/entity/TransformRuntimeColumns.h`. Do **not** work around it by defaulting velocity to zero | Implementer, P1 |
| Frame budget exceeded | `aicmd.frame.p95Ms` alert | Reduce `commander.maxCommandedEntities` and `maxTracksInPrompt`; confirm no worker is touching SDK state | Implementer, P1 |
| Simulation slows with local backend on CPU | Frame rate drop with no plugin metric change | Expected — the inference server is competing for memory bandwidth. Move inference to GPU or a second host | Owner (capacity decision) |
| Unexpected hosted egress | `commander.egressWarning` in a run that should be local | Set `commander.enabled = false` immediately; audit `claude.enabled` and `commander.backend`; review the order log for what was transmitted | **Owner, immediately** |
| API budget nearing exhaustion | `aicmd.tokens.*` cumulative | Switch `claude.model` to Haiku 4.5 or `commander.backend` to `local` | Owner |

### Deployment checklist

- [ ] `dumpbin /exports` shows all three required exports
- [ ] `commander.enabled = false` and `claude.enabled = false` in the deployed default config
- [ ] `aiCommander.lua` stub regenerated and the Lua language server re-pointed
- [ ] Order-log directory exists and is writable; rotation cap set
- [ ] Inference server reachable from the sim host (local backend only)
- [ ] `safety.*` clamps reviewed against the target scenario's operating envelope
- [ ] Replay determinism test green on the target machine
- [ ] For Phase 2 only: owner authorization recorded, API key present in the named environment variable, and the transmitted-field list reviewed

### Capacity planning

Per commanded entity, per cadence window: one HTTP request, ~1 KB up, ~1 KB down, one snapshot copy (< 4 KB), one order-log line (~1 KB). At the default 4 entities / 20 s that is ~0.2 req/s and ~700 KB of order log per scenario-hour. Negligible against the inference server's own footprint, which dominates every dimension.

### Dependencies and SLAs

| Dependency | Status in tree | Degraded behavior | Notes |
|---|---|---|---|
| Inference server (Ollama or llama.cpp) | **Not installed** | Commander degrades through AIC-VAL-2 to Tier-1 behavior | Blocks Phase 1b live runs, not Phase 1a. OQ-1, OQ-2 |
| GGUF models | Present (`data/ai/model/`, 6.5 GB) | — | 3B: 1.9 GB. 7B: 4.5 GB split across 2 files |
| OpenSSL runtime | Present (`bin/libssl-3-x64.dll`, `bin/libcrypto-3-x64.dll`) | Hosted backend disabled with a TLS diagnosis (AIC-BE-4) | Blocks Phase 2 if absent on a target machine |
| Anthropic API credit | $100 held, external | Requests fail; fallback ladder applies | Phase 2 only |
| VS 2026 + MSBuild | Resolved by `dev/setup-dev.cmd` via `vswhere` | Build blocked | — |
| `IThreadRunner` non-null at `initialize()` | **Unverified** — documented nullable | Fall back to an owned thread; if that fails, commander stays disabled | OQ-7 |
| `n8ro-llm` | **Absent** | None — no dependency by design | OQ-3 |
| Entitlement gate (`core/entitlement/AccessGate.h`, `LexActivator.dll` present) | Unknown applicability | Unknown | OQ-5 |
| `componentTransform` runtime columns (`velocityNed.*`) *(v1.2)* | Present, declared in `entity/TransformRuntimeColumns.h` | Commander refuses to enable — it does **not** substitute a zero velocity | The one dependency whose absence is silent rather than loud, which is why AIC-ARCH-4 probes it. OQ-9 |
| Tier-1 track / loadout reporting *(v1.2)* | N/A — a contract with the mission script, not the tree | Targeted orders reject `track`; waypoint postures still execute | Advisory by design. Watch `aicmd.tracks.reported` |

## Rollback strategy

### Trigger conditions

- Plugin frame cost exceeds the p99 budget in a live run.
- Any evidence of a worker touching SDK state (crash, corrupted entity, AddressSanitizer report, or a torn read reported by the exchange-slot stress test). *(v1.3 — TSAN is not available on this platform, §Corrections item 8; the trigger is correspondingly the evidence that can actually be produced.)*
- Any unauthorized egress — a `commander.egressWarning` in a run that was meant to be local.
- Order acceptance rate below 50 % with no configuration explanation.

### Rollback steps

1. **Immediate, no restart (seconds):** set `commander.enabled = false` via `applyConfigFields`. The commander stops issuing orders; the fallback ladder releases entities to Tier 1 within `commander.releaseAfterS`. This is the primary control and it is why the master switch exists separately from the backend selector.
2. **Immediate, hosted-egress specific (seconds):** set `claude.enabled = false`. Independent of step 1 so an egress can be stopped without stopping the run.
3. **Per-run (next scenario load):** set `commander.backend = "replay"` against a known-good log, or `"stub"`. Full pipeline, no model, deterministic.
4. **Full removal (one file, next run):** delete `ai-commander.dll` from `%N8RO_RELEASE_USER_SIM_PLUGINS%`. The engine loads without it. Tier-1 scripts see `aiCommander` as nil.
5. **Verification:** confirm scripts fall back — the reference script's `if aiCommander == nil or not aiCommander.isValid(entityId)` branch must run `navigation.resumeWaypointFollowing`.

### Data rollback

None required. The plugin writes only its own append-only order log; it mutates no scenario data, no entity state, and no platform file. This is a direct consequence of AIC-ARCH-1 and is the main reason the rollback story is this short.

### Partial rollback

Every phase is independently revertible: the Claude adapter is gated by `claude.enabled`, the local adapter by `commander.backend`, and the entire commander by `commander.enabled`. The Lua namespace remains registered in every case — scripts see "no order", never a missing function, so a rollback never breaks a script that was written against the API.

## Alternatives considered

### Option 1: In-process C++ plugin owning an `ILlmClient` and a worker (selected)

The plugin snapshots entity state on the sim thread, runs inference on a worker via `IHttpClient`, validates, and publishes orders for Tier-1 Lua to consume.

**Pros:**
- Uses only shipped, verified SDK surface: `IPlugin`, `IScriptingPluginService`, `MissionRegistrar`, `ComponentFieldAccess`, `IHttpClient`, `IThreadRunner`.
- Depends on nothing that is absent from the tree.
- The `ILlmClient` seam makes local and hosted backends a config change, and makes CI possible with no server at all.
- No platform binary changes, so rollback is deleting one file.

**Cons / trade-offs accepted:**
- Reimplements a slice of what `n8ro-llm` was designed to provide (an LLM request path), accepted because that module is not installed and its output is Lua scripts rather than orders.
- Owns an HTTP client and a worker lifecycle, which is real complexity; accepted because the alternative is a blocking call on the update thread, which is not an option at all.

### Option 2: Route through the existing MCP stack

Drive entities through `bin/n8ro-sim-bot.exe` and `bin/ai/n8ro-mcp.exe` over the ZMQ IPC endpoints in `data/ai/com/`, as brought up by `bin/ai/run-full-stack.cmd`.

**Pros:**
- Ships today and is plausibly the sanctioned AI integration path.
- If `n8ro-sim-bot` already exposes entity-control tools, most of this PRD's plugin becomes unnecessary.

**Cons:**
- Its entity-control tool surface is unverified — nobody has confirmed what it can actually command.
- Adds a process boundary and IPC failure modes to a latency budget that is already tight.
- MCP tool-calling is an orchestration-level interface; per-entity control at 20 s cadence may not be what it was built for.

**Why not chosen:** the capability is unverified, and building on an unverified surface risks discovering mid-implementation that it cannot command an entity. Recorded as OQ-4 — a "yes" answer supersedes Option 1 and reduces this PRD's plugin to a thin bridge, which is a cheaper outcome, not a wasted one, because the order schema, validator, and replay format all survive the change.

### Option 3: Wait for `n8ro-llm`

Consume `n8ro-llm/generate` and `n8ro-llm/generate/result` over the message bus, letting `SimLuaLLMHost` own inference and RAG.

**Pros:**
- A sanctioned service boundary, a documented 7-step RAG pipeline, and no HTTP client in this plugin.

**Cons:**
- Not installed: no DLL, no import library, no headers. Nothing in the tree hosts those topics.
- Its pipeline generates Lua scripts — an authoring aid, not a real-time control path.
- No installation date.

**Why not chosen:** you cannot depend on absent code. OQ-3 tracks it; if it lands, the `ILlmClient` seam absorbs it as a fifth adapter without touching the order schema, the validator, or the Lua API.

### Option 4: Pure-Lua LLM client

Have the mission script call the model directly.

**Why not chosen:** structurally impossible. The sandbox opens only `base`, `math`, `table`, `string`; `io`, `os`, `package`, `require`, `load`, and `dofile` are disabled; and every hook runs under an instruction budget that an unbounded wait would blow immediately. There is no HTTP and no async in Lua here, by design.

### Option 5: Do nothing / status quo

**Why not acceptable:** the tree carries 6.5 GB of models, a doctrine corpus, an MCP stack, and a documented-but-absent LLM module, and has no path from any of it to entity behavior. Every scenario's tactical repertoire stays frozen at authoring time, and the owner's brief — *"control an entity using AI inside a scenario, using the N8RO SDK"* — goes unanswered.

## Risks and open decisions

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| **Determinism loss.** `execution-models-and-timing.md` treats reproducibility as a design goal; an LLM in the loop breaks it outright | High | Certain — this is a property of the design, not a failure mode | Record/replay from day one (AIC-DET-1/2). Order log on by default. A per-mode guarantee table so nobody assumes more than holds. `stub` and `replay` backends give CI a deterministic path |
| **Threading.** `IHttpClient::send()` is blocking and single-thread-only; `ScriptingApiContext` collaborators are single-thread-only | Critical — undefined behavior, corrupted state | Medium if the pattern is not enforced structurally | Snapshot-by-value → worker → order slot (AIC-ARCH-2). Worker captures no SDK pointer, enforced by `static_assert` over the captured types plus a deep-copy-outlives-original test. One `IHttpClient` per worker. *(v1.3 — the mitigation previously read "TSAN in CI"; no TSan runtime exists on Windows, §Corrections item 8.)* Evidence in its place: the full suite green under AddressSanitizer (65/65) and a 20,000-publish exchange-slot stress test against a concurrent consumer with torn-read detection. See the residual-risk row below for what this does **not** buy |
| **Validation.** A model emits an illegal or hallucinated order | High — wrong entity commanded, fratricide, out-of-envelope waypoint | High — expected behavior of a 3B model without constrained decoding | Two-stage pipeline (AIC-VAL-1), 13 named reject reasons, adversarial test corpus, reject-and-retain (AIC-VAL-2), constrained decoding on both backends |
| **Data classification.** Scenario state sent to a hosted API leaves the machine; every file here carries an Arkheon proprietary header | Critical | Low if gated, certain if not | `claude.enabled` independent and default-false; enumerated transmitted-field allowlist asserted by test (AIC-SEC-2); mandatory egress warning; owner authorization required per the deployment checklist |
| **Unmet dependency.** No inference server ships | High — Phase 1b cannot run | Certain today | Recorded as a dependency with a named owner gap (OQ-2); `stub` backend keeps Phases 0/1a fully deliverable and testable without it |
| **Prompt injection via external track feeds** | Medium — model-chosen orders | Low, but the channel is real (`componentTrackIdentity` is ADS-B-sourced free text) | `componentTrackIdentity` excluded from prompts entirely; charset/length filtering on all remaining strings; the validator is the real defense |
| **Cadence too slow to be useful** (H1 wrong) | Medium — feature delivers less than hoped | Medium on CPU with the 7B | Default to the 3B; GPU decision recorded as OQ-2; if H1 fails, narrow scope to mission-start intent rather than chasing latency |
| **Entitlement gate blocks AI-using plugins** | Medium — plugin will not load on licensed machines | Unknown | OQ-5. Check `core/entitlement/AccessGate.h` behavior before Phase 1b deployment |
| **Silent zero from a runtime column.** *(v1.2)* A mistyped or renamed `componentTransform` runtime column reads back `0` with no error, so the snapshot carries a fabricated stationary own-ship and every order is computed from it | High — degraded order quality with no failing test and no log line; the defect is invisible precisely because it looks like valid data | Medium — the path convention differs from the schema's (dot vs slash) and the header states the failure is silent | AIC-ARCH-4 startup probe against a known-moving entity; commander refuses to enable on probe failure; probe result in the startup log, `getStats()`, and the runbook; a negative test asserts a broken path disables rather than degrades. OQ-9 pins the actual observed behaviour |
| **No race detector on the target platform.** *(v1.3)* A data race in the sim-thread/worker exchange can exist and go unobserved: ASan finds memory errors, not races — it models no happens-before relation and will not flag an unsynchronized access that does not corrupt memory on the interleavings it happens to run | Medium — a race that never manifests under test is still a race, and the failure it eventually produces (a torn or stale order) is one the fallback ladder makes look like a model failure rather than a threading bug | Low that one exists, given the structural design; **certain** that no tool on this platform would prove otherwise | Accepted, not mitigated away, and stated here so it is not mistaken for a covered case. What is actually held: (a) a compile-time proof the worker shares nothing (`static_assert` over the captured types + deep-copy-outlives-original), which is stronger than a detector on the *sharing* question and is the load-bearing argument; (b) the exchange slot is the single crossing point and is stress-covered at 20,000 publishes with a serial encoded into a second field, so a torn read is detected rather than merely improbable; (c) ASan 65/65 over the full suite, which excludes the memory-error class entirely. What is **not** held: no happens-before analysis, no coverage of interleavings the stress test did not hit, and no coverage of reorderings that x86-64's strong memory model hides but a weaker one would expose. If the plugin is ever built for a platform with a TSan runtime, running it there is the cheapest way to close this — recorded in §Out of scope |
| **Tier-1 reporting silently not wired.** *(v1.2)* A script adopts the commander but never calls `reportTrack`, so the model sees no tracks and every targeted order rejects `track` | Medium — looks like model failure, is actually an integration gap | Medium — it is a new obligation and easy to omit | `aicmd.tracks.reported` metric with an alert threshold; the runbook's `track` row checks it *first*; reporting is advisory by design so the entity still flies waypoint postures rather than failing |

### Open questions

Carried from the authoring brief unresolved, per instruction, plus questions this analysis surfaced.

| # | Question | Status | Decision target | Rationale (why open / what would resolve it) |
|---|---|---|---|---|
| OQ-1 | Which inference server on target machines — Ollama or llama.cpp server? | Open | Phase 1b start | llama.cpp loads the shipped `.gguf` files directly and offers GBNF grammars for hard output guarantees; Ollama needs `ollama create` plus a Modelfile to import them, and the 7B is split across two files. Resolved by a deployment decision plus a one-hour spike loading each. Pins the `local` adapter's endpoint path and payload shape |
| OQ-2 | Is a GPU present on target machines? | Open | Phase 1b start | Decides 3B vs 7B and the achievable order cadence: 3–6 s vs 10–15 s on CPU, both under ~2 s on GPU. Resolved by an inventory of the target machines. Directly determines whether H1 can be validated at all |
| OQ-3 | Is `n8ro-llm` going to be installed later? | Open | v1.1 planning | If yes, the plugin should consume `n8ro-llm/generate` over the message bus instead of owning an HTTP client. Resolved by a roadmap answer from whoever owns that module. The `ILlmClient` seam is designed so this is an added adapter, not a redesign |
| OQ-4 | Is the existing MCP stack (`bin/ai/run-full-stack.cmd`, `bin/n8ro-sim-bot.exe`) the sanctioned AI integration path? | Open | Phase 1a end | If `n8ro-sim-bot` already exposes entity-control tools, routing through it may beat a new plugin. Resolved by enumerating its MCP tool surface — a half-day investigation. A "yes" makes Alternative 2 supersede Alternative 1; the order schema, validator, and replay format survive either way |
| OQ-5 | Is there an entitlement/licensing gate on AI-using plugins? | Open | Phase 1b deployment | `core/entitlement/AccessGate.h` and `LexActivator.dll` are present in the tree. Resolved by reading `AccessGate.h`'s contract and testing plugin load on a licensed machine. If gated, the plugin needs an entitlement check at `initialize()` |
| OQ-6 | Which scenario and entity for the first demo? | Open | Phase 1b start | Existing scenarios include `baltic_sentinel`, `kamikaze_swarm_outback`, `oppint_blue_cap`, `oppint_red_interceptor`, `oppint_red_sam`, `shahed_launcher_truck`, `paramotor_waypoint_mission`, `global_air_traffic_showcase`. Resolved by an owner pick. `oppint_red_interceptor` is the natural candidate because its Tier-1 logic is already the quality bar and its posture vocabulary is this PRD's enum |
| OQ-7 | Does the host supply a non-null `IThreadRunner` to sim plugins at `initialize()`? | **Resolved 2026-08-01** | — | **Yes — both are non-null.** Observed on a live `n8ro-sim-local` run: *"PluginContext.services is non-null, PluginContext.threadRunner is non-null."* The plugin therefore uses `IThreadRunner::submitBackgroundTask` as its primary dispatch. The owned-thread fallback stays implemented per AIC-ARCH-2 — the field is documented nullable, and one host observation does not license removing a specified fallback — but it is now the contingency rather than the expected path |
| OQ-8 | Should the doctrine prefix be padded to the configured model's cache minimum? | **Open — now with a measurement** | Phase 2 start | Haiku 4.5's prompt-cache minimum is 4096 tokens. *(v1.3)* The prefix has been **measured**: 4,738 bytes ≈ **1,200 tokens** on a live engine run, logged as `prefixBytes` (§Corrections, item 9). It therefore silently does not cache on Haiku 4.5 as written. Three consequences, all evidence *into* the question and none of them answering it: the padding delta is **~2,900 tokens, not ~3,300**, so padding costs less than the v1.2 arithmetic implied; the uncached baseline the padding is judged against is **higher** than assumed ($0.00180/order, not $0.00140), so caching wins by more; and the uncached four-ship-hour figure now **exceeds** the §Success metrics ≤ $1.10 target while the cached one does not. **Deliberately left open.** What remains is not a measurement but a judgement the owner owns: whether ~2,900 tokens of *genuine* doctrine are worth writing. Padding with filler to earn a cache discount is a net loss in every dimension except the invoice, and this PRD will not pre-commit that call. Resolved at Phase 2 start by an owner decision on doctrine content, against the recomputed regimes in §Cost model |
| OQ-9 | Can `readComponentFieldReal` address the transform's **runtime** columns at all, and if so does a bad path really return `0` rather than `std::nullopt`? | **Resolved 2026-08-01** | — | *(Added v1.2, resolved same day.)* **Yes it can, and bad paths are LOUD, not silent.** Observed on a live run against entity `NeutralDuplexHome_01`: `velocityNed.x` resolved, the schema's slash form `velocityNed/x` **also** resolved, and the deliberately misspelled `velocityNed.q` returned `std::nullopt` while emitting `DynamicLayout::handle: no field at path 'velocityNed.q'` at ERROR level. The silent-zero warning in `TransformRuntimeColumns.h` describes the raw handle-resolution path, not `readComponentFieldReal`, which validates and reports. **Consequence:** AIC-ARCH-4's probe is a plain resolve-check; the moving-entity heuristic is not required and a stationary entity is a valid subject |

### Rabbit holes

- **Doctrine prompt engineering.** `data/ai/context/` holds a substantial RAG corpus (HAVA/DENIZ/KARA doctrine, electronic warfare, cyber, space, maritime, land, irregular ops, plus N8RO manuals). It is tempting to wire retrieval in early. Don't: retrieval makes the prefix volatile, which destroys both the local KV cache and the hosted prompt cache, which is the single largest latency lever in the design. Contain: hand-write 1–2 pages for Phase 1, timebox to one day, and defer retrieval to v1.1 with its own caching design.
- **Ollama GGUF import.** Importing the split 7B (`qwen2.5-7b-instruct-q4_k_m-00001-of-00002-002.gguf` + `-00002-of-00002.gguf`) into Ollama needs `ollama create` with a Modelfile and may need the split rejoined first. Contain: timebox to half a day; if it resists, llama.cpp server loads the files directly and OQ-1 resolves itself.
- **Making replay bit-exact.** It is tempting to promise trajectory reproducibility. Physics reproducibility belongs to the engine's timing configuration, not this plugin. Contain: scope the guarantee to "identical published orders at identical simulation times" and state the trajectory caveat in the guarantee table — already done in AIC-DET-2.
- **Multi-entity coordination creeping in.** Once one entity takes orders, "just let the model command the pair" is one prompt change away, and it silently introduces order-consistency and conflict-resolution problems the schema has no answer for. Contain: `commander.maxCommandedEntities` is a hard cap, orders are strictly per-entity, and `sectionId` stays reserved and unused until v1.1.
- **Retry storms against a slow local server.** A 30 s timeout plus retries against a CPU server that takes 15 s per order produces overlapping requests and a self-inflicted slowdown. Contain: per-entity request suppression is mandatory, `maxConcurrentRequests` defaults to 1, and the local adapter does not retry at all — the fallback ladder handles it.

## Cost model

*[Finance / Owner]* Phase 2 only. Phase 1 has no marginal cost.

**Assumptions.** *(v1.3 — recomputed against the measured prefix.)* Prompt ≈ **1,400** input tokens (stable prefix **~1,200, measured at 4,738 bytes on a live engine run** — §Corrections, item 9 — plus volatile suffix ~200); output ≈ 80 tokens for a constrained order; cadence 20 s → 180 orders per entity-hour; a four-ship scenario → 720 orders per scenario-hour. Budget: **$100 in held credit.**

The v1.2 table assumed a ~800-token prefix and therefore a 1,000-token prompt. Every uncached figure below is ~29 % higher as a result; nothing else in the model changed.

| Model | $/MTok in | $/MTok out | $/order | $/entity-hour | $/four-ship-hour | Four-ship hours on $100 |
|---|---|---|---|---|---|---|
| `claude-haiku-4-5` | $1 | $5 | $0.00180 | $0.324 | **$1.30** | **≈ 77** |
| `claude-sonnet-5` (introductory, through 2026-08-31) | $2 | $10 | $0.00360 | $0.648 | $2.59 | ≈ 39 |
| `claude-sonnet-5` (list) | $3 | $15 | $0.00540 | $0.972 | $3.89 | ≈ 26 |
| `claude-opus-5` | $5 | $25 | $0.00900 | $1.620 | $6.48 | ≈ 15 |

*(Prior v1.2 values, for comparison: Haiku $0.00140/order, $1.01/four-ship-hour, ≈ 99 hours.)*

**With prompt caching (Haiku 4.5, doctrine padded to the 4096-token minimum).** Cache reads bill at 0.1× base, cache writes at 1.25×. Per order: 4096 cached-read tokens × $0.10/MTok = $0.00041, plus 200 volatile × $1/MTok = $0.00020, plus 80 output × $5/MTok = $0.00040 → **$0.00101/order**, or **$0.73 per four-ship-hour ≈ 137 hours on $100**. Cache writes cost $0.0051 each and recur only when the 5-minute TTL lapses, which a 20 s cadence never allows — negligible. These figures are unchanged by the v1.3 measurement: a padded prefix is 4096 tokens regardless of what it was padded *from*.

**The caching decision is not free, and this is what OQ-8 turns on** *(reframed v1.3 — the arithmetic below was written against an assumed ~800-token prefix; the real figure is ~1,200)*. Two things move, in the same direction:

- **The padding delta is smaller than the table implied** — ~2,900 tokens to reach 4096, not ~3,300 — so there is less doctrine to write and less to pay for if it is never cached. Padding *without* caching still costs $0.0047/order, but that is now ~2.6× the unpadded price rather than the ~3.4× the old baseline suggested.
- **The uncached baseline it is judged against is higher** — $0.00180/order, not $0.00140 — so caching the padded prefix ($0.00101) now beats unpadded-uncached by **~44 %**, not ~28 %. In scenario-hour terms: **$0.73 cached-and-padded vs $1.30 unpadded**, and only the former sits under the §Success metrics ≤ $1.10 target.

So the economic case for padding is **stronger** than v1.2 stated, and the "ship it short" escape hatch has narrowed: the v1.2 framing rested on the doctrine possibly fitting in ~800 tokens, and it demonstrably does not — it is already ~1,200 and still nowhere near cacheable on Haiku 4.5. **This does not resolve OQ-8.** Cost is not the only axis, and the question the arithmetic cannot answer is whether ~2,900 further tokens of *genuine* doctrine exist to be written. Padding with filler to earn a discount remains a net loss in every dimension except the invoice, and that judgement is the owner's at Phase 2 start.

**Recommendation:** default `claude.model = claude-haiku-4-5`. At $1.30 per four-ship scenario-hour uncached — or $0.73 padded and cached — the $100 credit funds roughly **77 hours** of live four-ship operation in the worst case, still far beyond any plausible demo and evaluation need. Reserve Sonnet 5 for order-quality comparison runs (~39 hours at introductory pricing remains ample for A/B evaluation), and keep Opus 5 out of the control loop entirely; at 5× Haiku's cost for a decision that emits six posture values, it is the wrong instrument.

**Budget guard:** `aicmd.tokens.in` / `.out` accumulate in the order log; the runbook triggers a model downgrade or a switch to `local` at 80 % of budget.

## Validation and test plan

**Unit — order validator** *(no engine, no server; the highest-value tests in the plan)*
- Table-driven over ≥ 40 adversarial payloads: wrong types; out-of-range latitude/longitude/altitude/speed; `NaN` and `Infinity`; unknown enum members; extra top-level properties; missing conditional fields (`targetEntityId` on `engage`, `waypoint` on `hold`); a `reason` carrying injection text; hallucinated entity ids; a friendly-team target; a 10 MB body; a truncated JSON object; two JSON objects concatenated. Each asserts both rejection *and* the expected reason code — a test that only asserts rejection cannot tell a schema failure from a range failure.
- Clamp boundaries: values exactly at, just inside, and just outside each `safety.*` bound.
- Staleness: orders at `maxOrderAgeS` ± ε.
- Fallback ladder: assert each transition at its configured boundary, and that a retained order is never partially replaced.

- *(v1.2)* Stage-B B3 against the reported list: an order naming a target that was never reported is rejected `track`; an order naming a reported target passes; an order naming a target reported in the *previous* window but not the current one is rejected `track`; and an order validated against the list that accompanied its own snapshot passes even when the window has since rolled over.

**Unit — Tier-1 ingress** *(v1.2)*
- `reportTrack` / `reportLoadout` reject: unknown entity, entity not on the roster, wrong arity, wrong argument types, `NaN`/`Infinity` in `rangeM` or `snrDb`, and reporting while `commander.enabled` is false.
- Idempotence: reporting the same `targetEntityId` twice replaces rather than duplicates; the list never exceeds `commander.maxTracksInPrompt`, and the overflow report returns `false` rather than evicting an existing row.
- Lifecycle: taking a snapshot clears the lists; a window with no reports yields an empty picture rather than a stale one.
- Ingress sanitization: a `targetEntityId` carrying injection text or a control-character payload is charset-filtered and length-capped before it can reach a rendered prompt.

**Unit — prompt renderer**
- Prefix byte-stability across 100 renders with varying snapshots (AIC-BE-3).
- Transmitted-field allowlist via unique sentinels, asserting `componentTrackIdentity` sentinels are absent (AIC-SEC-2).
- *(v1.2)* Reported-list render determinism: the same set of reports delivered in different Lua call orders renders byte-identically and yields the same `snapshotHash`.
- *(v1.2)* No track attribute beyond `targetEntityId`, `rangeM`, `snrDb` appears in the rendered suffix — no `team`, `kind`, or `domain`.
- API-key redaction across prompt, logs, and order records.
- Unit traceability: re-read `schema-reference.json` and assert every unit in the order schema matches the file's `unit` key for the corresponding record — this is the test that keeps "derive, don't guess" true over time.

**Integration — stubbed client, no inference server** *(the CI gate)*
- Load the plugin into the engine with `commander.backend = "stub"`, run ≥ 200 simulated ticks against a test scenario.
- Assert: N orders accepted, per-frame p95 within budget, no order log corruption, Lua getters return the published order, `getOrderSerial` strictly increases.
- *(v1.3)* Run under **AddressSanitizer**, not TSAN — see the concurrency-evidence block below for why, and for what carries the threading claim in its place.
- Assert the worker never dereferences an SDK pointer (structurally, via a build-time `static_assert` on the captured types where the language permits, and via review otherwise).
- *(v1.2)* Runtime-column probe (AIC-ARCH-4): assert the probe passes on a healthy tree; assert that a deliberately misspelled column path makes the probe fail and leaves the commander disabled rather than producing a zero-velocity snapshot.

**Concurrency evidence — in lieu of ThreadSanitizer** *(v1.3)*

TSAN is not available on the target platform. VS 2026 Insiders ships `tsan_interface.h` and
`tsan_interface_atomic.h` under `VC\Tools\MSVC\14.51.36231\include\sanitizer\` but **no
`clang_rt.tsan` runtime anywhere**, and neither MSVC nor the bundled LLVM toolchain supports TSan on
Windows (§Corrections, item 8). The v1.2 plan's "run under TSAN" bullet and Phase 1a's "TSAN clean"
gate item were therefore unsatisfiable as written. They are replaced by three artifacts that were
actually produced, and by an explicit statement of what those artifacts do not cover.

| # | Evidence | What it substantiates |
|---|---|---|
| C1 | The full suite runs clean under **AddressSanitizer** (`/fsanitize=address`), **65/65** | No use-after-free, no heap or stack overflow, no double free, no use-after-scope anywhere in the plugin — including across the snapshot hand-off, which is where a lifetime bug between the sim thread and the worker would land |
| C2 | A dedicated concurrency test hammers the **sim-thread/worker exchange slot with 20,000 concurrent publishes** against a concurrent consumer, encoding each publish's **serial into a second field** so that a mismatch between the two fields identifies a torn read | The one shared crossing point in the design holds up under contention, and a torn or interleaved read is **detected** rather than merely improbable — the second field converts "we never saw corruption" into a positive check with a failing condition |
| C3 | The worker's callable **captures only value types**, asserted by `static_assert` over the captured types, plus a **deep-copy-outlives-original** test that destroys the source snapshot while the worker still holds its copy | The structural argument, which stands independently of any test run: a worker that shares nothing cannot race on shared state. This is the load-bearing claim; C1 and C2 corroborate it |

**What this does not cover, stated plainly.** AddressSanitizer detects **memory errors, not data
races** — it models no happens-before relation, and an unsynchronized access that does not corrupt
memory on the interleavings it happens to execute produces no report. So C1's 65/65 is silent on the
race question by construction, not by luck. C2 is a stress test, and a stress test samples
interleavings; passing 20,000 publishes bounds the probability of the races it can reach, it does not
prove their absence, and it runs on x86-64, whose strong memory model hides reorderings that a weaker
one would expose. C3 proves what the worker *captures*, not that the exchange slot's own
synchronization is correct. The honest summary: this set makes an unnoticed race unlikely and
clears the memory-error class over every path the suite exercises, but it is **not** equivalent to a
clean TSAN run, and no configuration available on this platform is. Carried as a standing residual risk in §Risks and as an
Out-of-Scope row, not closed.

**Replay determinism**
- Record a `stub` run to a log; replay it twice; assert byte-identical published-order sequences and identical per-tick position hashes.
- Replay a log against a *modified* scenario; assert `replay.divergence` records appear rather than silent acceptance — a replay that quietly diverges is worse than one that fails.

**Live scenario smoke** *(requires an inference server — OQ-1/OQ-2)*
- 10-minute run on the OQ-6 scenario with `commander.backend = "local"`.
- Assert: no frame exceeding 5 ms of plugin cost; acceptance rate ≥ 90 %; zero fratricide events; at least three distinct postures observed; entity completes the scenario.
- Repeat with `commander.enabled = false` and diff the behavior — the commander-off run must be identical to a run with the plugin absent.

**Negative / resilience**
- Server down (`send()` returns `std::nullopt`), timeout, HTTP 429, HTTP 500, malformed JSON, `stop_reason == "refusal"`, oversized body, and TLS unavailable. Each must reject-and-retain, write exactly one record, and leave frame time untouched.
- *(v1.2)* A commanded entity whose Tier-1 script never calls the reporters: orders still arrive, targeted orders are rejected `track`, waypoint postures still execute, and the run completes with no error state.

**CI requirement:** the unit, stubbed-integration, and replay-determinism suites SHALL run on every build and SHALL NOT require an inference server or network access. The live smoke is a manual gate at Phase 1b and Phase 2.

## Rollout and milestones

### Phase 0 — Scaffold

**Deliverables:** plugin cloned via `new-sim-plugin.cmd ai-commander sim-scripting`; relocated to its standalone repository with the `open-solution.cmd` edit applied, `.gitignore`, `README.md`, and `NOTICE` in place, and `docs/prd.md` carrying this document; builds `Release | x64` in VS 2026; auto-deploys to `%N8RO_RELEASE_USER_SIM_PLUGINS%`; registers an empty `aiCommander` namespace.

**Validation gate:**
- `dumpbin /exports` shows `create_plugin`, `destroy_plugin`, `get_plugin_signature`.
- Plugin coexists with `sim-scripting` — distinct `ProjectGuid` and plugin id, both DLLs load.
- Builds from the relocated repository with only `N8RO_RELEASE_ROOT` (or the documented fallback) resolving the tree — no path in the repository points into `dev/samples/`.
- `git status` is clean after a full `Release | x64` build: no build output, no `.vs/`, no order log, no `.env` staged.
- Engine run regenerates `data/resources/missions/stubs/aiCommander.lua`.
- Startup log records whether `services` and `threadRunner` are non-null (answers OQ-7).

### Phase 1a — Full pipeline on the stub backend

**Deliverables:** order schema, two-stage validator, fallback ladder, order record/replay, the complete 14-function `aiCommander` Lua namespace *(including the v1.2 `reportTrack` / `reportLoadout` ingress)*, the full config set, snapshot→worker→slot threading, the AIC-ARCH-4 runtime-column probe, and the `stub` and `replay` adapters.

**Validation gate:**
- All unit, stubbed-integration, and replay-determinism tests green; suite runs with no inference server and no network.
- Frame-cost p95/p99 within budget over a 200-tick run.
- *(v1.3, replacing "TSAN clean" — no TSan runtime exists on Windows, §Corrections item 8)* The concurrency-evidence set is green and its residual gap is acknowledged rather than waived:
  - full suite clean under AddressSanitizer (`/fsanitize=address`), 65/65;
  - the exchange-slot stress test passes 20,000 concurrent publishes against a concurrent consumer with no torn read detected by the second-field serial check;
  - the worker's value-only capture holds at compile time (`static_assert`) and the deep-copy-outlives-original test passes.
- *(v1.3)* The prompt prefix's `prefixBytes` is logged at startup and compared against the configured model's cache minimum, and the observed value is recorded as evidence into OQ-8 — **which this gate does not resolve**. Measured on the local tree: 4,738 bytes ≈ 1,200 tokens, below Haiku 4.5's 4096-token minimum.
- Adversarial corpus 40/40 rejected with correct reason codes.
- The reference Tier-1 script implements every AIC-ORD-2 row **and both reporting obligations**, and falls back correctly when `aiCommander` is nil.
- *(v1.2)* The runtime-column probe passes on the target tree, and a deliberately broken column path is shown to disable the commander rather than yield a zero-velocity snapshot.
- *(v1.2)* OQ-9 resolved — the observed behaviour of `readComponentFieldReal` against a runtime column, and against a misspelled one, is recorded.

### Phase 1b — Local backend

**Deliverables:** `local` adapter against the server chosen by OQ-1; constrained decoding; live smoke on the OQ-6 scenario; H1 and H2 measurements.

**Validation gate:**
- Live smoke passes all assertions.
- Acceptance rate ≥ 95 % over a 200-order soak; schema rejections < 1 %.
- p95 latency within the target for the chosen model and hardware.
- H2 measured: prefix-stable vs perturbed-prefix p95 recorded, whatever the outcome.
- H1 assessed by a domain reviewer on paired commander-on / commander-off runs.
- OQ-1, OQ-2, OQ-5, OQ-6 resolved.

### Phase 2 — Claude backend

**Deliverables:** `claude` adapter over raw HTTPS with structured outputs; refusal handling; token accounting; the authorization gate and egress warning; the transmitted-field allowlist test.

**Validation gate:**
- Owner authorization recorded before the first live hosted request.
- Allowlist test green; no key in any prompt, log, or record.
- Refusal, 429, 5xx, and TLS-unavailable paths each exercised and correct.
- Measured cost per order within 20 % of the Cost model's Haiku row; if not, the model reconciles the difference before further runs.
- p95 latency ≤ 2.5 s.
- OQ-8 resolved. *(v1.3 — the measurement half is already done: 4,738 bytes ≈ 1,200 tokens, taken at the Phase 1a gate. What remains at this gate is the owner's judgement on whether ~2,900 tokens of genuine doctrine are worth writing to reach Haiku 4.5's cache minimum, against the recomputed regimes in §Cost model — $1.30/four-ship-hour unpadded-uncached vs $0.73 padded-and-cached.)*

## Review checklist

- [x] All requirements have acceptance criteria
- [x] All FRs have **Customer scenario** + **Pain removed** fields
- [x] All FRs have a corresponding `UAC-{FR-ID}` entry in Appendix B
- [x] Naming/path conventions subsection present at the top of the FR section
- [x] Scope Authority subsection present
- [x] Out of Scope section present with structured entries (status / rationale / target / date)
- [x] Open Questions table has decision target + rationale on every entry; no owner column
- [x] Security implications assessed, including an enumerated transmitted-field list
- [x] Cross-component dependencies documented with in-tree verification status
- [x] Test plan covers every P1 requirement and runs without an inference server
- [x] Rollback strategy defined, with a seconds-scale first step
- [x] Key hypotheses are falsifiable and state what changes if wrong
- [x] Success metrics have baselines, targets, measurement methods, and gates
- [x] Source-control home, repository layout, ignore rules, and CI split specified
- [x] Per-file header convention for plugin sources decided and recorded
- [x] *(v1.2)* Every snapshot field traced to a reachable source, verified against the shipped headers rather than assumed
- [x] *(v1.2)* Both path conventions (schema slash-joined vs runtime dot-joined) documented, with the silent-failure mode called out and a probe requirement attached
- [x] *(v1.3)* Every validation-gate item names a tool or command that exists on the target platform, verified rather than assumed
- [x] *(v1.3)* Where a gate was replaced by weaker evidence, the residual gap is stated in §Risks and §Out of scope rather than absorbed into the restatement
- [x] *(v1.3)* The Cost model's input assumptions are traced to a measurement, not an estimate — prefix size measured at 4,738 bytes and recorded in §Corrections
- [ ] Owner authorization for the hosted backend — **outstanding, Phase 2 gate**
- [ ] Repository visibility confirmed — private assumed; publication needs the same authorization as the hosted backend
- [ ] OQ-1 through OQ-6 and OQ-8 — **outstanding** *(v1.3 — OQ-7 and OQ-9 resolved 2026-08-01; OQ-8 now has its measurement but not its decision, which is the owner's at Phase 2 start)*

## Appendix A: Units and frames, traced to source

Every quantity this PRD's order schema carries, with the `schema-reference.json` record it derives from. Units live only in that file — deliberately absent from the C++ headers.

| Quantity | Unit | Frame / datum | Schema record | `unit` key |
|---|---|---|---|---|
| Latitude | degrees | Geodetic WGS-84 | `/datablocks/positionGeodetic/latitudeDeg` | `Deg` |
| Longitude | degrees | Geodetic WGS-84 | `/datablocks/positionGeodetic/longitudeDeg` | `Deg` |
| Altitude | metres | **Height above the WGS-84 ellipsoid (HAE)** — not AGL, not MSL | `/datablocks/positionGeodetic/altitudeHaeM` | `M` |
| Cruise / commanded speed | metres per second | Ground speed | `/datablocks/waypoint/speed` | `Mps` |
| Initial speed | metres per second | Ground speed, level flight at t=0 | `/datablocks/componentTransform/speedMps` | `Mps` |
| Heading | degrees | Clockwise from **true** north | `/datablocks/componentTransform/headingDeg` | `Deg` |
| Orbit / loiter radius | metres | — | `/datablocks/componentNavigation/onWaypointReachedLoiterRadiusM` | `M` |
| Sensor detection radius | metres | Slant range | `/datablocks/componentSensor/detectionRangeM` | `M` |
| Mission tick period | seconds | Simulation time | `/datablocks/componentMission/updateIntervalS` | `S` |

**Not in the schema** *(clarified v1.2)*. The schema export carries only what an author states, so the transform's live motion state has no record above. Two other sources own it, and which one applies depends on whether you are in Lua or C++:

| Quantity | Unit / frame | Lua source (stubs) | C++ source (runtime columns) |
|---|---|---|---|
| Velocity | m/s, NED | `entityControl.getVelocityNed` → `velN, velE, velD` | `TransformRuntimeColumns.h` — `velocityNed.x` / `.y` / `.z` (x=North, y=East, z=Down) |
| Acceleration | m/s², NED | `entityControl.getAccelerationNed` | `accelerationNed.x` / `.y` / `.z` |
| Orientation | body-to-NED | `entityControl.getOrientationEulerDeg` → `headingDeg, pitchDeg, rollDeg` | `orientationBodyToNedQuat.w` / `.x` / `.y` / `.z` (unit quaternion, **not** Euler) |

Note the two path conventions differ: the schema uses `/`, the runtime columns use `.`. The runtime columns fail *silently* on a bad path — see §Naming and path conventions and AIC-ARCH-4.

Also from the generated stubs rather than the schema: sensor tracks report `range_m` in metres and `snr_DB` in decibels — the units carried by `aiCommander.reportTrack`'s `rangeM` and `snrDb` arguments (AIC-API-1). DIS entity kind follows SISO-REF-010: 1 = platform, 2 = munition; it is *not* transmitted in v1.2 (see §Out of scope).

## Appendix B: User acceptance criteria

### UAC-AIC-ARCH-1: Three-tier decision split
**GIVEN** a scenario author running `oppint_red_interceptor` with the commander enabled
**WHEN** the entity changes posture in response to an accepted order
**THEN** every position change traces to a `navigation.*` verb invoked by the Lua script, the plugin has issued zero `entityControl.requestUpdate*` calls and zero `writeComponentField*` calls against `componentTransform`, and a commander-disabled run of the same script is byte-identical to a run with the plugin absent.

### UAC-AIC-ARCH-2: Snapshot → worker → order-slot threading
**GIVEN** an operator running the 7B model on CPU, where one order takes 10–15 s
**WHEN** a request is in flight across many simulation frames
**THEN** `onTickFrame` cost stays under 0.5 ms at p95 and 2.0 ms at p99, the worker holds no SDK pointer — a property asserted at compile time by `static_assert` over the captured types, not merely observed — the full suite is clean under AddressSanitizer (65/65), and the exchange-slot stress test completes 20,000 concurrent publishes against a concurrent consumer with no torn read detected by its second-field serial check.

*(v1.3 — this UAC previously read "TSAN reports no race". There is no ThreadSanitizer runtime for Windows, so that condition could never be met on the target platform; §Corrections item 8. The criteria above are what was actually run. They do not carry the same guarantee — AddressSanitizer finds memory errors, not races, and a stress test samples interleavings rather than proving their absence — and the gap is held open as a risk row rather than closed by restatement.)*

### UAC-AIC-ARCH-3: One `ILlmClient` seam
**GIVEN** a CI machine with no inference server and no network
**WHEN** the integration suite runs with `commander.backend = "stub"`
**THEN** the full order pipeline — render, validate, record, publish, consume from Lua — executes with no I/O, and switching `commander.backend` at runtime changes the adapter without a restart.

### UAC-AIC-ARCH-4: Runtime-column startup probe
*(Added v1.2)*
**GIVEN** a release tree in which a `componentTransform` runtime column has been renamed or is otherwise unresolvable
**WHEN** the plugin initializes and probes `velocityNed.x` / `.y` / `.z`
**THEN** the probe fails, the commander stays disabled, the failing path is named in the startup log and surfaced through `aiCommander.getStats()`, no order is requested, and at no point is a snapshot built carrying a fabricated `0, 0, 0` velocity.

### UAC-AIC-ORD-1: Order document schema
**GIVEN** a tactics author reading the order contract
**WHEN** the model returns a response carrying an unknown property, an out-of-enum posture, or an altitude outside the configured bounds
**THEN** the order is rejected with reason `schema`, `enum`, or `clamp` respectively, no field is repaired, and the previously published order remains in force.

### UAC-AIC-ORD-2: Posture → verb mapping
**GIVEN** the reference Tier-1 script and an accepted order with `posture = "hold"`
**WHEN** the script's `onTick` runs
**THEN** it calls `navigation.requestHoldPosition(entityId, lat, lon, alt, orbitRadiusM, cruiseSpeedMps)` with the ordered values, and no verb outside the authorized set is invoked — and in the same tick, before reading the order, it has reported its track picture via `sensor.getTrackNr` / `getTrackById` → `aiCommander.reportTrack` and its stores via `weapon.getWeaponLoadout` → `aiCommander.reportLoadout`.

### UAC-AIC-VAL-1: Two-stage validation
**GIVEN** an operator concerned about hallucinated targets
**WHEN** the model emits an order naming an entity id that Tier 1 never reported through `aiCommander.reportTrack` for the window the order's snapshot came from
**THEN** Stage B rejects it with reason `track`, one `order.rejected` record is written carrying the truncated raw body, the `aicmd.reject.track` counter increments, and no `navigation.*` or `weapon.*` verb is invoked with that id.

### UAC-AIC-VAL-2: Reject-and-retain fallback ladder
**GIVEN** an inference server that dies 200 s into a scenario
**WHEN** the run continues to completion
**THEN** the entity holds its last accepted order for `orderValidityS`, transitions to the standing order, is released to Tier 1 after `releaseAfterS`, completes the scenario under `navigation.resumeWaypointFollowing`, and each transition is recorded exactly once.

### UAC-AIC-SEC-2: Transmitted-field allowlist
**GIVEN** an owner who must state what proprietary data leaves the machine
**WHEN** a prompt is rendered from a snapshot whose every string field carries a unique sentinel
**THEN** only allowlisted sentinels appear in the rendered bytes, no `componentTrackIdentity` sentinel appears, no `team` / `kind` / `domain` attribute appears on any track row, and the API key value appears in no prompt, log, or order record.

### UAC-AIC-API-1: The `aiCommander` Lua namespace
**GIVEN** a Tier-1 script author who has run the engine once to regenerate stubs
**WHEN** they call `aiCommander.getPosture(entityId)` for an entity with no order, with a valid order, and with a malformed argument
**THEN** they receive `nil, nil, nil`; the ordered posture/target/speed triple; and `nil, nil, nil` respectively — never a raised error — and `aiCommander.lua` in the stubs folder documents all fourteen signatures.

### UAC-AIC-API-1b: Tier-1 track and loadout ingress
*(Added v1.2)*
**GIVEN** a commanded entity whose script reports three tracks and two hardpoints in one cadence window
**WHEN** the next snapshot is taken and an order naming the second reported track comes back
**THEN** the prompt carried exactly those three tracks in ascending `targetEntityId` order with only `targetEntityId` / `rangeM` / `snrDb` per row, Stage-B B3 accepts the order, the reported lists were cleared when the snapshot was taken — and an order naming a fourth, unreported id would have been rejected `track` instead.

### UAC-AIC-API-2: Plugin configuration surface
**GIVEN** a platform owner configuring the plugin
**WHEN** they set `commander.backend = "claude"` while `claude.enabled` is false, or set `claude.baseUrl` to an `http://` URL
**THEN** `applyConfigFields` returns false, every prior value is unchanged, a reason is logged, and no hosted request is ever issued.

### UAC-AIC-BE-1: Local adapter
**GIVEN** a local inference server on `local.baseUrl`
**WHEN** the commander requests an order and the server is unreachable
**THEN** the transport failure (`send()` returning `std::nullopt`) is distinguished from a returned response carrying a non-2xx status in the reject reason, the worker's `IHttpClient` instance is used by no other thread, and the configured timeout is honoured.

### UAC-AIC-BE-2: Claude adapter
**GIVEN** an authorized Phase 2 run against `claude-haiku-4-5`
**WHEN** the API returns `stop_reason == "refusal"`
**THEN** the refusal is detected before `content` is read, the order is rejected with reason `refusal`, no `effort` parameter was sent, and the response's token counts are written to the order record.

### UAC-AIC-BE-3: Prompt prefix stability
**GIVEN** a run with a fixed configuration
**WHEN** 100 prompts are rendered from 100 different snapshots
**THEN** the prefix bytes are identical across all 100, the prefix contains no timestamp/entity id/counter, and startup logged `prefixBytes` and the derived token count against the configured model's cache minimum — a comparison that has a real observed value on the local tree (4,738 bytes ≈ 1,200 tokens, below Haiku 4.5's 4096-token minimum, so the shortfall warning fires on the default model until OQ-8 is decided).

### UAC-AIC-BE-4: TLS availability
**GIVEN** a target machine whose OpenSSL runtime is missing
**WHEN** the first hosted request is issued
**THEN** the `https` request returns `std::nullopt` while a plain-`http` control request to the same host returns a value, the plugin logs a TLS-availability diagnosis distinct from a generic transport failure, disables the hosted backend, and falls back per the ladder without retrying in a tight loop.

### UAC-AIC-DET-1: Order record format
**GIVEN** an engineer investigating a break-off at t=412 s
**WHEN** they read the order log
**THEN** they find the `order.requested` and `order.accepted` records for that entity and serial, with snapshot and prompt hashes, latency, token counts, and the full order including the model's stated reason — and no API key and no `componentTrackIdentity` field anywhere in the file.

### UAC-AIC-DET-2: Replay mode
**GIVEN** a recorded order log and a machine with no inference server
**WHEN** the run is replayed twice with `commander.backend = "replay"`
**THEN** both replays publish byte-identical order sequences at identical simulation times, no `IHttpClient` is constructed, Stage B still runs, and any divergence from live state produces a `replay.divergence` record rather than silent acceptance.

## Appendix C: Architecture decision records

> Stubs capturing this PRD's major decisions. Expand into full ADRs in the project decision log as implementation proceeds.

### ADR-1: Confine the language model to a Tier-2 intent layer

**Status:** Proposed
**Context:** `extension-points.md:140` states that scripting is a decision layer and authoritative state mutation stays in C++. Measured inference latency is 3–6 s (3B, CPU) to 10–15 s (7B, CPU), against a simulation running at frame cadence.
**Decision:** The model emits structured intent — posture, target, waypoint, ROE — every N seconds. Tier 1 validates and executes it with existing verbs. Tier 0 flies the entity. The model is never in the control loop and cannot express raw kinematics, structurally: the order schema has no property for heading, velocity, or acceleration.
**Consequences:**
- Inference latency becomes irrelevant to simulation correctness.
- The existing quality of `oppint_red_interceptor.lua` is preserved rather than replaced.
- The commander cannot react to fast events; Tier 1 must act correctly on a stale order, which is an explicit design obligation.

### ADR-2: One `ILlmClient` seam with four adapters, selected by config

**Status:** Proposed
**Context:** Phase 1 is local inference; Phase 2 adds a hosted API. No inference server ships in the tree, so CI needs a path with no server at all. `n8ro-llm` may land later.
**Decision:** A single interface taking a rendered prompt and returning a raw body or a transport failure, with `stub`, `replay`, `local`, and `claude` adapters chosen by `commander.backend`. Validation, recording, and publication are adapter-independent.
**Consequences:**
- CI runs the full pipeline deterministically with no server and no network.
- Phase 2 is a config change, not a fork.
- A future `n8ro-llm` message-bus path is a fifth adapter, not a redesign.
- Cost: an indirection and a lifecycle to manage for four implementations, three of which do no real work.

### ADR-3: Snapshot-by-value across a latest-wins slot

**Status:** Proposed
**Context:** `IPlugin.h:36` guarantees callbacks on a single update thread; `IHttpClient::send()` is blocking and single-thread-only; `ScriptingApiContext`'s collaborators are single-thread-only. `IThreadRunner::submitBackgroundTask` exists but `PluginContext::threadRunner` is nullable.
**Decision:** The sim thread builds a POD snapshot and publishes it to a mutex-guarded latest-wins slot. A worker copies it, runs inference and syntactic validation, and publishes a candidate order to a second slot. The sim thread drains that slot, runs semantic validation against live state, and publishes. The worker holds no SDK pointer.
**Consequences:**
- The sim thread never blocks on inference.
- Semantic checks that need `IEntityManager` stay where they are legal, which is why validation is two-stage rather than one.
- Stale snapshots are dropped rather than queued — correct, because a stale snapshot produces a stale order.
- Cost: the validator is split across two locations and must be read as one pipeline.
- *(v1.2)* The snapshot carries the Tier-1-reported track and loadout lists captured at snapshot time, and Stage-B B3 validates against *that* captured list rather than whatever is current at publication — otherwise a target legitimately visible when the order was requested could be rejected merely because inference outlived the cadence window.

### ADR-4: Record every order from day one; replay is a first-class backend

**Status:** Proposed
**Context:** `execution-models-and-timing.md` treats reproducibility as a design goal. An LLM in the loop breaks it outright, and retrofitting a record format after the fact never captures what the investigation actually needs.
**Decision:** JSONL order log on by default, carrying snapshot and prompt hashes, latency, token counts, and the full order. `replay` is an `ILlmClient` adapter, not a special mode — it goes through the same validation and publication path.
**Consequences:**
- A run is investigable and reproducible at the decision layer.
- CI gets a deterministic full-pipeline test.
- The guarantee is honest and bounded: identical *orders* at identical simulation times; trajectory reproducibility is inherited from the engine's timing configuration, not provided here.
- Cost: ~700 KB of log per scenario-hour, and a rotation policy to maintain.

### ADR-5: Credentials by environment-variable name, never by value

**Status:** Proposed
**Context:** `PluginConfigField` values are canonical strings persisted by the host. Every file in this tree carries a proprietary/confidential header.
**Decision:** `claude.apiKeyEnvVar` stores the *name* of an environment variable. The value is read via `std::getenv` at request time, never persisted, never logged, never returned by `getConfigFields()`. `claude.enabled` is a second, independent switch defaulting false, and an egress always logs a warning naming its destination.
**Consequences:**
- No credential is written to disk by the plugin.
- Data egress requires two positive acts and is always visible in the log.
- Cost: operators must set an environment variable before a run, which is friction — deliberate friction, on the one action in this design that cannot be undone.

### ADR-6: Tier 1 reports the tactical picture; the plugin does not fetch it

**Status:** Proposed *(added v1.2)*
**Context:** Sensor tracks and weapon loadout are runtime state with no public C++ read seam — no track component in the schema or `ComponentTypeNames.h`, no `IEntityManager` accessor, and no `ComponentFieldAccess` reader for the `list`-typed loadout. Both are reachable only from Lua, via `sensor.getTrackNr` / `getTrackById` and `weapon.getWeaponLoadout`. Three options were considered: add Lua ingress verbs; drop tracks and loadout from the snapshot; or synthesize a track list by scanning the entity roster.
**Decision:** Tier 1 pushes what it already sees, through `aiCommander.reportTrack` and `aiCommander.reportLoadout` (AIC-API-1). The plugin never reconstructs a tactical picture by other means, and roster-scanning is explicitly out of scope.
**Consequences:**
- The commander's view of the world is exactly the deterministic script's view. It cannot see further than the entity's own sensors, which is both correct and the honest reading of "the model issues intent, the script owns the facts".
- AIC-SEC-2 tightens: nothing reaches a prompt that a script did not hand over, the ingress verbs carry scalars only, and `team` / `kind` / `domain` leave the transmitted set.
- Stage-B B3 becomes a check against reported state rather than an engine query, so it validates the model against what the script actually observed — which is the comparison that matters for catching a hallucinated target.
- Reporting is advisory: a script that reports nothing still gets orders, but cannot receive a targeted one. The degradation is legible rather than silent.
- Cost: the Lua surface grows from 12 functions to 14, and the reference script carries an obligation it would not otherwise have. Accepted, because the alternative that needed no new verbs — roster synthesis — would have fed the validator a fiction and made B3 worse than useless.

### ADR-7: Substitute structural and stress evidence for a race detector

**Status:** Proposed *(added v1.3)*
**Context:** AIC-ARCH-2's threading claim was to be substantiated by a clean ThreadSanitizer run. No TSan runtime exists for this platform: VS 2026 Insiders ships `tsan_interface.h` / `tsan_interface_atomic.h` with no `clang_rt.tsan` library, and neither MSVC nor the bundled LLVM toolchain supports TSan on Windows, while ASan, UBSan, and the fuzzer runtimes all ship complete. The gate as written could not be passed by any build on the target platform.
**Decision:** Carry the threading claim on three artifacts instead — a `static_assert`-enforced value-only worker capture with a deep-copy-outlives-original test (structural), a 20,000-publish exchange-slot stress test with a serial encoded into a second field so torn reads are detected (empirical, targeted at the single crossing point), and the full suite clean under AddressSanitizer at 65/65 (excludes the memory-error class). State the residual gap versus a real race detector in §Risks and record race detection in §Out of scope rather than declaring the concern closed.
**Consequences:**
- The gate becomes passable and, more importantly, *meaningful*: an unsatisfiable checklist item trains reviewers to wave the checklist through, which costs more than the coverage it pretends to provide.
- The load-bearing argument moves from detection to structure. A worker that provably captures nothing shared is a stronger statement about *sharing* than any detector run, and it is checked at compile time on every build rather than in one CI job.
- What is lost is real and is not recovered by any of the three: no happens-before analysis, no coverage of unexecuted interleavings, and no visibility into reorderings x86-64's memory model hides. The design is not proven race-free; it is argued race-free and stress-corroborated.
- Cost: the residual risk is permanent for as long as the plugin is Windows-only. If it is ever built for a platform with a TSan runtime, running it there is the cheapest available closure and should be taken.

## Quality gate notes

Advisory. Gaps found while composing this PRD, not blockers.

- **Two dependencies are unresolvable from inside this tree.** The inference server (OQ-1, OQ-2) and the entitlement gate (OQ-5) both need answers from outside the release tree. Phases 0 and 1a are fully deliverable and testable without either, which is why the `stub` backend is Phase 1a's gate rather than a convenience.
- **Success metrics carry no historical baselines** because no implementation exists. Every baseline reads "N/A"; targets are derived from the brief's measured latency budget and from the engine's own frame cadence. The first Phase 1b run establishes real baselines and this section should be revised against them.
- **H1 is the load-bearing hypothesis and is the hardest to measure objectively.** "Posture transitions a reviewer marks appropriate" is a human judgment. If Phase 1b needs a harder signal, candidates are: time-to-first-valid-shot, shots per kill, and survival rate across paired runs — all computable from the existing entity logs. Worth deciding before the Phase 1b gate rather than during it.
- **OQ-4 could invalidate the chosen architecture cheaply, so it should be answered early.** It is scheduled at the Phase 1a gate for that reason: a half-day investigation of `n8ro-sim-bot`'s tool surface, before Phase 1b spends effort on the local adapter. The order schema, validator, and replay format survive a "yes", so the exposure is bounded to the adapter layer.
- **The doctrine text is unwritten and is on the critical path for order quality.** It is the one Phase 1 deliverable this PRD does not specify in detail, because its content is domain expertise rather than engineering. Flagged as a rabbit hole with a one-day timebox; if it needs more, that is a signal to reconsider the RAG deferral.
- **v1.3 note — a validation gate must name a tool that exists on the target platform.** "TSAN clean" survived from the authoring brief through two revisions and a design pass without anyone checking whether a ThreadSanitizer runtime ships for Windows. It does not, and the PRD consequently specified a gate no build could ever pass. The lesson generalizes past this one item: a gate is a claim about *this* toolchain, and every gate item should be traceable to a command someone has run here, in the same way §Corrections requires every SDK fact to be traceable to a shipped header. The other gate items were checked against this when v1.3 was written — `dumpbin /exports`, `/fsanitize=address`, and the replay suite all exist and all run — but the check should be part of writing a gate, not part of repairing one.
- **v1.3 note — two Phase 1a gate items were assumptions wearing the costume of measurements.** "TSAN clean" was unrunnable and the Cost model's ~800-token prefix was a guess that undershot the measured 1,200 by a third, carrying every uncached figure in the table down with it. Both were phrased with the confidence of observed facts. The pattern to watch: a number with no units-and-source trace behind it (Appendix A's discipline) reads exactly like one that has been measured, and the Cost model — the one section whose whole output is arithmetic — had no such trace on its most load-bearing input. §Corrections item 9 now carries it.
- **v1.2 note — the snapshot was specified from the Lua surface, not the C++ one.** Every field in the original §Exactly what is transmitted named a Lua verb, and two of them turned out to have no C++ equivalent. The lesson generalizes: for a C++ plugin, "which verb returns this?" is the wrong traceability question — "which header or schema record exposes this to *the plugin*?" is the right one. Appendix A now carries the Lua/C++ split explicitly so the next field added is checked against both columns.

## Changelog

### v1.3 — 2026-08-01

**Topics in this revision:** both from the Phase 1a gate, recorded in [PR #1](https://github.com/EgeCankaya/n8ro-ai-commander/pull/1).

- **"TSAN clean" is unsatisfiable on the target platform** *(constraint change + review finding)*. No `clang_rt.tsan` runtime ships anywhere in VS 2026 Insiders — only `tsan_interface.h` / `tsan_interface_atomic.h` headers under `VC\Tools\MSVC\14.51.36231\include\sanitizer\`, alongside complete ASan, UBSan, and fuzzer runtimes; neither MSVC nor the bundled LLVM toolchain supports TSan on Windows. As written the PRD permanently failed its own Phase 1a gate. Replaced by the evidence actually produced — ASan 65/65, a 20,000-publish exchange-slot stress test with second-field torn-read detection, and a `static_assert`-enforced value-only worker capture with a deep-copy-outlives-original test — with the residual gap versus a real race detector stated explicitly rather than absorbed.
- **The prompt prefix has a measured size** *(decision input, **not** an OQ resolution)*. 4,738 bytes ≈ 1,200 tokens on a live engine run, logged at startup as `prefixBytes`, against Haiku 4.5's 4,096-token cache minimum. Recorded as evidence into OQ-8, which stays **open** — the padding call is a Phase 2 cost judgement for the owner. The Cost model's arithmetic is recomputed off the measured figure instead of the ~800-token assumption.

**Sections updated:**
- §Header — Status to Draft v1.3; revision-history entry added.
- §Corrections verified in-tree — preamble extended for the gate-found items; **items 8 (TSan absent) and 9 (prefix measured) added**.
- §Success metrics — note added under the table: the measured prefix puts uncached Haiku at ~$1.30/four-ship-hour, above the ≤ $1.10 target; target deliberately left unchanged because meeting it is what OQ-8 asks.
- §Out of scope — **1 row added**: dynamic race detection (TSAN), status *Out of scope*, no target — substituted rather than deferred.
- §FRs — AIC-ARCH-2: acceptance criterion added making the `static_assert` capture check and the deep-copy test load-bearing in the absence of a race detector.
- §FRs — AIC-BE-3: Pain-removed prefix figure corrected to ~1,200 tokens; structure table carries the measured 4,738 bytes; the cache-minimum acceptance criterion notes the real observed value and that the shortfall warning is now *expected to fire* on the default model.
- §Source control and repository → CI split — the concurrency-evidence set placed on the self-hosted runner, with the reason no hosted configuration can substantiate the threading claim.
- §Observability → Logging — `commander.startup` gains `prefixBytes` as the field the measurement was read from.
- §Rollback strategy → Trigger conditions — "ASAN/TSAN report" restated as an ASan report or a stress-test torn read.
- §Risks — Threading row's mitigation rewritten off "TSAN in CI"; **1 row added**: no race detector on the target platform, with what is and is not held enumerated.
- §Open questions — OQ-8 status to *Open — now with a measurement*; rationale carries the measured figure, the smaller ~2,900-token padding delta, the higher uncached baseline, and an explicit statement that the judgement remains the owner's.
- §Cost model — **fully recomputed** against a 1,400-token prompt: assumptions, the four-model table (Haiku $0.00180/order, $1.30/four-ship-hour, ≈ 77 hours), the OQ-8 framing paragraph (padding delta ~2,900 not ~3,300; caching now wins by ~44 % not ~28 %), and the recommendation. The cached-and-padded figures are unchanged, as a padded prefix is 4096 tokens regardless of origin. Prior v1.2 values retained inline for comparison.
- §Validation and test plan — the integration suite's "Run under TSAN" bullet replaced with ASan; **new "Concurrency evidence — in lieu of ThreadSanitizer" block** added (C1/C2/C3 table plus an explicit non-coverage paragraph).
- §Milestones → Phase 1a — "TSAN clean" replaced with the three-part concurrency-evidence item; prefix-measurement item added, marked as *not* resolving OQ-8.
- §Milestones → Phase 2 — OQ-8 gate line notes the measurement half is done and names what remains.
- §Review checklist — 3 items added; the outstanding-OQ line corrected to OQ-1–6 and OQ-8.
- §Appendix B — UAC-AIC-ARCH-2 rewritten off "TSAN reports no race", with a note on why the replacement is weaker; UAC-AIC-BE-3 carries the observed prefix value.
- §Appendix C — **ADR-7 added** (substitute structural and stress evidence for a race detector).
- §Quality gate notes — 2 lessons recorded (a gate must name a tool that exists here; assumptions phrased as measurements).

**Sections explicitly verified no-change:**
- §One-liner · §Purpose and scope · §Source inputs · §Problem statement · §Prior art · §Goals · §Non-goals · §Key hypotheses (H2 concerns prefix *stability*, which the size measurement does not touch) · §Tenets · §Security posture — Trust boundaries / Enforcement model / Exactly what is transmitted / Threat model · §Naming and path conventions · AIC-ARCH-1 · AIC-ARCH-3 · AIC-ARCH-4 · AIC-ORD-1 · AIC-ORD-2 · AIC-VAL-1 · AIC-VAL-2 · AIC-SEC-2 · AIC-API-1 · AIC-API-2 · AIC-BE-1 · AIC-BE-2 · AIC-BE-4 · AIC-DET-1 · AIC-DET-2 · §Scope authority · §Performance requirements · §Cross-service impact · §Configuration and deployment (build/deploy flow, repository layout, ignore rules, inference-server prerequisites) · §Observability — Metrics / Health · §Operational readiness — Runbook / Deployment checklist / Capacity planning / Dependencies · §Rollback steps / Data rollback / Partial rollback · §Alternatives considered · §Rabbit holes · §Milestones Phase 0 / 1b · §Appendix A · Appendix B UACs other than ARCH-2 and BE-3 · ADR-1 through ADR-6

**New OQ entries:** none. Nothing in this revision is deferred: the TSan gap is an availability fact with no decision pending, so it is recorded as an Out-of-Scope row and a standing risk rather than a question awaiting an answer.
**Resolved OQ entries:** none. OQ-8 explicitly **not** resolved — evidence recorded, decision left to the owner at Phase 2 start.
**Out-of-Scope additions:** 1 row — dynamic race detection (ThreadSanitizer), substituted not deferred
**Out-of-Scope closures:** none
**FR changes:** +0 added, ~2 modified (AIC-ARCH-2, AIC-BE-3), −0 removed
**UAC changes:** +0 added, ~2 modified (UAC-AIC-ARCH-2, UAC-AIC-BE-3), −0 removed
**ADR changes:** +1 added (ADR-7)
**Scope guard:** no new FRs, no new config fields, no new Lua functions, no change to the posture vocabulary, ROE values, order schema, or backend set. Lua surface remains 14 functions; config set remains as specified in AIC-API-2.

### v1.2 — 2026-08-01

**Topics in this revision:**
- Sensor tracks and weapon loadout have no public C++ read seam, so two rows of §Exactly what is transmitted and Stage-B check B3 were not implementable. Resolved by Tier-1 ingress (`aiCommander.reportTrack` / `reportLoadout`) per owner decision.
- Four mechanism corrections verified against the shipped headers and generated stubs: `IHttpClient::send()` returns `std::optional<HttpResponse>`; detections arrive as repeating triples; transform velocity/orientation/acceleration are runtime columns on dot-joined paths that fail silently; Stage-B B1 uses `IEntityManager::getEntity`.

**Sections updated:**
- §Header — Status to Draft v1.2; revision history restructured as a list.
- §Source inputs — schema-reference scoped to *authored* fields; added `TransformRuntimeColumns.h` and `ComponentFieldAccess.h`; `send()` return type corrected.
- §Corrections verified in-tree — retitled; added items 4–7.
- §Out of scope — 3 rows added (plugin-side track/loadout reads; roster-scan synthesis, rejected not deferred; free-text track attributes, deferred v1.1).
- §Security posture → Enforcement model — transport failure restated as `std::nullopt`.
- §Security posture → Exactly what is transmitted — `tracks[]` and `own.loadout[]` re-sourced to the ingress verbs; `team`/`kind`/`domain` dropped from the track row; own-ship rows re-sourced to schema leaves, runtime columns, and `IEntity::getTeam()`; rationale paragraph added.
- §Naming and path conventions → Component access — two path forms tabulated with their differing failure modes; roster-synthesis prohibition added.
- §FRs — **AIC-ARCH-4 added** (runtime-column startup probe).
- §FRs — AIC-ORD-2: reporting obligation table + 2 acceptance criteria.
- §FRs — AIC-VAL-1: A1, B1, B3, B4 restated.
- §FRs — AIC-SEC-2: 2 acceptance criteria added (scalar-only track rows; ingress sanitization).
- §FRs — AIC-API-1: `reportTrack` + `reportLoadout` added (12 → 14 functions); reported-list lifecycle defined; 2 acceptance criteria added; `getStats()` gains `runtimeColumnProbe`.
- §FRs — AIC-BE-1, AIC-BE-3, AIC-BE-4: transport-failure mechanism corrected; deterministic reported-list render ordering added.
- §Cross-service impact → Interface changes — new Tier-1 → plugin data-flow direction recorded.
- §Observability — 2 metrics added (`aicmd.tracks.reported`, `aicmd.probe.runtimeColumns`); startup log fields extended.
- §Operational readiness → Runbook — `track` row now checks reporting first; probe-failure row added.
- §Operational readiness → Dependencies — 2 rows added.
- §Risks — 2 rows added (silent zero from a runtime column; Tier-1 reporting not wired).
- §Open questions — **OQ-9 added**.
- §Validation and test plan — new "Unit — Tier-1 ingress" block; B3 cases; render-determinism and scalar-only prompt cases; probe cases; reporting-absent resilience case; `statusCode == 0` phrasing corrected.
- §Milestones → Phase 1a — deliverables and gate extended (14-function namespace, probe, OQ-9).
- §Review checklist — 2 items added.
- §Appendix A — Lua-vs-C++ source split tabulated; `reportTrack` units traced; DIS kind marked not-transmitted.
- §Appendix B — **UAC-AIC-ARCH-4 and UAC-AIC-API-1b added**; UAC-AIC-ORD-2, UAC-AIC-VAL-1, UAC-AIC-SEC-2, UAC-AIC-API-1, UAC-AIC-BE-1, UAC-AIC-BE-4 updated.
- §Appendix C — **ADR-6 added**; ADR-3 consequence added.
- §Quality gate notes — v1.2 lesson recorded.

**Sections explicitly verified no-change:**
- §Purpose and scope · §One-liner · §Problem statement · §Prior art · §Goals · §Success metrics · §Non-goals · §Key hypotheses · §Tenets · §Trust boundaries · §Threat model · AIC-ARCH-1 · AIC-ARCH-2 · AIC-ARCH-3 · AIC-ORD-1 · AIC-VAL-2 · AIC-API-2 (config set unchanged — `commander.maxTracksInPrompt` already bounds the reported lists) · AIC-BE-2 · AIC-DET-1 · AIC-DET-2 · §Scope authority · §Performance requirements · §Configuration and deployment · §Source control and repository · §Inference-server prerequisites · §Health · §Deployment checklist · §Capacity planning · §Rollback strategy · §Alternatives considered · §Rabbit holes · §Cost model · §Milestones Phase 0 / 1b / 2

**New OQ entries:** OQ-9
**Resolved OQ entries:** none
**Out-of-Scope additions:** 3 rows — plugin-side C++ track/loadout reads; roster-scan track synthesis (rejected); free-text track attributes (deferred v1.1)
**Out-of-Scope closures:** none
**FR changes:** +1 added (AIC-ARCH-4), ~8 modified (AIC-ORD-2, AIC-VAL-1, AIC-SEC-2, AIC-API-1, AIC-BE-1, AIC-BE-3, AIC-BE-4, plus §Naming conventions), −0 removed
**UAC changes:** +2 added (UAC-AIC-ARCH-4, UAC-AIC-API-1b), ~6 modified, −0 removed
**Lua surface:** 12 → 14 functions

### v1.1 — 2026-08-01

Added §Source control and repository (standalone repo, layout, ignore rules, the single `open-solution.cmd` relocation edit, CI split, visibility); recorded the owner's decision that plugin source files carry no Arkheon per-file header; added the `prompt.doctrinePath` config field.

### v1.0 — 2026-07-31

Initial Comprehensive-tier draft.
