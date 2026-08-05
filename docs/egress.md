# What leaves this machine when the Claude backend is on

**Audience:** anyone who needs to answer "what did you send to a third party, and how do you know?"
**Scope:** `commander.backend = claude` with `claude.enabled = true`. In every other configuration
this document is moot — see §Nothing leaves at all, below.
**Status:** written 2026-08-03 against the adapter as implemented (PRD v1.8, AIC-BE-2); **revised
2026-08-05 for the fifth egress grant (PRD v1.8.11), which changes the *provenance* of the volatile
suffix from synthetic fixtures to real scenario state — see §Where those values come from.** Every
claim here is either enforced by a test named below or is a statement about code you can read.

---

## The short answer

One HTTPS POST per order, to `api.anthropic.com`, carrying:

1. a fixed block of text that contains **no scenario data at all** — the system prompt, the posture
   and ROE vocabulary, the order JSON Schema, and a hand-written page of generic tactical doctrine;
2. a small per-order block describing **one aircraft's tactical situation** — where it is, how fast
   it is going, which way it is pointing, whose side it is on, what its own sensors reported, and
   what is on its rails;
3. the API key, on a request header.

Nothing else. No file paths, no scenario or mission names, no entity profile names, no terrain or
database contents, no doctrine corpus, no licence data, and no free-text field sourced from an
external track feed.

---

## Nothing leaves at all unless two independent switches are both on

There is no configuration in which scenario data leaves the machine by accident.

| Switch | Default | What it does |
|---|---|---|
| `commander.enabled` | `false` | The master switch. With it false the plugin loads, registers its Lua namespace, and issues no orders. |
| `claude.enabled` | `false` | The **independent** egress gate. |

Setting `commander.backend = "claude"` while `claude.enabled` is `false` is **rejected as a
configuration error** — the commander stays disabled and logs why. It does **not** quietly fall back
to the local backend. That was a deliberate choice: a silent fallback would teach operators that the
authorization switch does not matter, and the next person to leave it off would be relying on a
habit rather than on a mechanism.

`claude.baseUrl` must begin with `https://`. A plain-`http` value is rejected at configuration time,
not at request time.

*Enforced by:* `ConfigRejectsClaudeBackendWithoutItsGate`, `ConfigRejectsPlainHttpClaudeBaseUrl`,
`ConfigDefaultsAreFailClosed`.

**When it is on, the log says so, at WARNING, on every run**, naming the destination host and the
categories of data involved. An authorized egress that leaves no trace in the log is
indistinguishable from an unauthorized one, so it leaves a trace.

---

## Exactly what is in the request body

### Part 1 — the stable prefix. No scenario data.

Byte-identical for the life of a run. It contains:

- the system prompt (the model's role, its constraints, and its instruction to refuse rather than
  guess),
- the posture and ROE vocabulary, in plain language,
- the order JSON Schema,
- the doctrine block: **1–2 pages of hand-written generic tactical doctrine**, held in
  `data/doctrine.txt`. It is generic by construction — it names no scenario, no platform, no place,
  and no unit. It is a file a human wrote and a human can read.

The `data/ai/context/` corpus — the large HAVA/DENIZ/KARA doctrine set the tree ships — is **not**
here and is not sent. Retrieval over it is explicitly out of scope.

### Part 2 — the volatile suffix. This is the scenario data.

One block per order, describing **one commanded aircraft**. This list is exhaustive:

| Field | Meaning |
|---|---|
| `simTimeS` | simulation clock, seconds |
| `own.entityId` | the aircraft's scenario-local runtime id string |
| `own.latitudeDeg`, `own.longitudeDeg`, `own.altitudeHaeM` | where it is |
| `own.velN`, `own.velE`, `own.velD` | how fast and which way, m/s, north-east-down |
| `own.headingDeg` | which way it is pointing, degrees from true north |
| `own.team` | whose side it is on |
| `own.loadout[]` | per hardpoint: name, weapon profile name, rounds remaining, rounds capacity |
| `tracks[]` | per contact its **own sensors reported**: target id, range in metres, signal-to-noise in dB |
| `situationNote` | ≤ 256 characters of free text set by the mission script, charset-filtered |

**On `tracks[]` and `loadout[]`:** the plugin cannot read either of these from the engine — no public
C++ read seam exists for sensor tracks or weapon stores. They are **pushed in** by the entity's own
deterministic mission script. The consequence is that the plugin can only transmit what a script
explicitly handed it, and the reporting verbs accept **numbers only**, so no free-text field can
enter a prompt through that path at all.

#### Where those values come from — and what changed on 2026-08-05

**The field list above is exhaustive and has not changed.** What changed is where the values in it
are drawn from, and that distinction is the one every egress grant in this project has turned on.

| | Provenance | Authorized by |
|---|---|---|
| **Until 2026-08-05** | The **six hand-authored situation fixtures** in `tests/live/LiveMain.cpp`. Same field set, values invented for a test — no scenario, no mission file, no shipped content | grants two through four (PRD v1.8.3, v1.8.5, v1.8.8) |
| **From 2026-08-05** | **Real scenario state**, read live from `RedSu35_01` in the shipped "Mariana Shield" scenario, via `tests\smoke\run-live-scenario.ps1` on the hosted backend | the **fifth grant** (PRD v1.8.11) |

**This is stated here because "it is the same fields" was, for four grants running, explicitly not
enough.** The v1.8.3 grant refused to read a fixture authorization as covering a scenario run on
exactly that reasoning. The boundary was released by an owner decision, recorded in the PRD before
any request was made under it — not by the argument finally being accepted.

**What a reader should take from it:** a position that leaves this machine after 2026-08-05 may be
the position of an aircraft in a shipped scenario rather than a number someone typed into a test.
The *shape* of what is transmitted is unchanged, and every assertion and allowlist test named in
this document still applies unchanged. Nothing new is transmitted. What is new is that the values
are real.

### Part 3 — the headers

`x-api-key` (the credential), `anthropic-version`, `content-type`.

---

## What is deliberately excluded, and why

| Not sent | Why it matters |
|---|---|
| `componentTrackIdentity` free text — `trackSource`, `callsign`, `originCountry` | These are documented as ingested from an external feed such as live ADS-B. They are **attacker-influenced text**. Excluding them entirely removes the injection channel rather than filtering it. |
| File paths, scenario names, mission names, entity **profile** names | They describe the installation and the exercise, not the tactical situation, and the model does not need them. |
| Terrain database, AI database, the `data/ai/context/` corpus | Not needed for a posture decision. |
| Entitlement or licence data | Never touched by this plugin. |
| Any configuration value except the model name and cadence | The rest describes this machine, not the situation. |

*Enforced by:* the transmitted-field allowlist test, which renders a prompt with unique sentinel
values planted in every excluded field and asserts none of them appears in the rendered output. It
tests for the sentinels' **absence**, so a field added to the renderer without being added to the
allowlist fails the test rather than slipping through.

---

## The API key

**The key is never stored anywhere.**

- Configuration holds the **name** of an environment variable (`claude.apiKeyEnvVar`, default
  `ANTHROPIC_API_KEY`). There is no configuration field that can hold a key value, so there is
  nothing on disk to leak and nothing to redact from the config editor.
- At request time the adapter reads that variable into a **local variable**, uses it, and lets it
  die with the call. It is never a member of the adapter object. That distinction is the point: a
  member could be picked up and formatted into a diagnostic message by some future edit, and there
  is no edit that can make a local outlive the function that read it.
- It goes on the `x-api-key` header and nowhere else — not the body, not a URL, not a log line, not
  an order record.

*Enforced by:* `ClaudeApiKeyAppearsOnlyInTheAuthHeader`, which puts a sentinel key in the
environment, makes a request, and asserts the sentinel is present on the header and **absent** from
the request body, the bearer-token field, the transport detail, and the recorded response body.
`ClaudeMissingApiKeyIsANamedConfigurationError` asserts that with no key set, **no HTTP request is
attempted at all**.

---

## The one plain-HTTP request, and what it does not carry

If an HTTPS request returns no response whatsoever, the adapter makes **one** plain-`http` GET to the
same host. This exists to tell two very different failures apart: a missing TLS runtime on this
machine, versus the network being down. Without the comparison they look identical, and an operator
would be sent to reinstall a runtime that was never the problem.

That request carries **no prompt, no key, and no body** — it is a bare GET. What it discloses is that
this host is being contacted, which DNS disclosed already. What it must never disclose is scenario
state, and carrying nothing is the structural guarantee of that rather than a promise.

It fires only after a failure, at most once per failed request.

*Enforced by:* `ClaudeTlsUnavailableIsDiagnosedDistinctly` asserts the control request's body is
empty and carries no key, and that the URL is plain `http`.
`ClaudeNetworkOutageIsNotReportedAsATlsProblem` asserts that when neither protocol answers, the
result is **not** a TLS claim.

---

## What comes back, and what is kept

The response is one JSON object. The order document is extracted from it and validated hard before
anything acts on it — the model's output reaches no engine verb without passing the full pipeline,
regardless of what it says.

Written to the order log (`logs/ai-commander/orders.jsonl`) on this machine:

- the order, if it was accepted;
- the reject reason and enough of the raw body to diagnose it, if it was not;
- input, output, and cache-read token counts, for cost accounting;
- stable hashes of the snapshot and the prompt — **hashes, not the prompts themselves**, so drift
  between runs is detectable without writing the scenario state into the log a second time.

The order log contains **no API key** and **no `componentTrackIdentity` field**. It is local, and it
inherits the release tree's classification like any other run artifact.

---

## How to check any of this yourself

| Question | Where to look |
|---|---|
| Is egress on right now? | First ten lines of the run log. If it is on there is a WARNING naming the host. |
| What exactly was sent? | `src/PromptRenderer.cpp` builds every prompt; nothing else can. `src/ClaudeLlmClient.cpp::buildRequestBody` builds every request body. |
| Is the excluded list actually excluded? | `tests/PromptRendererTests.cpp` — the sentinel allowlist test. |
| Can the key leak? | `tests/ClaudeAdapterTests.cpp::ClaudeApiKeyAppearsOnlyInTheAuthHeader`. |
| What did a given run send? | `logs/ai-commander/orders.jsonl` — one line per lifecycle event, with prompt hashes for correlation. |

Run `tests\bin\release\ai-commander-tests.exe` to execute all of the above. It needs no network, no
API key, and no inference server.
