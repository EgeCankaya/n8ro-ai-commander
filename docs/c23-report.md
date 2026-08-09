<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# C23 — the aircraft that stops flying, and the fix that closes it

**Status:** the fix specified as AIC-ORD-2 clauses 7 and 8 (PRD v1.8.36) is **implemented, tested,
and confirmed in the engine**.
**PRD version this report is written against:** v1.8.38.
**Date:** 2026-08-09.
**Cost:** none in money and none in authorization. **No inference server, no network, no egress
grant** — §§1–9 are re-derivation of logs already on disk plus a Lua change and six offline tests,
and §10.2's two confirming runs are ~4 minutes of local engine time with the commander asserted OFF
and no model in the loop at all.

> **Why this document exists.** C23's evidence was spread across §Corrections items 47(d), 50(c)–(f),
> 51 and 52(e)–(f) and AIC-ORD-2 clauses 7–8 — four revisions of argument in which two claims were
> made and then refuted by the project's own successor runs. A reader could not get the state of the
> defect without reconstructing that sequence. This document is the consolidated version, and it is
> written to be read without the PRD open.
>
> **It is not a summary of the PRD.** Every figure below was recomputed from the archive rather than
> quoted, and **§8 lists the places where the recomputation disagrees with the record.** Two of the
> PRD's statements about C23 do not survive, and one of them is load-bearing enough that it changed
> how the fix had to be implemented.

---

## 1. The defect in one paragraph

The language model reads the aircraft's own position and speed out of the prompt and hands them back
as the `hold` waypoint and the `cruiseSpeedMps`. Tier 1 passes the resulting order straight to
`navigation.requestHoldPosition`. The aircraft flies out, comes back, and **within one cadence window
of being inside the ordered orbit radius its ground speed collapses from 320 m/s to exactly
1.5000 m/s and latches there for the rest of the run.** Every order issued after that copies 1.5 back
and is rejected by Stage B's `safety.minSpeedMps` floor — **58 of the archive's 114 rejections,
50.9 %.** The aircraft does not recover when the fallback ladder retains the order, does not recover
when the ladder publishes its own standing order, and **does not recover when the ladder releases it
to Tier 1 entirely** — because the verb Tier 1 falls back to, `navigation.resumeWaypointFollowing`,
takes no speed argument and therefore cannot ask an aircraft to fly faster than it currently is.

**Nothing in that chain is a model failure.** The snapshot reports 1.5 m/s because the aircraft is
doing 1.5 m/s, and `data/doctrine.txt` explicitly instructs the model to *"start from the speed the
aircraft is already making"* and states that *"re-issuing the current speed is always a defensible
answer."* The model is following the doctrine it was given, on an aircraft that this project's own
deterministic tier parked.

---

## 2. Provenance — what was read, and how

| Source | What it gave |
|---|---|
| `~\Documents\N8RO AI Commander logs\*\orders.jsonl` — **23 runs, 324 resolved orders** | every `order.requested` (with its `own` block), `order.accepted` (with the order document), `order.rejected`, and every `fallback.*` transition |
| `…\2026080[78]*\commander-on-*.log` | the reference script's own `setMode` lines — **what the script actually flew**, as against what the model ordered — and the engine's 10 s `simulationTime` markers |
| `tools/acceptance-report.py` | the rejection census and its intervals, re-run rather than quoted |
| `C:\N8RO\data\resources\missions\stubs\{entityControl,navigation}.lua` | the verb surface — arities and, decisively, **which verbs carry a speed** |
| `src/FallbackLadder.cpp`, `include/CommanderConfig.h`, `include/Snapshot.h`, `data/doctrine.txt` | what the plugin publishes, the shipped bounds, and how `own.speedMps` is derived |

**The archive is outside the repository by design** (order logs carry live scenario state;
`*.jsonl` is a security-relevant ignore rule enforced by `tools/check-artifacts.ps1`). Nothing was
copied in. The re-derivation scripts were scratch and are not committed; §10 states how to reproduce
every figure.

**One methodological point, because the record contains a mistake of exactly this kind.**
§Corrections item 51(d) records that a previous analysis attributed post-release samples to the last
accepted order, because its posture tracker never cleared on `fallback.released`. The state machine
used here clears on `fallback.released` and re-arms only on `order.accepted`, which is why §3.6's
"no order in force" count is 12 rather than 0.

---

## 3. The mechanism

### 3.1 The speed echo is universal, and it is not confined to `hold`

Across **every accepted order in the archive that can be joined to the snapshot it answered — 61 of
61 — `cruiseSpeedMps` equals `own.speedMps` to the last bit of a double.** On all four postures that
carry a speed:

| posture | orders with a joinable snapshot | `cruiseSpeedMps == own.speedMps` exactly |
|---|---|---|
| `defend` | 25 | 25 |
| `hold` | 19 | 19 |
| `engage` | 13 | 13 |
| `ingress` | 4 | 4 |

This is the single most important fact for the fix, and it is why clause 8 says *"never to a speed
read from an order"*. **An order's speed is the aircraft's own current speed handed back to it.** It
can re-command a stall; it can never break one. Any remedy that recovers the aircraft to a speed
taken from an order recovers it to the speed it already has.

### 3.2 The position echo, with its honest denominator

**19 of 19 measurable `hold` orders were issued at 0.00 m from the aircraft's own reported
position** — the model copies `own.latitudeDeg` / `own.longitudeDeg` into the waypoint.

**The denominator matters and the record has been loose about it.** The archive holds **114 accepted
`hold` orders**; only **19** sit in runs whose `order.requested` records carry an `own` block at all
(the snapshot-in-record instrumentation landed at v1.8.28). The other 95 are **unmeasurable, not
compliant.** §Corrections item 51(a) is careful about this — *"four runs carrying `own` blocks"* —
but `docs/summary.md` and item 51's own headline read as *"every `hold` in the archive"*, and 83 % of
archived holds carry no evidence either way. The claim as it should be stated:

> Of the 19 archived `hold` orders whose ordered waypoint can be compared against the snapshot that
> produced it, **19 were at 0.00 m.** No archived `hold` has ever been *measured* at a non-zero
> distance.

The echo is also not `hold`-specific: the four measurable `ingress` orders were issued at **0.0 m and
11.0 m ×3** — the 11.0 m being the residual of the model rounding own position to two decimal places.
**No accepted order anywhere in the measurable archive was issued more than 11 m from the aircraft.**

### 3.3 The fallback ladder publishes the same geometry, by specification

This is the part of C23 that has nothing to do with the model, and it is easy to miss.

AIC-VAL-2 rung 2 specifies the standing order as *"`posture=hold` … **waypoint at the entity's
position at expiry**, `orbitRadiusM = safety.defaultOrbitRadiusM`"*, and `src/FallbackLadder.cpp`
implements exactly that (lines 61–95), inheriting `cruiseSpeedMps` from the last accepted order —
which, by §3.1, is an echoed speed. **So when the backend dies, the commander synthesizes the
degenerate order all by itself.** `hold`, at zero distance, at a speed the aircraft was already
making.

**27 of the 58 archived below-floor samples were taken at ladder level 2.** If clause 7 were ever
removed, rung 2 would re-create C23 on its own with no model in the loop at all.

### 3.4 What the aircraft actually does under the hold — and this corrects the record

§Corrections item 51(b) says the aircraft *"is ordered to hold where it already is, **has nowhere to
fly**, and coasts 320 → ~128 → 1.5000 m/s."* **The first clause is false and the third is a
composite.**

The aircraft does not stay on the point. Measuring the distance from own position at each 20 s sample
to the waypoint of the `hold` **actually in force at that moment**:

| run | aircraft | t (s) | distance to hold point | ordered radius | inside? | `own.speedMps` |
|---|---|---|---|---|---|---|
| `185750` | `RedSu35_02` | 180.4 | 3,588.6 m | 1,000 m | no | 320.0000 |
| | | 200.4 | 3,252.8 m | 1,000 m | no | 320.0000 |
| | | **220.4** | **987.3 m** | 1,000 m | **yes** | **128.0000** |
| | | 240.4 | 935.6 m | 1,000 m | yes | **1.5076** |
| | | 260.4 → 520.4 | 905.6 → 803.9 m | 1,000 m | yes | **1.5000** ×14 |

**Under a `hold` ordered at 0 m the aircraft ranges 3.2–4.4 km out and sustains 320 m/s for up to
sixty seconds with no decay whatever.** The collapse is not a coast; it is an abrupt loss of roughly
8 m/s² that begins when the aircraft is inside the ordered orbit.

The intermediate value in *"320 → ~128 → 1.5"* is run-specific: **128.0** (`185750`), **125.0**
(`133621`), **6.555** (`135722`), **4.274** (`095026`). What replicates exactly is the endpoint.

### 3.5 The latch

**52 of the 58 below-floor samples read exactly 1.5000 m/s.** The other six are the single
transitional sample of each collapse (1.5029, 1.5076, 4.274, 6.555, and two others). The aircraft
continues to drift at ~1.5 m/s — the distance to the hold point changes by ~30 m per 20 s sample and
oscillates rather than trending — so it is *moving*, very slowly, and not frozen.

**1.5000 is not derivable from anything this project ships.** `include/CommanderConfig.h` already
records that *"no minimum, stall, or performance envelope exists anywhere in the shipped platform
data"*. It is a property of the engine's navigation or mover, which is closed-source in this tree.
**This report does not claim to know why the aircraft stops at 1.5 m/s, only that it does, that it
does so under a hold it is inside, and that nothing above the engine recovers it.**

### 3.6 Why it survives all three rungs — and this one is readable off the stub surface

| ladder level at the sample | below-floor samples |
|---|---|
| 0 — live (an accepted order in force) | 4 |
| 1 — retained | 15 |
| 2 — standing order | 27 |
| 3 — **released; no order in force at all** | **12** |

At level 3 `aiCommander.isValid` is false, the reference script is on its own behaviour, and it calls
`navigation.resumeWaypointFollowing(entityId)`. **The generated stub gives that verb one argument and
no speed parameter.** It is the only verb the no-order path called. So the commander getting entirely
out of the way handed a stalled aircraft to the one verb in the namespace that has no way to un-stall
it — and the speed stayed at exactly 1.5000 through the release and to the end of the run.

**That is a mechanism read off the shipped interface, not inferred from an outcome**, and it is what
makes clause 8 necessary independently of clause 7.

### 3.7 The ratchet

Because the ordered speed is the observed speed (§3.1), a decaying aircraft is issued orders that
re-command the decay:

```
133621 / RedSu35_02:  hold@320.0000 → hold@320.0000 → hold@125.0000 → [1.5029, rejected thereafter]
185750 / RedSu35_02:  hold@320.0000 → hold@320.0000 → hold@128.0000 → [1.5076, rejected thereafter]
```

The 125 and 128 orders were **accepted** — they clear the 50 m/s floor — so the ratchet passes
through the validator intact. The floor only engages one window later, at which point every
subsequent order is rejected and rung 1 retains the last accepted `hold`.

---

## 4. What it costs

Recomputed with `tools/acceptance-report.py` over 23 runs and 324 resolved orders:

| | before the fix (23 runs) | after (25 runs, v1.8.39) |
|---|---|---|
| in-engine acceptance | **64.8 % [59.3, 70.0]**, n = 324 | **71.1 % [66.4, 75.5]**, n = 398 |
| rejections | 114 | 115 |
| **of which the C23 stall floor** | **58 (50.9 %)** | **58 (50.4 %) — unchanged, and now frozen** |
| of which `roster` | 10 (8.8 %) | 10 (8.7 %) |
| acceptance excluding both non-model classes | **82.0 % [76.8, 86.5]**, n = 256 | **85.8 % [81.5, 89.3]**, n = 330 |

**Half of C17's acceptance shortfall was this one defect**, and every one of those 58 rejections is
the validator correctly refusing an accurate report of a broken state.

***(v1.8.39 — the two confirming three-arm runs contributed 74 resolved orders and ONE rejection, a
`track`.)*** **The stall-floor count did not move.** All 58 predate the fix, so the figure is now a
historical total rather than a live proportion, and its share will fall with every further run. A
reader should take 50.4 % as *"half of every rejection this project ever recorded came from one
defect that no longer occurs"*.

All 58 are on **one aircraft**: `RedSu35_02`, in four runs. So is every below-floor *sample*. This is
worth stating plainly because the record says otherwise — see §8.

---

## 5. Question 1 — is *arrival* necessary?

**The record left this open in the form *"only when a hold is in force" is settled at 39 of 39;
"only when it arrives" is not.'"* The archive answers the successor question, and **the answer
contradicts the framing the PRD had settled on.**

### 5.1 The question as posed is malformed; its successor is not

§Corrections item 51(b) retired the arrival frame on the grounds that *"there is no arrival — the
aircraft is at the waypoint the moment the order is issued."* §3.4 shows that is false: the aircraft
leaves and returns. **The well-formed question is not "does it arrive at the point" but "is the
collapse conditional on the aircraft being inside the ordered `orbitRadiusM`?"**

### 5.2 The archive contains a natural experiment, twice

In two runs the aircraft moved into the radius. **In the other two, the radius moved around a
stationary aircraft** — a new `hold` was accepted with a larger `orbitRadiusM` while the aircraft's
position and speed were unchanged:

**`095026` / `RedSu35_02`**
- t = 143.4 — `hold` accepted, `orbitRadiusM` **1,000**.
- t = 160.4 — aircraft **3,252.7 m** from the point. **Outside.** 320.0000 m/s.
- t = 163.3 — `hold` accepted, `orbitRadiusM` **8,000**. Same aircraft, same speed, same place.
  **Now inside.**
- t = 180.4 — **4.274 m/s.** Elapsed since the radius changed: **17.1 s.**

**`135722` / `RedSu35_02`**
- t = 163.6, 183.8, 203.5 — three `hold` orders, `orbitRadiusM` **1,000** each.
- t = 180.4 / 200.4 / 220.4 — **3,588.6 / 3,252.8 / 3,253.1 m.** **Outside for sixty seconds under a
  continuously in-force hold, at 320.0000 m/s, with no decay at all.**
- t = 223.9 — `hold` accepted, `orbitRadiusM` **8,000**. **Now inside.**
- t = 240.4 — **6.555 m/s.** Elapsed: **16.5 s.**

**This separates the two candidate conditions.** *"A `hold` is in force"* was true for sixty seconds
without a collapse. *"The aircraft is inside the ordered orbit radius"* became true and the collapse
followed inside one cadence window. The discriminating variable is a field of the order document,
changed by the model, with the aircraft untouched.

### 5.3 The finding, stated at the strength the evidence carries

> **In 4 of 4 archived collapses the onset falls in the first 20 s sample after the aircraft is both
> under a `hold` and inside that order's `orbitRadiusM`. There is no archived interval in which an
> aircraft outside the ordered radius under a hold lost any speed at all: it sustained 320.0000 m/s
> for up to sixty seconds. In two of the four, the aircraft did not move — the ordered radius grew
> around it.**

What that does **not** establish: *why*. The engine is closed-source here. The furthest this report
goes is the observable — **the ordered `cruiseSpeedMps` is not maintained once the aircraft is inside
the ordered orbit** — and it does not attribute that to the hold controller, the mover, or anything
else it cannot see.

What it also does not establish: that a `hold` ordered *far away* would be safe once flown in. The
counter-instance still does not exist in the archive, for the reason §Corrections item 51(f) gives —
the model does not order distant holds. But **§5.2 is evidence that it would not be safe**, because
two of the four collapses happened to an aircraft that was 3.2 km from the point and merely had the
radius enlarged around it. That is nearer to a distant hold flown in than anything the record
previously had.

### 5.4 Why this changed the implementation

Clause 7 keys on *"where the ordered waypoint lies within the ordered `orbitRadiusM` of the
aircraft's **current position**"*. Read as a test at order-issue time, it would fire on 19 of 19
archived holds and miss the distant-hold case entirely. **Read as a per-tick test on the current
distance — which is what "current position" says — it fires exactly when the archive says the
pathology begins, including on a hold flown in from 55 km.** The implementation is per-tick. See §7.1.

---

## 6. Question 2 — the escape-condition hypothesis

§Corrections item 50(d) offered, *"explicitly as a hypothesis and not as a finding"*, that **a new
accepted order arriving before the floor is crossed prevents the latch.** The archive lets this be
sharpened, and the sharpened version is better supported and says something different.

### 6.1 The "survivors" were never exposed

Item 50(d) counted *"five arrivals across three runs — three collapsed and two did not."* Reading the
script's own `setMode` lines rather than the order log, the picture is not three-versus-two. It is
**exposure**:

| run | aircraft | time in `hold` **mode** | outcome |
|---|---|---|---|
| `095026` | `RedSu35_02` | ~320 s | collapsed |
| `135722` | `RedSu35_02` | ~360 s | collapsed |
| `185750` | `RedSu35_02` | ~360 s | collapsed |
| `095026` | `RedSu35_01` | **~20 s** | did not |
| `135722` | `RedSu35_01` | **~20 s** | did not |
| `185750` | `RedSu35_01` | **never entered `hold`** | did not |

*(Mode transitions are bracketed by the engine's 10 s `simulationTime` markers, so the 20 s figures
are 10–30 s.)*

The onset delay measured in §5.2 is 16.5–17.1 s. **The two "survivors" held for about one cadence
window — at or inside the observed onset delay — and were then pulled out.** They are not
counter-instances to anything; they are under-exposed cases. **That removes the only evidence the
archive contained against the onset claim.** There is no archived instance of a `hold` flown inside
its own orbit radius for more than one cadence window that did not end at 1.5000 m/s: **3 of 3.**

### 6.2 What actually pulled them out, and it is not "a new order"

Both survivors were pulled out by an ordered **`defend`**, and stayed there — `RedSu35_01` flew
`defend (ordered)` for the last **490 s** of both runs at ~150 m/s, never once below the floor.

`defend` is **the one posture in which Tier 1 already discarded the ordered speed.** The branch reads
`flyDefend(entityId, kDefendSpeedMps)` — a script constant of 320 m/s — and never passes `speedMps`
at all. So:

> The escape condition is **not** "a new accepted order". By §3.1 every accepted order carries the
> aircraft's own current speed, so a new order can only re-command the speed the aircraft already
> has. What the two escapes have in common is that **navigation passed to a verb carrying a speed
> Tier 1 chose rather than one the model echoed** — which is precisely clause 8's rule, operating by
> accident, in the one posture that already implemented it.

**Zero below-floor samples in the entire archive were taken under `defend`.**

### 6.3 What this is worth, honestly

The `flyDefend(entityId, kDefendSpeedMps)` half is a **source-level fact** — one line, no inference.
The outcome half is **n = 2 aircraft-runs**, with obvious confounds: `RedSu35_01` was also under
munition threat, was in a different part of the fight, and spent much of the run on the defensive
reflex. **No rate is claimed and none is available.** The hypothesis is upgraded from *"a new order
rescues it"* (which §3.1 makes impossible) to *"a Tier-1-chosen speed rescues it"* (which the source
supports and two runs are consistent with), and it is **still not a finding.** It is, however, the
same conclusion clause 8 reaches from a different direction, which is why it is recorded rather than
relied on.

---

## 7. The fix

### 7.1 Clause 7 — Tier 1 owns `hold`'s geometry

`lua/ai_commander_interceptor.lua`, new `flyHold`:

- **Outside the ordered `orbitRadiusM`:** unchanged — `navigation.requestHoldPosition`. §5.2 measures
  this state as healthy (320 m/s sustained for up to 60 s), and clause 7 is scoped to the case the
  archive indicts and no wider.
- **Inside it:** Tier 1 flies the orbit itself — `navigation.requestGoTo` to a point on the ordered
  circle at a fixed **lead angle** off the aircraft's own angular position, **at the ordered
  `cruiseSpeedMps`.** The model still supplies where and how wide; only the geometry moves.
- **The test is per-tick**, on the *current* distance, per §5.4.
- At distance exactly 0 — the case the model produces every time — `bearingAndDistance` returns 0.0,
  so the first leg is due north of the ordered point and the orbit builds from there. Arbitrary,
  deterministic, and the aircraft is flying by the next tick.
- The lead angle is recomputed from current position each tick rather than accumulated, so the aim
  point advances only as the aircraft advances. An accumulating angle would spin it at the tick rate.
- All aircraft orbit the same way round. `crank` deliberately splits the section left and right for
  lateral separation; an orbit is the opposite case, where two aircraft turning opposite ways about
  one point meet head-on twice per circuit.

### 7.2 Clause 8 — Tier 1 will not leave an entity below flying speed

Implemented as a **structural** property rather than as a check at each call site. Every navigation
call in the script now goes through one of four wrappers — `navGoTo`, `navTrackTarget`,
`navHoldPosition`, `navResume` — and those wrappers are the entire enforcement.

**Why a wrapper.** The clause reads *"under any posture and whether or not an order is in force"*.
That is a property of the whole file. There are **eleven navigation call sites** across six postures,
the winchester egress, the defensive reflex and the no-order path. Enforced by eleven guards it would
be one forgotten guard away from being false again — and §Corrections item 50(e) is this project's
own record of a specification landing and the code never following, undetected for four revisions
behind a passing test.

**The state machine.** `entityControl.getVelocityNed` is read once per tick, before any verb is
issued. Ground speed is `‖(velN, velE, velD)‖` — deliberately the same definition
`include/Snapshot.h`'s `groundSpeedMps` uses for `own.speedMps`, so the script's floor and Stage B's
bound describe the same quantity. Below **50.0 m/s** the entity latches into recovery; the latch
clears at **150.0 m/s**.

**What recovery changes.** Verbs that carry a speed keep the posture's geometry and get
`math.max(ordered, 300.0)`. The two verbs that *cannot* carry Tier 1's speed —
`resumeWaypointFollowing`, which has no speed parameter, and `requestHoldPosition`, whose speed was
measured not being honoured inside the orbit — are **replaced outright** by a 25 km level leg along
the current course at 300 m/s. There is no better argument to give them.

**What recovery deliberately does not change.** It suppresses no shot. AIC-VAL-2 requires that no
rung leave the entity less capable than the same entity with no commander installed; a recovery that
also grounded the launch path would breach that requirement while closing this one, which is the
shape of C21. `ReferenceScriptStillFiresWhileRecoveringFromAStall` pins it.

**The backstop.** `engage` and `crank` issue no navigation verb at all when the ordered target is
empty. A tick that commands nothing leaves a stalled aircraft stalled, and *"under any posture"*
includes the postures that decline to steer. `ensureRecoveryNavigated` runs after the launch decision
— after, so it cannot suppress a shot.

**The defensive reflex is its own recovery.** It commands `kDefendSpeedMps = 320`, which already
exceeds the recovery speed, so an aircraft that is both stalled and under fire is turned cold and
re-accelerated by one call. Clause 8 does not need to preempt survival to be satisfied.

### 7.3 Decisions that go beyond the literal text of the clauses

**§Scope authority requires the PRD to change before the code for any FR-behaviour change.** Clauses
7 and 8 are already specified, so implementing them needs no revision. These five are judgement calls
inside that licence, listed so the owner can disagree with any of them:

| # | Decision | Argument | If the owner disagrees |
|---|---|---|---|
| 1 | **Hysteresis** — recovery clears at **100 m/s**, not at the 50 m/s floor. *(v1.8.38 — was 150, and a run said no; see §10.2)* | With one threshold the aircraft crosses 50, navigation returns to the posture that stalled it, it decays, and it crosses again. That satisfies the letter of clause 8 and none of its intent. This is *how you implement* the clause without oscillation, not a new obligation. **The threshold now has a measurement on both sides of it**: 2× the floor below, and ~24 % clear of the 132.2–146.5 m/s band a recovering aircraft was measured settling in above | It is the one item here a reader might call an FR behaviour. A revision would state the second threshold in clause 8 |
| 2 | **`math.max(ordered, 300)` rather than substituting 300** | Clause 8 says re-accelerate to Tier 1's own value. Plain substitution would pull the defensive pump's 320 *down* to 300. `max` honours the clause's purpose — the floor is Tier 1's — and can never cause a stall | Substitute unconditionally and let the reflex re-issue |
| 3 | **The floor is a script constant (50.0), not read from `safety.minSpeedMps`** | The `aiCommander` namespace does not publish it, and it should not have to: a recovery that only fired where the *validator* would reject an order would not fire at all with the commander absent — which is where 12 of the 58 below-floor samples sit. The script's floor is a statement about flight; the validator's is a statement about orders | Add a getter to AIC-API-1 — **which needs a PRD revision, so it was not done here.** The duplication is a real divergence risk and is logged in §8 |
| 4 | **Orbit lead angle 30°, same direction for the section** | Clause 7 says *"from the arithmetic the script already carries"*; this is `pointAtBearing`, unchanged | — |
| 5 | **The wrapper refactor** touches every posture branch of a load-bearing file | Argued in §7.2. The diff is mechanical: eleven direct calls become eleven wrapper calls, and the guards those sites carried move into the wrappers | Revert to per-site guards and accept the enforcement risk |

### 7.4 Where the fix does not reach — stated rather than discovered later

1. **Boundary chatter.** Tier 1 takes the geometry inside the radius and hands it back outside. An
   aircraft that overshoots the circle could alternate between the two on successive ticks.
   **Assessed as benign:** both branches keep the aircraft flying — the engine's hold controller
   sustained 320 m/s in exactly that state — and clause 8 sits under both. A radius hysteresis is the
   remedy if a run ever shows it matters; it was not added because there is no evidence to size it
   against.
2. **A distant hold flown in is covered by clause 7's per-tick test, not by clause 8** — the takeover
   happens at radius entry, before the speed can fall. Clause 8 remains the backstop if that is
   wrong. ***(v1.8.38 — measured. §10.2 Q1: a hold ordered 20 km away and flown in decelerates at
   −20 m/s² from the first sample after the aircraft is inside the radius. The per-tick test takes
   the geometry on that same sample, so the collapse never begins.)***
3. **`resumeWaypointFollowing` with an exhausted route still ends the leg.** Once recovered, the
   script hands navigation back and the aircraft may decay again, recovering again. That is a
   sawtooth above 50 m/s rather than a latch at 1.5, which is what clause 8 asks for — but it is not
   *good* flying, and the real answer is a scenario whose routes do not run out.
4. **The offline suite asserts commands, not outcomes.** See §9.

### 7.5 The two layers the record rejected — reconsidered, and one of them deserves a second look

**Snapshot — agree, do not reopen.** Suppressing or clamping a low `own.speedMps` puts a value in the
prompt that does not match reality. That is C15 and C18 in reverse, and it would make the one
instrument that told the truth about this defect start lying. Nothing found here changes that.

**Doctrine — the rejection is right about what it rejected, and there is a different change it did
not consider.** The record rejects doctrine because *"a safety property must not rest on the model
reading an instruction"*, citing 4 of 5 models measured violating an explicit prompt rule. That
argument is about **adding a prohibition**. But `data/doctrine.txt` currently **instructs the echo**:

> *"Start from the speed the aircraft is already making. The situation block reports it as
> own.speedMps … it is the number to adjust up or down rather than to replace with a guess.
> **Re-issuing the current speed is always a defensible answer.**"*

**Deleting an instruction is not the same intervention as adding one**, and it does not depend on
obedience to work — it merely stops actively causing the behaviour. It is also **not a fix**: the
model may echo anyway, so it replaces nothing and clauses 7 and 8 stand regardless.

***(v1.8.38 — done, and QUALIFIED rather than deleted, which is the part worth arguing about.)***
The sentence is not merely an encouragement; the paragraph it sits in is **C15's fix**. *"Read
`own.speedMps`, not a velocity component"* is what stopped the model copying `velE` and emitting
−220.0, and deleting the paragraph would reopen a closed defect to close an open one. So the anchor
stays and only the false universal goes: *"always a defensible answer"* becomes *"a defensible answer
whenever the aircraft is flying normally, which is almost always"*, followed by the one case where it
is not. **`always` was measurably false** — 61 of 61 accepted orders took that advice, and 58 of them
were rejected for it — **so this is a correction of a false statement in the same block as the other
one, not a new prohibition.** It is still not a remedy for C23 and is not recorded as one.

---

## 8. Where the record does not survive re-derivation

| # | Claim in the record | Verdict |
|---|---|---|
| 1 | *19 of 19 archived `hold` orders issued at 0.0 m* (item 51(a)) | **Reproduced**, with the denominator corrected: 19 of the 19 that are **measurable**, out of **114** archived holds. `docs/summary.md`'s *"All 19 archived `hold` orders"* reads as the total and is not |
| 2 | *`data/doctrine.txt` states the speed rejection threshold as "a value at or below zero"* while it is `safety.minSpeedMps = 50.0` (item 52(f)) | **Verified false, and CORRECTED in v1.8.38** — see §7.5. **The replacement names the shape and not the number**, because writing `50` into a cacheable block with no mechanism keeping it in step with the config would re-create this exact defect the next time an operator lowers the bound, which `CommanderConfig.h` explicitly instructs them to do. The coupled pins moved with it: doctrine 9,782 → 10,612 bytes, prefix 11,600 → 12,430. **Growth, so the cached block moves further above Haiku's 4,096-token minimum rather than toward it.** Recorded as a correction of a false statement and **not** as a remedy for C23 |
| 3 | *39 of 39 samples below `safety.minSpeedMps`, across three runs and two aircraft, had `hold` in force* (item 50(c)) | **Not reproduced as stated.** There are now **58** below-floor samples across **four** runs: **46 under `hold`, 12 under no order at all.** The "39" was the two runs available at v1.8.34 (21 + 18). *"Three runs"* was two; ***"two aircraft" was one*** — every below-floor sample in the entire archive is on `RedSu35_02` |
| 4 | *45 under `hold`, 12 under none* (item 51(d)) | **Reproduced** for the three 2026-08-08 runs. The fourth run (`133621`) adds one more `hold` sample, giving 46/12 over the whole archive |
| 5 | *The collapse profile is 320 → ~128 → 1.5000* | **The endpoint replicates exactly** (52 samples at 1.5000). The intermediate value is run-specific: 128.0, 125.0, 6.555, 4.274. Quoting "~128" as the profile generalises one run |
| 6 | *"There is no arrival — the aircraft is at the waypoint the moment the order is issued"* (item 51(b)) | **Refuted.** §3.4, §5.2. The aircraft ranges 3.2–4.4 km out and the collapse begins on re-entry to the ordered radius |
| 7 | *"has nowhere to fly, and coasts 320 → …"* (item 51(b)) | **Refuted.** It flies, at 320 m/s, for up to sixty seconds under the same order. The loss is ~8 m/s², which is not a coast |
| 8 | *"the only hold beyond 10 km sits at t = 600.1, the final sample of a run"* (item 50(d)) | **Not reproducible.** By the method in §2 — waypoint of an accepted order against the `own` block of the request it answered — **no accepted order anywhere in the measurable archive is more than 11.0 m from own position.** The figure most likely came from the same analysis that produced the refuted item 50(d) claims. Recorded rather than quietly dropped |
| 9 | *Five arrivals, three collapsed, two did not; both survivors entered at ~148 m/s* (item 50(d)) | **Superseded.** §6.1: the two "survivors" were in `hold` mode for ~20 s against the collapsers' 320–360 s, and one aircraft-run never entered `hold` at all. They are under-exposed, not counter-instances |
| 10 | *`entityControl.getVelocityNed` is on the shipped stub surface and unused by the script* (clause 8) | **Verified on both counts** against `C:\N8RO\data\resources\missions\stubs\entityControl.lua` and the pre-fix script |
| 11 | *58 of 114 rejections (50.9 %) are the stall floor; 64.8 % [59.3, 70.0]* | **Reproduced exactly** by re-running `tools/acceptance-report.py` |
| 12 | *`tools/lint-prd.ps1` — 10 checks* | It reports **11**. Cosmetic; noted so the next reader does not treat it as drift |

### 8.1 Divergences found near what was touched, which nothing checks

§Corrections item 50(e) records that **FR↔UAC parity is checked mechanically and spec↔code agreement
is checked by nobody.** Looking for more in the neighbourhood:

1. **AIC-ORD-2's posture table still reads `hold` → `navigation.requestHoldPosition(...)` unqualified**,
   while clause 7 — eleven paragraphs below it in the same requirement — says it SHALL NOT be used in
   the at-position case. The table and the clause are in textual tension. **Not edited here** (a PRD
   change needs its own revision); recommended for the next one, as a pointer in the `hold` row.
2. **AIC-VAL-2 rung 2 specifies the defect.** *"Waypoint at the entity's position at expiry"* is the
   degenerate geometry, written down as a requirement and correctly implemented. The code matches the
   spec, so no linter could ever have caught it. **Clause 7 is what makes rung 2 safe**, and that
   dependency is not stated anywhere in AIC-VAL-2.
3. **The doctrine instructs the behaviour the fix works around** (§7.5). Not a divergence — a
   spec/behaviour interaction nobody has recorded.
4. **The script's speed floor duplicates `safety.minSpeedMps`** with no mechanical link (§7.3, item 3).
   This is a divergence *waiting* to happen: an operator who lowers the config bound for rotary-wing
   platforms, exactly as `CommanderConfig.h` instructs, gets a script that still recovers at 50 m/s.

---

## 9. What the tests prove, and what they cannot

Six tests in `tests/ReferenceScriptTests.cpp`, driving the real Lua file in the engine's own Lua VM
against recording stubs. No engine, no scenario, milliseconds. **Suite: 162/162.**

| test | pins |
|---|---|
| `…FliesHoldItselfWhenTheOrderedPointIsInsideTheOrbit` | clause 7: `requestHoldPosition` not reached; the aim point is on the **ordered** radius (8,000 m) at the **ordered** speed (320) |
| `…LeavesADistantHoldToTheEngineUntilItIsInsideTheOrbit` | clause 7's **boundary**: a hold 55 km away still goes to the engine, radius unaltered. Without this, "Tier 1 always computes hold" would pass everything else |
| `…RecoversFromTheArchivedStallUnderEveryPostureAndUnderNone` | clause 8, **8 cases** — no commander, released by the ladder, `hold`, `ingress`, `rtb`, `defend`, `engage` with no target, `crank` with no target — driven at the **archived** stall velocity `velN 0.0544, velE 1.4990, velD 0.0`. Asserts a speed ≥ 300 was commanded **and** that neither speedless verb was reached |
| `…StillFiresWhileRecoveringFromAStall` | AIC-VAL-2: recovering must not ground the aircraft |
| `…HoldsTheRecoveryUntilTheAircraftIsProperlyFlyingAgain` | the hysteresis band: still recovering at 60 m/s, released at 200 |
| `…DoesNotInventAStallWhenTheVelocityIsUnreadable` | unknown ≠ stopped; with no velocity the script behaves exactly as it did before v1.8.36 |

### 9.1 The falsification check — the part that makes them regressions

The tests were run against the **pre-fix** script (`git checkout -- lua/…`, tests unchanged):

```
[FAIL #157] ReferenceScriptFliesHoldItselfWhenTheOrderedPointIsInsideTheOrbit
            … must NOT be satisfied by navigation.requestHoldPosition — expected 'false', got 'true'
[FAIL #159] ReferenceScriptRecoversFromTheArchivedStallUnderEveryPostureAndUnderNone
            … the highest speed any verb carried this tick was -1 m/s
[FAIL #161] ReferenceScriptHoldsTheRecoveryUntilTheAircraftIsProperlyFlyingAgain
Summary: 159/162 passed, 3 failed
```

**The other two pass before and after, and that is correct** — they pin behaviour that was already
right and must stay right (the distant-hold boundary; the unreadable-velocity case). A test that only
ever fails on the old code is a regression; a test that never fails on the old code is a guard rail.
Both are wanted, and which is which should not be a matter of opinion.

`ReferenceScriptRecoversFromTheArchivedStall…` stops at its first failing case, so only the
no-commander row is shown above. Of its eight rows, **seven fail pre-fix and one — `defend` — passes,
because the pre-fix branch already read `flyDefend(entityId, kDefendSpeedMps)` and discarded the
ordered speed.** That row is labelled in the source as a positive control, and it is the same fact
§6.2 rests on.

### 9.2 What the offline suite cannot do, and this qualifies clause 8's acceptance criterion

**These tests assert the command, not the achieved speed.** The archive shows the two are not the
same thing: `RedSu35_01` flew `defend (ordered)` for 490 s under a commanded 320 m/s and sustained
about 150.

AIC-ORD-2's clause 8 acceptance criterion reads *"an entity whose ground speed is below
`safety.minSpeedMps` **recovers** above it within one cadence window … asserted in the offline Lua
suite"*. **The named instrument cannot assert recovery — only that a recovery was commanded.** That
is a gap between the criterion's words and its stated instrument, and it is recorded here rather than
papered over. **Confirming that the aircraft actually accelerates requires a run**, and that run is
§10.2.

***(v1.8.38 — the run was made and the gap is closed by measurement rather than by argument.)*** From
exactly 1.5000 m/s a commanded 300 m/s put both aircraft above the floor in **3.1 s** against the
criterion's 20 s, at **+20 m/s²**. **The criterion's wording and its instrument still disagree**, and
that is not fixed by one run — the offline suite will still only ever assert the command. The honest
statement is that the criterion is now **satisfied in fact on two aircraft in one run**, and that a
suite assertion cannot carry it. Whether the criterion should name the run as its instrument is a PRD
question, not this document's.

---

## 10. Reproducing this, and the one run that would close what remains

### 10.1 Every figure above

```powershell
# Acceptance, rejection census, intervals — reads the archive in place, writes nothing.
python tools\acceptance-report.py

# Build and test (setup.cmd must be called first, at top scope, from a .cmd file).
#   call C:\N8RO\setup.cmd
#   call C:\N8RO\dev\setup-dev.cmd
#   "%N8RO_RELEASE_MSBUILD_CMD%" ai-commander.vcxproj       /p:Configuration=Release /p:Platform=x64
#   "%N8RO_RELEASE_MSBUILD_CMD%" tests\ai-commander-tests.vcxproj /p:Configuration=Release /p:Platform=x64
# The unit exe needs setup.cmd sourced too, or it exits -1073741515.

powershell -ExecutionPolicy Bypass -File tools\lint-prd.ps1
powershell -ExecutionPolicy Bypass -File tools\check-artifacts.ps1
```

The §3–§6 tables come from `orders.jsonl` joined on `(entityId, serial)` between `order.requested`
(for `own`) and `order.accepted` (for the order), with an in-force state machine that **clears on
`fallback.released`**, and from the `[mission script] <id> -> <mode>` lines in `commander-on-*.log`
bracketed by the engine's `simulationTime=` markers.

### 10.2 The run that closes the rest — **run, and it answers both questions**

*(v1.8.38. Two runs, `tools/run-c23-probe.ps1` driving `tools/c23-hold-probe.lua`, archived at
`20260809T102936Z-c23-probe` and `20260809T104112Z-c23-probe`. **Commander OFF and asserted off** —
the harness refuses to start if `data/config/plugins/ai-commander.cfg` exists — so `aiCommander` is
nil throughout and no result here can be an artifact of a published order. **No model, no network,
no grant, no cost.** The release tree is restored byte-for-byte in a `finally` block.)*

The probe is an **instrument, not a Tier-1 script**: it implements no requirement, reads no order and
never fires. It drives `navigation` directly from simulation time and logs `getVelocityNed` and
`getPositionGeodetic` at **1 Hz**, which is what turns the archive's 20 s sampling bracket into a
number.

#### Q1 — does a `hold` ordered *far away* also stop the aircraft? **Yes, and the trigger is the orbit boundary.**

Run 1. `requestHoldPosition` at a point **20 km away**, `orbitRadiusM` 5,000, `cruiseSpeedMps` 320.
Both aircraft, identically:

| t (s) | distance to hold point | inside? | ground speed |
|---|---|---|---|
| 10.2 → 61.2 | 21,123 → 5,260 m | **no** | **320.0000 throughout — fifty seconds, no decay** |
| **62.1** | **4,972 m** | **yes** | 320.0000 |
| **63.0** | 4,693 m | yes | **302.0000** |
| 65.2 … 76.0 | 4,079 → 2,466 m | yes | 258 · 240 · 222 · 198 · 180 · 162 · 138 · 120 · 102 · 78 · 60 · 42 |
| 79.0 | 2,414 m | yes | **4.2744** |
| 83.2 → 84.1 | 2,405 m | yes | **1.5416 → 1.5169** |

**The deceleration begins in the first sample after the aircraft crosses inside the ordered radius**,
and it is a clean **−20 m/s²** until the last few m/s, where it becomes an exponential approach to
1.5000. The archive's 16.5–17.1 s "onset delay" was **entirely an artifact of its 20 s sampling**;
the real delay is under one second.

**So the 0 m echo is not the necessary condition.** It determines *when* the collapse starts —
immediately, because the aircraft begins inside the orbit — not *whether*. A hold ordered 20 km away
and flown in stops the aircraft exactly the same way.

**This is the case §5.3 said the archive could not settle, and it settles it in the direction that
matters for the fix: clause 7's per-tick test is not a nicety, it is the only reading that covers
this.** An order-issue-time test would have handed run 1's order straight to the engine.

**A detail worth its own line, because it is a replication rather than a coincidence.** The probe
reads **4.2744 m/s** at t = 79.0. The archived `095026` run reads `own.speedMps` = **4.274373306340072**
at t = 180.4. **Same curve, same point on it, four decimal places, two years of scenario time apart
and with a language model in one and not the other.**

**The damage confound is ruled out off the record, not assumed away.** The only detonations against
either Su-35 in run 1 occur at t ≈ 85 and t ≈ 149 — **after** the collapse — and the first is the one
that destroyed `RedSu35_01`. Nothing hit either aircraft while it was slowing.

#### Q2 — is a commanded 300 m/s *achieved*, or only issued? **Achieved, in 3.1 seconds.**

Run 2. Phase A holds at the aircraft's **own position** from t = 0 — the archived order, and
AIC-VAL-2 rung 2's published geometry — and phase B then issues exactly what the fixed script issues
while recovering: `requestGoTo` 25 km ahead at **300 m/s**, re-issued every tick.

**Phase A reproduces C23's onset with no model and no commander in the loop at all:** 220.0000 at
t = 0.1, **1.5000 by t = 23.1**, latched. That is §3.3's claim demonstrated rather than argued —
**the fallback ladder's own standing order stalls the aircraft by itself.**

**Phase B, from exactly 1.5000 m/s, both aircraft identical to four decimals:**

| t (s) | ground speed |
|---|---|
| 30.0 | **1.5000** — recovery commanded |
| 31.2 | 23.5000 |
| 32.2 | 42.5000 |
| **33.1** | **60.5000 — above `safety.minSpeedMps` 3.1 s after the command** |
| 47.1 | 299.1215 |
| 50.1 | **299.9563 / 299.9822** |

**Clause 8's acceptance criterion — "recovers above it within one cadence window" — is met with 3.1 s
against 20 s.** The acceleration is **+20 m/s²**, the exact mirror of the deceleration. §9.2's
caveat is discharged: the recovery is achieved, not merely commanded.

#### And the run found a defect in the fix, which is the best argument for having run it

After peaking at 299.98, **both aircraft settle at 132.2–146.5 m/s** under a *continuing* 300 m/s
command. The archive says the same thing from the other side: `RedSu35_01` flew `defend (ordered)`
for 490 s under a commanded 320 and held about 150.

**`kResumeFlyingSpeedMps` was 150.0 — inside that band.** The recovery latch clears at that
threshold, so in normal flight it **might never have cleared**: Tier 1 would have kept navigation for
the rest of the run, `hold` would never have orbited and `resumeWaypointFollowing` would never have
run. The value had nothing behind it but "half of Tier 1's cruise".

**It is now 100.0** — 2× the floor, so the entry/exit pair cannot chatter, and ~24 % below the lowest
settling speed observed. A new assertion at **132.2 m/s** pins the ceiling, so a future raise back
into the band fails a test instead of silently latching. **Neither the offline suite nor the archive
could have found this**, and it is the one thing in this report that a run bought outright.

### 10.3 The fix under a commander — two three-arm runs, and the comparison this project never had

*(v1.8.39. `20260809T143428Z-local` and `20260809T150635Z-local`, three arms × 600 s each at the
shipped default `qwen2.5:7b-instruct-q8_0`. **22 checks / 0 failed** both times. No network, no cost,
no grant. Everything in §§1–10.2 confirmed clauses 7 and 8 by offline test and by a **commander-off**
probe; **neither clause had ever executed under a model issuing real orders.**)*

**The model did not change, and that is what makes this a comparison rather than an anecdote.**
Across the whole archive, **52 of 52 measurable `hold` orders are still issued at 0.00 m from the
aircraft's own reported position** — the 19 from §3.2, plus **33 more in these two runs, every one
still at 0.00 m**. The speed echo is likewise untouched. **So the independent variable is Tier 1 and
nothing else.** Every previous attempt at this comparison either moved two variables (§Corrections
item 45(b)) or had to switch the model off entirely (item 47(a)).

| | archive, 3 runs | run 1 | run 2 |
|---|---|---|---|
| `RedSu35_02` minimum ground speed | **1.5000 m/s**, latched 320–360 s | **320.0000** — held for the full 600 s | **220.0000** |
| accepted `hold` orders on it | 19 measurable, all at 0 m | **20**, all at 0 m | **13**, all at 0 m |
| samples below `safety.minSpeedMps` | 21 / 18 / 18 | **0** | **0** |
| rejections | 21 / 18 / 18, all the floor | **0 of 38** | **1 of 37** (a `track`) |
| `navigation.requestHoldPosition` calls | every hold | **0** | **0** |
| clause 8 recoveries | — | **0** | **0** |

**Clause 8 never fired, and that is the designed relationship rather than a gap in the evidence:**
clause 7 prevented the onset on all 33 holds, so the safety net had nothing to catch. **The honest
consequence is that clause 8 remains unexercised in a commanded scenario** — its evidence is the
offline suite plus §10.2's probe, where it recovered an aircraft from 1.5000 m/s in 3.1 s.

**What these runs do not establish.** They do not price the fix in outcome terms: ON was 3/2/0/0 and
3/3/1/0 against an archive of 3/2/0/0, well inside what two runs of a stochastic scenario produce,
and **C21's close already recorded that an outcome rate needs repeats nobody has run.** Acceptance is
quoted as an interval over the pooled archive (§4), never as this run's rate.

**One result belongs here with its qualification attached rather than after it.** Run 2's commanded
arm scored **the archive's first ON-arm kill** — `RedSu35_02_wpn_1050_3` destroyed `BlueF16_02` at
0.025 m — **but a SAM had already taken that target to `wrecked` at `cumPk` 0.859**, exactly as in
both earlier archived kills. **A kill by the engine's definition; not an unaided one.** The better
result in the same run is not a kill at all: **`RedSu35_01` took `BlueF16_01` from untouched to
`wrecked` with two of its own missiles and no SAM contribution** — the first material damage a
commanded aircraft has done on its own.

**And one earlier reading of this project's own weakens.** The script-only arm returned **3/2/0/0 in
both runs**, matching `095026` rather than the 2/2/1/0 of `135722` and `185750`. That arm has now
produced each result twice, so §Corrections item 51(g)'s *"near-deterministic … two matching runs are
nearer one observation than two"* **does not hold as stated**, and `095026` is no longer the outlier.
The variance is real and unexplained.

#### Traps, for whoever runs it next

Never kill a live run mid-flight — the harness's `finally` restores the mission script, and a tree
left carrying the probe would drive every later scenario, interactive ones included. Do not build
while it runs; the DLL is locked. And **the probe has no defensive reflex and never fires**, so its
subjects are on a clock: run 1's first attempt put the fragile measurement last and lost it when both
aircraft were destroyed at t ≈ 85 and t ≈ 149. The phase order in the committed probe is that lesson.

---

## 11. Where this leaves C23

**The onset and the failure to recover are both closed in code, and the report is honest about which
parts of that are measured and which are argued.**

- **Measured:** the echo (61/61 speed, 19/19 measurable position), the collapse's dependence on being
  inside the ordered radius (4/4 in the archive, twice with the aircraft stationary; and **directly,
  at 1 Hz, on both aircraft in §10.2 Q1**), the latch at 1.5000 (52 archived samples, reproduced
  exactly by the probe), the survival of all three ladder rungs (12 samples with no order in force),
  the cost (58 of 114 rejections), and — **new in v1.8.38** — that a commanded 300 m/s is **achieved
  in 3.1 s from a dead stop**, and that a recovering aircraft **settles at 132.2–146.5 m/s**.
- **Read off the interface or the source, not inferred:** `resumeWaypointFollowing` carries no speed;
  rung 2 synthesizes the degenerate geometry by specification — **and §10.2's phase A then stalled an
  aircraft with that geometry alone, no model and no commander**; `defend` already discarded the
  ordered speed.
- **Argued, not established:** why the engine stops at 1.5 m/s, and why a recovering aircraft settles
  near 140 rather than the commanded 300 — both are inside a closed-source engine and this report
  does not guess; and that a Tier-1-chosen speed is what rescued the two archived survivors (n = 2,
  confounded).
- **No longer outstanding:** the distant-hold question (§10.2 Q1) and whether the recovery is
  achieved (§10.2 Q2).

**What the run changed in the fix itself, and it is the argument for having run it.** `kResumeFlyingSpeedMps`
was 150.0, chosen from nothing. The probe measured the settling band at **132.2–146.5 m/s**, so the
threshold sat *inside* it and the recovery latch might never have cleared — Tier 1 holding navigation
for the rest of the run, `hold` never orbiting. It is now **100.0**, with a test pinning the ceiling.
**Neither the offline suite nor the archive could have found that.**

**The register row can now close on everything except its governance tail.** The fix is implemented,
pinned by tests that demonstrably fail without it, confirmed in the engine on both of the questions
the offline suite could not reach, and — since v1.8.39 — **demonstrated under a commander issuing
real orders, twice, with the model's behaviour held constant and proven constant** (52 of 52
measurable holds still at 0.00 m). `RedSu35_02` held 320.0000 m/s through twenty accepted `hold`
orders where it had collapsed to 1.5000 in three runs out of three.

What remains is **not engineering**: the owner's view on the five judgement calls in §7.3, and
whether AIC-ORD-2 clause 8's acceptance criterion should name the run rather than the suite as its
instrument (§9.2).

**One thing worth carrying forward rather than filing as closed.** **Clause 8 has never fired in a
commanded scenario**, because clause 7 keeps preventing the onset. That is the design working, and it
also means the safety net's only evidence is the offline suite and one injected probe. **A net that
has never caught anything in production is not thereby proven** — it is untested where it matters,
and the right time to notice that is now rather than after some future posture finds a new way to
park an aircraft.
