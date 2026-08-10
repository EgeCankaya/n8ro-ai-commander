<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# Open-issues review — N8RO AI Entity Commander

**Audience:** the platform owner, and whoever picks up the next block of work.
**Scope:** the ten open questions carried at PRD v1.8.29, branch `close-c15-c16-file-c17`
(6 commits ahead of `main` at `432ba73`, unpushed).
**Date:** 2026-08-07.
**Status:** a review, not a specification. **Nothing here changes a functional requirement.**
Every recommendation that touches schema, a validator check, a config surface, the record format,
or a transmitted field is written as a recommendation *to be specified in `docs/prd.md` first*,
per §Scope authority. Where this document contradicts the PRD, it says so and shows the working;
the PRD is a source here, not an authority.

**Cost of everything in this document: $0.** No hosted request was made, no egress grant was used
or is requested, no engine run was performed. The new measurements are (a) re-analysis of archived
logs and (b) eight controlled requests to a local inference server that carry no scenario state.

**Gate state at the time of writing**, re-run rather than quoted:

| Gate | Result |
|---|---|
| `ai-commander.vcxproj` + `tests/ai-commander-tests.vcxproj`, `Release\|x64` | build clean |
| `tests\bin\release\ai-commander-tests.exe` | **144 / 144 passed, 0 failed** (1.70 s) |
| `tools\lint-prd.ps1` | **9 checks, 0 errors, 0 warnings** — PASS |
| `tools\check-artifacts.ps1` | 98 tracked files, 9 rules — PASS |

---

## Executive summary — what we set out to test, what we found, what it cost

*(§10 asks whether a two-page summary should exist. This is it. It is deliberately written so it
can be lifted out of this file whole.)*

**What we set out to test.** Whether a language model can usefully issue tactical *intent* —
posture, target, waypoint, rules of engagement — to an entity in a running scenario, while every
kinematic decision stays in the deterministic C++ and Lua tiers. Six success metrics were defined:
frame cost, latency, order acceptance, schema-rejection rate, replay reproducibility, and cost per
scenario-hour.

**What we found.** Five of the six metrics are met or explained. The sixth — latency — misses by
a factor of three and has a measured cause. **None of the six asks whether the commander helps**,
and when that question was finally asked, on 2026-08-07, from logs that had been on disk since
Phase 1b, the answer was that deploying the commander makes the commanded aircraft shoot 87 %
less, hit nothing, and die — 22 of 24 aircraft lost across twelve paired runs against 0 of 24 for
the control.

**The cause is not the language model, and this review sharpens that further.** The PRD (item 45)
attributes the deficit to one line of the reference Tier-1 script: `considerFiring` sits only on
the posture path, so an aircraft with no order in force never reaches its fire logic. That is
true, and it is **roughly a third of the problem**. This review establishes three further
mechanisms, all readable off the code and the archived records:

1. **`considerFiring` is reachable from exactly one of the six postures — `engage`.** Across every
   archived run, `engage` is **13 of 158 accepted orders (8.2 %)**. So the aircraft cannot shoot
   during 92 % of the time it *is* under a valid order, not merely during the gaps.
2. **All three rungs of the AIC-VAL-2 fallback ladder are non-fighting.** Rung 1 retains the last
   order — 63 % likely to be `hold`. Rung 2 publishes a standing order that is `hold` *by
   specification*. Rung 3 resumes waypoints. There is no rung on which the aircraft fights.
3. **The commander-aware script has no automatic defensive reflex at all.** The shipped script's
   *first* action every tick is a missile-defeat check against inbound munitions inside 15 km. The
   commander-aware script flies `defend` only when the model orders it, on a 20-second cadence
   with a measured p50 of 3.5 s and p95 of 9.5 s. **An inbound AMRAAM crosses 15 km in about 15
   seconds.** This, not the shooting deficit, is the most plausible loss mechanism — and it is
   currently unseparated from it.

**A second finding, independent of the first, and new to this review.** The largest remaining
rejection class (`reject.shape`, 29 % of all archived rejections) was diagnosed by a controlled
probe of the local decoder. **Numeric `minimum` / `maximum` are not enforced by the local
constrained decoder at all** — confirmed on four models across three families, and separately
confirmed to be inexpressible on the hosted path too, because the projection strips those keywords
and only a zero-width range survives as `const`. The rule "a `hold` order requires
`orbitRadiusM > 0`" is therefore **enforced by nothing anywhere except Stage A**, which rejects
the whole order. It is the only rule of that shape in the schema, and it accounts for every
`reject.shape` in the archive. An explicit prompt instruction does not fix it on four of five
models tested. §4 recommends repairing the field rather than rejecting the order, on C16's
existing precedent, using a default the config already defines.

**A third finding.** A quarter of all rejections (14 of 55) are the model failing a
discrimination the prompt structurally denies it. The track list carries `{id, rangeM, snrDb}` and
nothing else; the doctrine explicitly forbids inferring from the id. The model is then rejected
for engaging inbound munitions (4×), naming munitions that have already detonated (5×), and
engaging its own side's SAM radar (5×) — a contact **Tier 1 reported to it**, having had the team
information in hand and discarded it. §Out of scope's deferred row on track attributes carries the
revisit condition *"if order quality shows the model cannot discriminate targets without them"*.
That condition is met.

**What it cost.** ≈$2.57 of a $5 budget, six egress grants, and roughly thirty archived runs. No
grant is requested by this review.

**What we recommend, in order.**

| # | Action | Cost | Needs |
|---|---|---|---|
| 1 | Re-specify AIC-VAL-2's ladder so every rung fights, and fix the reference script | ~1 day | PRD revision, then code. No grant, no model |
| 2 | Add an outcome row to §Success metrics on "report, don't bar" footing | ~2 h | PRD revision, then harness |
| 3 | Repair `hold`'s orbit radius in Stage A instead of rejecting it | ~3 h | PRD revision, then code |
| 4 | Widen the track report with `kind` and `team` (the deferred row) | ~1 day | PRD revision, then code |
| 5 | Two owner decisions: standing hosted-egress authorization, and repo visibility | minutes | The owner |

**The one sentence an owner should take away.** The project has built a careful, well-instrumented
pipeline and has never once measured whether it helps; when it finally did, the answer was no —
and the reason is in this project's own deterministic tier, which means **no run to date has given
the language model a fair test, and the acceptance, quality and latency questions downstream are
all measured against a crippled Tier 1.**

---

## Method, and what this review did that the PRD did not

Four things were done here that had not been done before, all free and all local:

1. **The outcome analyser was re-run over the whole archive**, not the eleven pairs item 45
   quotes. There are now **twelve** post-C12 pairs; run `20260807T133621Z` was archived after item
   45 was written. The direction is unchanged and one number moves (§1).
2. **Every archived `orders.jsonl` was parsed** — twenty run directories, 250 requested orders, 213
   resolved. The PRD quotes posture *counts* in places but has never published the posture
   *distribution*, and it is the single most explanatory number in the archive (§1, §4).
3. **Every rejection in the archive was classified by reason and by detail string** — 55 of them.
   The PRD reasons about rejection classes run by run; pooled, two structural classes account for
   55 % of everything (§4, §7).
4. **A controlled decoder probe** was run against the local inference server: eight requests, each
   carrying a schema whose bound forbids the value the prompt asks for. This is a mechanism test,
   not a quality test — it asks what the runtime enforces, not what a model prefers. It carries no
   scenario state (§4).

**On the evidence rule.** §Scope authority's fourth rule — *a single run may open a question, it
may not close one, unless the question is a mechanism readable directly off recorded values* — is
observed throughout, and where this review asserts a mechanism it says which category the claim is
in. Item 42(c) is the cautionary precedent: it read a cluster of copied floats as the model
*selecting among fields*, which was a mechanism inferred from a distribution, and it was refuted
two revisions later by the first run that actually recorded the snapshot. **Two claims in this
document are distributional and are labelled as such** — the posture distribution in §1 and the
`RedSu35_02` address in §4 — and neither is used to carry a mechanism.

---

## 1. C21 — the reference Tier-1 script stops fighting. The fallback is about a third of it.

### What is established

**The outcome deficit, re-measured over the full archive.** `tools/analyse-outcomes.py` over
`C:\Users\I2fas\Documents\N8RO AI Commander logs`, restricted to post-C12 builds where the fire
path demonstrably works:

| | commander-ON | commander-OFF (shipped script) |
|---|---|---|
| paired runs | **12** | 12 |
| launches by the commanded aircraft | **4** | **32** |
| detonations from those launches | **0** | **16** |
| kills | **0** | **0** |
| commanded aircraft destroyed | **22** of 24 | **0** of 24 |

Item 45's table reads 11 pairs / 30 / 15 / 0 / 20-of-22. **Every direction holds and one figure is
now stale**: run `20260807T133621Z` was archived after the item was written and contributes
0 / 0 / 0 / 2 on and 2 / 1 / 0 / 0 off. The 87 % launch-suppression figure becomes 87.5 %. Nothing
turns on this; it is recorded because item 45's table will otherwise be quoted as current.

**`considerFiring` is unreachable from the fallback.** `lua/ai_commander_interceptor.lua:338-341`
— `fallBackToWaypoints` returns before any fire logic. Established, and correctly stated by the
PRD.

**No `RedSu35` munition has destroyed anything in any archived run, either arm.** Confirmed. The
control arm's sixteen detonations produced zero kills. The control is not winning; it is surviving
by not being anywhere dangerous.

### What the PRD assumes, and what the evidence actually shows

Item 45(a) presents the fallback as *the* mechanism. **It is one of four, and probably not the
largest.** All four are readable off the code, which puts them in the "mechanism, not rate"
category — with one labelled exception.

**(a) `considerFiring` is reachable from exactly one posture out of six.** In `onTick`, the call
appears once, in the `engage` branch (`ai_commander_interceptor.lua:387`). It is absent from
`crank`, `hold`, `ingress`, `defend`, and the unknown-posture arm. `rtb` deliberately ceases fire,
which is correct. So a commanded aircraft flying a *valid, accepted, in-force* order still cannot
shoot unless that order is `engage`.

The `crank` omission is the sharpest of these. `crank` exists precisely to support a shot already
in the air — the doctrine says so, and the shipped script cranks *between* launches inside its own
assessment window. In the commander-aware script, an `engage` → `crank` sequence means the
aircraft takes one shot and then stops shooting.

**(b) The posture distribution, which is a rate and is labelled as one.** Across all twenty
archived order logs, 158 accepted orders:

| posture | all runs | share | post-C12 | share |
|---|---|---|---|---|
| `hold` | 100 | **63.3 %** | 58 | 56.9 % |
| `defend` | 26 | 16.5 % | 23 | 22.5 % |
| `ingress` | 15 | 9.5 % | 11 | 10.8 % |
| **`engage`** | **13** | **8.2 %** | **6** | **5.9 %** |
| `crank` | 4 | 2.5 % | 4 | 3.9 % |

**This is a distribution and it does not establish a mechanism.** What it does establish is the
*scale* of the mechanism in (a): the one posture that can fire is the second-rarest thing the
model orders. Combined with (a), which is structural, the joint statement is safe: **the fire path
is reachable from 8.2 % of the orders this system has ever accepted.**

And the reason it is rare is not mysterious. `data/doctrine.txt` says, in the POSTURE SELECTION
block: *"Hold is the correct answer far more often than it feels."* The doctrine steers toward
`hold`; the script cannot fight in `hold`. **Nobody wrote those two sentences in the same room.**

**(c) All three rungs of the AIC-VAL-2 ladder are non-fighting, including the middle one, by
specification.** This is the part the PRD does not say at all. The ladder reads:

1. **Retain** the last accepted order for up to `orderValidityS` (120 s) — 63 % likely `hold`.
2. **Standing order** — *"default `posture=hold`, `roe=weaponsTight`, waypoint at the entity's
   position at expiry, `orbitRadiusM = safety.defaultOrbitRadiusM`"*.
3. **Release to Tier 1** — `resumeWaypointFollowing`.

Item 45 identifies rung 3. **Rung 2 is specified as `hold`**, which is exactly as non-fighting.
So the degradation path from a healthy commander to a dead one passes through three states and the
aircraft is unable to fire in all three. Fixing `fallBackToWaypoints` alone would leave rungs 1 and
2 intact.

**(d) There is no automatic defensive reflex, and this is the most likely loss mechanism.** Compare
the two scripts' tick order:

| | shipped `oppint_red_interceptor.lua` | `ai_commander_interceptor.lua` |
|---|---|---|
| first action each tick | `flyDefensiveIfThreatened` — *"Survival first: an inbound missile overrides every other consideration"* (line 383) | report tracks, read the published order |
| trigger | a **munition**-kind track inside `kThreatRangeM` = 15,000 m, `sensor.getClosestHostileTrackById(id, DIS_KIND_MUNITION)` | the model ordering `defend`, next cadence window |
| reaction time | one tick (~50 ms) | 20 s cadence + inference: measured p50 **3,476 ms**, p95 **9,488 ms** at 7B |
| what it flees | the inbound munition | `getClosestHostileTrackById(entityId)` with **no kind filter** (line 224) — the nearest hostile *anything* |

An AMRAAM closing at several hundred m/s covers 15 km in roughly fifteen seconds. **The
commander's control loop cannot react inside that window and was never intended to** — §Non-goals
says so explicitly: *"Anything reactive — missile defeat, merge maneuvering, launch timing — stays
in Tier 0/1 where it already works."* **The reference script does not implement the Tier-0/1 half
of that sentence.** Missile defeat is stated as Tier-1's job and then delegated to a 20-second
model loop.

The second row matters independently: even when `defend` *is* ordered, the commander-aware script
flees the nearest hostile track of any kind, which in a BVR merge is usually an aircraft, not the
missile. The stock script filters to munitions. So the ordered `defend` can turn the aircraft cold
from the wrong object.

**(e) Two further script-quality gaps, latent rather than demonstrated.** Neither is implicated in
the archived losses; both should be fixed with the rest.

- `firstLoadedHardpoint` (line 158) selects the first rail with ammo **regardless of range**,
  against `kMissileRangeM = 60000.0 × 0.8 = 48,000 m`. No hardpoint in the scenario has that
  reach: `R77_BVR` is 45,000 m and `R73_IR` is 12,000 m, and the shipped script's `selectHardpoint`
  picks by envelope, firing inside 36,000 m. So the commander-aware script will fire an IR missile
  at four times its kinematic reach if that rail happens to be first in the loadout table. **The
  four archived commanded launches all fell at 17.6–27.0 km and none exercised this**, so it is a
  latent defect, not an observed one — but `kMissileRangeM = 60000.0` corresponds to nothing.
- There is no `winchester` egress. The stock script flies home on an empty rail; the commander-aware
  script sets a situation note and waits for an order that may not come.

### Does `AIC-VAL-2`'s ladder need re-specifying?

**Yes, and the ladder is the right place to fix this rather than the script.** The ladder is what
made this a design decision instead of a bug, and a script fix that leaves rung 2 saying `hold`
would be corrected back the next time someone reads the requirement. Recommended shape:

- **The ladder's subject changes from "what to publish" to "what the aircraft may do."** Each rung
  states, explicitly, whether Tier 1 retains its own engagement authority. On every rung, it does.
- **Rung 2's standing order stops being `hold`.** `hold` was chosen as the safe default; on the
  measured evidence it is the *dangerous* default, because it is the posture in which the aircraft
  neither shoots nor evades. A standing order of `defend` with `weaponsFree`, or the removal of
  rung 2 in favour of an earlier release to Tier 1, are both defensible; the choice is the owner's
  and should be made against the numbers in (b) and (d).
- **A new acceptance criterion**, which is the one that would have caught this:
  *IF the backend is unreachable for the whole run, THEN the commanded entity's launch count,
  detonation count and survival SHALL be within a stated tolerance of the same scenario flown on
  the shipped script.* The existing criterion — *"SHALL complete the scenario under Tier-1
  behavior with no error state and no stall"* — is satisfied by an aircraft that flies in a
  straight line and dies, which is what happened.

### What the script should do between orders

Three options, with a recommendation.

| Option | What it is | Assessment |
|---|---|---|
| **Retain the last posture** | Keep flying the last accepted intent past its validity | **Rejected.** Does not help: the last posture is 63 % likely to be `hold`, which is the state that cannot fight. It also makes stale intent staler, against the doctrine's own "an order right now and wrong in thirty seconds" line |
| **Run the stock engagement logic underneath** | Fold the shipped script's tick order in as the substrate — defensive reflex first, then winchester/RTB, then target selection, launch discipline, crank — and let a valid order *override* posture and target where it has one | **Recommended** |
| **Something else** — e.g. a `weaponsFree`-gated free-fire mode | Let the script select its own targets whenever ROE permits | Partially subsumed by the above; the ROE gate is already the right seam and `weaponsFree` already means "the script's own selection" in the script's own comment (line 386) |

**The recommendation is the second, and the reason is that it is not new work.** The behaviour
already exists, is already the quality bar the PRD measures this script against
(`ai_commander_interceptor.lua:246`), and already ships. The commander-aware script should be the
shipped script **plus** an order-override layer, not a parallel implementation that reproduces
some of it. Concretely:

1. `flyDefensiveIfThreatened` runs **first, every tick, unconditionally** — before the order is
   even read. This is Tier 0 and no order should be able to suppress it.
2. Winchester → RTB, unconditionally.
3. Then the published order, if valid, sets posture and target.
4. **`considerFiring` moves out of the `engage` branch** to run after the posture switch on every
   posture that does not forbid it — `engage`, `crank`, `hold`, `ingress`, `defend` — gated by ROE
   and by launch geometry exactly as it is now. `rtb` keeps its cease-fire.
5. When no order is in force, target selection falls through to the stock two-ship deconflicted
   selection rather than to `resumeWaypointFollowing` alone.

This preserves the tiering claim rather than weakening it. ADR-1 confines the model to intent;
letting Tier 1 fight when Tier 2 is silent is the *strong* form of that argument, not a retreat
from it. It also makes AIC-VAL-2's rollback story true for the first time: *"deleting one DLL must
leave a script that still flies"* (line 15 of the script) is currently satisfied to the letter and
not in substance — it still flies, and it dies.

### Arm C, and whether to repeat it

**Arm C was not repeated in this session, and on the evidence above it should not be the next
run.** The reasoning:

- The harness has no third arm. Running arm C means either adding one or hand-orchestrating a
  release-tree swap outside the `finally` that restores the mission script, doctrine and deployed
  config — the exact failure mode §Traps warns about, and the one that silently enables the
  commander for later runs.
- More importantly, **arm C's n = 1 is no longer the weak link.** It was run to distinguish "the
  model causes this" from "the script causes this", and it did. The four mechanisms above are
  structural, readable in the source, and jointly sufficient to explain the observation without
  any run. A second arm-C run would add a fourth confirmation of something already established
  three ways — including the accidental instance at `221939Z`, where the commander was enabled and
  delivered zero orders.

**What should be run instead, and it is the same cost.** Once the script is fixed, run the *fixed*
commander-aware script with the commander **off**. That arm is the one that has never existed and
is the one that matters: it separates "the script is now as good as stock" from "the commander
adds or subtracts on top of a competent script." Until it passes, arm B measures nothing about the
model. It needs no inference server and is free.

### Cost to resolve

Roughly **one day**: a PRD revision re-specifying AIC-VAL-2's ladder and acceptance criteria
(~2 h), the script rewrite against the shipped script's structure (~3 h), a harness arm for
script-only control (~1 h), and one 600 s run of each of the three arms (~35 min). No grant, no
network, no model. §Corrections item 45 says *"it is where the next hour of work belongs"*; the
hour is optimistic by roughly a factor of eight, and the reason is (c) and (d), which had not been
found when that sentence was written.

---

## 2. §Success metrics contains no outcome metric

### What is established

Six metrics: frame cost, round-trip latency, acceptance rate, parse/schema rejection rate, replay
reproducibility, direct kinematic writes (and cost per scenario-hour). **Not one is an outcome.**
The live smoke asserts nineteen checks and none of them is either. Item 45(g) states this plainly
and it is correct.

### What the evidence shows

The gap is not merely that outcome went unmeasured — it is that **the metric set is satisfiable in
full by a system that does harm**. Every published verdict was green or explained while the thing
being commanded was dying in 22 of 24 runs. That is the strongest possible argument for the row,
and it is already in hand.

The instrument also already exists and needs no new plumbing: `tools/analyse-outcomes.py` reads
three line formats that the engine has emitted since Phase 1b. The harness has both logs in scope
at the point where it currently prints the H1 note.

### What belongs there, and on what footing

**Footing: report, don't bar — v1.7.5's precedent, and for the same reason.** That precedent was
set when the in-engine acceptance n was 10 and a bar would have fired on noise. The outcome n is
worse: twelve pairs, one scenario, one force laydown. A bar on kills would be a bar on a quantity
that is **zero in both arms**, and a bar on losses would currently fail every run. Both would train
readers to ignore the row, which is the failure v1.7.5 was avoiding.

Recommended row for §Success metrics:

| Metric | Baseline | Target | How measured | Timeline |
|---|---|---|---|---|
| Engagement outcome, commanded vs control | ON 4 / 0 / 0 / 22 · OFF 32 / 16 / 0 / 0 (12 post-C12 pairs, 2026-08-07) | **Reported, not barred.** A commanded arm launching or surviving materially less than its control is a finding to be explained before the next gate | `tools/analyse-outcomes.py` over the paired engine logs: launches, detonations, kills, losses, per arm | Every paired run |

Three details that matter:

- **Four columns, not one.** Item 45(d) corrected a "hits" column that counted detonations; the
  analyser's docstring already encodes the distinction. The row must carry launches, detonations,
  kills and losses separately or the same conflation returns.
- **The row states the two-variable confound in its own text.** The control arm swaps the shipped
  script *and* disables the commander (`run-live-scenario.ps1:414`). A reader who takes the row as
  a commander A/B will be wrong. Once §1's script-only arm exists, the row should carry three arms.
- **Baseline is populated, not `N/A`.** Every other row in that table was written before there was
  anything to put in it, and §Corrections is largely a record of what that cost.

### Should the live smoke carry it?

**Yes, and as a printed report plus one assertion — but not an assertion on the outcome itself.**
The assertion should be that **the comparison was computed at all**: that both arms' logs were
found and the four counts were produced. That is a harness-integrity check of exactly the kind the
"commander is ON" assertion already is (*"a green smoke over the stub backend's canned orders is
worse than a red one, because it looks like evidence"*). An outcome comparison that silently fails
to run is the same category of problem.

### Cost to resolve

**~2 hours.** A PRD revision adding the row (~30 min) and a harness block that shells out to the
existing analyser and prints the table (~1 h). No new instrument, no grant.

---

## 3. C17 — acceptance is measured by two instruments and only one is gated

### What is established, and what is not up for re-litigation

§Success metrics reads **MET at 100 %** on synthetic-fixture soaks (200 local, 240 hosted). The
engine measures far less. **The owner decided on 2026-08-06 that the bar stays on the fixtures**;
that decision is recorded, and this review does not reopen it. The open question is narrower and
is the one C17's row actually states: the table still shows green, and a reader who stops at the
green row forms a wrong impression.

### What the evidence shows — and the PRD's in-engine figure is one run stale

Recomputed from the archive this session. The PRD's post-fix figures come from item 42 (three 7B
pairs) and item 43 (three 14B pairs). **Run `20260807T133621Z` has been archived since and nobody
folded it in:**

| Sample | PRD figure | Recomputed | 95 % Clopper–Pearson |
|---|---|---|---|
| 7B post-fix | 50 / 58 = **86.2 %** | **59 / 70 = 84.3 %** (adds `133621Z`) | [73.6 %, 91.9 %] |
| 14B post-fix | 28 / 39 = **71.8 %** | 28 / 39 = 71.8 % (agrees; excludes the two `transport` rejections of the C20 host failure) | [55.1 %, 85.0 %] |
| Pooled post-fix | 78 / 97 = **80.4 %** | **87 / 109 = 79.8 %** | [71.1 %, 86.9 %] |
| Everything archived | — | 158 / 213 = 74.2 % | [67.8 %, 79.9 %] |

The Clopper–Pearson implementation was validated against the PRD's own published interval:
50/58 → [74.6 %, 93.9 %], against the document's "roughly [75 %, 93 %]". ✔

**The direction is unchanged and no verdict moves.** The point is that §Success metrics' headline
in-engine number is superseded by this project's own subsequent run, which is the same class of
staleness C17 is about.

**A contamination in the in-engine instrument that has never been named.** Of 55 archived
rejections, **10 (18.2 %) are `roster` — "entity 'RedSu35_0X' no longer exists"**. These are orders
that resolved after the aircraft they were for had been destroyed. They are not model failures in
any sense; they are the commander requesting for a dead entity. Excluding them moves pooled
acceptance from 74.2 % to **77.8 %**.

The coupling is the interesting part: **`roster` rejections can only occur when commanded aircraft
die, and commanded aircraft die because of C21.** So the in-engine acceptance rate — the number
C17 is a dispute about — is partly a function of the Tier-1 defect in §1. The two instruments
disagree for one more reason than item 41 lists, and it is a reason that will partly disappear when
§1 is fixed.

### How a reader should be prevented from taking the green row at face value

The PRD's current arrangement puts the green table at line 943 and the two-instrument discussion at
line 952 — nine lines and one heading later. A reader looking up "did acceptance pass?" reads the
table. **The fix is not more prose; it is that the verdict cell should not read `MET` unqualified.**

Recommended, in order of preference:

1. **Change the verdict cell itself** to `MET (fixtures) · 79.8 % in-engine`, with the instrument
   named in the cell. A qualified verdict cannot be quoted unqualified. This is a one-cell change
   and it is the only option that survives being read out of context, which is how this row is
   actually read.
2. **Add the in-engine row to the same table** rather than to a subsection below it, marked
   *reported, not gated*. Two rows in one table cannot be separated by a reader skimming.
3. Leave the arrangement and strengthen the prose. **Not recommended** — it has been tried across
   two revisions and this review found the number stale underneath it, which is evidence that the
   subsection is not being maintained as part of the table.

The owner's decision is preserved intact under all three: the bar stays on the fixtures. What
changes is that the fixture instrument is named at the point of the verdict.

**And the deeper caveat should travel with it.** Item 41's finding is that the fixtures do not
merely have easier inputs — *they have inputs this document wrote*. That is what makes 100 % on
them a weaker claim than the number looks, and it belongs in the cell's footnote, not four
paragraphs down.

### Cost to resolve

**~30 minutes.** A PRD revision changing one table cell and refreshing two figures. No run, no
grant. C17's row stays open until the two instruments agree or the fixture instrument is retired;
this only stops the row being misread in the meantime.

---

## 4. C14 — `reject.shape`, diagnosed: the constrained decoder cannot express the rule

### What is established

`reject.shape` is **the largest rejection class in the entire archive**: 16 of 55 (29.1 %). Fifteen
are `posture 'hold' requires orbitRadiusM > 0`; one is the mirror,
`posture 'defend' requires orbitRadiusM == 0`. Open since v1.8.23, never diagnosed, and previously
out of scope.

Full pooled rejection census, all twenty runs, n = 55:

| reason | n | share | note |
|---|---|---|---|
| `shape` | **16** | **29.1 %** | 15 × `hold` radius, 1 × `defend` radius — §4 |
| `roster` | 10 | 18.2 % | entity already destroyed — a C21 artifact, §3 |
| `range` | 7 | 12.7 % | 4 × `cruiseSpeedMps` ≤ 0, 3 × `reason` > 200 chars (pre-C16) |
| `clamp` | 6 | 10.9 % | 3 × speed 600 > 400, 2 × waypoint altitude 0, 1 × speed 1 < 50 (C19) |
| `fratricide` | 5 | 9.1 % | own SAM radar — §7 |
| `track` | 5 | 9.1 % | all five name `*_wpn_*` munition ids — §7 |
| `targetClass` | 4 | 7.3 % | B9, all four are munitions — §7 |
| `transport` | 2 | 3.6 % | the C20 host condition |

### What the PRD assumes

Item 39(d) frames C14 as *"a decision on whether C3's prefix reduction is safe for the local
backend"*, and §4's brief offers three candidate explanations: a schema-encoding gap, a prompt
gap, or a model failure. **The brief also notes that "the four-branch `oneOf` is supposed to make
`hold`'s orbit radius unconditional."** That supposition is the thing to test, and it is false.

### What the evidence shows — a controlled decoder probe

Eight requests to the local inference server, each carrying a `format` schema whose bound forbids
the value the prompt explicitly asks for. Temperature 0, seed 1, no scenario state, no cost. This
is a mechanism test: it asks what the *runtime* enforces.

| Keyword under test | Prompt asked for | Emitted | Bound held? |
|---|---|---|---|
| numeric `minimum` (1000) | 0 | `{"v": 0}` | **NO** |
| numeric `maximum` (10) | 999999 | `{"v": 999999}` | **NO** |
| zero-width numeric range (`min == max == 0`) | 4242 | `{"v": 4242}` | **NO** |
| `maxLength` (5) | a 60-word paragraph | `{"s": "Naval"}` | YES |
| `minLength`/`maxLength` 0/0 (forced empty) | the word BANDIT | `{"s": ""}` | YES |
| `enum` *(control)* | zebra | `{"p": "alpha"}` | YES |
| `additionalProperties: false` *(control)* | an extra field | `{"p": "x"}` | YES |
| **the shipped four-branch shape** | `hold` with radius 0 | `{"posture": "hold", "orbitRadiusM": 0}` | **NO** |

**The finding is clean and it is not about the model.** The local constrained decoder enforces
type, `required`, `enum`, `additionalProperties`, and string length. **It does not enforce numeric
bounds at all, in either direction, including a zero-width range.** The last row reproduces the
exact archived failure on demand: a document that satisfies **no branch** of the `oneOf`.

**Confirmed across four models in three families**, all emitting the same violating document from
the same schema:

| model | emitted |
|---|---|
| `qwen2.5:7b-instruct-q8_0` (the shipped default) | `{"posture": "hold", "orbitRadiusM": 0}` |
| `llama3.1:8b` | `{"posture": "hold", "orbitRadiusM": 0}` |
| `mistral:7b-instruct` | `{"posture": "hold", "orbitRadiusM": 0}` |
| `qwen3:8b` | `{"posture": "hold", "orbitRadiusM": 0.0}` |

**So: it is a schema-encoding gap — specifically, the four-branch encoding solves half of its own
problem.** §Corrections item 15's mechanism is stated as *"`targetEntityId` forced empty by a
zero-length bound, `orbitRadiusM` forced to zero by a zero-width numeric range."* The **string**
half works. The **numeric** half does not, on the local decoder, and never has.

**And the hosted path cannot express it either**, by inspection of `src/OrderSchema.cpp:265-305`:
`pinToConst` converts a zero-width numeric range to `const`, which the API *does* enforce — but the
hold branch's `{minimum: 1.0, maximum: 50000.0}` is not zero-width, so it survives `pinToConst`
untouched and is then **stripped whole** by the unsupported-keyword removal (item 19). There is no
`const` equivalent for "greater than zero."

**The conclusion, stated as strongly as the evidence allows:** *"a `hold` order requires
`orbitRadiusM > 0"` is enforced by nothing, on either backend, except Stage A rejecting the whole
order after the fact.* It is the only rule in the schema with that shape — every other conditional
rule is a forced-to-a-constant rule, and those all work. C14 is not a rate; it is a hole.

**Why the hosted soak measured 0/240.** Not because the projection holds the rule — it cannot. The
fixtures simply never produced a `hold` with radius 0. §Corrections item 20's reading, that *"the
mechanical projection preserved the four-branch encoding's effect"*, is **true for three of the
four rules and false for this one**, and the 0/240 is consistent with either explanation. This is
the item-42(c) failure mode again: a distribution of outputs read as evidence of a mechanism.

**The `RedSu35_02` address, labelled as distributional.** All 15 hold-radius rejections are on
`RedSu35_02` and none on `RedSu35_01`, while `RedSu35_02` also produced 41 accepted `hold` orders.
Eight of the fifteen occurred at `serial: 1` with an identical response body across five separate
runs on three different days — the cold-start snapshot is deterministic and so is the response.
**No mechanism is claimed from this.** Item 40's *"always on `RedSu35_02`"* was the symptom's
address and not its cause, and the same caution applies here.

### Is the prompt a viable remedy? Measured: no

The cheapest possible fix is a doctrine line. Tested directly — same schema, same adversarial
prompt, prefixed with an explicit, emphatic rule naming the field, the failure, and the remedy
value:

| model | result |
|---|---|
| `qwen2.5:7b-instruct-q8_0` (**shipped default**) | **still violates** |
| `llama3.1:8b` | still violates |
| `mistral:7b-instruct` | still violates |
| `qwen3:8b` | still violates |
| `qwen2.5:14b-instruct-q4_K_M` | complies — `{"posture": "hold", "orbitRadiusM": 5000}` |

**The prompt was deliberately adversarial** ("set orbitRadiusM to 0"), so this is a worst case, not
an average case, and it is one request per model rather than a rate. What it establishes is
sufficient for the decision: **a prompt line is not reliably sufficient on the shipped default
model**, so the cheapest remedy is off the table. Note also that `orbitRadiusM` is **not mentioned
anywhere in `data/doctrine.txt`** — the doctrine explains posture selection at length and never
mentions the fields a posture must carry — so the model's only current source for the rule is a
JSON `description` string attached to a keyword the decoder discards.

### Recommendation: repair the field, do not reject the order

**Stage A should treat a `hold` with `orbitRadiusM <= 0` the way it already treats an over-long
`reason`: repair it, mark it, and let the order through.** The precedent is C16's, and its
argument transfers exactly:

> *"`reason` carries no control authority … while rejecting on it discards the posture, the target,
> the waypoint and the ROE, every one of which does carry authority, and leaves AIC-VAL-2 holding
> the previous order in force."*

A missing orbit radius is the same shape of problem. The posture, waypoint, speed and ROE in a
rejected `hold` are all perfectly good; the order is discarded for a single scalar the decoder was
never able to constrain, and — per §1(c) — the ladder then holds a *stale* order in force, which is
strictly worse than a repaired one.

**The default value already exists and needs no invention:** `safety.defaultOrbitRadiusM`, default
**8,000.0** (`include/CommanderConfig.h:121`), is the value AIC-VAL-2 rung 2 already uses for
exactly this field in exactly this posture. Using it here makes the two consistent rather than
adding a policy.

Concretely, and to be specified in the PRD first:

- **AIC-ORD-1**: `orbitRadiusM` on the `hold` branch, when at or below zero, SHALL be replaced with
  `safety.defaultOrbitRadiusM` rather than rejected. The substitution SHALL be marked on the order
  record, as `reasonTruncated` is — a new `orbitRadiusRepaired` flag, on the same footing.
- **The lower bound on the mirror rule stays a rejection.** A non-`hold` posture carrying a
  positive radius is a different signal: the decoder *can* enforce that direction on the hosted path
  (`const: 0` via `pinToConst`), and a model emitting one on a branch that forbids it is saying
  something wrong rather than omitting something. One occurrence in the archive.
- **`reject.shape` keeps its counter and does not silently go to zero.** The repair is counted, so
  the rate remains visible; a fix that makes a metric green by removing its subject is the failure
  mode §Success metrics is already carrying one of.

### On the scope promotion

The brief promotes C14 from "correctly dormant" to in-scope. **The promotion was right**, and this
review would argue for it independently: at 29 % of all rejections it is the largest single class,
the diagnosis costs nothing, and the thing it turned out to be — a schema guarantee that is not a
guarantee — is a correctness question about the order contract rather than a quality question about
a model. The row's own framing ("is C3's prefix reduction safe for the local backend?") was too
narrow: C3 removed the only place the rule was *stated*, but the rule was never *enforced*, before
or after.

### Cost to resolve

**~3 hours.** PRD revision to AIC-ORD-1 and AIC-DET-1 (~1 h), the Stage-A branch and its
adversarial-corpus tests (~1.5 h), record-format field (~30 min). No run, no grant, no network.

---

## 5. Latency misses its target and is explained rather than fixed

### What is established

p95 **7,238 ms** against ≤ 2.5 s on `claude-haiku-4-5`. C10 (v1.8.20) established the cause is
**variance** — SD 3,977 ms — and not a floor: Haiku's time to response *headers* is mean 2,452 ms /
p50 1,510 ms, and two fully-cached mid-run requests took 10.5 s and 27.5 s to return headers.
C2's *"unreachable by any prompt-side change"* is a `claude-sonnet-5` property and was rescoped.
Nothing has been done since.

### What the evidence shows

**The target row has three entries and only one has ever been reported.** §Success metrics'
latency row reads *"Local 3B CPU: p95 ≤ 8 s. Local 7B CPU: p95 ≤ 20 s. Claude Haiku 4.5: p95 ≤
2.5 s"*, and every verdict in the document is against the Haiku entry. Measured from the archive
this session — the first time these have been computed:

| Backend | p50 | p95 | Target | Verdict |
|---|---|---|---|---|
| `qwen2.5:7b-instruct-q8_0`, in-engine (n = 115) | 3,400 ms | **7,975 ms** | ≤ 20 s | **MET**, with 60 % headroom |
| `qwen2.5:14b-instruct-q4_K_M`, in-engine (n = 43) | 6,066 ms | **17,109 ms** | (no row) | within the 20 s cadence, but only just |
| `claude-haiku-4-5`, fixtures (n = 240) | 2,602 ms | 7,238 ms | ≤ 2.5 s | MISSED by 190 % |

**The shipped default backend meets its latency target comfortably and the document has never said
so.** The 7B row was written for CPU inference; it is running on GPU. That is a real, free, positive
result sitting unreported next to a red one — and the asymmetry runs the opposite way to the usual
complaint about this project, which is worth recording.

**Note the 14B p95 of 17.1 s against a 20 s cadence.** That is not a target miss but it is a
control-loop margin of under three seconds, on the model this project does *not* default to. It is
the quantitative form of the C20 trap: the same model under VRAM pressure exceeded the 90 s
transport budget entirely.

### Should the target, the cadence, or the expectation move?

**The expectation, and it should move by being made explicit — not by moving the number.**

The reasoning:

- **The target should not move.** v1.3 already refused to move a target to match a measurement
  (*"moving a target to match a measurement would erase the signal"*), and the same argument holds
  a fortiori now that the cause is known. A ≤ 2.5 s p95 was set against no measurement, and it is
  a perfectly reasonable *aspiration* for a hosted request; what has been learned is that this
  provider's tail does not honour it, not that 2.5 s was the wrong thing to want.
- **The cadence should not move either.** It already absorbs the miss: 20 s against a p99 of
  7,099 ms, and no run in any phase was degraded by latency. C2 recorded this and it is still true.
- **What should move is that §Success metrics states, in the row itself, that this target is not
  control-loop-binding.** The row currently sits beside frame cost and acceptance, both of which
  *are* binding, and reads as if a miss meant something is broken. It doesn't. A miss here means
  the hosted path is slower than hoped and the design absorbed it, which is a materially different
  statement.

Recommended edit to the latency row's *How measured* column: add *"Informational for the hosted
path: the 20 s order cadence absorbs p99 and no run has been degraded. Binding only if the cadence
is ever reduced below ~3× p95."* That last clause is the useful part — it converts a permanently
red row into a **condition**, which is what the number is actually for.

**One thing that genuinely is open and is worth naming.** The variance cause (SD 3,977 ms, two
requests over 10 s to headers on a fully warm cache) is a **provider** property that no amount of
prompt work touches, and this project has no retry-on-slow path — only a 90 s transport budget and
a latest-wins slot. If the cadence is ever reduced, the remedy is a hedged request or a shorter
transport budget with a retry, not a smaller prompt. Recording that saves the next person the C2
decomposition.

### Cost to resolve

**~1 hour**, and it is a documentation change: the row edit above, plus publishing the local
latency figures that already exist in the archive. No run, no grant.

---

## 6. Recent changes that are unmeasured or unexercised

Three items, all verified against the archive this session. All three are genuinely unexercised;
none is wrong.

### (a) `own.courseDeg` — confirmed unexercised, and the doctrine gap is real

**Established.** C18 measured `headingDeg` frozen at **270.000° across fourteen samples on both
entities for 600 s, spread 0.000**, while `velN` swung −280 to +304, and at t = 40.1 the prompt
reported west while the aircraft flew east — **178.4° out**. Fixed by `own.courseDeg =
atan2(velE, velN)` in commit `5a7fc81`.

**Verified here.** Of twenty archived run directories, **exactly one** (`20260807T133621Z`) carries
an `own` block in its order records at all — that is C18's instrument, landed in `c98c37a` — and
that run's records carry **`headingDeg: 270.0` with `velE: −220.0`**, i.e. the *broken* field. Grep
across the archive returns **zero** occurrences of `courseDeg`. The claim is confirmed exactly: the
corrected value has never been sent to a model.

**The doctrine gap is real and is larger than "never mentions the field."** `data/doctrine.txt`
mentions `own.speedMps` (three times, with a whole CRUISE SPEED block), and `own.velN` / `velE` /
`velD` (explaining they are not speeds). It mentions **no heading or course field at all** — not
the old one, not the new one. So the volatile suffix has always carried a direction the doctrine
never told the model how to read, and for the entire project's life that direction was wrong.

**Recommendation.** Add a short READING THE SITUATION BLOCK paragraph naming `own.courseDeg` as
ground track in degrees true, and stating plainly that it is derived from velocity and is therefore
current — the last clause matters, because the model has no way to know the field changed meaning.
Then run one 600 s pair and confirm from the records that `courseDeg` varies. **That confirmation
is a mechanism check readable off recorded values** — "does this number ever change?" is the
example §Scope authority's fourth rule gives — so a single run closes it, exactly as a single run
closed C18 in the other direction.

**Sequencing note: this should run *after* §1's fix, not before.** A run whose Tier 1 stops
fighting cannot say anything about whether a corrected course field improves order quality, and
running it first spends the run and answers only the mechanism half.

### (b) C16's truncation path has never fired — confirmed, and the cap raise is doing real work

**Verified.** `reasonTruncated: 0` in **all 16** run-end stats records in the archive. The
truncate-and-mark path is unit-tested only (`tests/AdversarialCorpusTests.cpp`, four assertions).

**What the archive adds, which the PRD does not record.** Of 158 accepted orders, **14 carry a
`reason` longer than 200 characters**, and the longest is **449**. So:

- The cap raise (200 → 512) has admitted **14 orders that the old cap would have rejected whole** —
  and per §1(c), each rejection would have left a stale order in force. The fix is not merely
  untriggered-but-harmless; it is measurably load-bearing.
- The observed maximum, 449, sits at **88 % of the new cap**. That is closer than the derivation
  implies (512 chars ≈ 129 tokens ≈ one measured-mean 106.2-token response) and it means the
  truncation path is not far-fetched — a slightly more verbose model reaches it.

**Recommendation: leave it, and record the two numbers.** Adding an artificial trigger to exercise
the path in the field would be testing the test. The unit coverage is the right coverage for a
branch this rare. What is worth one line in §Corrections is that the cap has a measured 88 %
high-water mark, because that is the number that decides whether 512 survives a model change — and
it is exactly the C8 shape of question ("a ceiling sized from one run is sized against noise"), so
it should not be re-derived from one sample either.

### (c) C19's floor has fired once — confirmed

**Verified.** Exactly one occurrence in the archive:
`clamp — cruiseSpeedMps 1 is below safety.minSpeedMps 50`. One of 55 rejections.

**Assessment: this is the expected and correct outcome, and it should not be read as the check
being unnecessary.** C19 was opened because *five* under-50 orders had been **accepted** before the
floor existed, and item 43 records 11 of 78 accepted orders across both models commanding under
50 m/s. The floor's job is to move those from "accepted and flown" to "rejected and counted", and
the single firing is that mechanism working once on a much smaller post-fix sample. The row's own
text says as much: *"It does not fix C15's copying — it puts that failure back on the counter it
fell off."*

**One caveat worth recording.** C15's close (item 44) established that all nine accepted orders in
the instrumented run matched `own.speedMps` exactly, and item 42(e) established that the
degenerate `1.5` was an accurate report of a degenerate snapshot rather than model arithmetic. So
the floor now guards against a *snapshot freshness* problem more than a model problem, and if
snapshot freshness is ever fixed the floor may stop firing entirely. That is not a reason to remove
it, but it means "the floor rarely fires" should not later be read as "the floor is not needed."

### Cost to resolve

**(a) ~1 h** for the doctrine paragraph, plus one 600 s run already needed for §1. **(b) and (c):
~15 min** of §Corrections text each. No grant.

---

## 7. Order-quality tendencies the validator catches and nobody has fixed

### What is established

Three classes, all caught by Stage B and therefore invisible in the acceptance rate's *composition*
even though they depress its value:

| Class | Check | n (archive) | Detail |
|---|---|---|---|
| Engaging a munition | B9 `targetClass` | **4** | all four name `BlueF16_*_wpn_*` ids |
| Engaging its own SAM | B4 `fratricide` | **5** | all five `RedSAM_FireControlRadar`, all in run `213840Z`, all on `RedSu35_02` |
| Waypoint altitude 0 | B6 `clamp` | **2** | `waypoint altitude 0 m HAE is outside the configured envelope [100, 200…]` |
| *(related)* naming a munition that no longer exists | B3 `track` | **5** | all five `*_wpn_*` ids — a munition reported, then detonated before the order resolved |

### Doctrine gap, prompt gap, or model limit? The evidence says prompt gap, decisively

**This is the finding of this section and it is not in the PRD.** The prompt gives the model
`{id, rangeM, snrDb}` per track and nothing else (`src/PromptRenderer.cpp:159-167`, and AIC-SEC-2
asserts it: *"no `team`, `kind`, or `domain` field is rendered for a track"*). The doctrine then
says, in READING THE SITUATION BLOCK:

> *"Nothing else is known: not the contact's type, not its team, not its heading. **Do not infer
> from an id what the id does not say.**"*

**So the only discriminator available — the id string — is one the doctrine explicitly forbids
using, and the validator then rejects the model for failing to discriminate.** The doctrine and
Stage B are in direct contradiction, and the model is on the losing side of it. Fourteen of 55
rejections (25.5 %) are in this class.

**The fratricide case is the airtight one, and it indicts Tier 1 rather than the model.** Stage B
runs B3 before B4 (`src/OrderValidatorStageB.cpp:90-127`). All five fratricide rejections therefore
**passed B3**, which means `RedSAM_FireControlRadar` **was in Tier 1's reported track list**. The
commander-aware script's `reportTracks` (line 99) reports every sensor track unfiltered. The
shipped script's `listHostileAirPlatforms` (line 218) reads `entityControl.getEntityInfo(targetId)`
and filters by team, kind and domain — **so the team information is available in Lua, Tier 1 has it
in hand, and the commander-aware script discards it before reporting.** The model was handed its
own side's SAM radar as an indistinguishable contact and then rejected for naming it.

### Does any of it belong in Tier 1?

**Partly, and the PRD's existing objection does not cover the part that does.** C13's close records
that filtering Tier 1's reports was considered and rejected because it *"would blind `defend`* —
the model needs to see inbound munitions to know it is threatened. That argument is sound **for
munitions** and does not apply **at all to own-team contacts**: nothing in `defend` requires
visibility of a friendly SAM radar.

So the three classes split cleanly:

| Class | Belongs where | Why |
|---|---|---|
| **Own-team contacts** | **Tier 1 — filter them out** | C13's blinding objection does not apply. The script already has the team via `getEntityInfo`. One filter closes 5 of 55 rejections and removes a class the model cannot avoid |
| **Munitions** | **The prompt — widen the track report** | Must stay *visible* (C13) but must become *distinguishable*. §Out of scope's deferred row on free-text track attributes is the specified route |
| **Waypoint altitude 0** | **The doctrine** | The altitude envelope `[100, 20000]` is a config value the model is never told. Two occurrences; one doctrine sentence naming the floor |

**The deferred row's revisit condition is now met, and that is the cleanest way to raise this.**
§Out of scope carries *"Free-text track attributes (`team`, `kind`, `domain`) in the prompt —
Deferred … Revisit if order quality shows the model cannot discriminate targets without them."*
Nine rejections across two checks show exactly that. The row should be moved from Deferred to
scheduled, and it should be narrowed while it moves: **`kind` and `team` are enumerated scalars,
not free text**, so the row's own stated cost — *"each added string is a new injection surface to
charset-filter"* — does not apply to them. That distinction is what makes this cheap. `domain` can
stay deferred.

**A note on Tier 1's authority.** ADR-6 says Tier 1 reports the tactical picture and the plugin
does not fetch it, and §Out of scope rejects plugin-side synthesis of a track list from the entity
roster because *"a roster is not a sensor picture"*. Neither is violated here: filtering own-team
contacts out of a **sensor** picture, and reporting a **kind** that the sensor track already
carries, are both Tier 1 reporting what it sees more precisely. Nothing is synthesised.

### Cost to resolve

**~1 day total**, split: the own-team filter in the reference script is ~30 min and rides with §1's
rewrite; widening `reportTrack` with `kind` and `team` is a PRD revision to AIC-API-1 and AIC-SEC-2
plus the ingress verb, the charset/enum validation, the prompt renderer and the allowlist test
(~4 h); the doctrine sentence on the altitude floor is ~15 min. No grant, no network.

**One dependency worth stating.** Widening the transmitted field set touches AIC-SEC-2 and
therefore `docs/egress.md`, and both must be revised **before** any hosted request under the new
field set — the fifth grant's own text is explicit that a later grant does not inherit an earlier
one's boundary, and this would change what leaves the machine.

---

## 8. Statistical weakness — which claims are and are not supported by their n

Stated plainly, as asked. All intervals are 95 % Clopper–Pearson, computed this session with an
implementation validated against the PRD's own published [75 %, 93 %] for 50/58.

### Claims that ARE supported by their n

| Claim | Basis | Why it holds |
|---|---|---|
| **`considerFiring` is unreachable except from `engage`** | source | Structural. n is irrelevant |
| **All three AIC-VAL-2 rungs are non-fighting** | specification + source | Structural |
| **Numeric bounds are not enforced by the local decoder** | 3 controlled cases × 4 models | A mechanism test, not a rate. One violation disproves enforcement |
| **The hosted projection cannot express "> 0"** | source inspection | Structural |
| **`headingDeg` was frozen at spawn** | 14 samples, spread 0.000 | "Does this number ever change?" — the canonical mechanism question |
| **`courseDeg` has never been sent** | zero occurrences across 20 run directories | Absence over the whole population |
| **`reasonTruncated` has never fired** | 0 across all 16 stats records | Same |
| **Tier 1 reported its own SAM to the model** | B3 precedes B4; 5 records passed B3 | Deductive from check order |
| **The commanded arm launches less and dies more** | 12 of 12 pairs, same direction every time | A sign test on 12 concordant pairs is p ≈ 0.0005 even ignoring magnitudes |

### Claims that are NOT supported by their n

| Claim as it appears | The number | What it actually supports |
|---|---|---|
| Acceptance **≥ 95 %** met in-engine | 59/70 = 84.3 %, CI **[73.6 %, 91.9 %]** | **Excludes 95 %.** Does not reach 90 % either. No bar is established as met |
| The **≥ 90 %** live-smoke assertion | same | The interval straddles it. Neither met nor refuted |
| 7B **86.2 %** post-fix | 50/58, superseded by 59/70 = 84.3 % | The document's own next run moved it. Quote the pooled figure with its interval, not the point estimate |
| 14B **71.8 %** post-fix | 28/39, CI **[55.1 %, 85.0 %]** | A 30-point-wide interval. It supports "14B is not obviously better"; it does **not** support a 14.4-point gap against the 7B, whose intervals overlap heavily |
| "The commander scores nothing" | 0 kills / 22 aircraft-runs, CI **[0 %, 15.4 %]** | Bounds the kill rate below 15 %. Since the **control also scored zero**, this separates neither arm from the other on kills |
| `reject.shape` at **12.5 %** (item 39(d)) | 4 occurrences | A count, not a rate. The pooled figure is 16 of 55 rejections = 29 % **of rejections**, or 16 of 213 resolved = 7.5 % **of orders** — three different denominators have been used for this quantity in three places |
| Arm C reproduces arm B | n = 1 | Carries the **mechanism** and not a rate — correctly stated by item 45 and correct |
| The memorised-coordinate negative (C5) | n = 11 | A bounded negative. Unchanged and correctly labelled |

### The general point, and the one recommendation

**~97–109 resolved in-engine orders is enough to say the in-engine rate is materially below 95 %
and not enough to say what it is.** Every interval above is 15–30 points wide. The document's
habit of quoting point estimates to one decimal (86.2 %, 71.8 %, 80.4 %) implies a precision the
samples do not carry, and it is how 86.2 % came to be quoted after its own successor run existed.

**Recommendation, and it is cheap: quote in-engine acceptance as an interval, never as a point.**
`84.3 % [74 %, 92 %]` cannot be misread as precise and cannot silently go stale in the way a bare
decimal can. Apply it to the three figures in §Success metrics and the two in C17's row. This is a
formatting rule, not a re-measurement, and it costs one revision.

**Do not chase n.** Reaching a ±5 point interval on in-engine acceptance needs roughly 250–400
resolved orders — 25 to 40 paired runs at ~22 minutes each, nine to fifteen hours of wall clock —
and per §1 every one of those orders would be measured against a Tier 1 that stops fighting. **The
n problem is downstream of the C21 problem and should not be attacked first.**

---

## 9. Two governance boxes are unticked — stated, not resolved

Both are owner decisions. This review states them and recommends nothing about how they should be
decided.

### (a) There is no standing authorization for hosted egress outside a measurement grant

**The position, accurately.** Six grants exist (v1.8.1, v1.8.3, v1.8.5, v1.8.8, v1.8.11, v1.8.19).
Every one authorizes **measurement**, each says so in its own text, and each is scoped to named
experiments. v1.8.19 states explicitly that *a later grant does not inherit* an earlier one's
boundary, and v1.8.11 records that the real-scenario boundary was released by an owner **decision**
rather than by an argument that it was already covered.

**The consequence, stated plainly: `commander.backend = claude` cannot ship to anyone.** Not
because of a technical gap — the adapter is complete, tested, and has run against real scenario
state — but because there is no authorization under which an operator other than the measurer may
turn it on. The PRD's own checklist entry says this is the right posture for a project still
measuring itself, and that judgement looks sound.

**What the decision needs, if the owner wants to make it.** `docs/egress.md` already enumerates
what leaves the machine and is maintained against the adapter as implemented. A standing grant
would need to state: which scenarios or classes of scenario, which models, whether the operator
may change `claude.model`, and what happens to the order logs — which carry the same proprietary
scenario state and are the reason `*.jsonl` is a security-relevant ignore rule.

**One dependency this review adds:** if §7's recommendation to widen the track report is taken,
**`docs/egress.md` and AIC-SEC-2 change first**, and any standing grant would need to postdate that
change. Granting against today's field set and then widening it would be the inference the gate
exists to prevent.

### (b) Repository visibility is unconfirmed

**The position.** The checklist reads *"Repository visibility confirmed — private assumed;
publication needs the same authorization as the hosted backend."* Assumed, for the project's whole
life.

**Why it is not merely paperwork.** The repository is 98 tracked files including
`data/doctrine.txt`, the full order schema, `docs/prd.md`'s 3,827 lines describing a proprietary
platform's internals by component type and field path, and eight measurement CSVs under
`tests/live/data/`. `tools/check-artifacts.ps1` enforces that run logs stay out — and that guard is
built entirely on the premise that the *repository* is the safe side of the boundary. **If the
repository is public, the guard has been protecting the wrong thing.** The archive README makes the
same assumption explicitly when it justifies the tracked CSVs as committable.

**This is a five-minute check** — the hosting provider's setting — and it should be done before
anything else in this document, because several other decisions rest on the answer. It is listed
last only because it is the smallest.

### Cost to resolve

**Minutes, plus an owner's judgement.** Neither needs a run, a grant, or a line of code. (b) should
be checked today regardless of what is decided about (a).

---

## 10. There is no summary artifact — and there should be

### What is established

`docs/prd.md` is **3,827 lines**. It is a genuinely good document — the §Corrections section is a
better record of a project's own errors than most teams produce — but it is not readable by an
owner, and its most important finding is at line 859 in a section called "Corrections verified
in-tree."

**The concrete evidence that this is a real problem, not a stylistic complaint:** §Success metrics
is where a reader looks for the verdict, and the in-engine acceptance figure there was **one run
stale** when this review recomputed it (§3). The freshest, most consequential finding in the
project — item 45's outcome comparison — is reachable only by reading 900 lines of corrections or
by knowing to grep for it. A document that its own maintainers cannot keep synchronised across
3,827 lines is one that needs a shorter front door.

### Recommendation

**The executive summary at the top of this file is the artifact, and it should be extracted rather
than written again.** Two pages, four sections — what we set out to test, what we found, what it
cost, what we recommend — with every number traced to the PRD section or the archive that produced
it.

Three properties it must have, learned from what went stale here:

1. **It carries no number that is not also in the PRD**, so there is exactly one place a figure can
   be wrong.
2. **It is regenerated at each phase close, not maintained continuously.** A summary that drifts is
   worse than none, and this project has now demonstrated twice that continuously-maintained
   duplicate figures drift.
3. **It leads with the outcome finding**, not with the metric table. An owner who reads only the
   first paragraph should learn that the system currently makes the entity worse and that the cause
   is fixable and local.

**Where it should live:** `docs/summary.md`, alongside `docs/egress.md`, which is the existing
precedent for a short document aimed at a specific reader with a specific question.

### Cost to resolve

**~1 hour**, and most of it is done — extract §"Executive summary" above, add the traceability
column, and add a line to `tools/lint-prd.ps1` asserting that the summary's version stamp matches
the PRD's, so a stale summary fails the gate rather than being discovered by a reader.

---

## Appendix A — things found in passing

Small, verified, and not worth a section each.

- **The deployed doctrine is 892 bytes behind the repository's** (`C:\N8RO\data\doctrine.txt` is
  6,932 B; `data/doctrine.txt` is 7,824 B). The difference is exactly C15's CRUISE SPEED block. The
  build warns about this on every build. **The archived runs are unaffected** — the smoke harness
  copies the repo doctrine in, asserts the sizes match, and restores the operator's copy in its
  `finally`, which is correct behaviour. But any *interactive* run of "Mariana Shield" would use the
  stale block, and the warning will be ignored eventually because it fires every time. Worth either
  refreshing the deployed copy or downgrading the warning to fire only when a run is imminent.
- **Three checklist boxes are unticked, not two.** The third is OQ-3 (*is `n8ro-llm` ever
  installed*), which is C6 and correctly dormant — someone else's roadmap. §9 is right that only two
  are governance.
- **Three different denominators are in use for `reject.shape`.** Item 39(d) says 12.5 %, the brief
  says "2 of 8" and "4 of 11", and the pooled archive gives 16 of 55 rejections or 16 of 213
  resolved orders. All four are defensible; none states its denominator in the same sentence.
- **Run `20260806T223956Z` contains only an `orders-previous-run` file** — no engine logs, no
  `orders.jsonl`. It is invisible to both analysers. Harmless, but it means the archive's directory
  count (21) overstates the number of usable runs (20).

## Appendix B — how to reproduce every number in this review

All of it is free, local, and needs no engine and no inference server except where noted.

| Finding | Command |
|---|---|
| Outcome table (§1) | `python tools/analyse-outcomes.py "%USERPROFILE%\Documents\N8RO AI Commander logs" --since 20260806T171041Z` |
| Posture distribution, rejection census, acceptance intervals (§1, §3, §4, §8) | Parse every `*/orders.jsonl` in the archive; group `order.accepted` by `order.posture` and `order.rejected` by `reason` + `detail` |
| Decoder probe (§4) | Eight `POST http://localhost:11434/api/generate` requests with a `format` schema whose bound forbids the requested value; temperature 0, seed 1. **Needs Ollama running; carries no scenario state** |
| `courseDeg` unexercised (§6a) | `grep -rl courseDeg` over the archive → no matches; `own` block present in `20260807T133621Z` only, carrying `headingDeg` |
| `reasonTruncated` (§6b) | `grep -ho "reasonTruncated[^,}]*" */orders.jsonl` → `reasonTruncated":0` × 16 |
| Stage-B check order (§7) | `src/OrderValidatorStageB.cpp:90-127` — B3 returns before B4 is reached |
| Gates (header) | `tools\lint-prd.ps1`; `tools\check-artifacts.ps1`; `tests\bin\release\ai-commander-tests.exe` after `call C:\N8RO\setup.cmd` |
