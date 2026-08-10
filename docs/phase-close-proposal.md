<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# Proposal: close the unphased period — and the decisions that close with it

> # RESOLVED 2026-08-09 — **Option B, maintenance**, chosen by the owner.
>
> **Applied in PRD v1.8.41** (§Rollout's maintenance close, §Corrections item 57). **C17 and C23 both
> close; the register is empty.** Of §3's four decisions: **3.3 is fixed** (clause 8's criterion now
> names two instruments, each asserting what it can see) and **3.4 is taken** (C17 closes on C8's
> precedent). **3.1 — the standing hosted-egress authorization — remains outstanding and is the
> owner's**, tracked where it has always been tracked, in §Review checklist. **3.2 stands as
> implementation**, pinned by a test since v1.8.40.
>
> **This file said it would be deleted on decision. It is kept instead, and the reversal is
> deliberate:** §Corrections' standing convention is that superseded reasoning is kept rather than
> rewritten, and the argument for the option that was *not* taken is the part a later reader needs
> most — particularly §4's statement of what maintenance gives up. Deleting it would leave the
> decision recorded and its alternative unrecoverable.

**Status: RESOLVED — retained as the record of a decision, not as a live proposal.**
**Written against:** PRD v1.8.40, 2026-08-09. **Resolved into** PRD v1.8.41.
**Audience:** the platform owner. Every decision below was theirs; none of them was mine.

> **Why this is a separate file rather than a PRD edit.** §Scope authority says the PRD changes when
> a decision is made, not when one is suggested. Writing either option into §Rollout would *be* the
> decision. So this document states both, prices both, recommends one, and stays out of the PRD until
> you pick. **When you pick, the choice lands in §Rollout in its own revision and this file is
> deleted** — it is scaffolding, not a permanent artifact.

---

## 1. The problem this is trying to solve

**Phase 3 closed at v1.8.17. It is now v1.8.40 — twenty-three revisions of productive, unphased
work.** Those revisions closed C5, C8, C11, C12, C13, C14, C15, C16, C18, C19, C20, C21, C22 and the
engineering half of C23; they found and fixed the defect that was making commanded aircraft die; and
they produced this project's largest result. **None of that is in question.**

What is missing is a **close condition**. Phases 0, 1a, 1b, 2 and 3 each had deliverables, a gate and
a dated close. Since v1.8.17 there has been no gate to pass, so **the only status signal available is
the open-item register — and a defect register is never empty by construction.**

**The consequence is structural rather than emotional.** With no gate, "are we done?" has no
answerable form, and the honest answer to "what's left?" is always a list. That is the treadmill, and
it is a documentation defect rather than an engineering one.

**A second, smaller defect rides with it.** The register queues two kinds of row together:

| | Example | Who cares | Urgency |
|---|---|---|---|
| **Product defects** | C23 — commanded aircraft stop flying | the owner, anyone deploying | high, and they end |
| **Instrument defects** | item 56 — the measurement harness had not compiled for eleven revisions | this project only | real, but zero user impact |

Reading them in one list makes the instrument findings look like product risk. **They are not**, and
the descending severity of the last five revisions is invisible while they queue together.

---

## 2. The decision: Option A or Option B

### Option A — declare **Phase 4**, with a gate

**The claim it makes:** there is a defined block of work left, and passing a gate finishes it.

**Proposed deliverables** (four, deliberately few):

1. **A commanded run that exercises clause 8.** Clause 7 currently prevents every stall onset, so the
   safety net has never fired in a commanded scenario (§Corrections item 55(c)). Needs an injected
   below-floor state, as item 51(f)'s counter-instance did — an instrument, not more scenario repeats.
2. **The H1 re-review**, against a Tier 1 that fights. H1's marks were taken against a reference
   script that returned before its fire logic; that script no longer exists, so the local arm's 1-of-4
   was measured against a partner that could not act on a good order.
3. **Outcome repeats** — enough three-arm runs to say something about what the commander is worth, or
   an explicit statement that the question is being abandoned. It is currently n = 2 on the fixed
   build and no rate is claimable.
4. **The instrument/product register split** in §Carried out of Phase 3.

**Proposed gate — reachable with no network and no authorization:**

- Clause 8 observed firing and recovering an entity in a commanded scenario, or the criterion rewritten to name the instrument that can assert it.
- Unit, ASan, deployed-smoke and live-scenario suites green; `tests/live/` compiling in CI (**already true as of v1.8.40**).
- Every §Success metrics row carries an instrument that currently builds and a date within the phase.
- The register split applied, with each row labelled product or instrument.

**Proposed gate — requires you:**

- H1 re-review commissioned and its marks recorded, or H1 formally re-verdicted as unmeasurable against the current script.
- The four decisions in §3 below, resolved either way.

**Cost:** roughly a day of engineering plus a reviewer's afternoon. No grant. No money.

**What Option A buys:** a dated close and a document that can say *"this phase is finished."*
**What it costs:** it asserts that the remaining work is worth phasing. If items 2 and 3 never get a
reviewer or the repeats, Phase 4 ages exactly as OQ-4 did — a gate nobody can pass, which
§Corrections item 49(a) already names as the worst state a row can be in.

### Option B — declare **maintenance**

**The claim it makes:** the product is done; what remains is response to what breaks.

**What it would say, concretely:**

- **§Rollout gains a closing subsection**: Phases 0–3 delivered the system; **the product is
  feature-complete against this PRD's FR set** and no further phase is planned.
- **The register becomes the only tracker**, and its rule changes: a new row is opened only for a
  defect with a named mechanism or a decision with a named decider. **No row for "we should measure
  this someday"** — that is what §Out of scope's deferred rows with revisit conditions are for.
- **§Success metrics freezes.** Each row keeps its verdict, its date and its instrument. A metric is
  re-measured when its instrument or its subject changes, not on a calendar.
- **The four decisions in §3 are made, deferred with a testable condition, or cancelled** — using the
  OPEN / DEFERRED / CANCELLED vocabulary v1.8.33 and v1.8.36 already established.
- **`tools/lint-prd.ps1` and CI are the standing guard.** They already catch version drift, sentinel
  disagreement, requirement smells and — since v1.8.40 — instrument rot.

**Cost:** a couple of hours of documentation. No engineering. No grant.

**What Option B buys:** *"done"* becomes a state the project can actually be in, immediately, and
honestly — the FR set **is** delivered. **What it costs:** it forecloses the outcome question. The
commander's value would stand permanently at *"unmeasured; a null at n = 2 on the fixed build"*, and
anyone later asking "is this worth deploying?" gets no answer from this document.

---

## 3. The four decisions that close with it

These need answering under either option. Each is stated so you can answer in a sentence.

### 3.1 Standing authorization for hosted egress — *the only unticked checklist box that is a debt*

**Where it stands.** Six grants exist (v1.8.1, v1.8.3, v1.8.5, v1.8.8, v1.8.11, v1.8.19). **Every one
authorizes measurement**, each is scoped to named experiments, and v1.8.19 established that a grant
does not inherit an earlier one's boundary. **So there is no authorization under which the hosted
backend may run as a product**, and `commander.backend = claude` cannot ship to anyone.

**What the hosted path transmits, so the decision is against the real list** (§Exactly what is
transmitted, corrected v1.8.31): own entity id, geodetic position, altitude, course over ground,
ground speed, the three NED velocity components, own team; per track — target id, slant range, SNR,
and `kind`/`team` as closed vocabularies; per hardpoint — name, weapon profile name, ammo counts; and
the Tier-1 situation note, sanitized and capped. **No `componentTrackIdentity` free text. No scenario
team names.**

| Option | Consequence |
|---|---|
| **Grant standing authorization** | The hosted backend becomes shippable. Scenario state leaves the machine on every order, at a measured $1.05 per four-ship scenario-hour |
| **Refuse, keep per-measurement grants** | Status quo. The hosted path stays a measurement tool. **This is the current posture and it is defensible** — the project is still measuring itself |
| **Refuse permanently** | The `claude` adapter becomes dead code against a documented decision. ADR-2's seam survives; the row closes |

**My read:** nothing is blocked by refusing. The local 7B meets the latency target with 60 % headroom
and is the shipped default. **The only thing "grant" unlocks is quality**, and H1 says that gap is
real (hosted 9/10 appropriate against local 1/4) — but on n = 14 marked decisions with one reviewer.

**Related and separate: repository publication.** The Review checklist ties publication to the same
authorization. Worth noting that **the repository currently lives on a personal GitHub account**
(private, confirmed 2026-08-07) while holding `data/doctrine.txt`, the order schema, and this PRD's
description of platform internals by component type and field path. Not a leak, and recorded — but
you should know it rather than discover it.

### 3.2 C23's five judgement calls

All five are implementation choices made inside clauses 7 and 8's licence, listed in
`docs/c23-report.md` §7.3. **Four are uncontroversial.** The one worth your eye:

**Hysteresis.** Clause 8 names one threshold; the implementation needs two, or the aircraft oscillates
about the floor. The second (`kResumeFlyingSpeedMps`, now 100 m/s) is arguably FR behaviour that
belongs in the clause rather than in a Lua constant. **It shipped at 150 and a run caught it** — 150
sat inside the 132.2–146.5 m/s band a recovering aircraft settles in, so the latch might never have
cleared.

*Options:* leave it as implementation (a test pins it, as of v1.8.40) · promote the second threshold
into clause 8 in a revision.

### 3.3 Clause 8's acceptance criterion names an instrument that cannot assert it

The criterion says an entity below the floor *"recovers above it within one cadence window … asserted
in the offline Lua suite."* **The suite can only assert the command was issued.** The v1.8.38 probe
measured the actual recovery at 3.1 s — but that is a run, not the named instrument.

*Options:* rewrite the criterion to name the probe run · keep the suite and add "and confirmed by
`tools/run-c23-probe.ps1`" · leave it and accept that the criterion overstates its instrument.

### 3.4 C17 — does the row close?

Its remaining *item* landed in v1.8.40 (the normative rejection taxonomy). Its *question* — two
instruments that disagree — was answered by your 2026-08-06 decision that the bar stays on fixtures.
**What keeps the row open is that the decision does not make the instruments agree**, which is a
permanent condition rather than a task.

*Options:* **close it** as a stated position, on the precedent C8 set (closed on a decision, not on a
measurement) · keep it open as a standing caveat on every acceptance figure.

**My read:** close it. A row that can never be actioned is what v1.8.33's three-state fix was written
to eliminate, and the standing caveat already lives in §Success metrics where a reader meets the
number.

---

## 4. What I recommend, and why

**Option B — maintenance — with the four decisions taken at the same moment.**

**The argument.** The FR set is delivered. Every functional requirement has acceptance criteria and a
UAC; every open question is resolved or dispositioned; the register is down to two rows and both are
decisions. **The three things Option A would phase are not engineering** — one needs a reviewer, one
needs repeats nobody has committed to, and one is a documentation split. Phasing them creates a gate
whose pass condition depends on people and appetite that may not exist, and this project already has
a documented failure mode for exactly that.

**The honest cost of my recommendation, stated because it is the part I would push back on if you
proposed it to me:** Option B **permanently forecloses the outcome question.** The commander's value
would stand at "unmeasured", and the project's own summary says the first clean comparison ever run
returned a null at n = 1 (now n = 2). If you ever want to answer *"is this worth deploying?"*, the
runs are ~35 minutes each and cost nothing — and it is much easier to commit to that inside a phase
than after declaring the work finished.

**So the real question is narrower than A-versus-B:** *do you want the commander's value measured, or
is "it works, it is safe, and it does not make things worse" enough?* Option A if the first. Option B
if the second. **The engineering is finished either way.**

---

## 5. If you pick, here is what happens next

| You say | I do |
|---|---|
| **"Option A"** | Write Phase 4 into §Rollout with the deliverables and gate above, in its own revision; apply the register split; open the gate items as rows |
| **"Option B"** | Write the maintenance close into §Rollout; apply the register split; resolve or defer §3's decisions as you direct; delete this file |
| **"Option B, but measure outcomes first"** | Run the repeats now, then close. ~35 min per run, no grant |
| Answer any of §3 individually | Land that one in its own revision; the rest keep waiting |

**Nothing in this document is applied until you say so**, and none of it is blocked on anything but
your reading of it.
