<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# PRD review — v1.8.51, and an adversarial audit of the outcome campaign

**Audience:** the platform owner, and whoever picks up the next revision.

**Scope:** `docs/prd.md` at v1.8.51 (`4c56d7f`), reviewed against the PRDEngine Comprehensive-tier
rubric, with a targeted audit of §Corrections items 62–67 and §Validation's *"The outcome campaign
— a pre-registration"*. The audit exists because revisions v1.8.42–v1.8.51 were written by a single
agent that also designed the experiment, wrote the analysis tool, ran the campaign and declared the
result supported. There is no independent check anywhere in that chain, and this document is the
first one.

**Status:** written 2026-08-10 against `4c56d7f`. **Assessment only — no PRD edit is proposed here.**
Gates re-run at review time and green: `tools/lint-prd.ps1` **11 checks / 0 errors / 0 warnings**;
`tools/check-artifacts.ps1` **PASS**. No engine run, no build, no network, no cost, no grant spent.
Every finding below was derived from the repository, the git history, and the archive at
`C:\Users\I2fas\Documents\N8RO AI Commander logs`, read in place.

**Verdict: NEEDS REVISION.** The headline claim survives adversarial checking on its arithmetic and
on its chronology. It does not survive on its characterization of the evidence, and one load-bearing
citation is fabricated.

---

## 1. What could not be broken

Recorded first, because the brief was to attack and several attacks failed. A review that reports
only what it found would be the more flattering half.

**1.1 The numbers reproduce exactly.**

```
python tools/outcome-campaign.py --since 20260809T213423Z
```

returns per-run differences −0.6866, −0.7841, −0.9882, −0.6375; mean −0.7741; sd 0.1552;
95 % CI [−1.0210, −0.5272]; t = −9.976 on 3 df; p = 0.0021; 4 of 4 negative; VERDICT: CLAIM
SUPPORTED. Every published digit of §Corrections item 62(a). `tools/acceptance-report.py` likewise
regenerates **80.8 % [77.4, 83.8] n = 609** and **90.9 % [88.2, 93.2]**, matching item 62(h), the
§Success metrics cell and the pinned sentinel.

**1.2 The pre-registration is genuinely prior, and the timestamp hazard does not change that.**

`tests/smoke/run-live-scenario.ps1:124` stamps folders with `Get-Date -Format "yyyyMMddTHHmmssZ"` —
**local time with a literal `Z`**. `tools/run-c23-probe.ps1:57` and `tools/run-c8-floor-probe.ps1:77`
use `.ToUniversalTime()`. Both write `…Z-` folders into one directory (finding F11). Getting the
units right:

| Event | As labelled | Actual UTC |
|---|---|---|
| `83934a0` — v1.8.42, the pre-registration | 2026-08-09 21:15:49 **+0300** | **18:15:49 Z** |
| `20260809T213423Z-local` — campaign run 1 | local 21:34:23 | **18:34:23 Z** |

**The pre-registration precedes the first run by 18 min 34 s.** Corroborated by folder mtimes
(21:34 → 23:36 local across four runs) and a consistent ~30-minute three-arm cadence at 600 s per
arm. It also survives the naive misreading that treats the folder label as UTC. Prior either way.

**1.3 The tool is not tautological.** `tools/outcome-campaign.py` computes the three conjuncts,
prints them individually, and has a live NULL branch with the pre-registered null wording. It could
have returned NULL. Nor would any run selection available in the archive have flipped it: the full
archive at n = 9 also supports, at [−0.8609, −0.5866].

**1.4 The confound travels with every quotation of the result.** Item 62(f); the revision-history
entry at line 20; the v1.8.46 changelog; §Maintenance's v1.8.46 block; and `docs/summary.md`'s
blockquote. So does the interval form — the result is never quoted as a bare point at any site.

**1.5 The most convenient available fact is refused.** `damageDealt` is **+0.3930** — the commanded
arm dealt *more* damage while absorbing less, which would largely discharge the "fights less"
reading. Items 62(f) and 62(g) forbid using it in either direction, and it is not used anywhere.
That is discipline against interest and it is the strongest evidence in the document that the
pre-registration did real work.

**1.6 The ≤ 2.5 s hosted-latency disposition (item 65(f)–(g)) is correct.** The row reads MISSED,
still reads MISSED, the bar is not moved, and the verdict rests on §Corrections item 36's existing
fixed-term measurement (p95 3,811 ms floor / 5,263 ms nearest-rank to response *headers*), either of
which exceeds a 2,500 ms total target on its own. Stated as a verdict on the target rather than as a
new measurement. This is the honest disposition and the wording does not soften it.

**1.7 Item 67's decision to correct the claim rather than register a runner is well-reasoned.** A
self-hosted runner on a single-developer machine would execute on the same host, toolchain and
release tree that already builds locally — automating the running, not the environment — against
the exposure `.github/RUNNER-SETUP.md` states in its own words. The retirement condition is named.
Its problem is cascade, not judgement (F4, F5).

**1.8 Governance hygiene that holds.** The OQ table: nine rows, all resolved or dispositioned, no
owner column, decision targets expressed as observable events rather than dates. §Out of scope: all
twelve rows carry Item / Status / Rationale / Target / Added. FR↔UAC parity, the customer-obsession
fields, version coherence and the three-document acceptance sentinel are all machine-enforced and
all pass.

---

## 2. Findings

Each finding is classified **(a)** a false statement, **(b)** a true statement that misleads, or
**(c)** a gap.

### CRITICAL

#### F1 — "§Scope authority rule 4" does not exist. **(a) false statement.**

**Where:** §Validation, *The corroborating mechanism, which is not a rate and is reported
separately*: *"Under §Scope authority rule 4 a single run may close a question that is **a mechanism
readable directly off recorded values**."*

Four further call sites: §Corrections item 62(e) (*"the one category §Scope authority rule 4 lets a
small sample close"*); §Corrections item 47(a) and the v1.8.31 changelog (*"§Scope authority's
fourth rule"*); `tools/outcome-campaign.py:369`; `tools/c23-hold-probe.lua:28`.

**Evidence.** §Scope authority is three unnumbered paragraphs: the FR table is the contract; the
design must not exceed it; the PRD must not specify implementation detail beyond FR shape. It
contains **no numbered rules** and says nothing about evidence, sample size, mechanisms, or what a
single run may close. The section is byte-identical at the file's first commit (`fda8af2`), at
`31977a0` — the v1.8.31 commit that introduced the phrase *"fourth rule"* — and at `HEAD`:

```
git show fda8af2:docs/prd.md | awk '/^## Scope authority/{f=1} f{print} /^## Performance/{if(f)exit}'
git show 31977a0:docs/prd.md | awk '...'   # identical
git show HEAD:docs/prd.md    | awk '...'   # identical
git log --oneline -S "fourth rule" -- docs/prd.md   # → 31977a0 only
```

**The rule has never existed in any revision.** It was invented in v1.8.31 and has been cited five
times since, including by a Python tool and a Lua probe that print it to operators.

> ***(CORRECTED 2026-08-10, during the v1.8.52 revision that applied this finding — THE COUNT ABOVE
> IS WRONG. It is not five, it is **twelve**: seven in the PRD body (§Corrections items 47(a),
> 47(c), 49(b), 58(g), 62(e), the C5 register row, and §Validation), three in dated history
> entries, and two in tooling. This review enumerated the sites it happened to grep and stated the
> result as a total. **It is the same shape as the finding it was reporting** — a count asserted
> rather than counted, which is §Corrections item 66 — and it is recorded here rather than
> silently widened. The finding itself is unaffected and gets larger, not smaller. See §Corrections
> item 68(a).)***

**Why it is critical.** It is the sole cited authority for every *"closed on the mechanism, and
explicitly not on the rate"* decision in the document — C5, C21, C22, the C23 hold probe, and the
outcome campaign's corroborating mechanism. In a document whose central premise is that a claim
should be checkable, the exemption that licenses small-n claims points at a rule a reader who checks
will not find. `lint-prd.ps1` check 9 asserts that the **heading** `## Scope authority` exists, which
is why twenty revisions did not catch it; nothing validates that an internal cross-reference
resolves.

**Note on the underlying practice.** The discipline itself is real and precedented — C5 closed *"as
a bounded negative, on the mechanism and explicitly not on the rate"*, C21 and C22 likewise. What
does not exist is the numbered rule those closures cite as their authority.

---

#### F2 — The control arm is near-deterministic. The document established that principle, used it against itself once, and stopped applying it exactly when it began to cost something. **(c) gap, with (b) in item 62(b).**

**Where:** §Validation: *"**Both arms come from the same invocation of `tests/smoke/run-live-scenario.ps1`,
so each run yields one PAIRED observation** from identical initial conditions on one build."* And
§Corrections item 62(b): *"Same sign, overlapping intervals, **disjoint data**."*

**Evidence.** Across all nine archived three-arm runs, the `script-only` arm's `damageAbsorbed` takes
**exactly two values**:

| Value | Runs |
|---|---|
| **1.5758** (with `damageDealt` 0.8474, L3 D2 K0 X0) | 095026, 143428, 150635, 213423, 220459, 230605 — **six** |
| **1.3316** (with `damageDealt` 1.3538, L2 D2 K1 X0) | 135722, 185750, 223532 — **three** |

Three of the campaign's four control arms are **event-for-event identical** — same munition IDs in
the same order, seeker acquisition ranges to six decimals (`21943.976741 m`), and every `pk` to six
decimals (0.776325, 0.962174, 0.870323, 0.705485, 0.460906, 0.386459). Run `223532` diverges:
`RedSu35_01` fires one missile instead of two, the munition index shifts, and every `pk` differs.
The `off` arm behaves the same way — two trajectories, 1.0682 ×6 and 1.4480 ×3.

Reproduce with:

```
python tools/outcome-campaign.py          # full archive, SCRIPT-ONLY column
```

**Three consequences the document does not draw.**

1. ***"Disjoint data"* is true at the file level and does not establish independent replication.**
   The five planning runs drew controls {1.5758, 1.3316, 1.3316, 1.5758, 1.5758}; the four campaign
   runs drew {1.5758, 1.5758, 1.3316, 1.5758}. **The comparator side of both estimates is the same
   two deterministic trajectories.** What replicates across the two sets is the ON arm alone. Item
   62(b) offers "disjoint data" as the property that makes this the project's first result to
   survive its own repeat; the property it actually has is weaker than the sentence implies.

2. **The pairing gain is an artifact.** The reported sd of paired differences is **0.1552**, smaller
   than the ON arm's own sd of **0.2718**. That reads as pairing removing shared run-level noise. It
   comes from a single coincidence — run `223532` drew both the low ON value (0.3434) and the low
   control value (1.3316). With a two-point control at n = 4 this is not a property the design can
   be relied on to reproduce, and the pre-registration's rationale for pairing (*"identical initial
   conditions"*) describes a design in which there is almost no run-level noise to remove.

3. **Sign consistency carries even less than the p = 0.125 already conceded.** With the control
   pinned near 1.4 and the ON arm at 0.34–0.94, the sign of each paired difference is very nearly
   determined before the run starts. The pre-registration was right that 4-of-4 is not a second
   test; it is weaker still than that caveat implies.

**The document already knew, and said it better.** §Success metrics (v1.8.35) and §Corrections item
51(g): *"**But the script-only arm carries no model and is close to deterministic**, and the two
runs' kills are byte-identical — same munition id, `pk 0.985088`, `cumPk 0.998554` — so **two
matching runs of that arm are nearer to one observation than to two**."* That principle was applied
to **weaken a reading**. It appears in `README.md` and `docs/summary.md` as well. It is **absent from
v1.8.42's pre-registration and from v1.8.46's item 62**, the two places where applying it would have
qualified a favourable result.

**In fairness — the verdict is robust to this.** Recomputing the interval with the control held at
each of its two observed values:

| Counterfactual | mean | sd | 95 % CI |
|---|---|---|---|
| As run | −0.7741 | 0.1552 | [−1.0210, −0.5272] |
| All four controls at 1.5758 | −0.8352 | 0.2718 | [−1.2676, −0.4027] |
| All four controls at 1.3316 | −0.5909 | 0.2718 | [−1.0234, −0.1585] |

Both extremes exclude zero. **This is a defect in how the evidence is characterized, not in what it
supports.** The claim stands; the sentence *"disjoint data"* does not.

---

#### F3 — §Corrections item 66(d) cites an §Out of scope revisit condition that does not exist. **(a) false statement.**

**Where:** item 66(d): *"That is offered as an observation and not opened as a row — under
§Maintenance's entry rule a lint that does not exist is not a defect, and **the §Out of scope revisit
condition for it is *'the next time a claimed count is found stale.'*"***

**Evidence.** §Out of scope carries twelve rows (`lint-prd.ps1` counts 12). None concerns a lint
check, a claimed count, CI, or a runner. The newest `Added` date in the table is **2026-08-01**. The
definite article asserts an existing row; there is none.

**Why it matters.** Item 66 exists to correct a false claim the document made about its own
bookkeeping. It discharges that finding by making another one, in the clause that explains why no
row was opened.

---

### HIGH

#### F4 — The register is empty because both of its channels are, and the entry rule was narrowed to keep it that way. **(b) / (c).**

**The rule** (§Carried out of Phase 3, *Opening a row, from v1.8.41*):

> A row SHALL be opened only for one of two things: **1. A defect with a named mechanism** —
> something readable off recorded values, **source**, or a shipped interface. … **2. A decision with
> a named decider.** … **Everything else goes to §Out of scope as a DEFERRED row with a revisit
> condition.**

**Tested against v1.8.42 – v1.8.51:**

| Revision | Disposition | Assessment |
|---|---|---|
| v1.8.47 / v1.8.48 (items 63, 64) | C24 opened and closed | **Correct.** The rule works when something qualifies. |
| v1.8.50 (item 66) | No row; cites a phantom §Out of scope row | F3 |
| v1.8.51 (item 67) | No row; no §Out of scope row either | see below |

Item 67(e) declines a register row because the CI gap is *"not a defect with a named mechanism **in
shipped code**"*. The rule says *"recorded values, **source**, or a shipped interface"* — not
"shipped code". `actions/runners` returning zero is readable off an interface, and the workflow file
is source. Having declined the register, item 67 does not add the **§Out of scope row the rule makes
mandatory** for everything else; the retirement condition lives only in §Corrections prose.

**The structural finding: no §Out of scope row has been added since 2026-08-01** — none in the ten
revisions under review, none in the ~35 revisions since — while §Corrections gained ten items. **The
designated overflow channel has received nothing since the rule that designates it was written.**

**Answer to the question as posed:** the entry rule holds for the one item that qualified, and is
exited by narrowing for the two that did not, with the mandatory fallback skipped in both cases. The
register's emptiness is therefore partly a property of the disposal route rather than of the
backlog. Findings that would populate either channel land in §Corrections prose, where they are
narrative rather than tracked.

---

#### F5 — Item 67's own correction did not cascade. The retracted claim is still made twice, in §Maintenance. **(a) false statement, ×2, live.**

**Where:** §Maintenance, *"What that claim rests on, so it can be checked rather than believed"*:

> `tools/lint-prd.ps1` (11 checks) and `tools/check-artifacts.ps1` pass with zero warnings, the unit
> suite is green, and **since v1.8.40 CI compiles the measurement harness too.**

and §Maintenance, change 3:

> **`tools/lint-prd.ps1` and CI are the standing guard.** They already catch version drift, sentinel
> disagreement, requirement smells, FR↔UAC parity and — since v1.8.40 — **instrument rot**.

Item 67 establishes that `ci-selfhosted.yml` has no registered runner and has never run. Both
sentences survive unannotated at `docs/prd.md:3432` and `:3442–3443`. **The first is inside the
paragraph that justifies declaring the project feature-complete.**

**The mechanism to fix this exists and was applied once.** Item 56(f) was back-annotated in place —
*"BUT SEE THE CORRECTION AT ITEM 67: THIS GUARD IS INERT"* — and item 64(g) carries its correction
inline. The convention is real; item 67 applied it to one site of three.

---

#### F6 — The campaign result is absent from §Success metrics, and the row there still asserts its negation. **(b) true statement that misleads, by date.**

**Where:** §Success metrics, row *Engagement outcome, commanded arm vs control*, which still reads
*"**Three arms, TWO runs, 2026-08-08**"* in its Baseline cell and ends:

> **nothing about the commander's value is established and no rate is claimed at n = 2.**

The How-measured cell still names `tools/analyse-outcomes.py` *"reporting **four separate counts per
arm** — launches, detonations, kills, losses"* — the four columns §Corrections item 58(b) established
*"CANNOT ANSWER THIS QUESTION AT ANY n ANYONE WILL RUN"*.

**Why it matters.** The document's own rule, set in v1.8.30: *"§Success metrics is where a reader
looks for the verdict, and reporting only the first would be the more flattering half."* §Maintenance
change 2: *"Each row keeps its verdict, its instrument and its measurement date."* The campaign
cascaded into §Corrections, §Maintenance, the revision history, the changelog and `docs/summary.md`
— **and not into the metrics table**, where the instrument cell is now known-inadequate by the
document's own analysis.

**Recorded in the document's favour:** this staleness runs *against* the author's interest — the
table understates what the campaign found. It is evidence against motivated reasoning. It is
nonetheless the exact mechanism §Corrections item 66 records: a dated description read as current.

---

#### F7 — `README.md` was not cascaded, and nothing pins the outcome figure. **(b).**

**Where:** `README.md`, section *The finding that matters most*:

> **Nothing about what the commander is worth is established in either direction.**

`README.md` carries the campaign only as a source of 211 additional acceptance orders (line 65). The
headline result appears nowhere in it. `docs/summary.md` **was** updated, with the result and its
confound blockquote; the README was not.

**Compounding:** §Validation's pre-registration says the result *"SHALL be quoted as an interval and
never as a point, per §Success metrics and **the standing rule the linter enforces on the acceptance
sentinel**."* The linter pins the **acceptance sentinel** across three documents (check 8). **There is
no pin on the outcome figure.** The single most quotable number in the project is the one number with
no machine guard, in a document whose position is that *a rule someone has to remember is not a
control.*

---

#### F8 — Item 62(c) refutes the mechanism claim's premise; item 62(e) keeps the claim and the exemption. **(c) gap.**

**The premise** (§Corrections item 58(g), repeated verbatim in §Validation): *"Blue fires exactly
**two** air-to-air munitions at the commanded pair in **15 of 15** archived arm-runs, **so the
denominator is a property of the scenario rather than a sample**."* That sentence is the entire
reason the mechanism was exempted from the small-n discipline.

**The refutation** (item 62(c)): *"**In run `213423` Blue fired THREE** … **It is a sampling quantity
after all**."* Verified — the tool reports `2 of 3` for that arm.

**The gap.** Item 62(d) argues the refutation *"strengthens the primary result rather than
threatening it … the primary endpoint is total `pk` absorbed and **does not depend on the denominator
at all**."* That is true — and it rescues only the primary endpoint. The corroborating mechanism is
**per-munition and does depend on the denominator**. Item 62(e) nonetheless reports *"5 of 9 …
0 of 8 … 0 of 8"*, pools to *"9 of 19 against 0 of 36"* (both verified), and re-invokes the same
exemption — now resting on a premise the same item refuted three clauses earlier, via a rule that
does not exist (F1).

**The premise is discharged where it threatens the primary result and retained where it licenses the
secondary one.** This is the sharpest instance in the document of the reasoning pattern the audit was
commissioned to look for.

---

#### F9 — The refuted premise stands unmarked in the two places a reader will meet it. **(c) gap.**

§Validation's pre-registration still reads *"the scenario is fixed in that respect, so the
denominator is not a sampling quantity"* with **no correction marker**, and §Corrections item 58(g)
carries **no forward pointer** to item 62(c).

Item 58(h) states that the protocol lives in §Validation *"where a reader looking for how a number
was produced will find it"* — which is precisely where the refuted sentence is. The document's own
convention is to mark corrections in place (item 56(f), item 64(g)); it was not applied here.

---

### MEDIUM

#### F10 — The tool applies the test but not the population. **(c) gap; item 62(a) is (b).**

**Where:** item 62(a): *"**`tools/outcome-campaign.py` applied the rule and printed the verdict**, so
the analysis was not chosen once the numbers were visible."* Tool docstring: *"It does not choose its
own test."*

Both are accurate about the **test**. Neither is true of the **population**:

- `--since` is a free operator argument defaulting to `""` (all runs).
- There is no assertion that `n == 4`, no check that runs are post-C23-fix, and no record in the
  output of which `--since` value produced a given verdict.
- Conjunct 3 is coded `negatives == n`, not the pre-registered *"4 of 4"*.
- The pre-registration's *"**The five planning runs are excluded from the test**"* lives only in
  prose.

Run selection is therefore the one researcher degree of freedom the pre-registration most wanted
closed, and it is the one left outside the tool that closes the others.

**No evidence it was exercised.** The four campaign folders are exactly the runs postdating the
pre-registration commit, and the full-archive fit at n = 9 also returns CLAIM SUPPORTED, so no
selection available would have flipped the verdict.

**Secondary observation on wording.** `cb8780e` (the tool) was committed at 21:37:05 local — **2 min
42 s after run 1 began**, and about 27 minutes before run 1's control arm finished. *"That tool was
committed before the campaign finished"* is exact and true; *"before the campaign started"* would
have been false. No outcome numbers existed at that moment, so the substance holds — but the
sentence is doing more work than its plainness suggests.

---

#### F11 — Archive folder labels mix local and UTC under an identical form, and the PRD is silent. **(c) gap.**

`tests/smoke/run-live-scenario.ps1:124` — `Get-Date -Format "yyyyMMddTHHmmssZ"`, local time with a
literal `Z`. `tools/run-c23-probe.ps1:57` and `tools/run-c8-floor-probe.ps1:77` — `.ToUniversalTime()`.
Both write `…Z-` folders into one directory, differing by the local offset with no visible
distinction.

**Assessment of the response: there isn't one.** It appears nowhere in §Corrections, the register, or
§Out of scope. That is the wrong call on the document's own terms, twice:

1. It is precisely *"a defect with a named mechanism — something readable off … source"* under the
   entry rule — the same class as item 67's, which earned an item.
2. It bears directly on the campaign's audit trail. Prior-ness is established by comparing a commit
   timestamp to these labels, so an auditor performing the obvious check is comparing quantities in
   two different bases.

**Compounding:** `outcome-campaign.py`'s `--since` performs a **lexical string comparison** over these
ambiguous labels (`if not os.path.isdir(d) or name < since: continue`), and the tool's own USAGE
example — `--since 20260809T190000Z` — is a UTC-shaped value applied to local-stamped folders. The
population selection is correct here by accident of ordering, not by construction.

It did not bite. The mechanism by which it could is live and unrecorded.

---

#### F12 — *"The ON arm was shot at MORE — 9 munitions against 8."* **(b) true statement that misleads.**

Item 62(d). This is **one extra missile in one run of four**, on a quantity (`blueShotsAtRed`) that is
not a pre-registered endpoint and carries no power analysis — the same objection that bars `launches`
from being claimed at 17 runs' worth of power. The argument the clause supports (a denominator-free
endpoint is unaffected by an unequal denominator) is sound and does not need the exposure comparison
at all. As written, it invites the reading that the commanded arm faced materially more fire.

---

#### F13 — `outcome-campaign.py` degenerate-variance path. **(c) minor.**

`ci()` returns `mean, 0.0, mean, mean, inf, 0.0` when `sd == 0.0`. Conjunct 1 —
`c1 = (lo < 0 and hi < 0) or (lo > 0 and hi > 0)` — then reads a **zero-width interval as "excludes
zero"**, and the tool can print CLAIM SUPPORTED off a degenerate sample. Unreachable on this data
(sd 0.1552), and it would require every paired difference to be identical. Given a control arm that
takes two values across nine runs (F2), less remote than it looks.

---

## 3. §Corrections — navigable, or a place findings go to be buried?

**Buried is too strong. Unnavigable is fair.**

| Property | Value |
|---|---|
| Length | 1,216 lines — **27.9 %** of the document (307 KB of 1.10 MB) |
| Heading level | **H3**, nested under §Purpose and scope |
| Cross-references by item number | **640** across the document |
| Index, status column, or "still true?" marker | **none** |

Monotonic growth is defensible and the rationale in item 66(c) is right: *"a document that quietly
repairs its own claims is one a later reader cannot audit."* **Monotonic growth without reliable
back-annotation is not defensible**, because the only way to learn whether item *N* still holds is to
read items *N+1 … 67*.

**Is the self-correction load-bearing or performative? Load-bearing where it points; roughly half the
time it does not point.** Of the four correction chains traced:

| Chain | Back-annotated in place? |
|---|---|
| 67 → 56(f) | **Yes** — *"BUT SEE THE CORRECTION AT ITEM 67: THIS GUARD IS INERT"* |
| 66 → 64(g) | **Yes** — *"(Corrected v1.8.50 — this clause originally read …)"* |
| 62(c) → 58(g) and §Validation | **No** (F9) |
| 67 → §Maintenance ×2 | **No** (F5) |

The two that failed are the two where the correction had to travel *out of* §Corrections into a
section that carries a verdict. That is the pattern worth naming: **corrections propagate within the
history and stall at its boundary.** F5, F6 and F7 are all instances.

---

## 4. Voice

**The formatting hinders, and specifically it flattens severity.**

In item 62(a) a measured interval and a rhetorical gloss carry identical typographic weight:
*"**mean −0.7741, sd 0.1552, 95 % CI [−1.0210, −0.5272], t = −9.976 on 3 df, p = 0.0021**"* and
*"**The commanded aircraft absorb roughly half the damage the same script absorbs un-commanded.**"*
So does *"**4 of 4**"* — in the same clause where the pre-registration establishes that sign
consistency is corroboration only and cannot reach significance at this n. **The emphasis argues
against the caveat.**

ALL-CAPS clause headers are used both for genuine self-corrections (*"A CLAIM THIS DOCUMENT MADE IN
v1.8.42 IS REFUTED BY THE VERY CAMPAIGN IT WAS WRITTEN FOR"*) and for ordinary framing (*"THE
CONFOUND STANDS"*, *"WHAT IT DOES NOT BOUND"*), so caps cannot be used to locate the corrections
either.

The net effect on a reader meeting −0.7741 for the first time: **no typographic signal separates what
was measured from what is being asserted about it.** The prose is unusually precise and the
formatting works against it. Where bold is genuinely load-bearing — the interval, the confound, the
"not claimed" declarations — it is indistinguishable from where it is decoration.

---

## 5. Dimension scores — PRDEngine Comprehensive tier

| # | Dimension | Rating | Key finding |
|---|---|---|---|
| D1 | Structure compliance | Adequate | 13 required sections machine-enforced; §Corrections is 27.9 % of the doc as an H3 with no index |
| D2 | Problem framing | **Strong** | Job-story shape, named persona, explicit why-now, documented workaround |
| D3 | Scope coherence | **Weak** | F5, F6, F7 — three cascade failures, on the newest result and the newest correction |
| D4 | FR customer-obsession | **Strong** | Customer scenario / Pain removed / acceptance criteria enforced on every FR |
| D5 | FR↔UAC pairing | **Strong** | 17 trace lines, 19 UACs, parity gated by `lint-prd.ps1` check 1 |
| D6 | Path & SDK conventions | **Strong** | §Naming and path conventions present; FRs reference rather than restate |
| D7 | Scope authority | **Weak** | F1 — cited five times, including by two tools, for a rule it does not contain |
| D8 | Out-of-scope completeness | Adequate | All 12 rows complete; inert since 2026-08-01 (F4); F3 cites a phantom row |
| D9 | Hypothesis & assumption verifiability | Adequate | F2 — control-arm determinism never stated as an assumption of the campaign design |
| D10 | Open-question discipline | **Strong** | 9 rows, all dispositioned, no owner column, targets are events not dates |
| D11 | Success-metrics testability | **Weak** | F6 — the newest result absent; the row asserts its own negation; instrument known-inadequate |
| D12 | Milestone observability | **Strong** | Phase gates carry observable validation throughout |
| D13 | Requirement smells | **Strong** | Smell scan over 5,265 lines passes; prose is precise (see §4 for formatting) |
| D14 | Design readiness | N/A | Post-design — product feature-complete under §Maintenance |

---

## 6. Priority

| # | Severity | Item |
|---|---|---|
| 1 | **Critical** | **F1** — the fabricated rule, and every closure resting on it |
| 2 | **Critical** | **F2** — the control arm's two-value distribution, and what *"disjoint data"* can and cannot mean |
| 3 | **High** | **F5, F6, F7** — three cascade failures, one of them the correction filed one revision ago |
| 4 | **High** | **F4 + F3** — the entry rule's overflow channel is inert and has been cited as though it were not |
| 5 | **High** | **F8, F9** — the mechanism claim's premise, refuted in one place and load-bearing in two others |
| 6 | Medium | **F10, F11** — population selection lives outside the tool, over labels in two time bases |
| 7 | Medium | **F12, F13**, and the voice assessment in §4 |

---

## 7. What this review does not do

It proposes **no PRD edit**, by instruction. It moves no verdict, opens no register row, and asserts
nothing about §Success metrics. Every finding above is a statement about the document, checkable from
the repository, the git history, and the archive by the commands quoted.

It also does not re-litigate the result. **The commander reduces damage absorbed on this endpoint, in
this scenario, at this model, and the interval is [−1.0210, −0.5272].** That claim reproduced under
every check applied to it. What did not reproduce is some of what the document says *about* that
claim.
