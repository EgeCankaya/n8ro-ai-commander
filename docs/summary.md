<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# N8RO AI Entity Commander — summary

**PRD version:** v1.8.30
**Date:** 2026-08-07
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

**All four are fixed in v1.8.30**, specified in the PRD first and then implemented: AIC-VAL-2's ladder
is re-specified so every rung fights, AIC-ORD-2 gains the reference script's engagement obligations,
and the script is now the shipped script's behaviour **plus** an order-override layer rather than a
parallel implementation of part of it. **The fix is not yet confirmed by a run** — that is C22.

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
| Order acceptance rate | **MET (fixtures)** · **79.8 % [71, 87] in-engine, not gated** — two instruments, one bar (**C17**, owner-decided) |
| Parse/schema rejection rate | **MET** — 0.00 % over 776 orders, two backends, three models |
| Plugin cost per frame | **MET** |
| Replay reproducibility | **MET** |
| Order round-trip latency | **MET** on the shipped local default; **MISSED** on the hosted path, informational and not control-loop-binding |
| **Engagement outcome** | **Reported, not barred.** New in v1.8.30 — the row that did not exist while the system was doing harm |

*Source: §Success metrics.*

## What it cost

**≈$2.57 of a $5 budget**, six egress grants, and roughly thirty archived runs. **Every finding in
v1.8.30 was free**: no network, no grant, no engine run — re-analysis of archived logs plus eight
controlled requests to a local inference server carrying no scenario state. *Source: §Cost model,
§Corrections item 46.*

## What we recommend, in order

| # | Action | Cost | Needs |
|---|---|---|---|
| 1 | **Run C22's arm** — the fixed script with the commander off. Until it passes, the commanded arm measures nothing about the model | ~35 min | No grant, no model, no network |
| 2 | Re-measure the outcome comparison over the fixed script, and fold the result into §Success metrics' new row | ~1 h | The same runs |
| 3 | Decide the standing hosted-egress authorization — **and revise `docs/egress.md` and AIC-SEC-2 first**, because v1.8.30 changed what leaves the machine | minutes | The owner |
| 4 | Re-run the domain review of posture appropriateness against a Tier 1 that fights (H1's marks were taken against one that did not) | ~2 h | A domain reviewer |

## What is still open

| # | Item |
|---|---|
| **C21** | The reference script's engagement gap — **specified and implemented in v1.8.30, open until a run confirms it** |
| **C22** | The script-only arm has never been run. The harness gained it in v1.8.30 |
| **C17** | Acceptance is measured by two instruments and only one is gated. **Owner-decided 2026-08-06: the bar stays on the fixtures.** The row stays open because the decision does not make the instruments agree |
| **C8** | A `maxTokens` ceiling sized from one 48-order run. Binds only if a non-Haiku model is adopted |
| **C5** | A bounded negative at n = 11; changes no conclusion |
| **C6** | Depends on a roadmap answer this project cannot produce |

Two governance boxes: **repository visibility is confirmed private** (checked 2026-08-07), and there
is **still no standing authorization for hosted egress outside a measurement grant** — so
`commander.backend = claude` cannot ship to anyone. That is the right posture for a project still
measuring itself, and it is a decision someone has to make. *Source: §Review checklist.*

---

## The one sentence an owner should take away

The project built a careful, well-instrumented pipeline and **never once measured whether it helps**;
when it finally did, the answer was no — **and the reason was in its own deterministic tier**, which
means no run to date has given the language model a fair test. That defect is now fixed and
specified. **Whether the fix works is one 35-minute run away, and it needs no money and no
authorization.**
