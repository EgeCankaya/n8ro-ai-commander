<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# N8RO AI Entity Commander — summary

**PRD version:** v1.8.36
**Date:** 2026-08-08
**Audience:** the platform owner, and whoever picks up the next block of work.

> **This document carries no number that is not also in `docs/prd.md`.** Every figure below cites the
> section that owns it, so there is exactly one place a figure can be wrong. It is **regenerated at
> each phase close, not maintained continuously** — a summary that drifts is worse than none, and
> this project has twice demonstrated that continuously-maintained duplicate figures drift.
> `tools/lint-prd.ps1` fails when the version stamp above does not match the PRD's, so a stale
> summary fails the gate rather than being discovered by a reader.

---

## What we set out to test

Whether a language model can usefully issue tactical **intent** — posture, target, waypoint, rules of
engagement — to an entity in a running scenario, while every kinematic decision stays in the
deterministic C++ and Lua tiers that already exist. Everything reactive — missile defeat, merge
manoeuvring, launch timing — stays in Tier 0/1. The model is confined to a closed JSON order schema
and never produces raw kinematics.

*Source: §Purpose and scope, §Goals, ADR-1.*

## What we found

**The headline, first, because it is the finding an owner needs and it was reachable from logs that
had been on disk since Phase 1b.** Deploying the commander made the commanded aircraft **shoot 87.5 %
less, hit nothing, and die** — over twelve paired runs, **4 launches against the shipped script's 32,
0 kills against 0, and 22 of 24 commanded aircraft destroyed against 0 of 24.**

**The cause is this project's own deterministic tier, not the language model.** Four mechanisms, all
readable off the source, and an isolating arm with no model in the loop at all reproduces the result:

| # | Mechanism |
|---|---|
| 1 | The reference script returned before any fire logic whenever no order was in force |
| 2 | The launch path was reachable from **one of six postures**, and that posture is **8.2 % of every order ever accepted** |
| 3 | **All three rungs of the fallback ladder were non-fighting**, rung 2's standing order being `hold` by specification |
| 4 | **No automatic defensive reflex at all** — the shipped script's first action every tick is a munition-defeat check inside 15 km; the commander path is a 20 s cadence plus a p50 of 3.5 s, against an inbound missile that crosses 15 km in about fifteen seconds |

*Source: §Corrections items 45 and 46(a); C21.*

**All four were fixed in v1.8.30 and the fix is confirmed in v1.8.31.** The arm that had never
existed — the **fixed** script run with the commander off and no model, which is the only comparison
in the project that changes exactly one thing — goes from **0 launches and 2 of 2 aircraft lost** to
**3 launches, 2 detonations, 0 of 2 lost**, ending the run with both jets fighting under no order at
all. **C21 and C22 close on that mechanism, and explicitly not on a rate:** one run does not establish
what the defect was worth. *Source: §Corrections item 47(a).*

**Two results from that run that should travel with it.** The commanded arm and the script-only arm
were **identical on every column** — the first unconfounded commander comparison this project made,
and a null at n = 1. And the fix immediately revealed a second defect it had been masking: an
aircraft that stops flying, after which every order is rejected for copying its own stopped speed.
Carried as **C23**. *Source: §Corrections item 47(b), 47(d).*

***(v1.8.34 — a second complete run changes both of those, and adds a first.)***

| run | build | ON | SCRIPT-ONLY | OFF |
|---|---|---|---|---|
| `095026` | old | 3 / 2 / 0 / 0 | 3 / 2 / 0 / 0 | 2 / 1 / 0 / 0 |
| `135722` | old | 3 / 2 / 0 / 0 | **2 / 2 / 1 / 0** | 4 / 2 / 0 / 0 |
| `185750` | new | 3 / 2 / 0 / 0 | **2 / 2 / 1 / 0** | 4 / 2 / 0 / 0 |

**The identity does not replicate, and is refuted rather than merely unsupported** — *"identical on
every column"* is a universal claim and one counter-instance disproves it. **Nothing about what the
commander is worth follows**, and the obvious reading of the repeat is too strong: **the script-only
arm carries no model and is near-deterministic** — its two kills are byte-identical — so those two
runs are nearer one observation than two, and **`095026` is the outlier that wants explaining.** The
commanded arm was 3 / 2 / 0 / 0 in all three runs; **the variance sits in the arm without the model.**

**Those kills are the only two in eighteen archived runs**, both in the arm with no model in the loop.
**The qualification belongs with the number:** two SAM hits had already left the target `wrecked`
before the Su-35's missile finished it. A kill by the engine's definition; not an unaided one.

**And C23's mechanism was misread twice — the second time by this project's own analysis.** **All 19
archived `hold` orders were issued at 0.0 m from the aircraft's own position:** the model echoes own
position back as the waypoint, so **there is no "arrival"** — the aircraft is told to hold where it
already is, has nowhere to fly, and coasts 320 → ~128 → 1.5 m/s. **And the stall survives a full
release to Tier 1** — twelve archived samples sit below the floor with **no order in force at all**,
while the reference script calls `resumeWaypointFollowing`. So the fallback ladder is **not** what
makes it permanent, which is what v1.8.34 claimed. *Source: §Corrections items 50, 51.*

**A second finding, independent of the first.** The largest rejection class in the archive
(`reject.shape`, **16 of 55, 29.1 %**) was diagnosed by a controlled probe of the local decoder:
**numeric bounds are not enforced by the constrained decoder at all**, confirmed on four models across
three families, and the hosted schema projection cannot express *"greater than zero"* either. The rule
was enforced by nothing anywhere except the validator rejecting the whole order. It is now **repaired
rather than rejected**. *Source: §Corrections item 46(b); C14, closed.*

**A third.** **A quarter of all rejections (14 of 55) were a discrimination the prompt structurally
denied the model** — the track row carried an id, a range and a signal strength while the doctrine
forbade inferring anything from the id. The validator's own check order proves Tier 1 handed the model
its own side's SAM radar. The track report now carries `kind` and `team` as closed vocabularies, and
own-team contacts are filtered in Tier 1. *Source: §Corrections item 46(c).*

**And one positive result that had gone unreported for the project's whole life.** The **shipped
default backend meets its latency target comfortably** — p95 **7,975 ms** against ≤ 20 s, 60 % of
headroom. Only the hosted entry of that row had ever been measured, and only the hosted entry misses.
*Source: §Success metrics; §Corrections item 46(d).*

### Where the metrics stand

| Metric | Verdict |
|---|---|
| Cost per four-ship scenario-hour | **MET** — $1.05 against ≤ $1.10 |
| **Fixture** acceptance rate | **MET** — 100 % on the synthetic soaks. *(v1.8.36 — the row is named for its instrument now, so it cannot be read as the engine's)* |
| **In-engine** acceptance rate | **Not gated, by owner decision.** **64.8 % [59.3, 70.0]** over 324 resolved orders — and **82.0 % [76.8, 86.5]** once the two non-model-failure classes are removed. **More than half the shortfall is C23** |
| Parse/schema rejection rate | **MET** — 0.00 % over 776 orders, two backends, three models |
| Plugin cost per frame | **MET** |
| Replay reproducibility | **MET** |
| Order round-trip latency | **MET** on the shipped local default (p95 7,975 ms against ≤ 20 s); **MISSED** on the hosted path (240-order soak p95 4,615 ms against ≤ 2.5 s), informational and not control-loop-binding |
| **Engagement outcome** | **Reported, not barred.** New in v1.8.30 — the row that did not exist while the system was doing harm |

*Source: §Success metrics.*

<!-- in-engine-acceptance: 64.8 [59.3, 70.0] n=324 runs=23 -->

***(v1.8.36 — the in-engine figure now has an owner and a pin.)*** It had gone stale **four times**,
always the same way: a run was archived, nobody recomputed, and the old number was requoted until a
reader caught it. **`tools/acceptance-report.py` regenerates it** — refusing to print until its
interval code reproduces the PRD's own published 50/58 → [74.6 %, 93.9 %] — and
**`tools/lint-prd.ps1` fails the build if this document, the PRD and the README disagree.** Same
mechanism as the version stamp above, which has never drifted. *Source: §Corrections item 52(c).*

## What it cost

**≈$2.57 of a $5 budget**, six egress grants, and roughly thirty archived runs. **Every finding in
v1.8.30 was free**: no network, no grant, no engine run — re-analysis of archived logs plus eight
controlled requests to a local inference server carrying no scenario state. *Source: §Cost model,
§Corrections item 46.*

## What we recommend, in order

| # | Action | Cost | Needs |
|---|---|---|---|
| 1 | **Decide C23's layer** — an uncommanded aircraft that runs out of route stops flying, and it took 21 of 21 rejections in the confirming run. Script, doctrine or snapshot; the choice is a design decision | ~half a day | No grant, no model |
| 2 | **Repeat the three-arm run** — everything from 2026-08-08 is n = 1, including the neutral commander result | ~35 min each | No grant, no model, no network |
| 3 | Re-run the domain review of posture appropriateness against a Tier 1 that fights (H1's marks were taken against one that did not) | ~2 h | A domain reviewer |
| 4 | Decide the standing hosted-egress authorization. `docs/egress.md` is accurate again as of v1.8.31, having been stale on three counts | minutes | The owner |

## What is still open — and what is dormant, which is not the same thing

***(v1.8.33 — this list said five for several revisions and the honest answer is two.)*** The
register recorded only **open** and **closed**, so a row nobody should touch and a live question
looked identical. It now records **three** states, and a **DEFERRED** row must carry a revisit
condition a reader can test without running anything. *Source: §Carried out of Phase 3;
§Corrections item 49.*

**OPEN — two rows, and both are decisions rather than engineering.**

| # | Item |
|---|---|
| **C23** | **The model orders every `hold` at the aircraft's own position; the aircraft stops there**, after which every order is rejected for copying its own stopped speed. Not a new defect — the aircraft used to die before it could sit in it. *(v1.8.35 — **19 of 19** holds ordered at 0.0 m, so there is no "arrival"; and **the stall survives a full release to Tier 1**, so the fallback ladder is not what sustains it. **The question now has two ends and they are different layers** — the onset, and the failure to recover. **The counter-instance cannot be got by running more scenarios; it must be injected.** The layer is still the owner's to pick)* |
| **C17** | Acceptance is measured by two instruments and only one is gated. **Owner-decided 2026-08-06: the bar stays on the fixtures.** The row stays open because the decision does not make the instruments agree |

**CLOSED AND CANCELLED v1.8.36 — two owner decisions, and the register is down to two live rows.**

- **C8 CLOSES.** `claude-haiku-4-5` is and will remain the default, so `claude.maxTokens = 512` is
  simply correct and the row's condition does not arise. **Closed on the decision, not on the
  measurement** — 673 is still a *sample* maximum that says nothing about any other model. The
  revisit condition is **kept as a tripwire**: a non-Haiku default needs re-measurement over ≥ 2
  runs and must never be scaled from another model's headroom.
- **C6 is CANCELLED — not closed, and not answered.** Whether `n8ro-llm` is ever installed is
  someone else's roadmap answer, **nothing here is blocked on it**, and the row implied a debt that
  never existed. **The design decision is untouched:** §Out of scope, §Alternatives Option 3 and
  ADR-2's seam already say what to do if it ever lands. Two checklist boxes now stand unticked and
  **both are governance**.

**CLOSED earlier — C5**, as a bounded negative at n = 11, on the mechanism and explicitly not
on a rate. The Perth substitution is the standing argument for Stage B's geofence — the bound is
what caught it — and 11 waypoint-carrying orders support no rate at all. **It carries one rule
forward: a bounded negative at n = 11 must never lead a summary of this project**, which is what the
pre-v1.8.32 README did while C21 appeared nowhere in it. *Source: §Corrections items 48(e), 49(b).*

**Two checklist boxes are unticked and both are governance** *(v1.8.36 — it was three; OQ-3's was
cancelled rather than answered)*: repository visibility is confirmed private (checked 2026-08-07),
and there is **still no standing authorization for hosted egress outside a measurement grant** — so
`commander.backend = claude` cannot ship to anyone, which is the right posture for a project still
measuring itself and is a decision someone has to make. *Source: §Review checklist.*

---

## The one sentence an owner should take away

The project built a careful, well-instrumented pipeline and **never once measured whether it helps**;
when it finally did, the answer was no — **and the reason was in its own deterministic tier**, not in
the language model. **That defect is now fixed, and the arm built to test it confirms the fix.** What
the commander itself is worth remains genuinely unmeasured: the first clean comparison ever run
returned a null at n = 1, and the fix immediately exposed a second defect it had been hiding.
