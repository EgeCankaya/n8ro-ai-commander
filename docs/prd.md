<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# N8RO AI Entity Commander — Product Requirements Document

> **One-liner:** A `n8ro-sim` plugin that lets a language model issue tactical *intent* — posture, target, waypoint, rules of engagement — to entities in a running scenario, while every kinematic decision and every state mutation stays in the deterministic C++ and Lua tiers that already exist.

**Date:** 2026-07-31 (revised 2026-08-05)
**Status:** Draft v1.8.15
**Revision history:**
- v1.8.15 — **§Corrections item 32: C1 RAN. The hosted backend works inside the engine, on real scenario state — the run held across four grants.** 19 checks, 0 failed, on `oppint_red_interceptor` in the shipped "Mariana Shield" with its commander-off control. Both entities commanded, 3 distinct postures, **0 timeouts**, `reject.schema`/`reject.shape` **0 %**, frame cost max **2.87 ms** over 12,001 frames, release tree left clean. **C3's change is confirmed on the product** — a real in-engine request read **5,118 cached tokens**. **The most valuable observation is a rejection:** Stage B refused `cruiseSpeedMps 450` against a 400 bound, which is Goal 3 demonstrated on the product rather than on a corpus. **And two harness numbers are corrected:** the acceptance rate counted **two still-in-flight requests** in its denominator (10/13 = 76.9 % printed; **10/11 resolved**, and no rate at n=11), and the "p95" over **ten** samples was **the maximum** — the exact error v1.7.4's Finding 3 recorded, still being printed four revisions later. Both fixed in the script. **C1 CLOSES.** ≈$0.02.
- v1.8.14 — **§Corrections item 31: C3's quality half ran, both arms accepted 120/120, the pre-registered rule fired, and the prose schema is REMOVED FROM THE SHIPPED PREFIX** — the first shipped-code change any Phase 3 diagnostic has produced. Prefix **17,756 → 8,750 bytes**; cost **$1.05 → $0.88** per four-ship-hour (**−16.6 %**); headroom against the ≤ $1.10 target **4 % → 20 %**; `reject.schema` and `reject.shape` **0.00 %** in both arms. Arm B's 120/120 bounds true acceptance at **≥ 97.53 %** against the ≥ 95 % gate. **What it does not license, as item 30 said in advance:** the arms cannot distinguish 100 % from 99 %, and **acceptance is not quality**. **A new constraint:** the cached block falls to **5,075 tokens** against Haiku's 4,096 minimum — margin **86 % → 24 %** — so item 22's *"a page of doctrine can be deleted without consequence"* is **no longer true** past ~980 tokens. **C3 CLOSES.** ≈$0.32 against ≈$0.40 estimated.
- v1.8.13 — **§Corrections item 30: C3's quality arm, PRE-REGISTERED. No request has been made at the time of this revision.** Exact Clopper–Pearson bounds at n=120/arm: **120/120 bounds the true acceptance at ≥97.53 %** and **119/120 at ≥96.11 %** — both clear the ≥95 % gate; **118/120 bounds it at 94.85 % and does not.** Resolvable difference between arms ≈**2.5 percentage points**, so the run can rule out a gate-breaching quality cost and **cannot distinguish 100 % from 99 %** — stated in advance so a null cannot later be read as more than it is. **Decision rule fixed now:** ≥119/120 with eject.schema/eject.shape at 0.00 % → the prose-schema removal is safe to make; ≤118/120 → not made; **any schema or shape rejection → not made regardless of count.** Every prior arm in this project was assessed for power *after* its result, and twice the answer was no.
- v1.8.12 — **§Corrections item 29: the TTFT instrument ran, it does not measure what item 28(f) predicted, and C2 CLOSES on a number nobody was arguing about.** 96 orders, two doctrine arms, under the fifth grant. **TTFT correlates with total latency at 0.995** — under structured outputs the service emits ~6 deltas per response and the first arrives *after* the generation, so the probe subtracts a near-constant 2.2 s flush rather than generation. **SD(afterFirst) is 314 ms**, which refutes item 28(f)'s premise directly: generation is a large share of the **mean** and a negligible share of the **variance**, and *a share of the mean is not a share of the variance*. The estimator that helps is one nobody proposed and the probe was already recording — **time to response headers**, ~25–33 % lower SD. **And it closes C2: headers alone, before one token exists, are mean 3,157 ms / p95 6,917 ms, so the fixed term exceeds the 2.5 s total-latency target and no prompt-side change can reach it.** The doctrine comparison is **null on all three estimators**; 2,202 cached tokens are worth −103 ms [−940, +733]. **§Success metrics still reads MISSED — the target is not moved to fit the measurement.** ≈$0.84 against ≈$0.50 estimated, overrun reported.
- v1.8.11 — **the fifth egress grant, recorded and pushed before any request made under it — and the first grant to cross the boundary the previous four deliberately held.** Three experiments: **①** the C2 fixed term via **time-to-first-token** (≈$0.50), with a **null result written into the grant in advance** as a real outcome rather than a reason to run a third pair of arms; **②** C3's quality half, a paired arm at n≈120 (≈$0.40), **conditioned on the arm's resolvable effect size being recorded before the run**; **③** **C1 — the in-engine live-scenario run, AUTHORIZED**, after being held across four grants. C1 is released by an **owner decision**, not by an argument that made it look already-covered — `docs/egress.md` is revised in this same commit to record that the volatile suffix's **provenance** changes from six synthetic fixtures to real scenario state while its **field set does not**. §Out of scope's streaming row is **unchanged** and still governs the product path; the carve-out admits streaming only as a measurement instrument under `tests/live/`, which is the only place it could live — `IHttpClient::send()` is blocking and whole-body-only. **Measurement only:** no latency remedy, no prose-schema removal, and no `claude.maxTokens` value for Sonnet follows from any of it. ≈$1.00 against $3.68 remaining.
- v1.8.10 — **§Corrections item 28: the C2 re-analysis, which corrects this document's own record twice and strikes one over-claimed clause. No new request, no network, no cost.** Range restriction is *demonstrated* — banding Sonnet's output to Haiku's width collapses r² **0.734 → 0.036**, and the closed form predicts **0.019** on Haiku's spread. But the two Haiku arms **never disagreed** (their slope CIs overlap), so v1.8.5's stated reason was wrong even though its verdict was right; and they license nothing in the other direction either (arm A's CI includes zero *and* excludes Sonnet's slope). The clause *"not by either quantity the product controls"* is **struck**. The C2 remedy is unchanged and now priced: at ±776 ms of intercept noise a repeat two-arm comparison needs a ≈1,100 ms gap to resolve and ≈$10 to reach by sample size, so **TTFT is the cheaper instrument as well as the better one**. The four per-order CSVs behind items 25, 27(f) and 28 are **archived in-tree**.
- v1.8.9 — **the ceiling-raised re-run reported. §Corrections item 27: the truncation thesis is confirmed outright, the tail is three times shorter than the estimate v1.8.6 refused to make, the built-in control was swamped, and C2 reopens from a run that was not about C2.** 48 orders on `claude-sonnet-5` at `maxTokens = 8192` under the v1.8.8 grant, paired with the 512 run. **(a) Acceptance 48/48 (100.0 %) against 44/48**, with `reject.schema`/`reject.shape` still 0.00 % and now no rejections at all — the 91.7 % was a configuration artifact end to end. **(b) Max output 673 tokens, 1.31× the old ceiling**, 7 of 48 above 512; v1.8.6 declined to scale by Haiku's 4.0× headroom, which would have said **~2,000 — 3× too high**, so the refusal is vindicated by measurement. **(c) The censored mean was nearly right:** 269.5 → **271.9**, +0.9 %; item 25 was right that it was a lower bound and right that the tail was un-sizable from it, and the understatement turned out to be under 1 % — reported because "you could not have known" honestly includes the times the unknown was small. **(d) The raise cost +0.5 %** ($5.65 → $5.67/four-ship-hour) because billing is on *actual* output tokens — while at 512 the four truncated orders were **billed for 512 tokens each and produced nothing**. **(e) The control could not do its job:** per-fixture means moved by up to **71 tokens** between the two runs and one fixture's max moved 478 → 193; a raise can only *lengthen*, both material shifts are *shortenings*, so they are sampling variance — **which makes 673 a sample maximum and not a bound, and any ceiling sized from one 48-order run a ceiling sized against noise.** **(f) C2 reopens:** latency-vs-output r² is **0.004 / 0.071 on Haiku** but **0.574 / 0.734 on Sonnet**, whose output range is 5× wider — **restricted range mechanically suppresses r²**, so v1.8.5's "dominated by service-side variance" was concluded from two arms with a flattened x-axis. Generation is **57 %** of Sonnet's mean round trip. **(g) No config value changes.** The default stays 512 because the default model is Haiku; the Sonnet numbers an operator needs are now on the record, and choosing a headroom policy over a noisy sample max is not this document's call.
- v1.8.8 — **the owner's fourth egress grant, recorded before the request made under it: the ceiling-raised re-run C7 needs.** Asked for explicitly rather than inferred, because the v1.8.5 grant enumerated two experiments and both had run — "same fixtures, same budget, therefore covered" is the inference §Tenet 4's gate exists to prevent. Same boundary as its three predecessors: the **six synthetic `LiveMain` fixtures**, no real scenario state, **`run-live-scenario.ps1` still ungranted** and C1 unchanged. **48 orders on `claude-sonnet-5` × the same six fixtures, with `claude.maxTokens` raised to 8,192** — the bound v1.8.7 derived — **paired with the censored run so the only difference is the ceiling**. A probe built to observe a censored tail must not itself censor, and since billing is on *actual* output tokens, an unused ceiling costs nothing. **The design carries its own control:** `max_tokens` is an enforced ceiling the model **is not aware of**, so the 44 already-completed responses should come back the same length; if they move, the run has found something item 25 does not predict. **≈ $0.38 against $4.06 remaining**, time-boxed by the 2026-08-31 introductory rate. **Measurement only — no config value is chosen in advance of the data**, and the default stays 512 until it reports.
- v1.8.7 — **the C7 config-surface revision, which changes the *shape* of `claude.maxTokens` and deliberately does not change its value.** Required before any code touches the field (§Tenet 4's companion rule: a config field is a contract, and it is revised in this document first). Three changes and one refusal. **(1) §Corrections item 26 — there are three response-size ceilings in this system and none of them knows about the other two:** `claude.maxTokens` in **tokens** (configurable, default 512, and **unbounded above** — the only check is `>= 1`), Stage A's `kMaxResponseBodyBytes` at **64 KiB** which rejects an over-long body as `range`, and `OrderRecorder`'s `kMaxRecordedBodyBytes` at **4 KiB** which truncates the recorded body **silently, with no marker**. The record cap is the *lowest* of the three and would be crossed first by any meaningful raise — so the diagnostic record for exactly the long responses that motivate raising the ceiling is the first thing a raise would destroy. **This is not hypothetical: the one `rawBody` in this tree's order log is exactly 4,096 bytes**, already truncated mid-array, on a *rejected* order. **(2) `claude.maxTokens` gains an upper bound of 8,192**, derived rather than picked — 64 KiB ÷ a deliberately generous 8 bytes/token, so a response *at* the ceiling cannot be rejected by Stage A for length, and comfortably inside the ~16,000-token band a **non-streaming** request can complete in (this adapter has no streaming path). Both models' API maxima (64K Haiku 4.5, 128K Sonnet 5) are far above it: the binding constraint is this product's, not the API's. **(3) The field's documentation becomes model-conditional**, following the precedent `claude.effort` already set one row below it. **The refusal: the default stays 512.** It is correct for the default model and it remains un-derivable for any other, exactly as item 25 established — **and a bound is not a value.**
- v1.8.6 — **§Corrections item 25: the truncation is confirmed structurally, and the question it raises is shown to be unanswerable from the data that raised it.** Re-analysis of the per-order rows the v1.8.5 runs already wrote — **no new request; the `--csv` output was on disk.** All **4** Sonnet rejections have `tokensOut` **exactly 512** and **0 of 44** accepted responses do, so the cap partitions the run with no exceptions; the largest *completed* response is **502 — 10 tokens of headroom**. Truncation is **concentrated in two of six fixtures** (`winchester, one track close` 3/8, `no tracks at all` 1/8, the rest 0/8), so verbosity is situation-dependent and one scalar ceiling is covering non-overlapping distributions. Against the same 512 ceiling, **120 Haiku orders peak at 127 tokens — 4.0× headroom against Sonnet's 1.02×**, which is the quantitative form of "Haiku-shaped." **And the limit: `269.5` is a *censored* mean** — the 4 truncated responses were counted at 512 when their true lengths are ≥ 512 and unobserved, so 269.5 is a **lower bound**, the completed-only mean is 247.4, and the true mean is above both. **The distribution is censored exactly where the open question lives**, so the correct `maxTokens` is *not derivable from this run* — and scaling it by Haiku's headroom ratio is the same reasoning that missed by 79 % in item 24(a). The question moves from **unmeasured to censored**; it does not close. Carried as **C7**, blocked on an owner decision, with **no config change made**.
- v1.8.5 — **three Phase 3 diagnostic runs, of which two returned negative results and the third refuted a §Cost model assumption.** (a) **The C2 latency decomposition is INCONCLUSIVE.** Two arms of 60 orders differing only in doctrine size: output-token count explains **0.4 % and 7 %** of latency variance (r²), so the intercept/slope split is fitting noise and the two arms disagree about it by 5×. Cutting 1,617 cached tokens moved p50 **down** 16 % and p95 **up** 54 % — not the signature of a real fixed-cost effect. And the same configuration measured p95 **3,674 ms at n = 60 against 4,615 ms at n = 240**, so some of the reported miss is sampling. Latency is dominated by service-side variance, and **the next step is a better instrument — time-to-first-token separated from generation — not a fix.** (b) **C3's cost thesis is confirmed and its quality question is not:** the smaller prefix cost **$0.92/four-ship-hour against $1.05**, −12 %, but that arm accepted 59/60 where the full-doctrine arm accepted 60/60. The prose-schema removal stays **unmade**. (c) **§Corrections item 24 — tokenization is model-specific, and `claude.maxTokens` is Haiku-shaped.** The identical prefix is **7,608 tokens on Haiku and 10,493 on Sonnet, +37.9 %**, so §Cost model's scaled non-Haiku rows were wrong: measured Sonnet is **$0.00784/order versus the $0.00439 predicted, +79 %**, and the Opus row is now a floor rather than a figure. Sonnet also accepted **44/48 (91.7 %)** — but with `reject.schema` and `reject.shape` both **0.00 %** and all four failures reading `parse: body is not well-formed JSON` or `content array carries no text block`, which is a **truncation** signature: Sonnet's mean output is 269.5 tokens against a `maxTokens` of 512 chosen when the only measured model wrote ~105. **That is a configuration finding and must not be cited as an order-quality one.**
- v1.8.4 — **PHASE 2 CLOSED. Every gate item has a result, two of them failures, and the cost model is rebuilt on live `usage` for the third revision running.** The measurements: **240/240 orders accepted (100 %)**, `reject.schema` and `reject.shape` both **0.00 %**, cache hit rate **240/240**, cost **$1.05/four-ship-hour against a ≤ $1.10 target — MET**. The failures, reported as failures: **p95 is 4,615 ms against a ≤ 2.5 s target, missed by 85 %** (p50 2,602 ms also over); and **measured cost per order is +39.4 % from §Cost model's own row, outside its ±20 % gate** — the document's model was wrong, not the product. §Corrections item 19's prediction is **CONFIRMED on both halves** by three requests that isolate them: the canonical schema is rejected `"Schema type 'oneOf' is not supported"`, a hybrid differing only in `oneOf`→`anyOf` is rejected `"For 'integer' type, properties maximum, minimum are not supported"`, and the shipped projection is accepted — so the projection is load-bearing rather than insurance. **§Corrections item 22:** the order schema is transmitted **twice** per request — once as prose in the prefix, once in `output_config` — so OQ-8's 4,489-token count measured the prompt text while **7,608 tokens** are what cache and bill; that is ~71 % of the prefix and ~52 % of the bill, and it is why the cost model missed. It also **withdraws** v1.8.2's cache-cliff warning: the real margin is 86 %, not 9.6 %. **§Corrections item 23:** `input_tokens` *excludes* cached tokens rather than containing them, an error that printed a negative price loudly enough to be caught — and would have shipped silently had the two populations been closer in size. **The memorised-coordinate comparison ran:** Perth does **not** reproduce (0 of 11), but the same run found a **round-number longitude** substitution twice on one situation, putting waypoints 540 km outside the geofence. Different wrong number, same failure mode, same containment — a better argument for Stage B than the original finding. Five items are carried out of Phase 2 with reasons and destinations, including the in-engine live-scenario run, which was **not** performed because the v1.8.3 grant did not cover it.
- v1.8.3 — **recorded the owner's second egress grant — the full, snapshot-carrying Phase 2 measurement gate — before the first request made under it, and recorded the boundary the grant does not cross.** The v1.8.1 grant was prefix-only; it resolved OQ-8 and expired with it. This one authorizes the schema-acceptance probe, the ~240-order soak, the p95 sample, and the memorised-coordinate comparison, all of which transmit the volatile suffix: position, velocity, heading, team, reported tracks, and loadout, enumerated rather than summarised. **The boundary:** it covers the six synthetic situation fixtures in `tests/live/LiveMain.cpp` and **not** `run-live-scenario.ps1` against `oppint_red_interceptor`, which would transmit real scenario state from a deployed mission file. Same field set, different provenance — and treating those as interchangeable because the fields match is exactly the inference §Tenet 4's gate exists to prevent, so the in-engine live-scenario run stays ungranted and is carried out of Phase 2. Also recorded: **the live harness has no Claude path** — `tests/live/LiveMain.cpp` constructs `LocalLlmClient` directly, so every gated measurement needs a backend selector wired through `runWorkerCall`'s `ILlmClient&` before it can run. That is ungated work and was not visible when the gate was written.
- v1.8.2 — **OQ-8 is RESOLVED on a measurement, and reconciling §Cost model to it exposed that the section had been 2.8× understated for five revisions.** The deployed prefix measures **4,489 tokens** against Anthropic's tokenizer (17,756 bytes, 3.955 bytes/token, `claude-haiku-4-5`, 2026-08-04) — it **clears** the 4,096-token prompt-cache minimum by 393 tokens, so **the prefix caches as written, the padding delta is zero, and the judgement OQ-8 reserved for the owner since v1.2 — whether ~2,900 tokens of genuine doctrine were worth writing — never has to be made.** §Cost model is recomputed off the measured count: **$0.76 per four-ship-hour cached, against the ≤ $1.10 target — met**, with $3.66 uncached, which misses by 3.3×. Three things this revision records beyond the number. (a) **§Corrections item 21** — every uncached figure from v1.3 onward was computed against the 1,200-token doctrine-less prefix that item 16 had *already established* was never deployed; a fact was corrected in one section and left standing in the section whose entire output was arithmetic over it. §Cost model's assumptions now carry a measured/assumed status column so a superseded input is visible without re-deriving the chain. (b) The **budget line said $100 and the balance is $5** — every "hours on $100" figure this document printed was 20× reality, unnoticed because the number was never binding until now. (c) **The margin is 9.6 %.** Deleting ~1,554 bytes of doctrine drops the prefix under the minimum, at 4.8× the cost, with no error and no counter — so AIC-BE-3's startup comparison inverts from a standing reminder of a known-unmet condition into the **only** detector of a silent cost regression, and its criterion is rewritten accordingly. Two model inputs remain **assumed** and are named as such: the ~200-token suffix and ~80-token output, both carried unmeasured since v1.2, both readable from `usage` on the first live inference request.
- v1.8.1 — **recorded the owner's authorization for hosted egress, scoped narrowly to a prefix-only token count; and recorded that the probe it authorizes cannot run on an unfunded account.** The authorization is deliberately not blanket: it covers the stable prefix — system prompt, vocabulary, order schema, doctrine — and **no volatile suffix**, so no scenario state leaves the machine under it. Every scenario-carrying run stays gated. The second half is a prediction this document made and the first live request refuted: `count_tokens` was assumed reachable on a zero-balance key because it is a utility endpoint rather than an inference call. **It is not** — the API gateway rejects on credit balance before the distinction matters (§Corrections item 20). OQ-8 therefore has a funding precondition, which is a smaller obstacle than a technical one but is not the zero it was recorded as.
- v1.8 — **reconciled AIC-BE-2 against what a hosted adapter actually needs, before writing one.** Three changes, each of which the FR as drafted could not have anticipated because it predates the adapter. (a) **The Claude envelope unwrap is placed on the seam Phase 1b built for Ollama**: a third `EnvelopeFormat` value passed into Stage A by value, so Stage A gains a shape and never gains a backend. `RejectReason::Refusal` — declared since v1.2 and produced by nothing — becomes the `stop_reason == "refusal"` outcome, checked before `content` is read. (b) **AIC-BE-1's "send the same schema object, not a hand-copied variant" rule is restated to admit a mechanical projection**, because the hosted structured-output path does not accept the numeric and string bound keywords the four-branch encoding is built out of. The canonical document is unchanged — it measured 0/12 and a prediction is not grounds to touch it — and the hosted document is *computed* from it rather than typed beside it. A projection cannot drift; a copy can. (c) **AIC-BE-3 gains the prefix/suffix boundary as a value crossing to the worker**, because the hosted cache breakpoint has to be placed there and `LlmRequest` carried no way to say where "there" is. Also recorded: **OQ-8's remaining measurement is itself a live hosted request**, so it sits behind the authorization gate with everything else rather than being the free ten minutes it was written as.
- v1.7.6 — **Phase 1b CLOSED.** The live-scenario smoke **passes, 17 checks / 0 failed**, against the gate as re-specified in v1.7.5, and every Phase 1b gate item now has a result rather than a blocker. What the phase does not claim is enumerated in §Carried out of Phase 1b — five named, owned items, none of them a gate item marked green by omission. One is upgraded by this revision: the memorised-coordinate substitution **reproduces** — a third run drew `−31.952247, 115.857309` on a *different* entity, agreeing with the first to four decimal places on a coordinate that appears nowhere in the prompt. Three observations across two entities and three runs make it a characterised failure mode with a known containment rather than a sampling accident, and it is the strongest concrete evidence this phase produced for why Stage B is not optional.
- v1.7.5 — **refuted v1.7.4's explanation of the acceptance failure, then found the real one by fixing the observability gap that had made it a guess; and re-specified the live-smoke gate against what the OQ-6 scenario can supply.** v1.7.4 hypothesised that the `geofence` rejections came from a doctrine block that says *"egress toward the home field"* while carrying no coordinates. **Measured offline: false** — across nine situations and ten waypoint-carrying orders, three of them drawing `rtb`, the model put the waypoint on own-ship position every time at 0 m. AIC-DET-1's `rawBody` was empty on every Stage-B rejection, so the record said how far the waypoint was and never where; **fixed, the run repeated, and the offending order names itself: `posture: hold` with `waypoint: −31.952876, 115.860450` — Perth, Western Australia.** The model substitutes a memorised real-world coordinate for a waypoint whose correct value was own-ship position. That is a low-rate hallucination of a well-formed, in-range, entirely wrong number — not a doctrine gap, not schema-constrainable, and exactly the case the Stage-B envelope exists to catch. The live-smoke gate is re-specified to assert over the **commanded window** rather than a wall-clock 10 minutes, to drop *"entity completes the scenario"* (the commanded entity is opposed; whether it survives is the scenario's outcome, not the commander's correctness), and to **report** acceptance rather than bar it — the ≥ 95 % bar stays where the sample size is, on the 200-order soak.
- v1.7.4 — **ran the two Phase 1b gate items that v1.7.1 recorded as unreachable, and reported what they produced.** The in-engine live smoke and the H1 paired runs both executed, because v1.7.2 made `commander.enabled` reachable on the headless host; H1's two logs exist for review. **The live smoke FAILED one assertion — acceptance 50 % against a ≥ 90 % bar** — and the failure is recorded with its cause rather than restated into something that passes: all three rejections were the Stage-B safety envelope refusing bad orders, two of them waypoints ~5,300 km away, which traces to a doctrine block that says *"egress toward the home field"* while carrying no coordinates by design. Two further findings: the commanded entities are **destroyed at ~85 s**, so the gate item's "10-minute run" is not what this scenario delivers and the sample sizes are correspondingly small; and the reported p95 is the second-highest of five samples, so no percentile claim is made from it. `reject.shape` held at **0 %** against situations nobody authored — the first evidence for it from outside the hand-written set.
- v1.7.3 — **added Stage-B check B8 and the `loadout` reject reason**, resolving the one standing order-quality miss the Phase 1b gate recorded. A winchester aircraft with a close contact drew `engage` where doctrine says `rtb`; two focused doctrine iterations moved it not at all, which is §Rabbit holes' timebox signal rather than a reason to keep writing. B8 rejects `engage`/`crank` when the Tier-1-reported loadout for the order's snapshot window is **entirely dry** — at least one hardpoint reported and every one of them at `ammoCount == 0`. An **empty** reported loadout deliberately does **not** trigger it: that means "this script does not report stores", which §Validation requires to keep receiving orders, and B3 already rejects its targeted orders. This adds a reject reason, not a Lua function, posture, order field, or config field. The value of the change is that an unmeasurable quality complaint becomes a counted rejection with a runbook row behind it.
- v1.7.2 — **authorized a deployed-configuration source for the headless host**, resolving §Corrections item 17 and unblocking the two Phase 1b gate items it stranded. AIC-API-2 previously specified *what* the config surface is and left *how it arrives* entirely to `applyConfigFields()`, which only the UI host calls — so `commander.enabled` could not be turned on for an automated run, and the live-scenario smoke and H1 were both unreachable. The plugin now reads `data/config/plugins/ai-commander.cfg` at `initialize()` as its **default source**, through the same `tryParseConfigFields` all-or-nothing path. This adds **no config field** — it adds a *source* for the fields this FR already defines. The asymmetry it removes is the defect: the UI host has applied that file since Phase 0 and the shipped example config has told operators to put it there since Phase 0, so a stale file could already enable the commander on the UI path while the headless path silently ignored it. Fail-closed is unchanged and now carries a test: absent file → compiled defaults → disabled.
- v1.7.1 — **corrected the branch partition** on the first live run of the shipping adapter. v1.7 split the schema by waypoint presence, which grouped `defend` with `engage` and `crank`; those postures agree about waypoints and disagree about targets, so the branch could not state the target rule and 2/12 live orders were rejected *"defend must not carry a targetEntityId"*. Partitioning by **whole constraint profile** yields four branches — transit, hold, targeted, defend — and measured **0 rejections in 24 live orders**. AIC-ORD-1's acceptance criterion is strengthened to assert branch bounds against the A6 predicates per posture, which is the check that would have caught v1.7's split. Prefix growth (4,738 → 14,074 bytes) recorded as a real cost and as evidence into OQ-8.
- v1.7 — **restructured AIC-ORD-1's embedded schema and resolved OQ-5 and OQ-6**, on measurements taken at Phase 1b start. v1.6 predicted that Stage-A `shape` rejections would be non-trivial and would fall as prompt wording improved; both halves were tested and the second is false. The shipped prompt against the shipped schema produced **10/12 shape rejections**, because Ollama's `format` compels only what the schema's `required` array names, and this model **omits every optional field** rather than over-emitting as v1.6's models did. Adding a field-presence block to the prompt moved nothing; `if`/`then`/`else` was confirmed unhonoured; a schema expressed as **`oneOf` over two posture-discriminated branches produced 0/12 with the prompt untouched**. AIC-ORD-1 now specifies that shape. Separately, **OQ-5 resolves "No"** — the entitlement subsystem is absent from every sim binary and from the import library plugins link, so the contingency check it proposed could not have been written — and **OQ-6 resolves to `oppint_red_interceptor` / `RedSu35_01`** by owner pick.
- v1.6 — **resolved OQ-1 and OQ-2** against the target machine and a live spike, unblocking Phase 1b. Ollama 0.32.5 is installed and serving on `localhost:11434` with 14 instruct models already imported, so the shipped split GGUFs need no import at all; its `format` parameter enforces the AIC-ORD-1 schema (3/3 valid across two models), which retires the GBNF concern. A GPU is present — RTX 4070 Ti SUPER, 16 GB — giving **~1.7 s warm** round trips on a 7B. Three findings folded in: model **cold start (22–46 s) exceeds `commander.requestTimeoutS`**, so the first order of a run times out as configured; `local.model`'s default was a GGUF *filename* where Ollama needs a *tag*; and JSON Schema cannot express AIC-ORD-1's conditional-presence rules, so Stage-A `shape` rejections are load-bearing rather than incidental.
- v1.5 — restated §Source control's CI paragraph against the **built** implementation. v1.1's split assumed hosted runners could execute the order-schema and validator fixtures; they cannot, because every test links `n8ro-core` (`JsonValue` for the schema and Stage-A validator, `TestRunner` for the harness). The real boundary is *compiles / does not compile*, not *which FRs*. Documented the two workflows that now exist, the badge obligation, the shared-release-tree cleanup obligation, and why `clang-format` ships advisory rather than gating.
- v1.4 — **resolved OQ-4** by enumerating the MCP stack's tool surface from the shipped binaries. `n8ro-sim-bot.exe` registers exactly two tools (`workbook_describe_api`, `workbook_eval`) and **no entity-control tool**; `n8ro-data-bot.exe`'s ~37 tools are all database-authoring operations. Alternative 2 does not supersede Alternative 1. Cascaded through §Prior art, §Out of scope (Deferred → Out of scope, verified absent), §Cross-service impact, §Alternatives Option 2 (rewritten off evidence, with four concrete disqualifications), and §Quality gate notes. Recorded `workbook_eval` as an adjacent LLM-facing arbitrary-Lua channel that reaches the same safety goal by human approval where this plugin reaches it by a closed schema.
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
assumption. Items 10–12 were found by a spike against the installed inference server (2026-08-01).
Items 13–14 were found at **Phase 1b start** (2026-08-02) by measuring what items 10 and 12
predicted; one of those predictions did not survive contact. Each is load-bearing.

1. **HTTPS is available.** `IHttpClient.h` warns that `https` needs the OpenSSL build. `bin/libssl-3-x64.dll` and `bin/libcrypto-3-x64.dll` are both present, so the Phase 2 call to `api.anthropic.com` is not blocked on a missing TLS backend. This must still be asserted at runtime (see AIC-BE-4).
2. **A sanctioned async mechanism exists.** `IThreadRunner::submitBackgroundTask` is declared in the SDK. The plugin does not need a hand-rolled `std::thread` — but `PluginContext::threadRunner` is documented nullable, so a fallback is still required (OQ-7).
3. **The bot executables live in `bin/`, not `bin/ai/`.** `bin/ai/` contains only `.env`, `n8ro-mcp.exe`, and `run-full-stack.cmd`; `n8ro-data-bot.exe` and `n8ro-sim-bot.exe` are one level up in `bin/`, alongside the readiness flag `bin/n8ro-mcp-ready.tmp`. Relevant only to OQ-4.
4. **Sensor tracks and weapon loadout have no public C++ read seam.** *(v1.2)* There is no track component in `schema-reference.json` (225 records) or `ComponentTypeNames.h` (15 components); `IEntityManager` exposes no track accessor; `SensorScriptingApi.h` is an opaque factory returning `unique_ptr<IScriptingApiModule>`. `sensor.getDetectionList` is documented as returning *"runtime detections"* and exists only in Lua. Separately, `componentWeaponStoreManagement/loadout` is a schema `list`, and `ComponentFieldAccess` has no list reader; live remaining ammo is available only through `weapon.getWeaponLoadout`, also Lua-only. **Consequence:** the plugin cannot build the `tracks[]` and `own.loadout[]` rows of §Exactly what is transmitted by itself, and Stage-B check B3 cannot query the engine. Resolved by Tier-1 ingress — see AIC-API-1 `reportTrack` / `reportLoadout`.
5. **A transport failure is `std::nullopt`, not `statusCode == 0`.** *(v1.2)* `IHttpClient::send()` returns `std::optional<HttpResponse>` and yields `std::nullopt` on a transport / TLS failure or a malformed URL. A response that *is* returned always carries a real status-line code. The `HttpResponse::statusCode == 0` sentinel documented in the struct is therefore not what a caller observes on failure. Affects Stage-A check A1, AIC-BE-1, and AIC-BE-4.
6. **Detections arrive as repeating triples, not a list.** *(v1.2)* `sensor.getDetectionList(sensorEntityId)` returns `targetEntityId, range_m, snr_DB` repeated as Lua multiple return values, and returns *no* values when there is no detection. The countable enumeration idiom is `sensor.getTrackNr(id)` followed by `sensor.getTrackById(id, i)` with `i` **1-based**. Affects the reference Tier-1 script and the snapshot.
7. **Transform velocity, orientation, and acceleration are runtime columns, not schema leaves.** *(v1.2)* They are declared only in `TransformRuntimeColumns.h`, addressed by **dot**-joined paths (`velocityNed.x`), and — per that header — a path that does not resolve reads back `0` **silently, without an error**. That defeats this PRD's rule that a `std::nullopt` on a required snapshot field aborts the snapshot, because a mistyped path yields a plausible zero instead of a `nullopt`. Drives AIC-ARCH-4 and OQ-9. Stage-B B1's `entityControl.exists` is likewise a Lua verb; the C++ equivalent is `IEntityManager::getEntity(id) != nullptr`.
8. **ThreadSanitizer does not exist on this platform.** *(v1.3)* No `clang_rt.tsan` runtime ships anywhere in Visual Studio 2026 Insiders. `VC\Tools\MSVC\14.51.36231\include\sanitizer\` carries `tsan_interface.h` and `tsan_interface_atomic.h` — **headers only**, with no library to link — alongside the ASan, UBSan, and fuzzer runtimes, which do ship complete. Neither MSVC nor the bundled LLVM toolchain supports TSan on Windows. **Consequence:** the v1.2 Phase 1a gate item "TSAN clean" was permanently unsatisfiable, and a gate no build can pass is worse than no gate — it trains everyone to wave the checklist through. Replaced by the concurrency-evidence set in §Validation and test plan, with the residual gap recorded as a risk and race-detector coverage moved to §Out of scope. Affects the Threading risk row, the integration suite, the Phase 1a gate, the rollback triggers, and UAC-AIC-ARCH-2.
9. **The rendered prompt prefix is ~1,200 tokens, not ~800.** *(v1.3)* Measured on a live engine run: **4,738 bytes**, logged at startup as `prefixBytes`, ≈ 1,200 tokens. Haiku 4.5's prompt-cache minimum is 4,096 tokens, so the prefix as written silently does not cache — the direction v1.2 already assumed, but now with a real number rather than the "roughly 800–1500" range OQ-8 was written against. **Consequence:** the Cost model's per-order arithmetic was computed from a 1,000-token prompt and is understated; it is recomputed against ~1,400 input tokens below. This is **evidence into OQ-8, not a resolution of it** — whether to pad to the cache minimum remains a Phase 2 cost judgement for the owner.

10. **A cold model load costs more than the whole request timeout.** *(v1.6)* Measured on the verification host: a warm 7B answers in ~1.7 s, but the **first** request after Ollama evicts a model took **22.6 s (qwen2.5 7B)** and **45.9 s (llama3.1 8B)**. `commander.requestTimeoutS` defaulted to 30 s, so the first order of every run would have timed out — reliably, not occasionally — and driven the fallback ladder from the moment the commander enabled. Ollama holds a model for its `keep_alive` window (5 minutes by default), which a 20 s cadence sustains indefinitely, so this is a first-order-of-a-run cost rather than a recurring one. Default raised to 90 s. A model warm-up at backend construction would remove it entirely and is a Phase 1b design option, deliberately **not** pre-committed here.
11. **`local.model` named a file where Ollama needs a tag.** *(v1.6)* The default was `llama-3.2-3b-instruct-q4_k_m` — the shipped GGUF's filename. Ollama addresses models by tag (`qwen2.5:7b-instruct-q8_0`). As shipped, the local backend would have failed its first request with a model-not-found, and the failure would have surfaced as a generic transport rejection rather than as a configuration error.
12. **JSON Schema cannot express the conditional-presence rules, so Stage-A `shape` is load-bearing.** *(v1.6)* Ollama's `format` enforced types, required fields, and both enums 3/3 in the spike — and both models still produced orders that AIC-ORD-1 forbids: non-zero `orbitRadiusM` on `ingress` and `engage`, and `engage` with an empty `targetEntityId`. Draft-07 cannot state "`orbitRadiusM` must be 0 unless `posture == hold`" without `if`/`then`/`else`, which the constrained-decoding path does not reliably honour. **Consequence:** constrained decoding secures A4 (schema) but not A6 (shape); the Stage-A conditional checks are the only thing enforcing those rules, and the `reject.shape` counter is expected to be non-trivial in Phase 1b rather than near-zero. *Caveat on the evidence:* the spike used a minimal hand-written prompt, not the real `PromptRenderer` prefix, which states the conditional rules and embeds the schema with its per-field descriptions. This bounds the failure mode; it does not measure the shipping system's rate.

13. **The conditional-presence failure is *omission*, not over-emission — and `oneOf` fixes it where the prompt cannot.** *(v1.7)* Item 12's caveat asked for the shipping system's rate; it was measured, and it inverted the failure mode. Against the **real** `PromptRenderer` prefix, the **real** doctrine block, and the **shipped** Stage-A A6 rules, `qwen2.5:7b-instruct-q8_0` at temperature 0 produced **10/12 `shape` rejections over six situations spanning all six postures**. The cause is not the over-emission item 12 recorded: this model emits **no optional field at all** — no `targetEntityId`, no `orbitRadiusM`, no `waypoint`, on any order — because Ollama's `format` compels exactly what the schema's `required` array names, and none of the three is in it. Four levers were measured against that baseline:

    | Lever | `shape` rejections |
    |---|---|
    | shipped prompt + shipped schema | **10/12** |
    | `targetEntityId` + `orbitRadiusM` added to `required` | 4/12 |
    | + an explicit field-presence block in the prompt | 4/12 — **no change** |
    | + one worked example per posture | 2/12 |
    | + `if`/`then`/`else` on waypoint presence | 2/12 — **not honoured**, as item 12 predicted |
    | schema as **`oneOf`** over two posture-discriminated branches | **0/12** |
    | the same `oneOf`, **prompt untouched** | **0/12** |

    **Two consequences, in opposite directions.** Item 12's *mechanism* is confirmed — `format` secures A4 and not A6, and `if`/`then`/`else` does not close the gap. Item 12's *remedy* is refuted: the prompt is not the lever. A field-presence block stating the A6 rules imperatively changed nothing at all, which is the direct measurement of the "shape rejections measure prompt quality" hypothesis this document carried into the Phase 1b gate. What closes it is a schema the decoder can actually enforce: `oneOf` over `{ingress, hold, rtb}` **with** `waypoint` required and `{engage, crank, defend}` **without** it. That adds no field, posture, configuration value, verb, or backend — it re-expresses the field contract of AIC-ORD-1's table in a form a constrained decoder can hold. AIC-ORD-1 is restated accordingly. **Stage-A A6 is not relaxed by one line:** the decoder becomes a second line of defence, not a replacement, because "the validator is the real defence" and a backend that stops honouring `format` must still be caught.

14. **Today's cold load is 3.9 s, not 22–46 s — and a warm-up ping saves nothing.** *(v1.7)* Re-measured with `keep_alive: 0` forcing eviction before each sample: first order cold **3,860 ms** (server-reported `load_duration` 2,451 ms), second order warm 1,170 ms. The gap against item 10's 22–46 s is almost certainly the OS page cache — item 10 measured a cold *disk* read, this measures a VRAM-evicted but file-cached load — so **item 10 is not superseded; it remains the worst case**, and both sit inside the 90 s timeout. The design question item 10 left open is answered by the same measurement: a `num_predict: 1` warm-up ping costs **2,503 ms** and leaves the first real order at 1,432 ms, i.e. **3,935 ms against 3,860 ms without it**. A warm-up relocates the cold cost, it does not remove it, so the adapter does **not** warm the model at construction. What it does instead is answer item 11's complaint directly: a one-shot `GET /api/tags` preflight on the worker, on the first request only, so a mistyped model tag surfaces as a named configuration error instead of a generic transport rejection.

15. **Two branches were the wrong number; the partition is by constraint profile, not by waypoint.** *(v1.7.1)* Item 13's remedy was right and its partition was not. v1.7 split the document by waypoint presence, which put `defend` in the same branch as `engage` and `crank`. Those three agree about waypoints and **disagree about targets** — `engage` and `crank` require a non-empty `targetEntityId`, `defend` forbids one — so that branch could not state the target rule unconditionally, and the first live soak through the shipping adapter measured **2/12 rejections**, every one of them reading *"posture 'defend' must not carry a targetEntityId"*. A branch can only state a rule its postures agree on. The A6 rules induce exactly **four** constraint profiles across the six postures — transit (`ingress`, `rtb`), hold, targeted (`engage`, `crank`), and defend — and at four branches the same harness measured **0 rejections in 24 orders**, `reject.schema` and `reject.shape` both 0 %. **The lesson generalizes past this schema:** v1.7's acceptance criterion asked only that the branches exist, which the wrong split satisfied. The criterion now asserts, for every posture in every branch, that the branch's bounds agree with the A6 predicate for that posture — a test the wrong split fails. The cost is that shared field descriptions repeat per branch: the rendered prefix went from 4,738 bytes to **14,074**, and measured p95 from ~1.7 s to ~2.2 s. Both are recorded rather than optimized away, and the prefix figure is material to OQ-8.

16. **The doctrine block was never deployed, and nothing said so.** *(v1.7.1)* `prompt.doctrinePath` defaults to `data/doctrine.txt`, which the plugin resolves against the **host's** working directory — the release root — and nothing ever placed the file there. Verified: `C:\N8RO\data\doctrine.txt` did not exist, and `readDoctrine` returned empty silently, so the prefix rendered `"(none provided)"`. **Consequence, and it reaches backwards:** every deployed run since Phase 0 has been running doctrine-less, including the one that produced item 9's measured `prefixBytes = 4,738`. That figure is a *doctrine-less* prefix, not the shipping one. With the doctrine loaded and the four-branch schema, the deployed prefix measures **17,756 bytes**. The failure mode is the worst kind: no counter moves, no rejection is logged, and only order *quality* degrades. Fixed on both sides — `initialize()` now logs a WARNING naming the resolved path when the file is missing and an INFO with its byte count when it loads, and the post-build event seeds it into the release tree **only if absent** (the file is edited by whoever tunes tactics; clobbering their edits on every rebuild would be worse than not seeding it).

17. **The headless host does not apply per-plugin configuration, so the in-engine live smoke cannot be automated.** *(v1.7.1)* Phase 1a recorded this as a note in the example config; Phase 1b confirmed it and hit the consequence. A `data/config/plugins/ai-commander.cfg` carrying `commander.enabled=true` and `commander.backend=local` produced `backend=stub enabled=false` in the startup log of `n8ro-sim-local.exe`. Applying that file is the UI host's job, through the plugin config editor, which calls `applyConfigFields()`. **Consequence:** §Validation's live-scenario smoke — *"10-minute run on the OQ-6 scenario with `commander.backend = "local"`"* — is not reachable from the headless binary as the system stands, and the Phase 1b gate item that depends on it is **not satisfied**. This is the same shape as the v1.2 "TSAN clean" item: a gate item no available configuration can pass. It is recorded as unmet rather than restated into something that passes, and the two ways out — drive the run from the UI host manually, or give the plugin a documented way to take configuration on a headless run — are an owner decision, because the second one changes how configuration reaches a fail-closed switch.

    **Resolved 2026-08-03 (v1.7.2): the second way out, authorized and specified in AIC-API-2.** The plugin reads `data/config/plugins/ai-commander.cfg` at `initialize()` as its default source, through the same all-or-nothing `tryParseConfigFields` path the host path uses. The reasoning that made this the right call rather than the risky one: **the asymmetry was the defect.** The UI host has applied that exact file since Phase 0, and the shipped example config has instructed operators to place it there since Phase 0 — so a stale file could *already* enable the commander on the UI path. What the headless host had was not a safety property; it was a blind spot that made one host silently ignore a file the other honoured. Fail-closed is untouched and now carries a test rather than an argument: absent file → compiled defaults → `commander.enabled == false`. The residual exposure is stated plainly rather than waved off — **a stale `ai-commander.cfg` left in a release tree will now take effect on a headless run as well as an interactive one.** That is bounded by the deployment checklist, which already requires the deployed default config to ship both `commander.enabled` and `claude.enabled` false, and by the startup log line this revision makes mandatory: every run now states which file it read and how many fields it applied, so an unexpected configuration is visible in the first ten lines of a log rather than inferred from behaviour.

18. **Scenarios live in the database, not in the seed JSON.** *(v1.7.1)* Discovered while wiring the live smoke, and stated because it cost a wrong turn: `data/resources/seed/realistic_scenario_seed_data.json` is an *import source*, and the engine loads scenarios from binary records under `data/db/N8roSimSchema/Profiles/Scenario/*.n8ro.instance` (the run failed with *"cannot open file: .../Mariana Shield AI.n8ro.instance"* after the seed was edited). Those records are compressed binary that no text edit produces, so creating a scenario **variant** needs the data-authoring tooling. The live smoke therefore swaps the mission *script* the shipped scenario already points at, with backup and restore, rather than adding a scenario.

19. **The hosted structured-output path does not accept the keywords the four-branch encoding is built out of.** *(v1.8 — read from Anthropic's published structured-output support list at Phase 2 start, **not yet measured against the live API**, and recorded here as a prediction with a documented basis rather than as a finding.)* That list names `enum`, `const`, `anyOf`, `allOf`, and `$ref`/`$def` as supported, and names **numeric constraints (`minimum`, `maximum`, `multipleOf`) and string constraints (`minLength`, `maxLength`) as unsupported**. §Corrections item 15's four-branch encoding does not rest on the branching alone — it rests on the branching *plus pinned bounds*: `targetEntityId` is forced empty by `minLength: 0, maxLength: 0`, `orbitRadiusM` is forced to zero by `minimum: 0, maximum: 0`, and `schemaVersion` is pinned by `minimum: 1, maximum: 1`. **Strip those and the mechanism that took `reject.shape` from 10/12 to 0/12 is gone**, leaving exactly the A6 failure mode item 13 measured.

    **Two things make this actionable rather than alarming.** First, the official SDKs are documented to *strip* unsupported constraints before sending and re-validate client-side — and stripping is only worth implementing if the API **rejects** them, since keywords that were merely ignored could be passed through untouched. So the likely hosted behaviour is a 400, not a silent degradation, and a 400 is loud. This adapter is raw HTTP and has no SDK stripping on its behalf, so it must do the projection itself. Second, **Stage-A A6 is unchanged and is what makes being wrong here safe**: a backend that stops honouring structured output must fail as a `shape` rejection, never as an accepted order, and that requirement has been in AIC-BE-1 since v1.7.

    **Consequence, and the part that is a decision rather than an observation:** the canonical `orderJsonSchema()` is **not** modified. It measured 0/12 against the deployed local backend, and a documented prediction about a second backend is not grounds to touch a measured-good artifact — that is the mistake v1.7.1 was written about, in the opposite direction. Instead AIC-BE-1's one-definition rule is restated to admit a **mechanical projection**, and the hosted document is computed from the canonical one. What remains genuinely unknown is whether the hosted path 400s or ignores, and what `reject.shape` actually does on Claude; both are answered at the first authorized request and neither is assumed here.

    **(v1.8.4 — MEASURED 2026-08-04, and the prediction holds on both halves.)** Three requests to `POST /v1/messages`, each differing from the last in one thing, carrying two schema documents and a fixed placeholder message and **no scenario state**. The API rejects with a 400 that names the offending keyword, exactly as the "SDKs strip, therefore the API rejects" reasoning above inferred — it does not silently ignore.

    | Document sent | Result | The API's own words |
    |---|---|---|
    | **Canonical** (`oneOf` + all bounds) | **400** | `output_config.format.schema: Schema type 'oneOf' is not supported` |
    | **Hybrid** (`oneOf`→`anyOf`, **all bounds retained**) | **400** | `output_config.format.schema: For 'integer' type, properties maximum, minimum are not supported` |
    | **Projection** (what the adapter ships) | **200** | accepted |

    **The hybrid is why this is a full confirmation rather than half of one.** The canonical document fails on `oneOf` *before* any bound is evaluated, so that request alone settles the `oneOf` half and leaves the bounds half untested — and the projection changes both things at once, so comparing it against the canonical cannot separate them either. A document differing from the canonical in exactly one respect isolates the second half, and it is rejected on `minimum`/`maximum` by name. **Both halves of the prediction were right, and the projection is load-bearing on both counts rather than insurance against one.**

    **A methodological correction, recorded because this loop made the error before catching it.** The obvious way to test this is to drive the adapter and see whether it succeeds. That is invalid: **the adapter sends the projection**, so the keywords the prediction is about are not in the request, and adapter success is evidence about the projection and none at all about the prediction. The first probe run this loop performed did exactly that and printed *"PREDICTION REFUTED"* on a 200 — the wrong conclusion from a correct observation of the wrong thing. The probe is now `tests/live/probe-canonical-schema.ps1`, which posts the documents themselves, and `--mode schema` says in its own output that it cannot settle item 19. **The general form: when a mitigation is already deployed, exercising the mitigated path measures the mitigation, not the hazard.**

    **What does *not* follow from this, stated because the temptation is real.** The canonical document is still not modified. It measured 0/12 locally, and now that `oneOf` is *confirmed* unsupported on the hosted path there is a live argument for re-expressing it as `anyOf` everywhere and deleting the projection — but that would trade a measured-good local encoding for an unmeasured one to remove a computation that costs nothing, which is R1's reasoning unchanged. The projection stays.

20. **`count_tokens` is not reachable on a zero-balance account, so OQ-8 has a funding precondition.** *(v1.8.1 — measured 2026-08-03 by the first live request this project ever made.)* v1.8 scoped the owner's authorization around a prefix-only token count on the assumption that `POST /v1/messages/count_tokens` would be free — it is a utility endpoint, not an inference call, so the reasoning was that a key on an unfunded account would still be able to call it. The request was sent and returned **HTTP 400: *"Your credit balance is too low to access the Anthropic API."*** The endpoint may well be unbilled; **the gateway refuses the request before billing is consulted at all**, and "costs nothing" and "works on an empty account" turn out to be different claims. Only the first was true.

    **Consequence:** OQ-8 cannot be resolved for free, and the "the measurement half is already done / this is ten minutes of work" framing this document has carried since v1.3 now has a precondition attached in both directions — a live call *and* a funded account. Nothing else changed: the deployed prefix was dumped from the real `PromptRenderer` at **17,756 bytes** (doctrine 6,932, schema 8,951), carries **zero non-ASCII characters**, and the request reached the API and was rejected on billing rather than on form — so the shape, the encoding, and the credential path are all proven and the measurement is a single command once an account is funded.

    **A second, smaller finding, recorded because it is the third instance of the same failure mode.** The probe script initially reported the 400 with an *empty* message, because `$_.ErrorDetails.Message` is routinely blank on Windows PowerShell 5.1 for a non-2xx from `Invoke-RestMethod` — the body is only on the response stream. A legible API error was rendered as a bare status code, and diagnosing it cost a second round trip. This is the same shape as item 16's silent `readDoctrine` and v1.7.5's block-buffered stdout: **a channel that fails quietly instead of loudly.** Fixed by reading the stream; noted here because the pattern keeps recurring and the fix is always the same — never trust a diagnostic channel that can return empty on failure.

21. **The §Cost model was 2.8× understated for five revisions, and the budget line was 20× wrong for seven.** *(v1.8.2 — measured 2026-08-04.)* The prefix measures **4,489 tokens** (17,756 bytes, 3.955 bytes/token) on Anthropic's tokenizer. Every uncached figure in §Cost model from v1.3 through v1.8.1 was computed against **1,200** tokens — the doctrine-less prefix item 16 established was never deployed — so the Haiku row read **$0.00180/order and $1.30/four-ship-hour** where the deployed prefix costs **$0.00509 and $3.66**. The direction is the damaging one: the table understated cost, and it understated it in the row the default configuration uses.

    **What makes this item worth writing rather than a silent table update** is that the error survived a correction *aimed directly at it*. Item 16 established in v1.7.1 that the 4,738-byte measurement was doctrine-less and that the real prefix was 17,756 bytes — and §Cost model, the one section whose entire output is arithmetic over that number, was not recomputed. A fact was corrected in one section and left standing in the section that consumed it. §Corrections' own v1.3 note already warns that *"a number with no units-and-source trace behind it reads exactly like one that has been measured"*; the sharper version is that **a number with a correct trace to a superseded source reads exactly the same way.** The remedy applied here is the status column now in §Cost model's assumptions table: each input names whether it is measured or assumed, so a superseded input is visible without re-deriving the chain.

    **Two inputs remain assumed and are now the largest uncertainty in the model:** the ~200-token volatile suffix and the ~80-token output, both carried since v1.2 and neither ever measured. They are ~6 % of the cached per-order cost, so they cannot move the ≤ $1.10 verdict — but they are exactly the shape of the thing this item is about, and both are readable from `usage` on the first live inference request.

    **And the budget line said $100 when the balance is $5.** Unnoticed because "≈ 77 four-ship hours" is comfortable at either number and nobody checks a figure that is not binding. It is binding now: at the measured cached rate the balance funds **6.6 four-ship-hours**, and the Phase 2 gate at ~240 orders costs ~$0.25 of it.

    **(v1.8.4 — and item 21's own headline number was itself measured against the wrong object. See item 22.)** The 4,489-token count above is a correct measurement of the *prompt prefix text* and an undercount of *what is actually cached and billed*, by 69 %. The `$0.76/four-ship-hour` figure this item introduced is superseded within one revision of being written. That is the third consecutive revision in which this section's headline number moved, and the pattern is now the finding: see item 22.

22. **The schema is transmitted TWICE per request, and OQ-8 measured only one of the two copies.** *(v1.8.4 — measured 2026-08-04 from `usage` on the first live inference request.)* The first real order returned:

    ```
    "usage": { "input_tokens": 199, "cache_creation_input_tokens": 7608,
               "cache_read_input_tokens": 0, "output_tokens": 122 }
    ```

    **The cached block is 7,608 tokens. OQ-8 measured the prefix at 4,489.** The 3,119-token gap is not error and not overhead — it is the **order schema, sent a second time**. `PromptRenderer` renders the schema into the prompt *as text* (8,951 of the prefix's 17,756 bytes), and the adapter *also* sends it as `output_config.format.schema`, where it costs a further **~3,125 tokens** measured directly by posting the projection alone. Both sit before the cache breakpoint, so both are cached, and 4,489 + 3,119 reconciles to the observed 7,608 to within seven tokens.

    **Roughly 71 % of the cached prefix is the order schema, in two different renderings of the same document.** Neither is redundant *given the current design* — the prose rendering is what a model reads for field semantics, and `output_config` is what structurally constrains the output — but structured outputs enforces the shape regardless of whether the prompt describes it, so the prose copy is now doing much less work than it did on the local backend, where `format` was the only constraint. Dropping it would remove ~2,263 tokens (30 % of the cached block) and still leave 5,345, comfortably over the 4,096 minimum. **That is a Phase 3 question and is deliberately not acted on here:** prefix optimization is named out of scope in §Rabbit holes, the prose rendering has never been measured *against* its absence, and this project's own §Corrections is mostly a record of what happens when a measured-good artifact is changed on the strength of a plausible story.

    **What this does and does not change about OQ-8.** It does not change the answer: 7,608 clears the 4,096 minimum by *more* than 4,489 did, so the prefix caches as written, and the margin is **3,512 tokens (86 %)** rather than the 393 tokens (9.6 %) item 21 reported. **The cache-cliff warning in item 21 is therefore overstated and is corrected here** — a page of doctrine can be deleted without consequence, and it would take removing ~86 % of the prefix to stop caching. AIC-BE-3's startup comparison remains the right guard; it is simply not the hair-trigger item 21 made it out to be.

    **What it does change is the cost, again, and in the direction that misses the target.** §Cost model is recomputed a second time in two revisions.

    **The methodological point, which is the reason this is an item and not a footnote.** OQ-8 asked "how many tokens is the prefix?" and was answered by measuring the prefix. The question that *pays the bill* is "how many input tokens does a request cost?", and those differ by everything the adapter adds after the renderer stops — here, an entire second copy of the schema. **`count_tokens` on a constructed string measures the string; only `usage` on a real request measures the request.** The probe was built to avoid an estimate and it did, precisely and against the right tokenizer — on the wrong object. This is the same failure as item 21's superseded input in a new costume: a number with an impeccable provenance chain, traced to something that is not quite the thing being paid for.

    **Confirmed over a sample:** the 240-order soak reported `cache_read_input_tokens = 7,608` on **240 of 240** requests — identical every time, and a 100 % hit rate. So the prefix is not merely cache-*eligible*, it demonstrably caches in practice, which is the half of OQ-8 a token count could never establish.

23. **`input_tokens` excludes cached tokens; it does not contain them.** *(v1.8.4 — measured, after the harness printed a negative price.)* Anthropic's `usage` object reports three disjoint input populations: `input_tokens` (uncached, billed 1×), `cache_read_input_tokens` (hits, billed 0.1×), and `cache_creation_input_tokens` (writes, billed 1.25×). The live harness's first cost function treated reads as a *subset* of `input_tokens` and subtracted them to avoid double-counting. In steady state that is `179.7 − 7,608`, and the soak duly reported **`cost per order $-0.00614`** and a four-ship-hour of **−$4.42**.

    **The bug was caught by reading the adapter rather than by the absurd output**, which is worth stating precisely because the absurd output would have caught it anyway — a negative price is not a subtle error. The instructive part is the counterfactual: had the two populations been *nearly* the same size instead of differing by 40×, the same wrong formula would have produced a plausible number and shipped. **Field names do not tell you whether a set of counters partitions or nests, and the only thing that does is a response with known contents.** The first live response had `input_tokens: 199` beside `cache_creation_input_tokens: 7608`, which settles it in one look — the information was already in hand before the wrong formula ran.

    Cache *creation* is still not captured by `ILlmClient` or by the harness, and §Cost model's cached rows exclude it. Quantified rather than waved away: one write of 7,608 tokens costs $0.0095 on Haiku and is amortized across every order in a run — **$0.00004/order over 240 orders**, under a tenth of a percent. It is recorded as a known omission with a size attached, not as a rounding decision.

24. **Tokenization is model-specific, and `claude.maxTokens` is sized for Haiku's verbosity.** *(v1.8.5 — measured 2026-08-04 over a 48-order Sonnet run.)* Two findings from the first non-Haiku model this project has run, both of which invalidate something the document asserted.

    **(a) The same prefix is not the same token count.** 17,756 bytes tokenizes to **7,608 tokens on `claude-haiku-4-5`** and **10,493 on `claude-sonnet-5`** — **+37.9 % for identical bytes**. §Cost model's non-Haiku rows were produced by scaling Haiku's measured counts by each model's published *rate*, holding the token count fixed, and that row is now measured at **$0.00784/order against the $0.00439 it predicted — +79 %**. The document flagged this as an untested assumption in v1.8.4 and named the price of removing it as "one request"; it was one request, and the assumption was false. **The remaining Opus row is the same estimate produced the same way and should be read as a floor.**

    **(b) `claude.maxTokens = 512` truncates Sonnet, and the acceptance rate that results is not a quality measurement.** Sonnet accepted **44/48 (91.7 %)**, below the ≥ 95 % gate, where Haiku accepts 100 %. The four rejections are **2 × `parse: body is not well-formed JSON`** and **2 × `envelope: content array carries no text block`** — and `reject.schema` and `reject.shape` are both **0.00 %**. That combination is a truncation signature, not a bad-order signature: Sonnet's mean output is **269.5 tokens** against a 512 ceiling chosen when the only measured model wrote ~105, so its longer responses are cut off mid-document and fail as malformed JSON rather than as wrong orders. **When Sonnet completes a response, the order is well-formed every time.**

    **The conclusion to resist is "Sonnet is worse."** What is measured is that a *configuration value* is Haiku-shaped. Raising `claude.maxTokens` for verbose models is the candidate fix and it is **not made here**: it is a config-surface change, it needs a PRD revision first, and the correct value is unknown because nobody has measured where Sonnet's output length actually distributes. Recorded so the 91.7 % is never cited as an order-quality finding, which is precisely what it looks like in a table.

25. **The truncation is confirmed exactly, and the output-length distribution is right-censored at the very point the open question is about.** *(v1.8.6 — derived from the per-order rows the v1.8.5 runs already wrote, via `--csv`. **No new request was made; this is re-analysis of measured data.**)* Item 24(b) argued truncation from the *reject-reason* codes. The per-order rows settle it structurally and then place a hard limit on what the same data can answer.

    **(a) The two populations separate perfectly at the cap.** All **4** rejections have `tokensOut` **exactly 512**, and **0 of the 44** accepted responses do. Not "mostly" — the cap partitions the run without a single exception. The reject-reason argument was circumstantial; this is the mechanism. **The largest *completed* response is 502 tokens — 10 tokens of headroom**, and 3 of the 44 completed responses land within 30 tokens of the ceiling. The cap is not occasionally brushed; the distribution's upper mass is pressed against it.

    **(b) Truncation is concentrated by situation, not spread across the run.** Of six fixtures, **two** account for every truncation:

    | Fixture | Truncated | `tokensOut`, ascending |
    |---|---|---|
    | `winchester, one track close` | **3/8** | 318, 444, 485, 495, 502, *512*, *512*, *512* |
    | `no tracks at all` | **1/8** | 168, 334, 359, 394, 443, 458, 468, *512* |
    | the other four fixtures | **0/8** each | none above 478; four of them none above 396 |

    Sonnet's verbosity is **situation-dependent**, so a single scalar ceiling is being asked to cover distributions that do not overlap. A cap safe for `shot away, supporting` (max 154) is nowhere near safe for `winchester, one track close`.

    **(c) The comparison that makes "Haiku-shaped" quantitative.** Over 120 Haiku orders across both C2 arms, the largest output is **127 tokens** against the same 512 ceiling — **4.0× headroom, 0 at cap**. Sonnet's largest *completed* output is 502 — **1.02× headroom**. That ratio, not the acceptance rate, is the measurement of the finding.

    **(d) And here is the limit — `269.5` is a *censored* mean and must be labelled as one.** The 4 truncated responses were counted at 512, but their true lengths are **≥ 512 and unknown**: the model was cut off, so what it *would* have written was never observed. So **269.5 is a lower bound on the true mean, not the mean**; the mean over completed responses only is **247.4**; and the true value lies above both. Neither figure is wrong, but neither answers "where does Sonnet's output length distribute" — because **the data is censored exactly where the question lives.** What is now known is the distribution *up to* 512 and that **8.3 %** of the mass lies above it.

    **Therefore the correct `claude.maxTokens` is still not derivable from this run, and the tempting way to derive it is the error item 24(a) just paid for.** Scaling Sonnet's ceiling by Haiku's 4.0× headroom ratio gives ~2,000 — which is precisely the "scale one model's measurement by a ratio and hold the shape fixed" reasoning that missed by 79 % one item ago. **The tail is measured by re-running with the ceiling raised, and by nothing else.** This item narrows the open question from *unmeasured* to *censored*; it does not close it, and no config change follows from it.

26. **There are three response-size ceilings in this system, in two different units, and none of them knows the other two exist.** *(v1.8.7 — read out of the tree while specifying the C7 config change. **No request, no network.**)* Items 24(b) and 25 treat `claude.maxTokens` as though it were the only thing bounding a response. It is not, and it is not even the *lowest*.

    | Ceiling | Where | Unit | Value | What happens at it |
    |---|---|---|---|---|
    | `claude.maxTokens` | `CommanderConfig.cpp`, → `max_tokens` | **tokens** | `512` default, validated **`>= 1` and nothing else** | The model is cut off mid-document. Surfaces as `parse` or `envelope`, never as a length error — item 24(b) |
    | `kMaxResponseBodyBytes` | `OrderValidatorStageA.h:15`, checked by A0 **before the parse** | **bytes** | **65,536** | The whole order is rejected `range`, loudly and correctly |
    | `kMaxRecordedBodyBytes` | `OrderRecorder.h:93`, applied to `rawBody` | **bytes** | **4,096** | The record is truncated **silently** — `sanitizeText` cuts at the limit and writes no marker, so a truncated body is indistinguishable from a short one |

    **(a) The lowest ceiling is the silent one, and it is the diagnostic.** 4,096 bytes is roughly 1,000–2,400 tokens at this project's own measured byte/token ratios (the 17,756-byte prefix is 7,608 Haiku tokens and 10,493 Sonnet tokens — 2.33 and 1.69 B/token). So a raise of the output ceiling to anywhere in the 2–5× range starts truncating `rawBody` — while Stage A's 64 KiB cap, at ~16,000–39,000 tokens, does not come into play at all. **The order of the ceilings is therefore backwards for the purpose:** the first thing a raise breaks is the record of exactly the long responses that motivated it.

    **(b) This is observed, not projected.** The single `rawBody` in this tree's order log (`logs/ai-commander/orders.jsonl`, from the Phase 1b local run) is **exactly 4,096 bytes**, cut mid-way through the Ollama `context` token array, on an `order.rejected` record. The cap has already truncated a diagnostic record in this project, on the one class of record — a rejection — that exists to be diagnosed, and nothing in the record says so.

    **(c) The consequence for C7 is a bound, and a bound is not a value.** `claude.maxTokens` is unbounded above today, and the anticipated next action on it is an operator raising it by hand in a `.cfg` file. A ceiling of **8,192** is derivable from the ceilings above without touching the censored distribution at all: 65,536 ÷ 8 bytes/token — generous by 3–5× against this project's own measurements — guarantees a response *at* the configured ceiling cannot be rejected by A0 for length, and 8,192 sits inside the ~16,000-token band a **non-streaming** request completes in, which matters because AIC-BE-2 has no streaming path and adding one is out of scope. The API's own limits (64K output on Haiku 4.5, 128K on Sonnet 5) are an order of magnitude above and are not the binding constraint. **None of this picks a default**, and §Corrections item 25's conclusion is unchanged: the value stays 512 and the tail stays unmeasured.

27. **The ceiling-raised re-run: the truncation thesis is confirmed, the tail is far shorter than the refused estimate, the control is uninformative, and C2 reopens.** *(v1.8.9 — 48 orders on `claude-sonnet-5`, the same six fixtures, `claude.maxTokens` raised to 8,192, under the v1.8.8 grant. Paired with the 512 run: same model, same fixtures, ceiling the only difference.)*

    **(a) Confirmed, directly rather than by inference.** Acceptance **48/48 (100.0 %)** against **44/48 (91.7 %)**, with `reject.schema` and `reject.shape` still 0.00 % and now **no rejections of any kind**. Item 24(b) argued from reject-reason codes that "when Sonnet finishes a response, the order is well-formed every time." Removing the cut-off point removed every failure. **The 91.7 % was a configuration artifact end to end, and nothing about Sonnet's order quality was ever in question.**

    **(b) The tail is short, and item 25's refusal was right by a factor of three.** Maximum output **673 tokens — 1.31× the old ceiling**, with **7 of 48 (14.6 %)** above 512. v1.8.6 declined to scale Sonnet's ceiling by Haiku's 4.0× headroom, which would have produced **~2,000**. The measured maximum is **673**. The refused estimate was **3× too high**, and the refusal is now vindicated by measurement rather than only by principle.

    **(c) The censored mean was nearly right — and there was no way to know that in advance.** 269.5 (the censored lower bound) against **271.9** measured: **+0.9 %**. Item 25 was correct that 269.5 was a lower bound and correct that the tail could not be sized from it. **The size of the understatement turns out to be under one percent**, which is reported here because the honest form of "you could not have known" includes the cases where the unknown turned out to be small. What the censoring cost was never the mean — it was **8.3 % of the orders**.

    **(d) The raise is nearly free, and truncation was pure waste.** Output tokens 12,935 → 13,052 (**+0.9 %**); cost $0.00784 → **$0.00788**/order, $5.65 → **$5.67**/four-ship-hour at list, **+0.5 %**. Because `max_tokens` bills on **actual** output, an unused ceiling costs nothing. Meanwhile at 512 the four truncated responses were **billed at 512 output tokens each and yielded no order at all**. **+0.5 % bought 91.7 % → 100 %.**

    **(e) The control could not do its job, and the reason is the more important number.** The design predicted the sub-512 population would be unchanged, because `max_tokens` is a ceiling **the model is not aware of**. Per-fixture mean output moved by **−71.1, −41.4, −20.8, +13.9, +4.5, −1.3** tokens between the two runs, and one fixture's maximum moved **478 → 193**. **A ceiling raise can only lengthen a response; both material shifts are shortenings; therefore they are run-to-run sampling variance and not a ceiling effect** — the control is not violated, it is *swamped*. **And that is the finding: if per-fixture mean output moves by up to 71 tokens between two identically-configured runs, then 673 is a sample maximum and not a bound, and any ceiling sized from a single 48-order run is sized against noise.**

    **(f) C2 reopens, from a run that was not about C2.** The latency-vs-output regression across every hosted run this project has:

    | Run | Model | Output range | **r²** | intercept | slope |
    |---|---|---|---|---|---|
    | C2 arm A, n=60 | Haiku 4.5 | ≤ 127 tok | **0.004** | 2,454 ms | 1.8 ms/tok |
    | C2 arm B, n=60 | Haiku 4.5 | ≤ 127 tok | **0.071** | 466 ms | 20.8 ms/tok |
    | Sonnet @ 512, n=48 | Sonnet 5 | 124–512 *(censored)* | **0.574** | 3,472 ms | 10.1 ms/tok |
    | Sonnet @ 8192, n=48 | Sonnet 5 | 124–673 | **0.734** | 2,788 ms | 13.8 ms/tok |

    v1.8.5 concluded from the first two rows that "latency is dominated by service-side variance, not by either quantity the product controls." **That conclusion was drawn from two arms whose predictor barely varied.** Haiku's output spans roughly 60–127 tokens; Sonnet's spans 124–673 — **five times the range**. **Restricted range mechanically suppresses r²**, so 0.004 and 0.071 may be an artifact of a flattened x-axis rather than evidence that generation time does not matter. Where the predictor has room, output length explains **57–73 %** of latency variance and generation is **57 %** of the mean round trip (2,788 ms fixed + 3,754 ms generating 271.9 tokens).

    **This does not establish that output length drives Haiku's latency**, and it must not be quoted as if it did. What it establishes is that **C2's answer is model-dependent, and that "INCONCLUSIVE" was a conclusion about a measurement whose x-axis had been squeezed flat.** Note also that the intercept moved **3,472 → 2,788 ms** across two runs of the same configuration — a change no ceiling raise can cause — so roughly ±700 ms of it is run-to-run noise at n=48 and **the intercept must not be quoted to three digits**.

    **→ This subsection is superseded in two places by §Corrections item 28** *(v1.8.10)*, which turns the suspicion above into a demonstration and then narrows what it licenses. **(i)** The Sonnet @ 512 row in the table above is wrong to include the four truncated orders: their `tokensOut` is the cap, not a measurement, and regressing on a censored predictor inflates the fit. The accepted-only row is **n=44, r² 0.499, slope 9.88** — read that one. **(ii)** The r² column leads with the wrong statistic throughout; the slope with its interval is the test. Item 28 restates all four runs that way.

    **(g) What follows for the config, and what does not.** **The default stays 512.** `claude.model` defaults to `claude-haiku-4-5` and 512 is 4.0× that model's measured worst case; nothing here touches it. What this run supplies is the number an operator switching to Sonnet has been missing: **measured max 673, mean 271.9, 14.6 % above 512, and per-fixture means that move by up to 71 tokens between runs.** **No Sonnet value is written into the config**, because choosing one is a policy question — how much headroom to allow over a noisy sample maximum — and this item's job was to supply the measurement, not to make that call. **C7's measurement half closes here.**

28. **The C2 re-analysis: the two Haiku arms never disagreed, range restriction is demonstrated rather than asserted, and one clause of v1.8.5's conclusion has to be struck.** *(v1.8.10 — re-analysis of the per-order rows from all four hosted runs, every one of them already on disk. **No new request, no network, no cost.**)* Item 27(f) reopened C2 on a *suspicion*: that Haiku's near-zero r² might be an artifact of a flattened x-axis rather than evidence about the system. That suspicion is now demonstrated. Demonstrating it also exposed a second error in the same paragraph — one this document made itself, in the sentence that read the two Haiku arms as evidence of anything.

    **(a) The slope with its interval is the test. r² is not, and item 27(f)'s table led with the wrong column.**

    | Run | Model | output range | SD | **slope, 95 % CI** | r² | resid SD |
    |---|---|---|---|---|---|---|
    | C2 arm A, n=60 | Haiku 4.5 | 76–126 | 15.6 | **1.81** [−5.63, +9.25] | 0.004 | 453 ms |
    | C2 arm B, n=60 | Haiku 4.5 | 80–127 | 14.4 | **20.81** [+1.49, +40.12] | 0.071 | 1,087 ms |
    | Sonnet @ 512, n=44 | Sonnet 5 | 122–502 | 134.6 | **9.88** [+6.89, +12.88] | 0.499 | 1,348 ms |
    | Sonnet @ 8192, n=48 | Sonnet 5 | 124–673 | 176.4 | **13.80** [+11.40, +16.21] | 0.734 | 1,482 ms |

    **r² answers "how much of *this sample's* scatter does the predictor explain", which depends on how far the predictor was allowed to move. The slope answers "how many milliseconds per token", which does not.** Range restriction attenuates the first and leaves the second unbiased — so a table that leads with r² is reporting a property of the sampling as though it were a property of the system.

    **One row moved, and the reason is instructive.** Item 27(f) reported Sonnet @ 512 at **n=48, r² 0.574, slope 10.1**; the row above is **n=44, r² 0.499, slope 9.88**. The difference is the four truncated responses. Their `tokensOut` is not a measurement of anything — it is the cap, recorded identically for four responses of unknown and differing true length (item 25(d)). Regressing latency on a *censored* predictor pins four points at x = 512 with the long latencies that produced them, which inflates r² and steepens the slope. **The four rows belong in the acceptance count and nowhere near the regression.** The corrected row is the accepted-only fit.

    **(b) The two Haiku arms never disagreed.** v1.8.5 called the 1.8-vs-20.8 split *"two arms of the same system disagreeing about it by 5×, and that is the tell."* **Their confidence intervals overlap** — arm A spans [−5.63, +9.25] and arm B spans [+1.49, +40.12]. Two estimates too imprecise to conflict were read as evidence of conflict. The 5× ratio is what dividing one noisy number by another noisy number does, and it was quoted as a diagnosis. **Two imprecise estimates that differ are not two estimates that disagree, and the check is whether the intervals overlap.**

    **(c) Range restriction is demonstrated in this data rather than argued from.** You cannot widen Haiku's output range after the fact, so the test runs the other way: **narrow Sonnet's to Haiku's width and see whether a known-real relationship collapses into a Haiku-shaped one.** Restricted to 124–200 tokens — n=29, SD **19.7** against Haiku's 15.0 — **Sonnet's r² falls 0.734 → 0.036 and its slope 13.80 → 4.20.** The relationship did not change; only the window did.

    The closed form agrees without needing the banding at all. For `r² = b²·SD²ₓ ÷ (b²·SD²ₓ + s²)`, Sonnet's own slope (13.8 ms/tok) and residual noise (1,482 ms) evaluated at Haiku's spread (SDₓ = 15.0) predict **r² = 0.019** — sitting between the measured **0.004** and **0.071**. **A real 13.8 ms/token effect, observed over Haiku's range, is predicted to look exactly as absent as it in fact looked.**

    **(d) And it licenses far less than it looks like, which is the half that matters.** It is tempting to conclude the effect was there all along and Haiku's arms simply could not see it. **They cannot support that either.** Arm A's interval **includes zero and excludes 13.8**; arm B's includes both. Haiku's data is *consistent with* a Sonnet-sized effect and *equally consistent with none* — which is the definition of uninformative, not the definition of hidden. **What is established is that the two arms could not have detected the effect, not that the effect is present.**

    **(e) So v1.8.5's verdict was right, its stated reason was wrong, and one clause over-claimed and is struck here.** The verdict — INCONCLUSIVE, take no action, build a better instrument — survives intact and was the correct call. The reason given for it was that the two arms disagreed, and they did not. And this sentence:

    > *"latency is dominated by service-side variance, **not by either quantity the product controls**"*

    **cannot be concluded from an experiment with no power to detect the effect it is denying.** The clause asserts a negative about the prefix and the output length from arms that could not have seen either. It is struck from §Phase 2 carried items and annotated in the v1.8.5 changelog entry, which is left otherwise intact because a changelog records what the document said at that revision. **What the arms establish is a noise floor. A noise floor is a fact about the instrument.**

    **(f) What C2 needs, now with the number that decides it.** The remedy item 27(f) named — a lower-variance estimator, not a larger sample — is unchanged, and this re-analysis supplies the arithmetic that rules out the cheaper substitute. Sonnet's residual SD is **1,482 ms**, so the **intercept's** standard error at n=48 is `1,482 · √(1/48 + 271.9²/Sxx)` = **396 ms**, a 95 % interval of **±776 ms** — which is precisely the size of the **3,472 → 2,788 ms** intercept drift item 27(f) observed between two *identically configured* runs. A two-arm doctrine comparison would therefore need an intercept gap above **≈ 1,100 ms** (`1.96 · 396 · √2`) to be detectable at n=48 per arm, and the prefix delta at issue is ~2,200 *cached* tokens, which will not cost anything like that. Buying detectability with sample size instead needs roughly **640 orders per arm** to bring the resolvable gap near 300 ms — **1,285 orders, about $10.13** at Sonnet's measured $0.00788/order, against a $5 budget already $1.32 spent. *(The 300 ms target is a stated assumption, not a measurement: nobody has measured what a ~2,200-cached-token prefix delta is worth in fixed cost. It is chosen as the smallest gap it would be worth resolving; the script prints the figure for any target.)*

    **Time-to-first-token is not a bigger sample; it is a different estimator.** It *removes* generation from the measured quantity rather than trying to average it away, and generation is where the 1,482 ms of scatter lives — 57 % of the mean round trip, by item 27(f)'s own decomposition. **That is why the instrument is the answer and a third pair of arms is not.**

    **(g) The data this item is computed from is now in the tree.** Every figure above comes from four per-order CSVs written by `--csv` during runs already paid for, and until this revision those files existed only in scratch. They are archived under `tests/live/data/` with the script that produces the table, so **every number in items 25, 27(f) and 28 is re-derivable without a network request** — which is the property a re-analysis has to have to be worth more than the transcript it was first written in.

29. **The TTFT instrument was built and it does not measure what item 28(f) said it would — and the reason refutes item 28(f)'s premise. C2 closes anyway, on a different number.** *(v1.8.12 — 96 orders on `claude-sonnet-5`, two doctrine arms of 48, under the fifth grant (v1.8.11). The instrument is `tests/live/measure-ttft.ps1` over bodies captured by `--mode ttft-dump`.)*

    **(a) The instrument sends the right request, and that was designed for rather than hoped for.** The request bodies are **captured at the transport boundary** — a fake `IHttpClient` installed through the adapter's existing test seam records what `ClaudeLlmClient` hands the transport, so what is measured is the adapter's own construction and not a hand-written approximation of it. The probe adds exactly one field, `"stream": true`, and **asserts the byte delta** rather than trusting it. This is the direct countermeasure to item 22, where a probe measured the right object's neighbour and produced a number with an impeccable provenance chain to the wrong thing. **96/96 orders completed, 95/96 read the cache** (10,493 tokens on arm A, 8,291 on arm B).

    **(b) Time-to-first-token is total latency minus a constant. It carries almost no independent information.**

    | | arm A, full doctrine | arm B, short doctrine |
    |---|---|---|
    | **corr(TTFT, total)** | **0.995** | **0.991** |
    | total − TTFT | 2,212 ms, SD **314** | 2,308 ms, SD **408** |
    | deltas per response | 5–7 (~49 tokens each) | 5–7 (~41 tokens each) |
    | `afterFirst` ~ tokensOut, slope | **−0.63** [−1.08, −0.18] | **−0.69** [−1.48, +0.09] |
    | `TTFT` ~ tokensOut, slope | **+10.94** [7.55, 14.32] | **+15.90** [11.95, 19.85] |

    Under structured outputs the service does not emit one delta per token — it emits about six per response. **The entire per-token slope sits in TTFT, and the segment after the first delta is flat**, so the first delta arrives *after* essentially all of the generation has already happened. What the streaming probe subtracts is not generation; it is a near-constant 2.2-second flush.

    **(c) So TTFT is not a lower-variance estimator, and item 28(f)'s reasoning for expecting one was wrong.** SD(TTFT) is **2,989 / 3,056 ms** against SD(total) **2,878 / 2,959 ms** — a ratio of **1.04 and 1.03**. It is very slightly *worse*.

    Item 28(f) argued: generation is 57 % of the mean round trip, therefore generation is where the ~1,480 ms of scatter lives, therefore removing it removes the scatter. **The middle step does not follow, and this run measures why: SD(`afterFirst`) is 314 and 408 ms.** Generation is a large share of the **mean** and a negligible share of the **variance**. **A share of the mean is not a share of the variance**, and item 28(f) — in a revision whose entire subject was reading one statistic as though it were another — treated them as the same quantity. **This document has now made that class of error three times** (item 25's r², item 28(a)'s r², and here), which is why it is recorded as a class rather than as an incident.

    **(d) The estimator that does help is the one nobody proposed, and it was already being recorded.** Time to **response headers** — before any token, and therefore genuinely free of generation:

    | | SD, arm A | SD, arm B | corr with total |
    |---|---|---|---|
    | `headers` | **2,203 ms** | **1,973 ms** | 0.63 / 0.78 |
    | `TTFT` | 2,989 ms | 3,056 ms | **0.995 / 0.991** |
    | `total` | 2,878 ms | 2,959 ms | — |

    **~25–33 % lower SD, and a correlation that shows it is a different measurement rather than a shifted copy.** It costs nothing — the probe timestamps it on the way past. **The lesson generalises past this run: the useful cut point was one event earlier than the one that had a name.**

    **(e) The doctrine comparison is null on every estimator, and on the best one the sign is backwards.** Arm A minus arm B, the prefix delta being **2,202 cached tokens**:

    | Estimator | A − B | 95 % interval | |
    |---|---|---|---|
    | `headers` | **−103 ms** | [−940, +733] | includes zero |
    | `TTFT` | +549 ms | [−660, +1,758] | includes zero |
    | `total` | +454 ms | [−714, +1,621] | includes zero |

    **This is the null result the fifth grant pre-accepted, and it is reported as an outcome rather than as a failed attempt.** The best estimator resolves anything above **837 ms** (against total's 1,168 ms — the 28 % improvement is real but did not change the verdict), and it puts the cost of 2,202 cached prefix tokens at **−103 ms**, with the *short*-doctrine arm marginally **slower**. That is the **second** time a prefix reduction has failed to move latency in the predicted direction: v1.8.5 cut ~1,617 cached tokens and moved p50 down 16 % while moving p95 **up** 54 %. Two independent runs, one of them with a purpose-built instrument, and no consistent prefix effect in either.

    **(f) And the number that actually closes C2 was never the one being argued about.** The ≤ 2.5 s p95 target is **not reachable by any prompt-side change**, and this run demonstrates it directly rather than by decomposition:

    > **Time to response headers — before one token has been generated — has a mean of 3,157 ms and a p95 of 6,917 ms on arm A, and 3,260 / 7,357 ms on arm B.**

    **The fixed term alone exceeds the total-latency target, by 26 % at the mean and by 176 % at p95.** No reduction in prompt size, output length, or schema can bring a 2.5 s p95 out of a path whose pre-generation phase is already 6.9 s at p95. **The target was set in v1.2 against no hosted measurement of any kind and it was never achievable on this path** — which is a different and more useful finding than "the p95 is 4,615 ms and we do not know why."

    **What follows, and what does not.** The remedy is not prompt-side: it is a different model, a different target, or accepting the miss — and **the 20 s cadence already accepts it**, absorbing p99 with room to spare, which is why no run in any phase has been degraded by this. **§Success metrics' latency row is left unchanged and still reads MISSED.** Moving a target to match a measurement is the one thing this document has refused since v1.3, and it is not going to start here. **C2 closes with a cause rather than a remedy.**

    **(g) Cost, including the overrun.** 109 requests ≈ **$0.84**, against **≈$0.50** estimated in the fifth grant — **+68 %**. Thirteen of those requests were instrument development, and they were not free: the first smoke run measured two samples pinned at ~30,500 ms that turned out to be .NET's `Expect100Continue` handshake, not the service. **That is a probe measuring its own client stack and reporting it as latency**, caught only because 30,498 and 30,501 ms are not what variance looks like. Running total **≈$2.16 of $5**.

30. **C3's quality arm, pre-registered: what n=120 can and cannot resolve, and the decision rule, both written down before the run.** *(v1.8.13 — no request has been made at the time of this entry. The fifth grant (v1.8.11) conditions this run on exactly that ordering.)*

    **Why this is an item and not a paragraph in the results.** Every prior arm in this project was designed, run, and *then* assessed for whether it could have detected anything — and twice the answer was no (the C2 doctrine arms, §Corrections item 28(d); the TTFT instrument, item 29(c)). **A power statement written after a null result is indistinguishable from an excuse.** This one is written first so that it cannot become one.

    **(a) The change under test.** `PromptRenderer::build` renders the order schema into the prefix as prose; `ClaudeLlmClient` *also* sends it structurally as `output_config.format.schema`. §Corrections item 22 measured the duplication at **~71 % of the cached prefix**, and v1.8.5 confirmed the cost half — cutting the prefix saved **12 %** per four-ship-hour. **What has never been measured is what the prose copy is worth.** The arm removes exactly that block and nothing else, with structured outputs left **on**, because the premise of the proposed change is that the structural copy does the constraining.

    **(b) What n=120 per arm can resolve, exactly.** The metric is acceptance rate, the same one §Success metrics gates at **≥ 95 %**. Exact (Clopper–Pearson) one-sided 95 % lower bounds:

    | Arm B result | Point estimate | True acceptance is at least | vs the ≥ 95 % gate |
    |---|---|---|---|
    | **120/120** | 100.00 % | **97.53 %** | **clears** |
    | **119/120** | 99.17 % | **96.11 %** | **clears** |
    | 118/120 | 98.33 % | 94.85 % | does not clear |
    | 117/120 | 97.50 % | 93.67 % | does not clear |

    Between the two arms, the resolvable difference is **≈ 2.5 percentage points**. **So this run can rule out a quality cost large enough to breach the gate, and it cannot distinguish 100 % from 99 %.** That is the honest limit and it is stated in advance: a 1-point true difference will read as noise here and **must not** be reported as evidence of no difference — which is precisely the error item 28(d) records.

    **(c) The decision rule, fixed now.**

    - **≥ 119/120 accepted**, with `reject.schema` and `reject.shape` both at 0.00 % → the true rate clears the gate with margin. **The prose-schema removal is safe to make on this evidence, and C3's cost saving is available.**
    - **≤ 118/120** → the removal is **not** made. The lower bound has fallen through the gate, and a 12 % saving does not buy a gate breach.
    - **Any rejection reading `schema` or `shape`** → the removal is not made **regardless of the count**, because that is the prose copy doing structural work the projection was assumed to cover, and one such rejection is a mechanism rather than a rate.

    **(d) What this run is not.** It is not a measurement of order *quality* beyond acceptance — a well-formed order that is tactically worse would pass Stage A and count as accepted. Acceptance is what the gate is written in and what the arms can compare; **a semantic quality difference would be invisible here, and this item says so now rather than after the number arrives.**

31. **C3's quality half: both arms accepted 120/120, the pre-registered rule fired, and the prose schema is removed from the shipped prefix.** *(v1.8.14 — 240 orders on `claude-haiku-4-5`, 120 per arm, under the fifth grant. The decision rule was fixed in item 30 and pushed **before** this run; nothing below chose a threshold after seeing a number.)*

    **(a) The result.**

    | | Arm A — as shipped | Arm B — prose schema removed |
    |---|---|---|
    | Prefix | 17,756 B | **8,750 B** (−9,006, −50.7 %) |
    | Accepted | **120/120 (100.0 %)** | **120/120 (100.0 %)** |
    | `reject.schema` / `reject.shape` | 0.00 % / 0.00 % | **0.00 % / 0.00 %** |
    | Cache hits | 119/120 @ 7,544.6 tok | 119/120 @ **5,075.4 tok** |
    | $/order · $/four-ship-hour | $0.001464 · $1.05 | **$0.001220 · $0.88** |

    **(b) The rule fires, and it fires on the branch that was written down first.** Item 30 fixed it: *≥ 119/120 with `reject.schema` and `reject.shape` both 0.00 % → the removal is safe to make.* Arm B returned **120/120**, which bounds its true acceptance at **≥ 97.53 %** (exact, one-sided 95 %) against the **≥ 95 %** gate. **The removal is made** — in `PromptRenderer::build`, and it is the first shipped-code change any Phase 3 diagnostic has produced.

    **(c) What this does not license, restated because item 30 said it in advance.** The arms **cannot** distinguish 100 % from 99 %; the resolvable difference is ~2.5 points. And **acceptance is not quality** — an order that is well-formed but tactically worse passes Stage A and counts here as accepted. **What is established is that removing the prose copy does not push the acceptance rate through the gate.** It is not established that the two prompts produce equally good orders, and no sentence in this document should be read as saying so.

    **(d) The saving is bigger than v1.8.5's, for a reason worth keeping.** 12 % there against **16.6 %** here, because that arm cut 6,780 bytes of *doctrine* and this one cuts **9,006 bytes of duplicated schema**. §Corrections item 22 predicted the post-removal cached block at ~5,345 tokens; measured **5,075**, **−5 %**. **That is the first prediction in this document's cost history to land inside 10 %**, and it landed because it was computed from a measured `usage` figure rather than scaled from a ratio.

    **(e) And the new constraint the change creates, which is the part a later editor will trip over.** The cached block falls from 7,608 to **5,075 tokens** against Haiku 4.5's **4,096** minimum. **The margin was 3,512 tokens (86 %); it is now 979 (24 %).** §Corrections item 22 concluded that item 21's cache-cliff warning was overstated — *"a page of doctrine can be deleted without consequence"*. **That is no longer true.** It holds up to roughly 980 tokens and fails after that, and falling out of cache costs **5.7×** what the prose removal just saved. **The cliff warning was overstated when it was written and it is now approximately correct**, which is what happens when the thing a margin protects gets spent.

    **(f) One byte, and why it is in this item.** The first cut of the code change also deleted the blank line that had separated the schema block from `DOCTRINE:`, making the shipped prefix **8,749** bytes against the **8,750** that arm B measured. A one-byte divergence between the artifact under test and the artifact in production — invisible, harmless, and exactly the shape of every other item in this section. **The newline was restored and the reason written into the source**, so the prefix that ships is byte-identical to the prefix that was measured.

    **(g) Cost.** 240 orders ≈ **$0.32** against ≈$0.40 estimated. Running total ≈ **$2.48 of $5**.

    **C3 CLOSES.** Both halves are now measured: the cost half in v1.8.5 and again here, the quality half here. The change it proposed is made.

32. **C1: the hosted backend ran inside the engine, against real scenario state, and it works — and the harness printed two numbers that must not be quoted.** *(v1.8.15 — `run-live-scenario.ps1 -Backend claude`, 600 s commander-on plus a 600 s commander-off control, on `claude-haiku-4-5` against `oppint_red_interceptor` in the shipped "Mariana Shield". Under the fifth grant (v1.8.11). **19 checks, 0 failed.**)*

    **This is the run that was held across four grants.** Every hosted figure in this document before it described **six synthetic fixtures** — the same field set as a real snapshot, populated with values invented for a test. This one draws them from a shipped scenario. **The distinction was never technical; it was whose decision it was.**

    **(a) It works, and the parts worth naming are the ones a fixture cannot exercise.**

    | | |
    |---|---|
    | Plugin load, namespace, AIC-ARCH-4 probe | pass |
    | Backend actually hosted | `backend=claude enabled=true` **asserted**, not assumed |
    | Entities commanded | **both** — RedSu35_01 and RedSu35_02 |
    | Postures observed | **3 distinct** — `defend`, `engage`, `ingress` |
    | `reject.schema` / `reject.shape` | **0 % / 0 %** |
    | Timeouts | **0**, including the first order of the run |
    | Plugin frame cost | p50 **0.0023 ms**, max **2.87 ms** over 12,001 frames — inside the 5 ms bound |
    | Stage-B fratricide rejections | 0 |
    | Release tree afterwards | **clean** — config removed, `oppint_red_interceptor.lua` restored to 19,032 bytes |

    **(b) C3's change is confirmed on the product, not just on fixtures.** A real in-engine request reported `cache_read_input_tokens: **5,118**` — the reduced prefix, caching in the engine, on real scenario state, one commit after it was measured on fixtures at 5,075. **The shipped artifact behaves as the arm that authorized it did.**

    **(c) The most valuable single observation is a rejection.** One order was rejected `clamp`: the model asked for `cruiseSpeedMps` **450** against `safety.maxSpeedMps` **400**, and Stage B refused it. **That is Goal 3 — "no invalid, hallucinated, or unsafe order ever reaches a `request*` verb" — demonstrated on the product rather than on a corpus.** The rejection carried its full `rawBody`, which is AIC-DET-1 doing what v1.7.5 added it for. **A clean run would have been weaker evidence than this one.**

    **(d) And now the two numbers the harness printed that are not what they look like.**

    **The acceptance rate had the wrong denominator.** It printed **"10 of 13 requested (76.9 %)"**. Thirteen were requested, ten accepted, one rejected — **two were still in flight when the engine stopped**, having been issued in the last seconds of a fixed-length run. They have no verdict because the run did not wait for one, and counting them against acceptance charges the backend for a decision it was never allowed to make. Resolved: **10 of 11 (90.9 %)**. **Neither figure is a rate at n=11**, and this document has said so twice already — C5 records that "11 waypoint orders cannot establish a rate." **76.9 % reads exactly like an order-quality result and is not one, which is the same shape as the 91.7 % configuration artifact in item 24(b).** The script now reports resolved orders and names the in-flight count.

    **The "p95" was the maximum.** It printed **"p95 order latency: 5552 ms"** over **ten** samples, where the 95th-percentile index lands on the last one. The ten were **2,816 / 2,826 / 2,927 / 2,963 / 3,061 / 3,137 / 3,194 / 3,217 / 3,665 / 5,552 ms** — a median of **3,137 ms** and one outlier that became the headline. **PRD v1.7.4's Finding 3 recorded precisely this — "the reported p95 is not a p95", 10,363 ms being the second-highest of five — and this script went on printing one for four revisions after that finding was written.** A recorded lesson that stayed in the changelog and never reached the instrument. The script now reports min/median/max with n, and refuses to print a percentile.

    **(e) What C1 establishes, stated narrowly.** The hosted path works end to end inside the engine on real scenario state: real component reads, Stage B against real entity state, the Lua tier consuming published orders, the safety bound holding, the cache reading, the frame budget intact. **What it does not supply is any rate or percentile** — ~13 orders in ten minutes cannot, and the ≥ 95 % and latency bars stay where they are, on the soak. **The honest sentence this run buys is: "the backend works," in place of "the backend works on fixtures."** That is all it was ever for, and it is not a small thing.

    **(f) Cost.** 13 orders ≈ **$0.02** against ≈$0.10 estimated. Running total ≈ **$2.50 of $5**.

    **C1 CLOSES.**

## Problem statement

> **When** an operator wants an entity in a running N8RO scenario to behave adaptively — to change posture, re-prioritize targets, or break off — **they must** hand-author that judgment ahead of time as a deterministic Lua ladder, **which means** every scenario's tactical repertoire is frozen at authoring time and every new situation requires a scripting change by someone who knows both the tactics and the Lua API.

The tree ships 6.5 GB of quantized language models (`data/ai/model/`), a doctrine corpus (`data/ai/context/`), an MCP stack, and a documented `n8ro-llm` module — and no working path from any of it to entity behavior in a running scenario. Meanwhile `oppint_red_interceptor.lua` demonstrates how good the deterministic layer already is: two-ship target deconfliction, launch-range discipline at 0.8 of kinematic reach, shoot-look-shoot with a flight-time-derived assessment window, 40° crank geometry, defensive pump inside 15 km, winchester egress. Its 455 lines encode real judgment. What they cannot encode is *situational* judgment — the script's posture ladder at lines 346–439 (`defend` → `rtb` → `ingress` → `crank` → `engage`) is a fixed priority cascade, and changing it means editing Lua.

The commander's job is to replace exactly that cascade — the posture selection — with model-issued intent, and to leave the flying logic alone.

### Prior art and lessons learned

- **`n8ro-llm` was designed for this space and does not solve it.** `docs/modules/n8ro-llm/` describes a `SimLuaLLMHost` with a 7-step RAG pipeline on message-bus topics `n8ro-llm/generate` and `n8ro-llm/generate/result`. There is no `n8ro-llm.dll`, no import library, and no headers under `include/`. Beyond being absent, its pipeline generates *Lua scripts* — an authoring aid, not a control path. What we learn: the useful seam is a message-bus request/response boundary, and this plugin should be structured so it can adopt that boundary later without redesign (see OQ-3).
- **The MCP stack exists and does not control entities.** *(v1.4 — verified; OQ-4 resolved.)* `bin/ai/run-full-stack.cmd` brings up `n8ro-data-bot.exe` → `n8ro-sim-bot.exe` → `bin/ai/n8ro-mcp.exe` over ZMQ IPC. The sim bot registers exactly two tools — `workbook_describe_api` and `workbook_eval` — and the data bot's ~37 tools are all database-authoring operations that never touch a running scenario. What we learn: the tree's existing AI surface is built for *authoring* (edit the scenario) and *interactive debugging* (eval Lua against a running sim), and nobody has yet built the third thing — a bounded, autonomous, per-entity control path. That is the gap this PRD fills, and it is a gap by design rather than by omission: `workbook_eval` deliberately gates mutation behind human approval, which is the right answer for a debugging tool and the wrong shape for a 20-second cadence.
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

*(v1.8.4 — the cost target is **MET on a live measurement**, and the latency target is **MISSED**. Both stated here because §Success metrics is where a reader looks for the verdict, and reporting only the first would be the more flattering half.)*

| Metric | Target | Measured (240 orders, `claude-haiku-4-5`, 2026-08-04) | Verdict |
|---|---|---|---|
| Cost per four-ship scenario-hour | ≤ $1.10 | **$1.05** ($0.001464/order, cached at a 240/240 hit rate) | **MET**, 4 % headroom |
| Order round-trip p95 | ≤ 2.5 s | **4,615 ms** (p50 2,602 · p99 7,099) | **MISSED** by 85 % |
| Order acceptance rate | ≥ 95 % | **100 %** (240/240) | **MET** |
| Parse/schema rejection rate | < 1 % | **0.00 %** | **MET** |

The cost figure supersedes v1.8.2's $0.76, which was computed against the prompt text alone and missed the second copy of the schema the adapter also sends (§Corrections item 22). **§Cost model's *model* of that cost was separately wrong by +39 %, outside its own ±20 % gate** — the product meets the target and the document's arithmetic about it did not, and those are recorded as two results rather than netted into one. The latency target was set in v1.2 against no hosted measurement; nothing was tuned toward it and the number is reported as it fell (§Carried out of Phase 2, C2).

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
| Routing through the MCP stack (`n8ro-sim-bot.exe`) | Out of scope | *(v1.4 — was Deferred pending OQ-4; OQ-4 resolved "No".)* Enumerated from the shipped binary: the sim bot exposes two tools, `workbook_describe_api` and `workbook_eval`, and no entity-control tool. Reaching entities through it would require the model to emit Lua source, which forfeits AIC-ORD-1's closed-schema guarantee, and its mutating path is gated on human approval by design. See §Alternatives Option 2. | N/A — verified absent, not deferred | 2026-08-01 |
| Fine-tuning or training any model | Out of scope | No training infrastructure ships; not the owner's brief. | N/A | 2026-07-31 |
| UI surface (`n8ro-ui` plugin) for order inspection | Deferred | Orders are observable via the order log and `aiCommander.getStats()`. A UI is presentation, not capability. | v1.1 | 2026-07-31 |
| Streaming / partial order consumption | Out of scope | An order is atomic; a half-parsed order is a rejected order. Streaming adds failure modes and buys nothing at an 80-token output. *(v1.8.11 — narrow carve-out, and it does not touch this row's subject.* **The product path does not stream and this row still governs `ILlmClient` and the adapter.** *What is admitted is a streaming read inside `tests/live/` as a **measurement instrument**: time-to-first-token is the estimator §Corrections item 28(f) argues for, and it cannot be observed any other way — `n8ro::core::IHttpClient::send()` is blocking and returns a whole body, with no chunk or headers-received hook, so the product seam could not stream even if this row said it should. A probe that measures the fixed term is not a consumer of partial orders.)* | N/A | 2026-07-31 |
| GPU provisioning or inference-server packaging | Out of scope | No inference server ships in this tree. Installing and running one on target machines is a deployment responsibility, recorded as a dependency and OQ-2. | N/A | 2026-07-31 |
| Plugin-side (C++) reading of sensor tracks or weapon loadout | Out of scope | No public read seam exists: no track component in the schema or `ComponentTypeNames.h`, no `IEntityManager` accessor, and `ComponentFieldAccess` has no reader for the `list`-typed loadout (§Corrections, item 4). Tier 1 reports both instead (AIC-API-1). Revisit only if the SDK later exposes a track surface — the ingress verbs would then become an optional override rather than the only path. | N/A until the SDK exposes one | 2026-08-01 |
| Plugin-side synthesis of a track list from the entity roster | Out of scope | Technically available via `IEntityManager::getAllEntities()`, and deliberately rejected: a roster is not a sensor picture. Substituting one would silently grant the model vision through terrain and beyond sensor range, and Stage-B B3 would then validate hallucinated targets against that fiction rather than catching them. | N/A — rejected, not deferred | 2026-08-01 |
| Dynamic race detection (ThreadSanitizer) over the sim-thread/worker boundary | Out of scope | No TSan runtime exists on the target platform: VS 2026 Insiders ships `tsan_interface.h` headers with no `clang_rt.tsan` library, and neither MSVC nor the bundled LLVM toolchain supports TSan on Windows (§Corrections, item 8). Substituted, not skipped — ASan over the full suite, a 20,000-publish exchange-slot stress test with torn-read detection, and a `static_assert`-enforced value-only worker capture (§Validation and test plan). The residual gap is recorded as a risk row rather than closed. | N/A until a race detector exists for this toolchain | 2026-08-01 |
| Free-text track attributes (`team`, `kind`, `domain`) in the prompt | Deferred | The v1.2 ingress verbs carry scalars only. Adding attributes means widening `reportTrack`, which is a PRD revision, and each added string is a new injection surface to charset-filter. Revisit if order quality shows the model cannot discriminate targets without them. | v1.1 | 2026-08-01 |

## Key hypotheses

- **H1: Posture-level intent at a 15–30 s cadence produces observably better behavior than a fixed posture cascade,** because the reference script's own posture transitions occur on tens-of-seconds timescales (assessment windows are clamped to 10–30 s; the defensive pump triggers at 15 km closure). *Signal:* count of posture transitions per engagement that a domain reviewer marks "appropriate given the situation", commander-on vs commander-off. *Validated by:* paired scenario runs on the same seed. *If wrong:* the cadence is too slow for any useful decision and the commander's value is limited to pre-engagement setup — in which case the honest response is to narrow the scope to mission-start intent rather than to speed up the loop.
- **H2: A byte-stable prompt prefix reduces p95 local latency by ≥ 30 %,** because both llama.cpp and Ollama cache the processed KV of an unchanged prefix and skip re-evaluating it. *Signal:* p95 latency with a byte-stable prefix vs. a prefix whose whitespace is perturbed each request. *Validated by:* A/B measurement over 100 orders on one scenario. *If wrong:* prefix discipline still costs nothing, but the cadence targets must be re-derived from raw prompt-eval throughput. — **MEASURED 2026-08-02, NOT SUPPORTED.** 100 orders each way through the shipping adapter against Ollama 0.32.5 / `qwen2.5:7b-instruct-q8_0` on the RTX 4070 Ti SUPER: stable p95 **2,291 ms**, perturbed p95 **2,362 ms**. The perturbed prefix is **3.1 %** slower, not 30 %. On this hardware prompt evaluation is a small share of the round trip — a GPU eats a 4,400-token prefix in a fraction of the time it spends generating 80 tokens — so the cache has little to give back. The "if wrong" clause governs: prefix discipline is kept because it costs nothing and because the **hosted** backend's cache discount is a separate and much larger prize, but the local cadence targets rest on raw throughput rather than on this hypothesis. The measurement includes Ollama's own prompt cache, deliberately: it measures the deployed system, not the model's KV reuse in isolation.
- **H3: Constrained decoding (Ollama's `format` locally, `json_schema` structured output on Claude) drives the parse/schema rejection rate below 1 %,** versus double-digit rates from free-form prompting of a 3B model. *Signal:* the `reject.schema` counter over a 200-order soak, constrained vs unconstrained. *Validated by:* the same soak run twice. *If wrong:* the retry budget and the "retain last valid order" fallback carry more weight than planned, and the effective order cadence degrades by the retry factor. *(v1.6 — "GBNF grammar locally" was written before OQ-1 resolved to Ollama, whose `format` takes the schema object directly. v1.7 — H3 concerns `reject.schema` only; whether the same mechanism reaches `reject.shape` is a separate question and is answered in §Corrections item 13: it does, but only once the schema is shaped so it can be.)*

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

`waypoint` is required WHEN `posture ∈ {ingress, hold, rtb}` and SHALL be **absent** otherwise — for `engage`, `crank`, and `defend`, Tier 1 computes the geometry itself, and an order carrying a field nothing reads is an order whose author and reader disagree about what was commanded.

**How the conditional rules are encoded** *(v1.7, corrected v1.7.1 — see §Corrections items 13 and 15)*. The embedded schema document SHALL express the table above as **`oneOf` over posture-discriminated branches, one per distinct field-constraint profile**. Every posture in a branch agrees with that branch on every rule, so each branch states its rules **unconditionally** — which is the property a constrained decoder can hold and `if`/`then`/`else` is not:

| Branch | `posture` enum | `waypoint` | `targetEntityId` | `orbitRadiusM` |
|---|---|---|---|---|
| transit | `ingress` \| `rtb` | **required** | bounded to **exactly 0 characters** | bounded to **exactly `0`** |
| hold | `hold` | **required** | bounded to exactly 0 characters | bounded to **`[1, 50000]`** |
| targeted | `engage` \| `crank` | **not a property of this branch at all** | `minLength: 1` | bounded to exactly `0` |
| defend | `defend` | not a property of this branch at all | bounded to exactly 0 characters | bounded to exactly `0` |

Every branch carries `additionalProperties: false`, so a branch's omission of `waypoint` is a prohibition rather than a silence, and every branch requires `targetEntityId` and `orbitRadiusM` so a decoder is compelled to emit both. The union of the four `posture` enums is exactly the six-value vocabulary, each appearing once — a posture in two branches would make the `oneOf` ambiguous and hand the model a choice of which rules apply to it. This is an encoding decision, not a contract change: every field, type, range, and conditional value in the table above is unchanged, and `targetEntityId` / `orbitRadiusM` remain *conditional in value* while becoming *unconditional in presence* — which is what the table already said by giving each of them a specified value in both branches ("empty otherwise", "`0` otherwise").

The reason it is stated here rather than left to the implementation is that it is load-bearing for the Phase 1b acceptance gate. A flat schema with an optional `targetEntityId` measured 10/12 `shape` rejections against a model that simply omits what it is not compelled to emit; this shape measured **0 rejections in 24 live orders through the shipping adapter**. **Stage-A check A6 is unchanged and remains the enforcement** — the decoder is a second line, not a substitute, and an adapter whose backend silently stops honouring the schema must still be caught by the validator.

The branch count is a consequence, not a choice: the A6 rules induce exactly four constraint profiles across six postures, and any coarser grouping puts two postures with different rules in one branch, which that branch then cannot state (§Corrections item 15). The cost is that the shared field descriptions repeat per branch, which grew the rendered prefix from 4,738 bytes to **14,074**. That is recorded rather than optimized away, and it is material to OQ-8: at roughly 3,500 tokens the prefix is now near Haiku 4.5's 4,096-token cache minimum instead of well under it. *(v1.8.2 — measured, and the estimate in the previous sentence was low. The full deployed prefix is **4,489 tokens**, which is **over** the minimum rather than near it, so the per-branch repetition this paragraph records as a cost is also what pays for the cache discount. It was not designed for that and the coincidence is not a justification — but the branch count is now load-bearing for cost as well as for correctness, and §Corrections item 21 records that the margin is only 393 tokens.)*

**Fields the model is structurally forbidden to emit.** The schema has no property for heading, pitch, roll, velocity components, acceleration, turn rate, load factor, hardpoint selection, or fire commands. A response carrying any additional property is rejected under `additionalProperties: false` (AIC-VAL-2). This is what "never produces raw kinematics" means concretely.

**Acceptance criteria:**
- A JSON Schema document matching this table is embedded in the plugin and is the single source used for (a) Stage-A validation, (b) the Claude `output_config.format.schema`, and (c) the local adapter's `format` parameter. One definition, three consumers. *(v1.7 — (c) was "generating the local GBNF grammar"; OQ-1 resolved the local backend to Ollama's `format`, which takes the schema object directly.)*
- Every unit in the table is traceable to a `unit` key in `schema-reference.json`, verified by a test that re-reads the file.
- A response with an unknown top-level property is rejected.
- *(v1.7, corrected v1.7.1)* The embedded document is `oneOf` over the branches above, and a test asserts, **for every posture in every branch**, that the branch's waypoint, `targetEntityId`, and `orbitRadiusM` bounds agree with the Stage-A A6 predicates for that posture — not merely that some branch exists. A single flat object with optional conditional fields does not satisfy this criterion, because it does not constrain what the model emits (§Corrections item 13); nor does a branch whose postures disagree with each other (§Corrections item 15).

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
| B8 | *(v1.7.3)* WHEN `posture` is `engage` or `crank`, the Tier-1-reported loadout for this order's snapshot window is not **entirely dry**. A loadout is entirely dry when at least one hardpoint was reported AND every reported hardpoint has `ammoCount == 0` | `loadout` |

**B8, and why it is a new reason rather than a widened B3** *(v1.7.3)*. The Phase 1b gate recorded one standing order-quality miss: a **winchester** aircraft with a close contact draws `engage` where doctrine says `rtb`. Two focused doctrine iterations moved it not at all, which §Rabbit holes names as the timebox signal rather than a reason to keep writing. It is a quality miss and not a safety one — Tier 1 will not fire on an empty rail whatever the posture — but `oppint_red_interceptor` genuinely goes winchester and has a winchester egress path, so a ten-minute demo run shows an aircraft pursuing a contact it cannot shoot. Safe, and it reads as broken.

The resolution is to make the validator catch it, which converts an unmeasurable quality complaint into a counted rejection with a runbook row behind it.

It is a **new** reason rather than a widened `track` because the two carry different diagnoses and therefore need different responses: `reject.track` climbing means *the model is hallucinating target ids, or Tier 1 has stopped reporting the picture*; `reject.loadout` climbing means *the model is ordering shots the aircraft cannot take*. Folding the second into the first would destroy exactly the information this reason set exists to carry.

**"Entirely dry" is defined negatively on purpose.** An **empty** reported loadout — no hardpoints reported at all — SHALL NOT trigger B8. That case means *this Tier-1 script does not report stores*, which §Validation requires to keep receiving orders, and it is already handled: a script that reports nothing also reports no tracks, so B3 rejects every targeted order for it. Rejecting on absence would break a documented path and buy nothing. Absence of information is not evidence of a dry rail, and the validator SHALL NOT invent it.

B8 covers `engage` and `crank` only. `defend` is a reaction to an inbound munition rather than a shot, and `ingress` / `hold` / `rtb` carry no target.

**Acceptance criteria:**
- Every rejection increments a named counter and writes one `order.rejected` record carrying the reason and the raw body, truncated to 4 KB.
- *(v1.7.3)* B8 rejects `engage` and `crank` when every reported hardpoint is dry, and SHALL NOT reject when no hardpoint was reported at all — asserted as two separate tests, because the second is the one a naive implementation gets wrong.
- *(v1.7.3)* B8 runs **after** B3 and B4, so a hallucinated or friendly target on a dry aircraft is recorded as `track` or `fratricide` rather than `loadout`. The more specific diagnosis wins the record.
- *(v1.7.3)* The loadout B8 validates is the one that **accompanied the order's snapshot**, not the one current at publication time — the same rule B3 already applies to tracks, and for the same reason: an aircraft that fired its last missile while inference was in flight must not retroactively invalidate an order that was correct when it was requested.
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
| `commander.requestTimeoutS` | Int | `90` | Written to `HttpRequest::timeoutS` (SDK default is 15). *(v1.6 — raised from 30.)* Sized for a **cold model load**, not for steady state: a warm 7B answers in ~1.7 s, but the first request after Ollama evicts the model took **22–46 s** in the spike, and 46 s > 30 s means the first order of every run timed out as previously configured. See §Corrections item 10 |
| `commander.maxOrderAgeS` | Real | `45.0` | Stage-B staleness bound |
| `commander.orderValidityS` | Real | `120.0` | Fallback ladder step 1 |
| `commander.releaseAfterS` | Real | `300.0` | Fallback ladder step 3 |
| `commander.maxTracksInPrompt` | Int | `8` | Bounds the volatile suffix |
| `prompt.doctrinePath` | Text | `data/doctrine.txt` | Repository-relative path to the doctrine block of the stable prefix (AIC-BE-3). A file, not a `Text` field: it is 1–2 pages, edited by whoever tunes tactics rather than whoever rebuilds the DLL, and its token count is what OQ-8 turns on. Read once at `initialize()`; a change mid-run does not take effect, preserving prefix byte-stability |
| `local.baseUrl` | Text | `http://localhost:11434` | Matches `bin/ai/.env` `OLLAMA_BASE_URL` |
| `local.model` | Text | `qwen2.5:7b-instruct-q8_0` | *(v1.6 — was `llama-3.2-3b-instruct-q4_k_m`.)* An **Ollama tag**, not a GGUF filename; the old default named a file and would have failed on first request (§Corrections item 11). The 7B is the default now that OQ-2 resolved to a GPU — it measured ~1.7 s warm against the 8B's ~4.5 s, and the CPU-era reasoning that made the 3B "the latency-viable default" no longer applies |
| `local.temperature` | Real | `0.0` | Greedy decoding; lowest run-to-run variance |
| `local.grammarEnabled` | Bool | `true` | *(v1.6 — meaning pinned by OQ-1.)* Sends the embedded AIC-ORD-1 schema as Ollama's `format` parameter. Not GBNF — Ollama's JSON-Schema enforcement is the equivalent guarantee and is what the spike measured at 3/3. Set false only to reproduce an unconstrained-decoding baseline for H3 |
| `claude.enabled` | Bool | `false` | **Independent authorization gate.** Data egress |
| `claude.baseUrl` | Text | `https://api.anthropic.com` | Must be `https://` |
| `claude.model` | Text | `claude-haiku-4-5` | |
| `claude.maxTokens` | Int | `512` | **Model-shaped, like `claude.effort` below it.** Range `1 … 8192`. The default is sized for `claude.model`'s default: over 120 orders Haiku 4.5 peaks at **127 tokens — 4.0× headroom**. It is **not** safe for a more verbose model: Sonnet 5 truncated **4 of 48** at this value, and the truncation surfaces as `parse`/`envelope`, never as a length error (§Corrections items 24(b), 25). *(v1.8.9, measured — **Sonnet 5 at an 8,192 ceiling: max 673 tokens, mean 271.9, 14.6 % above 512, and 48/48 accepted.** 673 is a *sample* maximum, not a bound: per-fixture means move by up to 71 tokens between identically-configured runs, §Corrections item 27(e). Deciding how much headroom to allow over that is a policy call and is left to whoever switches the model.)* The upper bound is derived, not chosen: 8,192 keeps a full-length response under Stage A's 64 KiB `kMaxResponseBodyBytes` and inside the non-streaming completion band (§Corrections item 26) |
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

**How configuration reaches the plugin** *(v1.7.2)*. Two sources, with a defined precedence.

1. **`applyConfigFields()`** — the host-driven path. The UI host calls it from the plugin config editor. It is the authority: an explicit application always wins.
2. **`data/config/plugins/ai-commander.cfg`** — the **deployed default source**, read once at `initialize()`. The path is fixed and resolved against the host's working directory (the release root), matching how `prompt.doctrinePath` already resolves. It is deliberately **not** itself configurable: a config path that is configurable has no bootstrap.

The file is the flat `key=value` format the release tree already uses for per-plugin configuration (`data/config/plugins/n8ro-skyfeed.cfg` is the shipped exemplar), extended to tolerate `#` comment lines, blank lines, and surrounding whitespace — because the example config this repository ships is largely comments and has instructed operators to copy it to that directory since Phase 0. An empty value is an empty string, not an absent field.

Its contents are applied through the **same** `tryParseConfigFields` path `applyConfigFields()` uses, so all-or-nothing application, the `claude.enabled` gate, the `https://` check, and every range check hold identically whichever source the value arrived from. There is one commit path, not two.

**Why this exists** *(v1.7.2, §Corrections item 17)*. `n8ro-sim-local.exe` never calls `applyConfigFields()`. Before this revision that made `commander.enabled` unreachable on the headless host, which is the host every automated run uses — so §Validation's live-scenario smoke and the H1 paired runs were both unsatisfiable, and the Phase 1b gate recorded them as unmet. The change removes an asymmetry rather than creating a risk class: the UI host has applied this exact file since Phase 0, so a stale file could *already* enable the commander there.

**Fail-closed is unchanged.** No file means compiled defaults, and the compiled default for `commander.enabled` is `false`. The master switch still requires a positive act by an operator; v1.7.2 changes only which hosts can observe that act.

**Acceptance criteria:**
- `applyConfigFields` validates every field and returns `false` on any invalid value, leaving all prior values unchanged — partial application is not permitted.
- `claude.baseUrl` not matching `https://` is rejected.
- `commander.backend = "claude"` with `claude.enabled = false` is rejected with a logged reason.
- `getConfigFields()` never returns an API key value.
- *(v1.8.7)* `claude.maxTokens` outside `1 … 8192` is rejected, naming the field and the bound. The upper bound exists because the field is unbounded above today and the anticipated next act on it is an operator raising it by hand: past ~8,192 tokens a completed response can exceed Stage A's `kMaxResponseBodyBytes` and be rejected `range`, which would replace one silent length failure with a different one. **The bound is not a recommendation** — the default remains 512 and §Corrections item 25 stands: no other value is derivable from measurement yet.
- *(v1.7.2)* `initialize()` reads `data/config/plugins/ai-commander.cfg` when present and applies it through `tryParseConfigFields`, **before** the backend is constructed — so the startup log reports the backend the run will actually use rather than a compiled default that has already been superseded.
- *(v1.7.2)* **Fail-closed survives, asserted by test.** WITH no such file present, the plugin SHALL run on compiled defaults with `commander.enabled == false` and `commander.backend == stub`.
- *(v1.7.2)* A file carrying one invalid field applies **nothing**, retains every prior value, and logs the reason naming the field — the same all-or-nothing semantics as the host path, because it is the same code path.
- *(v1.7.2)* `initialize()` logs, at INFO, either the path it read and how many fields it applied, or that no file was found at that path. A missing file is INFO and not a warning: absent-and-disabled is the state §Operational readiness's deployment checklist *requires* of a default deploy, and warning on the correct state teaches operators to ignore warnings. This criterion exists because §Corrections item 16 is the record of what a silently-unresolved path costs — it hid for two phases.
- *(v1.7.2)* An explicit `applyConfigFields()` overrides the file **in either call order**. IF the host applies configuration before `initialize()`, THEN the file SHALL NOT be read, so a host-supplied configuration is never silently replaced by a deployed default.

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
- *(v1.6 — pinned, OQ-1 resolved to Ollama.)* The request is `POST {local.baseUrl}/api/generate` with a JSON body carrying `model` (`local.model`, an Ollama **tag**), `prompt` (prefix + suffix per AIC-BE-3), `stream: false`, `options.temperature` (`local.temperature`), and — WHEN `local.grammarEnabled` — `format` set to the embedded AIC-ORD-1 schema. The order document is returned as a JSON **string** in the envelope's `response` field, so Stage-A check A2 unwraps that field before A3 parses it.
- WHEN `local.grammarEnabled` is true, the adapter SHALL send the same schema object AIC-ORD-1 embeds — not a hand-copied variant. One definition, three consumers holds here or it holds nowhere. *(v1.8 — restated to admit one thing and still forbid the other. An adapter MAY send a **mechanical projection** of the canonical document, computed from it by a function in the same translation unit that defines it; an adapter SHALL NOT send a document authored separately. The distinction is the whole of the rule: a projection cannot drift from its source, because it is derived on every build; a copy drifts the first time someone edits one of the two. Any projection SHALL be accompanied by a test asserting that it accepts exactly the set of orders the canonical document accepts. This is what §Corrections item 19 requires of the hosted backend, and it changes nothing for the local one, which sends the canonical object unprojected.)*
- The adapter SHALL NOT treat `format` as a substitute for validation. *(v1.7 — restated.)* With AIC-ORD-1's `oneOf` encoding, `format` does now hold the conditional-presence rules (0/12 rejections measured, §Corrections item 13), which the flat schema did not — but the adapter SHALL still route every response through Stage A unchanged, and Stage-A A6 SHALL remain the enforcement. A backend that stops honouring `format`, or is swapped for one that never did, must fail as a `shape` rejection rather than as an accepted order.
- The timeout SHALL accommodate a cold model load, which is 22–46 s on the verification host against ~1.7 s warm (§Corrections item 10). A timeout sized for steady state fails the first order of every run. *(v1.7 — a VRAM-evicted but page-cached load re-measured at 3.9 s, §Corrections item 14; item 10's figure stands as the worst case and governs the timeout.)*
- *(v1.7)* The adapter SHALL NOT warm the model at construction — measured to relocate the cold cost rather than remove it (§Corrections item 14) — and SHALL NOT block `initialize()` on any network call. It SHALL, on its first request only and on the worker, distinguish an unreachable server from a `local.model` tag the server does not have, and report the latter as a configuration error naming the tag rather than as a generic transport failure (§Corrections item 11).

**Trace:** UAC-AIC-BE-1

#### AIC-BE-2: Claude adapter (Phase 2)
The system SHALL, WHEN `claude.enabled` is true, issue `POST {claude.baseUrl}/v1/messages` over raw HTTPS with headers `x-api-key`, `anthropic-version: 2023-06-01`, and `content-type: application/json`, requesting a schema-guaranteed order via structured outputs.

**Customer scenario:** The owner wants order quality and sub-2-second latency for a demo, and holds $100 in API credit.

**Pain removed:** There is no official Anthropic C++ SDK; without a specified raw-HTTP contract, every implementer invents their own request shape and discovers the constraints by trial.

**Acceptance criteria:**
- The order schema from AIC-ORD-1 is sent as `output_config: {"format": {"type": "json_schema", "schema": {…}}}`. *(v1.8 — the document sent is the **mechanical projection** AIC-BE-1 now admits, not the canonical object: the hosted path does not accept the bound keywords the canonical encoding uses. See §Corrections item 19.)*
- *(v1.8)* The projection SHALL be computed from `orderJsonSchema()` by a function beside it, SHALL contain no keyword outside the hosted path's supported set, and SHALL preserve every **pin** in the canonical document — a bound whose minimum equals its maximum — as an equivalent `const`. A test SHALL assert both properties, and SHALL assert that the adversarial corpus resolves identically against the projection and the canonical document. None of this requires a network.
- Assistant prefill is **not** used — it returns 400 on all current models.
- `stop_reason == "refusal"` is checked **before** reading `content`, and resolves to reject-reason `refusal`. *(v1.8 — the guard is on `stop_reason` and SHALL NOT be on `stop_details`, which may be null even on a refusal; branching on the latter would read `content` on exactly the responses that must not be read.)*
- *(v1.8)* The unwrap SHALL be performed by Stage-A check A2 under a third `EnvelopeFormat` value, passed into `validateStageA` **by value** exactly as `OllamaGenerate` is. Stage A SHALL NOT acquire a client, a backend identity, or any knowledge of which adapter produced the body. `refusal` is the only reject reason this envelope can produce that no other envelope can, and it is produced in the one place reject reasons are produced.
- `max_tokens` comes from `claude.maxTokens` (default 512). *(v1.8.7 — the default is **Haiku-shaped**, not model-independent: it is 4.0× Haiku 4.5's measured worst case and 1.02× Sonnet 5's, and at 512 Sonnet truncates 4 of 48. `max_tokens` is an **enforced ceiling the model is not aware of**, so truncation is structural rather than a quality failure, and it arrives as `parse`/`envelope`. Running a more verbose model requires raising this field and measuring first — see §Corrections items 24(b), 25, 26 and the `claude.maxTokens` row in AIC-API-2.)*
- WHEN `claude.model` is a Sonnet 5 model, the request sets `thinking: {"type": "disabled"}` or `output_config.effort = "low"` on the latency path; WHEN it is Haiku 4.5, no `effort` parameter is sent. *(v1.8 — for Haiku 4.5 this is a prohibition, not an omission: `effort` is rejected on that model, so a configured `claude.effort` SHALL be suppressed rather than forwarded.)*
- 429 and 5xx are retried at most once with backoff inside the same worker call; a second failure resolves to reject-reason `transport`.
- Input and output token counts from the response are written into the order record for cost accounting. *(v1.8 — `usage.cache_read_input_tokens` SHALL be recorded alongside them. Without it there is no way to distinguish a cache hit from a miss, and whether the prefix caches is the entire remaining question in OQ-8.)*
- *(v1.8)* The API key SHALL be read via `std::getenv(claude.apiKeyEnvVar)` **into a local at request time** and SHALL NOT be stored as a member of the adapter. This is structural rather than procedural: a member can be formatted into a diagnostic string by a later edit, and a local cannot outlive the call that read it.

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

*(v1.8.2 — that expectation is inverted by the measurement, and the criterion's purpose changes with it.)* The deployed prefix is **4,489 tokens**, over Haiku 4.5's minimum, so the warning is expected **not** to fire on any of the three models — and a run in which it *does* fire on Haiku now means the prefix has **shrunk below 4,096**, which costs 4.8× per order and produces no other symptom. The criterion has swapped roles: it was a standing reminder of a known-unmet condition, and it is now **the only detector of a silent regression**. Two consequences follow. (a) The token figure it compares against must be a real count and not a byte-ratio estimate — the ratio measured on this corpus is **3.955 bytes/token**, valid for this prefix's mix of prose and JSON schema and not a general constant, so it may be used to *predict* a crossing and never to *report* one. (b) The margin belongs in the log line, not just the verdict: *"4,489 tokens, 393 over the 4,096 minimum"* tells an operator editing doctrine how much room they have, where *"prefix clears the cache minimum"* tells them nothing until the day it stops being true.

*(v1.8.4 — the criterion is right and the number it was written around is not the one that matters.)* What actually caches is **7,608 tokens**, not 4,489: the adapter sends the order schema a second time in `output_config.format.schema`, and it is cached alongside the prompt (§Corrections item 22). Measured at a **100 % hit rate over 240 orders**. So the margin over the 4,096 minimum is **3,512 tokens (86 %)**, and v1.8.2's warning that deleting a page of doctrine would quadruple the bill is **withdrawn** — it would take removing the great majority of the prefix. Two things this changes for the criterion itself. (a) **What it compares must be what is cached, not what the renderer produced.** A check that measures `prefix()` alone would report 4,489 and be wrong about the quantity it exists to guard, by 69 %, in the safe direction — which is worse than being wrong loudly. (b) `usage.cache_read_input_tokens` on the second and later requests of a run is the *only* confirmation that the eligibility the startup check asserts is actually being realised; the startup check predicts, and that field observes. AIC-BE-2 already requires it be recorded, and this is the measurement that shows why.
- *(v1.8)* The **byte offset at which the prefix ends** SHALL cross to the worker alongside the rendered prompt, so a hosted adapter can place its cache breakpoint exactly there. It travels as a length on the request value — not as a second string — so the prompt remains the single record of what was sent, `promptHash` is unaffected, and the value cannot desynchronise from the text the way a parallel copy could. Adapters with no cache concept ignore it. **Rationale:** a breakpoint placed at the end of the whole prompt writes a distinct cache entry per request and reads none, which is the silent way to pay the write premium and receive nothing; the discount §Cost model computes requires the breakpoint at the prefix/suffix boundary and nowhere else.
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
{"t":412.50,"frame":24750,"event":"order.requested","entityId":"RedSu35_01","serial":13,"backend":"local","model":"qwen2.5:7b-instruct-q8_0","snapshotHash":"fnv1a64:9f2c…","promptHash":"fnv1a64:41ab…"}
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
| **Measured, warm** — qwen2.5 7B q8_0, RTX 4070 Ti SUPER *(v1.6)* | — | **~1.7 s** | — | No — 3 samples, not a distribution |
| **Measured, warm** — llama3.1 8B q4_K_M, same host *(v1.6)* | — | ~4.5 s | — | No — 3 samples |
| **Measured, COLD** — first request after model eviction *(v1.6)* | — | **22–46 s** | — | **Yes** — bounds `commander.requestTimeoutS` |
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
- **`max_tokens` at 512** — the output is a bounded order, and a small cap bounds the worst case. *(v1.8.7 — **this bullet was written as though the cap were model-independent, and it is not.** It bounds the worst case for Haiku 4.5, which uses 25 % of it. For Sonnet 5 the same cap does not bound the worst case; it **produces** one, converting 8.3 % of responses into `parse`/`envelope` rejections. The optimization is real and its correctness is per-model — §Corrections items 24(b), 25, 26.)*

## Cross-service impact

### Affected components

| Component | Impact | Changes required |
|---|---|---|
| `n8ro-sim` engine | None — no platform binary changes | None. The plugin loads from `%N8RO_RELEASE_USER_SIM_PLUGINS%` |
| Mission Lua scripts | New optional namespace available | A script opts in via `aiCommander.requestCommand`; scripts that don't call it are unaffected |
| Generated stubs (`data/resources/missions/stubs/`) | A new `aiCommander.lua` appears | Regenerated by running the engine once after deploy; the Lua language server is re-pointed at the folder |
| Inference server (external) | New runtime dependency | Must be installed and running on target machines. **Not shipped** |
| `n8ro-llm` | None | Deliberately not depended upon |
| MCP stack (`n8ro-mcp.exe`, `n8ro-sim-bot.exe`) | None | *(v1.4)* None ever, not "none in v1" — OQ-4 resolved: the sim bot exposes no entity-control tool, so there is no surface to integrate with or to conflict over. The two systems can run side by side; only this plugin is autonomous |

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

**Continuous integration is partial, by necessity — and the split is not where v1.1 assumed.** *(Restated v1.5 against the built implementation.)* v1.1 expected hosted runners to carry "order-schema validity plus accept/reject fixture round-trips (AIC-ORD-1, AIC-VAL-1)". They cannot. **Every test in the suite links `n8ro-core`**: the order schema and Stage-A validator are built on `n8ro::core::JsonValue`, and the harness is `n8ro::core::TestCase` / `TestRunner`. So zero of the 67 tests run on a hosted runner.

That is a consequence of two deliberate choices, both of which stand: `JsonValue` ships `validateAgainstSchema`, which is what makes AIC-ORD-1's "one definition, three consumers" literal rather than aspirational; and the SDK's test framework adds no third-party dependency to a repository that is meant to be buildable from a licensed install alone. The cost lands here, and it is worth paying.

The real split is therefore **not** "some FRs hosted, some self-hosted" but **anything that compiles goes self-hosted; anything that does not runs hosted**:

| Runner | Checks | Why it can run there |
|---|---|---|
| **Hosted** (`.github/workflows/ci-hosted.yml`) | PRD structural lint (`tools/lint-prd.ps1`); tracked-artifact guard (`tools/check-artifacts.ps1`); Lua syntax; commit FR-tagging; `clang-format` *(advisory — see below)* | Operates on text. Needs no SDK, no compiler, no release tree |
| **Self-hosted** (`.github/workflows/ci-selfhosted.yml`, labels `self-hosted, windows, n8ro-release`) | `Release \| x64` build; `dumpbin /exports` (AIC-API-1); the 67-test suite; the AddressSanitizer run (ADR-7); the 25-check deployed-artifact smoke | All require the SDK, VS 2026's v145 toolset, and a licensed release tree |

Two obligations follow. **Any build badge must state which runner produced it** — a green hosted badge means the PRD linted and nothing classified was committed; it does not mean the plugin compiles. And **the self-hosted runner's release tree is shared with interactive use**, so the workflow uninstalls the plugin it deployed on exit; leaving a PR's build in `userPlugins/sim` would have the next interactive scenario silently load it.

**The PRD lint earns its place by having caught something real.** OQ-4 sat `Open` past its "Phase 1a end" decision target across three revisions and was found only by a `/prd-review` pass after the milestone had already closed. "An unresolved question with no decision target" and "an FR with no UAC" are mechanical properties of the document, and a script checks them on every push. It cannot judge whether a rationale is *good* — only that one exists. Rating the argument stays the reviewer's job.

*(`clang-format` is advisory rather than gating: the config was added after the code was written and all 48 sources differ from it on comment-column alignment alone. Enforcing it today would mean an 8,400-line reformat commit with no behavioural content, landing on an open PR and destroying `git blame`. The intended path is a dedicated reformat commit added to `.git-blame-ignore-revs`, after which the job becomes a gate.)*

*(v1.3)* The concurrency-evidence set that replaces the unavailable TSAN gate (§Validation and test plan) lands on the **self-hosted** side for the same reason: the AddressSanitizer build is a `/fsanitize=address` build of the same solution and needs the SDK, and the exchange-slot stress test links the plugin. Only the `static_assert` capture check is toolchain-portable, and it is a compile-time property of code the hosted runner cannot compile anyway. There is no configuration in which a hosted runner substantiates the threading claim.

### Inference-server prerequisites

| Backend | Prerequisite | Ships in tree? |
|---|---|---|
| `stub` | None | — |
| `replay` | A previously recorded order log | — |
| `local` | **Ollama** on `local.baseUrl`, serving the tag named by `local.model`, with `format` support for the AIC-ORD-1 schema. *(v1.6 — OQ-1 resolved; the llama.cpp / GGUF-import comparison this row used to carry is retired. Nothing needs importing: the models are already tags. v1.7 — the `oneOf` schema of §Corrections item 13 is honoured by this server, measured 0/12.)* | **Yes on the verification host** (Ollama 0.32.5, `qwen2.5:7b-instruct-q8_0`); **no** on any other machine — nothing in this repository installs or manages a server |
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
| Orders rejected for `shape` | *(v1.7)* `aicmd.reject.shape` non-trivial | This is the signal that constrained decoding is **not** in force — with AIC-ORD-1's `oneOf` schema it measures near zero, so a climbing counter means `local.grammarEnabled` is false, the server is ignoring `format`, or the embedded schema was flattened. Inspect `rawBody`: an order missing `targetEntityId` or `orbitRadiusM` entirely is the flat-schema signature (§Corrections item 13). Do **not** respond by relaxing A6 | Implementer |
| Orders rejected for `track` | `aicmd.reject.track` dominant | **First check `aicmd.tracks.reported`** *(v1.2)* — if it is 0, the Tier-1 script is not calling `aiCommander.reportTrack` and the model is being asked to pick targets it was never shown; this is a script bug, not a model failure. If it is non-zero, the model is hallucinating ids; verify `commander.maxTracksInPrompt` is not truncating the intended target | Implementer |
| Orders rejected for `geofence` or `clamp` | *(v1.7.5)* `aicmd.reject.geofence` / `aicmd.reject.clamp` non-zero at low rate | **Expected on the local 7B, and the envelope doing its job.** Measured in-engine: the model occasionally emits a memorised real-world coordinate in place of a waypoint — one observed order held `−31.952876, 115.860450`, which is Perth, against an own-ship position near Guam — and independently, an out-of-envelope cruise speed. Every field is well-formed and in range, so neither the schema nor the decoder can catch it; Stage B is the only thing that does. **Read `rawBody` on the record** *(delivered as of v1.7.5)* to see the offending order. It becomes a finding only if the rate climbs far enough to starve an entity of orders — in which case report the `fallback.level` progression, which is the actual operational harm, rather than the rejection | Implementer |
| Orders rejected for `loadout` | *(v1.7.3)* `aicmd.reject.loadout` non-zero | The model is ordering `engage`/`crank` for an aircraft whose every reported hardpoint is dry. **This is the check working, not a fault** — it is the Phase 1b winchester miss being caught rather than shown. Expect it to appear late in an engagement, alongside `fallback.level` rising as the entity stops receiving offensive orders and Tier 1 flies its egress. It becomes a *finding* only if it dominates from the start of a run, which would mean Tier 1 is reporting `ammoCount = 0` for a loaded aircraft — check `reportLoadout`'s source rows before suspecting the model | Implementer |
| Commander refuses to enable at startup | `aicmd.probe.runtimeColumns` = `fail` *(v1.2)* | A `componentTransform` runtime column no longer resolves — the release tree changed under the plugin. Read the startup log for the failing path and reconcile against `include/n8ro-sim/entity/TransformRuntimeColumns.h`. Do **not** work around it by defaulting velocity to zero | Implementer, P1 |
| Frame budget exceeded | `aicmd.frame.p95Ms` alert | Reduce `commander.maxCommandedEntities` and `maxTracksInPrompt`; confirm no worker is touching SDK state | Implementer, P1 |
| Simulation slows with local backend on CPU | Frame rate drop with no plugin metric change | Expected — the inference server is competing for memory bandwidth. Move inference to GPU or a second host | Owner (capacity decision) |
| Unexpected hosted egress | `commander.egressWarning` in a run that should be local | Set `commander.enabled = false` immediately; audit `claude.enabled` and `commander.backend`; review the order log for what was transmitted | **Owner, immediately** |
| API budget nearing exhaustion | `aicmd.tokens.*` cumulative | Switch `claude.model` to Haiku 4.5 or `commander.backend` to `local` | Owner |

### Deployment checklist

- [ ] `dumpbin /exports` shows all three required exports
- [ ] `commander.enabled = false` and `claude.enabled = false` in the deployed default config
- [ ] *(v1.7.2)* `data/config/plugins/ai-commander.cfg` is either **absent** or carries `commander.enabled = false`. This checkbox became load-bearing in v1.7.2: that file now takes effect on the **headless** host as well as the interactive one, so a stale copy left from a demo enables the commander on every subsequent automated run. The startup log states which file was read and how many fields were applied — check that line rather than assuming
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
| Inference server — **Ollama 0.32.5** | **Installed and serving** on `localhost:11434`, 14 instruct models imported *(v1.6, verification host)* | Commander degrades through AIC-VAL-2 to Tier-1 behaviour | OQ-1 resolved. Still external and still not shipped in the tree: any *other* host needs its own install. Nothing in this repository installs or manages it |
| GPU — **RTX 4070 Ti SUPER, 16 GB** | **Present** *(v1.6, verification host)* | On a GPU-less host the CPU latency rows govern and the cadence must be re-derived | OQ-2 resolved for this host only. One machine is not a fleet inventory |
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
- Ships today, and is the closest thing in the tree to an existing AI integration path.

**Cons:** *(v1.4 — no longer speculative. The tool surface was enumerated from the shipped binary; OQ-4 is resolved.)*
- **It exposes no entity-control tool.** `n8ro-sim-bot.exe` registers exactly two `IToolHandler` implementations: `workbook_describe_api` (retrieve the Lua API catalog) and `workbook_eval` (execute a Lua snippet on the running sim, one-shot, at the next frame boundary). That is a Lua *evaluation bridge*, not a control surface.
- **The one path that does reach entities requires the model to emit Lua source.** `workbook_eval`'s own description offers "full access to the MissionRegistrar scripting API… query entity state, modify behavior, inject commands". Routing orders through it means the model emits code, and a JSON Schema cannot constrain a Lua string — so AIC-ORD-1's `additionalProperties: false` guarantee, and with it the structural claim that the model *cannot express raw kinematics*, would simply cease to exist.
- **It is human-in-the-loop by design.** Its `isMutating` parameter is documented as "Mutating calls are held for user approval before they run… approval is the safe default." An autonomous 20 s cadence cannot run through an approval gate, and removing the gate would remove the control the tool was built around.
- **It supplies none of the surrounding machinery.** No per-entity roster, no cadence, no staleness bound, no fratricide check, no fallback ladder, no order log. AIC-VAL-1, AIC-VAL-2 and AIC-DET-1/2 would have to be built regardless — on top of a process boundary and IPC failure modes, against a latency budget that is already tight.
- **It is optional in its own stack.** `run-full-stack.cmd` treats a missing `n8ro-sim-bot.exe` as a normal outcome and continues.

**Why not chosen:** *(v1.4 — resolved, not deferred.)* Not "unverified" any more: verified absent. The MCP stack's sim-domain surface is a workbook Lua eval bridge for interactive debugging and authoring, and `n8ro-data-bot`'s ~37 handlers are all database-authoring tools that never touch a running scenario. Alternative 2 does **not** supersede Alternative 1. The prediction that "the order schema, validator, and replay format survive either way" held — but for the opposite reason to the one anticipated: they survive because this was never a control path, not because the plugin collapses into a bridge over one.

**Worth recording separately:** `workbook_eval` is an LLM-facing arbitrary-Lua-execution channel into a running simulation, and it ships today. That is adjacent to this PRD's threat model even though it is out of its scope. Note that the sim bot reaches the same safety goal by a different route than this plugin does — it gates mutation behind *human approval*, where the commander gates it behind a *closed schema plus a two-stage validator*. Both are defensible; they are not interchangeable, and an operator running both should know only one of them is autonomous.

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
| **Unmet dependency.** No inference server ships | High — Phase 1b cannot run | *(v1.6)* No longer current on the verification host, where Ollama 0.32.5 is installed and serving; still certain on any other machine | Resolved for that host by OQ-1. Nothing in this repository installs or manages a server, so a second host repeats the gap in full. `stub` backend keeps Phases 0/1a deliverable without one |
| **Cold model load exceeds the request timeout.** *(v1.6)* A warm 7B answers in ~1.7 s; the first request after model eviction took 22–46 s | Medium — the first order of every run times out and the entity starts on the fallback ladder rather than under command. Presents as a transport failure, so it looks like a server problem rather than a configuration one | **Certain** with a steady-state-sized timeout; that is how it was found | `commander.requestTimeoutS` raised 30 → 90 s. A warm-up request at backend construction would remove it outright and is a Phase 1b design option. `keep_alive` sustains the model at a 20 s cadence, so this is first-order-only, not recurring |
| **Constrained decoding does not enforce the conditional rules.** *(v1.6; **largely retired v1.7**)* A flat schema left every conditional field optional, and `format` compels only what `required` names | Was: a `reject.shape` rate the ≥ 95 % acceptance gate cannot absorb — measured at **10/12** on the shipped prompt | **Retired as a live risk** on the local backend | **Resolved by encoding, not by prompt.** AIC-ORD-1's `oneOf` branches measured **0/12** with the prompt untouched (§Corrections item 13). What remains is a *regression* risk rather than a design one: a backend that stops honouring `format`, or a future flattening of the schema, returns the old rate. Held by Stage-A A6, which is unchanged and still rejects `shape`; by `reject.shape` being reported separately from `reject.schema`; and by the runbook row that reads a climbing `shape` counter as "constrained decoding is not in force" rather than as a prompt problem. The v1.6 mitigation's last clause — "if prompt work does not move it" — was tested: prompt work does not move it |
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
| OQ-1 | Which inference server on target machines — Ollama or llama.cpp server? | **Resolved 2026-08-02 — Ollama** | — | *(v1.6.)* Decided by what is installed and by a spike, not by preference. **Ollama 0.32.5** is installed and serving on `localhost:11434` with **14 instruct models already imported** (`qwen2.5:7b-instruct-q8_0`, `llama3.1:8b-instruct-q4_K_M`, and others). The v1.1 objection — that Ollama needs `ollama create` plus a Modelfile to import the shipped split 7B — is **moot**: nothing needs importing. The GBNF objection is also retired: Ollama's `format` parameter takes a JSON Schema and enforced AIC-ORD-1 **3/3 across two models** in the spike, which is the same guarantee GBNF was wanted for. Pins the `local` adapter to `POST {local.baseUrl}/api/generate` with `format` set to the embedded order schema (AIC-BE-1) |
| OQ-2 | Is a GPU present on target machines? | **Resolved 2026-08-02 — Yes** | — | *(v1.6.)* Inventory of the development/verification host: **NVIDIA RTX 4070 Ti SUPER, 16 GB VRAM**, driver 610.74. That comfortably holds either shipped model (3B ≈ 1.9 GB, 7B ≈ 4.4 GB) with room for a 14B. Measured warm round trips on this host: **~1.7 s (qwen2.5 7B q8_0)** and ~4.5 s (llama3.1 8B q4_K_M) — see §Performance. H1 is therefore testable at the 20 s cadence, and the "CPU inference competes with the simulation for memory bandwidth" consequence in §Resource constraints does not apply on this host. **Scope caveat:** one machine is not a fleet inventory. If the commander is ever run on a host without a GPU, the CPU rows in §Performance govern and this answer does not transfer |
| OQ-3 | Is `n8ro-llm` going to be installed later? | Open | v1.1 planning | If yes, the plugin should consume `n8ro-llm/generate` over the message bus instead of owning an HTTP client. Resolved by a roadmap answer from whoever owns that module. The `ILlmClient` seam is designed so this is an added adapter, not a redesign |
| OQ-4 | Is the existing MCP stack (`bin/ai/run-full-stack.cmd`, `bin/n8ro-sim-bot.exe`) the sanctioned AI integration path? | **Resolved 2026-08-01 — No** | — | *(v1.4.)* Its tool surface was enumerated from the shipped binary. `n8ro-sim-bot.exe` registers exactly **two** `IToolHandler` implementations — `WorkbookDescribeApiHandler` and `WorkbookEvalHandler`, exposed as `workbook_describe_api` and `workbook_eval` over topics `sim/workbook/eval` / `eval_response`. There is **no entity-control tool of any kind**. (`n8ro-data-bot.exe`, by contrast, registers ~37 handlers, all authoring-domain create/edit/list/get against the *database* — not the running simulation.) Entity control is reachable through `workbook_eval` only in the sense that arbitrary Lua is, which disqualifies it for this purpose on four counts recorded in §Alternatives Option 2. **Alternative 2 therefore does not supersede Alternative 1**, and the plugin does not become a thin bridge |
| OQ-5 | Is there an entitlement/licensing gate on AI-using plugins? | **Resolved 2026-08-02 — No** | — | *(v1.7.)* Four checks, each independently re-runnable. (a) **No plugin-facing surface:** `Entitlement` / `AccessGate` / `capabilityId` appear in exactly six SDK headers, all under `include/n8ro-core/core/entitlement/`, and none under `plugin/`; `PluginContext` carries `metadata`, `services`, and `threadRunner` and nothing else, so a plugin has no handle with which to consult a gate. (b) **Not in the linked import library:** `lib/n8ro-core.lib` exports no `AccessGate` or `Entitlement` symbol — the contingency this row proposed, "an entitlement check at `initialize()`", would not have linked. (c) **Not in any sim binary:** scanning every `bin/*.dll` and `bin/*.exe` for `LexActivator` / `Entitlement` / `Cryptlex` hits `LexActivator.dll` and **`N8RO.exe`** only; `n8ro-sim-local.exe`, `n8ro-sim-app.exe`, `n8ro-sim-starter.exe`, and `n8ro-sim-bot.exe` carry none of it. (d) **Observed:** the plugin loads, registers, probes, and runs on this licensed machine (smoke 25/25 at the Phase 1a gate, and again at Phase 1b start). Entitlement is a product-activation concern of the GUI shell, upstream of and invisible to plugin loading. **Consequence: no code.** *Caveat, the same one OQ-2 carries:* one licensed host; a machine whose shell will not activate is a different question, and that gate sits above the simulation entirely |
| OQ-6 | Which scenario and entity for the first demo? | **Resolved 2026-08-02 — `oppint_red_interceptor`, entity `RedSu35_01`** | — | *(v1.7.)* Owner pick, on the candidate this row already named: its Tier-1 logic is the quality bar and its posture ladder is where this PRD's posture enum came from, so H1 is measured against the strongest available baseline rather than a weak one. Concretely, the entity lives in seed scenario **"Mariana Shield"** (`data/resources/seed/realistic_scenario_seed_data.json`, entry 6) — two Red Su-35-class interceptors against two Blue F-16-class fighters and a Red SAM battery in the Mariana/Guam sector. The live smoke runs `n8ro-sim-local.exe --scenario "Mariana Shield AI" --run-ms …`, where the `AI` scenario is an **additive** duplicate of that entry with `RedSu35_01`'s `missionScriptPath` pointed at the reference Tier-1 script; no shipped record is modified, and `"Mariana Shield"` itself is the commander-off control for the H1 paired runs. `RedSu35_02` stays on stock Tier-1 logic in both runs, which gives a within-run control alongside the paired one |
| OQ-7 | Does the host supply a non-null `IThreadRunner` to sim plugins at `initialize()`? | **Resolved 2026-08-01** | — | **Yes — both are non-null.** Observed on a live `n8ro-sim-local` run: *"PluginContext.services is non-null, PluginContext.threadRunner is non-null."* The plugin therefore uses `IThreadRunner::submitBackgroundTask` as its primary dispatch. The owned-thread fallback stays implemented per AIC-ARCH-2 — the field is documented nullable, and one host observation does not license removing a specified fallback — but it is now the contingency rather than the expected path |
| OQ-8 | Should the doctrine prefix be padded to the configured model's cache minimum? | **Resolved 2026-08-04 — No. It caches as written** | — | Haiku 4.5's prompt-cache minimum is 4096 tokens. *(v1.3)* The prefix has been **measured**: 4,738 bytes ≈ **1,200 tokens** on a live engine run, logged as `prefixBytes` (§Corrections, item 9). It therefore silently does not cache on Haiku 4.5 as written. Three consequences, all evidence *into* the question and none of them answering it: the padding delta is **~2,900 tokens, not ~3,300**, so padding costs less than the v1.2 arithmetic implied; the uncached baseline the padding is judged against is **higher** than assumed ($0.00180/order, not $0.00140), so caching wins by more; and the uncached four-ship-hour figure now **exceeds** the §Success metrics ≤ $1.10 target while the cached one does not. **Deliberately left open.** What remains is not a measurement but a judgement the owner owns: whether ~2,900 tokens of *genuine* doctrine are worth writing. Padding with filler to earn a cache discount is a net loss in every dimension except the invoice, and this PRD will not pre-commit that call. Resolved at Phase 2 start by an owner decision on doctrine content, against the recomputed regimes in §Cost model. **(v1.7.1 — the question may have answered itself.)** Two Phase-1b findings move the prefix past the threshold this question was asked about. §Corrections item 16: the 4,738-byte measurement was taken with **no doctrine loaded at all**, because nothing deployed the file — so it was never the shipping prefix. §Corrections item 15: the four-branch schema repeats its shared field descriptions per branch. The deployed prefix now measures **17,756 bytes ≈ 4,500 tokens**, which is **above** Haiku 4.5's 4,096-token minimum rather than well below it. If that holds when the hosted backend is wired, the prefix caches **as written**, the padding delta is zero, and OQ-8 resolves without anyone writing filler. What keeps it open is that this has not been measured against Anthropic's tokenizer — a byte-to-token ratio derived from one local model is an estimate, and the cost model should not be rewritten on an estimate. **(v1.8 — the remaining measurement is itself a live hosted request, and this row was written as though it were free.)** Anthropic's tokenizer is reachable only through `POST /v1/messages/count_tokens`; there is no offline Claude tokenizer, and a general-purpose tokenizer from another vendor is a *different* tokenizer — using one would reproduce the exact error this row already refuses, in a new costume. So the measurement sits **behind the Phase 2 authorization gate** with every other live call, and the resolution plan is: count the deployed prefix against `claude.model`; compare to that model's cache minimum; recompute the four-ship-hour figure against §Success metrics' ≤ $1.10; and state plainly whether it is met. Confirmation that the prefix cached in practice comes from `usage.cache_read_input_tokens` on the second and later requests of a run, which AIC-BE-2 now requires be recorded — a prefix over the minimum that still reports zero cache reads means something else is invalidating it, and that is a different finding than this question asks about. **(v1.8.2 — MEASURED, and the answer is no padding is needed.)** `POST /v1/messages/count_tokens` against `claude-haiku-4-5`, 2026-08-04, on the 17,756-byte prefix dumped from the deployed `PromptRenderer`: **4,489 input tokens**, i.e. **3.955 bytes/token**. That **clears** the 4,096-token minimum by **393 tokens**, so the prefix caches as written, **the padding delta is zero, and the judgement this question reserved for the owner — whether ~2,900 tokens of genuine doctrine were worth writing — does not have to be made at all.** The applicable four-ship-hour figure is **$0.76 against the ≤ $1.10 target: met.** Two things this resolution does *not* claim. (a) It is a count, not an observation of caching: that the prefix is *eligible* to cache is measured, that it *does* cache is confirmed only by a non-zero `usage.cache_read_input_tokens` on a live inference run, which remains gated. (b) **The margin is 9.6 %** — about 1,554 bytes of doctrine — so this answer is a function of the current doctrine file and is invalidated by shrinking it. The question is closed; the *guard* on it (AIC-BE-3's startup comparison) is now load-bearing rather than ornamental, and §Corrections item 21 says why |
| OQ-9 | Can `readComponentFieldReal` address the transform's **runtime** columns at all, and if so does a bad path really return `0` rather than `std::nullopt`? | **Resolved 2026-08-01** | — | *(Added v1.2, resolved same day.)* **Yes it can, and bad paths are LOUD, not silent.** Observed on a live run against entity `NeutralDuplexHome_01`: `velocityNed.x` resolved, the schema's slash form `velocityNed/x` **also** resolved, and the deliberately misspelled `velocityNed.q` returned `std::nullopt` while emitting `DynamicLayout::handle: no field at path 'velocityNed.q'` at ERROR level. The silent-zero warning in `TransformRuntimeColumns.h` describes the raw handle-resolution path, not `readComponentFieldReal`, which validates and reports. **Consequence:** AIC-ARCH-4's probe is a plain resolve-check; the moving-entity heuristic is not required and a stationary entity is a valid subject |

### Rabbit holes

- **Doctrine prompt engineering.** `data/ai/context/` holds a substantial RAG corpus (HAVA/DENIZ/KARA doctrine, electronic warfare, cyber, space, maritime, land, irregular ops, plus N8RO manuals). It is tempting to wire retrieval in early. Don't: retrieval makes the prefix volatile, which destroys both the local KV cache and the hosted prompt cache, which is the single largest latency lever in the design. Contain: hand-write 1–2 pages for Phase 1, timebox to one day, and defer retrieval to v1.1 with its own caching design.
- **Ollama GGUF import.** Importing the split 7B (`qwen2.5-7b-instruct-q4_k_m-00001-of-00002-002.gguf` + `-00002-of-00002.gguf`) into Ollama needs `ollama create` with a Modelfile and may need the split rejoined first. Contain: timebox to half a day; if it resists, llama.cpp server loads the files directly and OQ-1 resolves itself.
- **Making replay bit-exact.** It is tempting to promise trajectory reproducibility. Physics reproducibility belongs to the engine's timing configuration, not this plugin. Contain: scope the guarantee to "identical published orders at identical simulation times" and state the trajectory caveat in the guarantee table — already done in AIC-DET-2.
- **Multi-entity coordination creeping in.** Once one entity takes orders, "just let the model command the pair" is one prompt change away, and it silently introduces order-consistency and conflict-resolution problems the schema has no answer for. Contain: `commander.maxCommandedEntities` is a hard cap, orders are strictly per-entity, and `sectionId` stays reserved and unused until v1.1.
- **Retry storms against a slow local server.** A 30 s timeout plus retries against a CPU server that takes 15 s per order produces overlapping requests and a self-inflicted slowdown. Contain: per-entity request suppression is mandatory, `maxConcurrentRequests` defaults to 1, and the local adapter does not retry at all — the fallback ladder handles it.

## Cost model

*[Finance / Owner]* Phase 2 only. Phase 1 has no marginal cost.

**Every input below is now MEASURED from live requests.** *(v1.8.4 — from `usage` over a 240-order soak against `claude-haiku-4-5`, 2026-08-04. This section has been recomputed in three consecutive revisions; the reason it should stop moving is that it is no longer arithmetic over a constructed string, it is the arithmetic the invoice does.)*

| Input | Value | Source |
|---|---|---|
| Cached prefix (`cache_read_input_tokens`) | **7,608 tokens** | Measured, and identical on **240 of 240** requests |
| Uncached input (`input_tokens`) | **179.7 tokens** mean | Measured. The volatile suffix. §Cost model assumed ~200 from v1.2 — **the closest thing to a correct assumption this document has carried**, off by 10 % |
| Output (`output_tokens`) | **104.7 tokens** mean | Measured. Assumed ~80 since v1.2 — **31 % low** |
| Cache hit rate | **100 %** (240/240) | Measured. The prefix does not merely *clear* the minimum, it demonstrably caches in practice |
| Cadence | 20 s → 720 orders/four-ship-hour | Configuration |
| Budget | **$5**; **$0.40 spent** to date | Corrected v1.8.2 from a $100 figure that was never true |

**Note what `input_tokens` is not.** It **excludes** cached tokens rather than containing them: 179.7 uncached and 7,608 cached are disjoint populations, reported in separate fields and billed at different rates. The harness's first cost function subtracted reads from `input_tokens` on the assumption that reads were a subset, which in steady state produced a **negative** cost per order — a wrong number loud enough to be caught, which is the only reason it was. §Corrections item 23.

| Model | $/MTok in | $/MTok out | $/order **cached** | $/four-ship-hour **cached** | $/order **uncached** | $/four-ship-hour **uncached** |
|---|---|---|---|---|---|---|
| `claude-haiku-4-5` | $1 | $5 | **$0.001464** | **$1.05** | $0.00831 | $5.98 |
| `claude-sonnet-5` (introductory, through 2026-08-31) | $2 | $10 | $0.00293 | $2.11 | $0.01662 | $11.97 |
| `claude-sonnet-5` (list) | $3 | $15 | $0.00439 | $3.16 | $0.02493 | $17.95 |
| `claude-opus-5` | $5 | $25 | $0.00732 | $5.27 | $0.04156 | $29.92 |

*(Prior values, every one of them superseded: v1.2 $0.00140/order · v1.3 $0.00180 · v1.8.2 $0.00105. The first two were computed against a prefix that was never deployed; the third against the prompt text only, missing the second copy of the schema the adapter also sends — §Corrections item 22.)*

~~Non-Haiku rows scale the measured token counts by published rates. Tokenization is assumed identical across current Claude models; only Haiku was measured.~~ **(v1.8.5 — that assumption is MEASURED and FALSE. §Corrections item 24.)** The identical 17,756-byte prefix tokenizes to **7,608 tokens on `claude-haiku-4-5` and 10,493 on `claude-sonnet-5`** — **+37.9 %**. Sonnet is also **2.5× more verbose** (269.5 output tokens against Haiku's 105.7). Both scaled rows below were therefore wrong in the same direction, and the measured Sonnet row is:

| `claude-sonnet-5` (list), MEASURED | $/order | $/four-ship-hour | vs the scaled estimate |
|---|---|---|---|
| 48 orders, 2026-08-04, `maxTokens = 512` | **$0.00784** | **$5.65** | scaled row said $0.00439 / $3.16 — **+79 %** |
| 48 orders, 2026-08-04, `maxTokens = 8192` *(v1.8.9)* | **$0.00788** | **$5.67** | **+0.5 % for the raise** — and 91.7 % → 100 % acceptance |

*(v1.8.9 — the second row is what a ceiling raise actually costs, and the answer is "almost nothing," because **billing is on actual output tokens, not on the ceiling**: total output moved 12,935 → 13,052, +0.9 %. The 269.5 figure above is superseded by a measured **271.9** — it was a censored lower bound and it understated by 0.9 %. And the direction that matters is the other one: at 512 the four truncated orders were **billed for 512 output tokens each and produced nothing** — 2,048 output tokens bought zero usable orders. §Corrections item 27.)*

**The Opus row is still an estimate and should now be read as a floor, not a figure.** It was produced by the same scaling that just failed by 79 % on the one row that got measured.

**Is the ≤ $1.10 target met? Yes — by 4 %.**

| Regime | $/four-ship-hour | vs ≤ $1.10 |
|---|---|---|
| **Cached — the applicable regime**, confirmed at 240/240 hits | **$1.05** | **MET**, 4 % headroom |
| Uncached, if caching ever stops | $5.98 | missed by 5.4× |

***(v1.8.14 — superseded by measurement, and the direction is down.)*** **C3 landed**: the prose copy of the order schema is gone from the prefix (§Corrections item 31), which is a change to the shipped artifact and therefore to every figure above. Measured over the 120-order arm that authorized it:

| | prefix | mean cache-read tok | $/order | $/four-ship-hour | vs ≤ $1.10 |
|---|---|---|---|---|---|
| Before (arm A, = the rows above) | 17,756 B | 7,544.6 | $0.001464 | **$1.05** | MET, 4 % headroom |
| **After (arm B, = what now ships)** | **8,750 B** | **5,075.4** | **$0.001220** | **$0.88** | **MET, 20 % headroom** |

**−16.6 % per order, and the headroom against the target goes from 4 % to 20 %.** That is larger than the 12 % v1.8.5 measured, because that arm cut 6,780 bytes of doctrine and this one cuts **9,006 bytes of duplicated schema**. **The cache still reads** — 119/120 hits, and 5,075 tokens clears Haiku 4.5's 4,096 minimum. **But the margin is now 979 tokens (24 %) where it was 3,512 (86 %)**, so §Corrections item 22's "a page of doctrine can be deleted without consequence" no longer holds: it is now roughly true up to ~980 tokens and false after that. The uncached row is unchanged in kind and falls in proportion.

**And the ±20 % model-accuracy gate is MISSED.** Measured $0.001464/order against §Cost model's v1.8.2 Haiku row of $0.00105 is **+39.4 %**, nearly double the tolerance. Both statements are true at once and neither cancels the other: **the product meets its cost target and this document's model of that cost was wrong by 39 %.** The model was wrong, not the measurement — the two errors that produced it are §Corrections items 22 (the schema counted once when it is sent twice) and the 31 %-low output-token assumption carried since v1.2.

**Where the money actually goes**, which no previous version of this table could show:

| Component | $/order | Share |
|---|---|---|
| Cache read (7,608 tok @ 0.1×) | $0.000761 | **52 %** |
| Output (104.7 tok @ 5×) | $0.000524 | 36 % |
| Uncached input (179.7 tok @ 1×) | $0.000180 | 12 % |

**Over half the bill is the cached prefix, and ~71 % of that prefix is the order schema in two renderings** (§Corrections item 22). Dropping the prose copy — which structured outputs makes largely redundant — would take the prefix to ~5,345 tokens, still cache-eligible, at **$0.00124/order = $0.89 per four-ship-hour**, restoring real headroom under the target. **Not done here:** prefix optimization is out of scope in §Rabbit holes, and the prose rendering has never been measured against its absence. It is the single highest-value cost lever this project has identified and it belongs to Phase 3.

**The margin over the cache minimum is 3,512 tokens (86 %), not the 9.6 % v1.8.2 reported.** That figure was computed against the 4,489-token prompt text; the block that actually caches is 7,608. **The "delete a page of doctrine and the cost quadruples" warning in §Corrections item 21 is therefore withdrawn** — it would take removing the great majority of the prefix to fall under 4,096. AIC-BE-3's startup comparison stays as the guard, but it guards a distant boundary rather than a hair-trigger.

**Recommendation:** default `claude.model = claude-haiku-4-5`, unchanged, now on measured ground. At **$1.05 per four-ship scenario-hour** the $5 balance funds roughly **4.7 hours** of live four-ship operation. Keep Opus 5 out of the control loop: at $5.27/hour for a decision that emits six posture values it is the wrong instrument, and that judgement survives every revision of this table because it never depended on the disputed numbers.

**Budget guard:** `aicmd.tokens.in` / `.out` accumulate in the order log; the runbook triggers a model downgrade or a switch to `local` at 80 % of budget.

## Validation and test plan

**Unit — order validator** *(no engine, no server; the highest-value tests in the plan)*
- Table-driven over ≥ 40 adversarial payloads: wrong types; out-of-range latitude/longitude/altitude/speed; `NaN` and `Infinity`; unknown enum members; extra top-level properties; missing conditional fields (`targetEntityId` on `engage`, `waypoint` on `hold`); a `reason` carrying injection text; hallucinated entity ids; a friendly-team target; a 10 MB body; a truncated JSON object; two JSON objects concatenated. Each asserts both rejection *and* the expected reason code — a test that only asserts rejection cannot tell a schema failure from a range failure.
- *(v1.7)* Schema-shape assertions over the embedded document: it is `oneOf` with exactly two branches; the waypoint branch's `posture` enum is `{ingress, hold, rtb}` and its `required` names `waypoint`; the geometry branch's `posture` enum is `{engage, crank, defend}` and it declares **no** `waypoint` property; both branches require `targetEntityId` and `orbitRadiusM` and both set `additionalProperties: false`. This is the test that keeps §Corrections item 13 from silently regressing — a flattened schema still validates every legal order, so nothing else in the suite would notice.
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

*(v1.7.5 — re-specified after the first run that could actually execute it. The v1.2 wording asked for things the OQ-6 scenario cannot supply, and the reasons are recorded in §Phase 1b live-smoke findings rather than quietly dropped.)*

- A run of **up to 10 minutes** on the OQ-6 scenario with `commander.backend = "local"`, ending when the last commanded entity leaves the scenario. **The commanded window, not the wall-clock window, is what is asserted over.** *(v1.7.5 — the previous wording said "10-minute run" and could not be satisfied: `oppint_red_interceptor`'s Red flight is destroyed by the Blue package at ~85 s, so the remaining ~515 s command nothing. A gate must name something the target can do — the same lesson as the v1.3 "TSAN clean" item.)*
- Assert: **no frame exceeding 5 ms of plugin cost**; **zero fratricide events**; **at least three distinct postures observed**; **no order times out**; **`reject.schema` < 1 %**; and the commander is verifiably ON with the configured backend, asserted from the startup log rather than assumed.
- **Acceptance rate is REPORTED here, and is not held to a bar.** *(v1.7.5.)* This is not a bar being lowered to turn a red run green — the failing run stays recorded in §Phase 1b, with its cause. It is the acceptance measurement being assigned to the instrument that has the sample size for it. A ≤ 10-minute engagement yields **~10 orders**, and a rate computed over 10 samples cannot distinguish a 50 % regression from three unlucky orders. **The ≥ 95 % acceptance bar remains, unchanged, on the 200-order soak**, which is where it has always been measured and where n is large enough to mean something. What the live smoke uniquely proves is that the pipeline works *inside the engine* — real component reads, real Stage B, real Lua consumption — and none of those need a rate to demonstrate.
- **Every rejection in the run SHALL be accounted for by reason**, and any Stage-B rejection SHALL carry enough of the offending order to diagnose it *(v1.7.5 — see AIC-DET-1; the first run's `geofence` rejections recorded the distance but not the waypoint, so the cause could not be read off the log)*.
- "Entity completes the scenario" is **withdrawn as an assertion** *(v1.7.5)*. The commanded entity is a Red fighter opposed by a Blue CAP; whether it survives is the scenario's outcome, not the commander's correctness, and asserting it would make the gate fail whenever the opposition wins.
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

> **PHASE CLOSED 2026-08-03 (v1.7.6).** Every gate item has a result. The live-scenario smoke and
> the H1 paired runs — the two items v1.7.1 recorded as unreachable — both ran once AIC-API-2 gained
> a deployed configuration source, and the live smoke now **passes, 17 checks / 0 failed**, against
> the gate as re-specified in v1.7.5. What the phase does **not** claim is listed under §Carried out
> of Phase 1b below; those are open items with owners, not gate items quietly marked green.

**Gate result, 2026-08-02** *(v1.7.1 — measured through the shipping adapter on the verification host: Ollama 0.32.5, `qwen2.5:7b-instruct-q8_0`, RTX 4070 Ti SUPER; live-smoke and H1 rows updated 2026-08-03, v1.7.6)*

| Gate item | Result |
|---|---|
| Acceptance ≥ 95 % over a 200-order soak | ✅ **100.0 %** (200/200) |
| `reject.schema` < 1 % | ✅ **0.00 %** |
| `reject.shape` reported separately | ✅ **0.00 %** — the four-branch schema, not prompt work (§Corrections 13, 15) |
| p95 within target; baseline ~1.7 s warm | ✅ **p50 1,723 ms / p95 2,163 ms / p99 2,208 ms**. Above the 1.7 s baseline by ~25 %, and the cause is known: the prefix grew from 4,738 to 17,756 bytes carrying the four branches and the doctrine. Far inside the ≤ 20 s local-7B target |
| The **first order of a run** completes rather than timing out | ✅ **4,566 ms, accepted**, from a model force-evicted with `keep_alive: 0`. Second order 2,120 ms. Budget 90 s |
| H2 measured | ✅ measured, and **not supported**: 2,291 ms stable vs 2,362 ms perturbed, a 3.1 % difference against a predicted ≥ 30 % (see §Key hypotheses) |
| H1 assessed on paired runs | ✅ **run 2026-08-03** — two 600 s logs from identical initial conditions, commander-on and commander-off, retained for the domain review. The review itself is a judgement and is not claimed here |
| Live smoke on the OQ-6 scenario | ✅ **PASS 2026-08-03 — 17 checks, 0 failed**, against the gate as re-specified in v1.7.5. Reachable at last: §Corrections item 17 resolved, and the run measured the `local` backend asserted from the startup log rather than assumed. Acceptance across the three runs was 50 %, 60 %, 70 % (5, 6, 7 of 10); it is **reported, not barred**, at this sample size, and every rejection was the Stage-B envelope catching a genuinely bad order — cause identified in §Phase 1b live-smoke findings. The ≥ 95 % bar remains on the 200-order soak |
| OQ-5 and OQ-6 resolved | ✅ both, at phase start |
| Unit suite green, ASan-clean, no server or network in CI | ✅ **87/87**, and **87/87 under AddressSanitizer** |
| Deployed-artifact smoke | ✅ **25/25** |

#### Phase 1b live-smoke findings *(v1.7.4, 2026-08-03)*

The in-engine run became possible once AIC-API-2 gained a deployed configuration source (v1.7.2). It measured `commander.backend = local` with `commander.enabled = true`, both asserted from the startup log rather than assumed. **One assertion failed and three findings came out of it that no offline harness could have produced.**

| Live-smoke assertion | Result |
|---|---|
| Commander is ON with the `local` backend | ✅ `backend=local enabled=true`, config applied in full, by path and field count |
| No frame exceeding 5 ms of plugin cost | ✅ **p50 0.0018 ms / p95 0.0025 ms / max 0.262 ms** over **12,001 frames** |
| Acceptance ≥ 90 % | ❌ **50 %** — 10 requested, 5 accepted, 3 rejected, 2 unresolved at entity death |
| Zero fratricide | ✅ 0 |
| At least three distinct postures | ✅ `defend`, `hold`, `ingress` |
| Entity completes the scenario | ❌ both commanded entities **destroyed at ~85 s** — by the scenario, not by the commander |
| `reject.schema` | ✅ **0.00 %** (0 of 10) |
| `reject.shape`, reported against no bar | ✅ **0.00 %** (0 of 10) |
| No order timed out | ✅ 0, and the **first order of the run was accepted at 8,040 ms** from a genuinely cold model |

**Finding 1 — the acceptance miss is the safety envelope refusing bad orders, and its cause is nameable.** All three rejections were Stage B: two `geofence` and one `clamp`.

- `geofence`, `RedSu35_02` at t=22.7 s: *"waypoint is 5,311,394 m away, beyond safety.geofenceRadiusM 200000 m"*
- `geofence`, `RedSu35_01` at t=24.8 s: *"waypoint is 5,305,327 m away"*
- `clamp`, `RedSu35_02` at t=83.0 s: *"cruiseSpeedMps 600 exceeds safety.maxSpeedMps 400"*

Two independent orders, seconds apart, both proposing a destination ~5,300 km away, is systematic rather than random.

**The hypothesis this document offered, and its refutation** *(v1.7.5)*. v1.7.4 proposed that a posture carrying a waypoint needs a destination, that the doctrine instructs *"egress toward the home field"*, and that the doctrine **by design** *"carries no scenario, platform, or mission specifics"* — so, asked to go home with no home given, the model invents a coordinate.

**That was measured and it is false.** A `geo` probe in the live harness runs each situation against the live model through the shipping prompt and prints the waypoint, its distance from own-ship, and whether the geofence would take it. Across **nine situations and ten waypoint-carrying orders — including three that draw `rtb`, the exact posture the hypothesis was about — the model set the waypoint to own-ship position every time, at 0 m.** Not one order was outside the fence. The doctrine's missing home field does not produce a wild waypoint; the model's actual failure mode is the opposite one, of declining to go anywhere.

**The actual cause, read off the log once `rawBody` was delivered** *(v1.7.5)*. Stage-B rejections had been recording an **empty `rawBody`** — AIC-DET-1 promises the raw body on every rejection, and the Stage-B path was passing an empty string although the candidate carries the body throughout. Fixed, the live run repeated, and the rejected order says plainly what no amount of reasoning from a distance figure could:

```
posture: hold,  reason: "Maintain orbit over friendly positions."
waypoint: { latitudeDeg: -31.952876, longitudeDeg: 115.860450, altitudeHaeM: 10000.0 }
```

**−31.952876, 115.860450 is Perth, Western Australia** — a heavily-memorised real-world coordinate pair. Own-ship was near Guam, hence 5,928,884 m. The model did not fail to find a home field: **it substituted a remembered city coordinate for a waypoint whose correct value was own-ship position.** The posture is `hold`, the one case where the answer is unambiguously "where you already are" and which the offline probe got right ten times out of ten.

**It reproduces** *(v1.7.6)*. A third run produced the same coordinate on a *different* entity — `RedSu35_02`, `−31.952247, 115.857309`, 5,932,920 m — with the same `posture: hold` and the same `reason: "Maintain orbit over friendly positions."` Three observations across two entities and three runs, agreeing to four decimal places on a coordinate that appears nowhere in the prompt, is a stable property of this model on this prompt rather than a sampling accident. Recording it that way matters: a one-off would be noise to watch, whereas a reproducible substitution is a **characterised failure mode with a known containment**, and it is the strongest concrete evidence this phase produced for why Stage B is not optional.

That reframes the defect entirely. It is not a gap in the doctrine's content — it is **a low-rate hallucination of a plausible-looking absolute coordinate**, which no amount of doctrine wording reliably prevents and which the schema cannot constrain, because every value it emitted is a well-formed number in a legal range. **The geofence is the only thing standing between that order and an aircraft flying to Australia**, which is precisely the case §Tenets' "the validator is the real defence" was written for. The control worked.

The second rejection in the same run is a different defect on the same posture, and it is worth recording next to the first because the contrast is instructive: `cruiseSpeedMps: 600` against a 400 m/s bound, with a waypoint of `13.484045, 144.991216` — **correct, adjacent to own-ship**. So the model gets the geography right and the kinematics wrong in one order, and the reverse in another. These are independent low-rate lapses, not one systematic misunderstanding.

**Consequence for the runbook** *(v1.7.5)*: a `geofence` or `clamp` rejection is **expected at low rate on this model** and is the envelope doing its job. What would be a finding is either climbing to a rate that starves an entity of orders, at which point the fallback ladder rather than the rejection is the problem to report.

**Two smaller observations from the same records**, recorded and not fixed: the body stored is the **whole Ollama envelope**, whose `context` token array consumes most of the 4 KB cap — the order itself survives only because it sits at the front, and recording the unwrapped `response` would be strictly more useful per byte. And `sanitizeText` strips `"` and `\`, so `rawBody` is human-readable but is **not** re-parseable JSON; anything wanting to replay a rejected body would need a different encoding.

**This is not resolved by widening `safety.geofenceRadiusM`.** Moving a bound to make a measurement pass erases the signal, which is the same reasoning §Cost model already applies to OQ-8's target. The bound is not what is wrong.

**Finding 2 — the "10-minute run" premise does not fit this scenario.** Both `RedSu35_01` and `RedSu35_02` were destroyed by Blue AMRAAMs at ~85 s (`pk=0.999999` and `pk=1.000000`). The remaining ~515 s of the 600 s run commanded nothing: the ladder walked `retained` → `released` for both entities and the run ended with `rosterSize: 2` and no live subject. So the run measured **~85 seconds of live commanding**, not ten minutes, and the sample sizes here are correspondingly small — 10 requests, 5 accepted. The gate item asks for a 10-minute run on the OQ-6 scenario; the OQ-6 scenario does not keep its Red flight alive for ten minutes against this Blue package. That is a defect in the gate item's phrasing, not in the system, and it is recorded rather than restated.

**Finding 3 — the reported p95 is not a p95.** 10,363 ms is the second-highest of **five** samples (8,040 / 10,363 / 1,880 / 4,442 / 3,055 ms). At n=5 no percentile is meaningful. What the numbers do show is that in-engine latencies run materially above the offline harness's 2,163 ms p95, which is expected and has a mechanism — `commander.maxConcurrentRequests` is 1 and two entities share it, so requests serialize — but the in-engine figure is **not measured** to a useful precision by this run and no p95 claim is made from it.

**Two gaps this run exposed, both recorded rather than fixed here:**

- **`reject.shape` = 0 % now has a live measurement behind it.** The 0.00 % from the 200-order soak was against six hand-written situations; this run put the model in front of situations nobody chose and the rate held at 0 of 10. Small n, but it is the first evidence from outside the authored set.
- **Stage-B rejections wrote an empty `rawBody`**, so a `geofence` rejection recorded *how far* the waypoint was but not *where* it was. The runbook could not diagnose the cause from the order log alone. AIC-DET-1 says the record carries the raw body; the Stage-B path was passing an empty string although the candidate carries the body throughout. **Fixed in v1.7.5** — and it is the reason v1.7.4's explanation of this very failure was a guess that v1.7.5 then had to withdraw.

**B8 was not exercised by this run.** No order carried a target at all — every accepted order had an empty `targetEntityId`, so `engage`/`crank` never occurred and the winchester path was never reached. B8's evidence remains its unit tests.

**Order quality, reported separately from acceptance** because a run can be 100 % accepted and still be tactically wrong. Across six situations spanning every posture, five drew the appropriate posture — `defend` on an inbound munition, `crank` while a shot is being supported, `hold` with no contacts, `engage` on a closing pair with a full rail. The sixth is a standing miss: a **winchester** aircraft with a close contact draws `engage` where doctrine says `rtb`, and two focused doctrine iterations did not move it. It is an order-quality miss rather than a safety one — Tier 1 will not fire with an empty rail whatever the posture — and it is the input to the H1 review when that becomes possible.



**Deliverables:** *(v1.6 — OQ-1 and OQ-2 resolved, so these are concrete rather than conditional.)* the `local` adapter against **Ollama** per the pinned contract in AIC-BE-1; constrained decoding via Ollama's `format` parameter carrying the embedded AIC-ORD-1 schema; live smoke on the OQ-6 scenario; H1 and H2 measurements.

**Validation gate:**
- ✅ Live smoke passes all assertions *(v1.7.6 — 17 checks / 0 failed, against the gate as re-specified in v1.7.5)*.
- Acceptance rate ≥ 95 % over a 200-order soak; **schema** rejections < 1 %.
- *(v1.6, **restated v1.7**)* `reject.shape` is reported separately and is **not** held to the < 1 % bar. v1.6 justified that by saying shape rejections measure *prompt* quality; that was measured at Phase 1b start and is **false** — a field-presence block stating the A6 rules imperatively changed the rate not at all (§Corrections item 13). What moves it is the schema encoding, and with AIC-ORD-1's `oneOf` branches the measured rate is **0/12**. The counter therefore stays separate for a different reason than v1.6 gave: it is now a **regression detector**. A non-trivial `reject.shape` no longer means "the prompt needs work" — it means constrained decoding is not in force, and the runbook says so.
- p95 latency within the target for the chosen model and hardware. Baseline to beat on the verification host: **~1.7 s warm** (qwen2.5 7B q8_0, RTX 4070 Ti SUPER).
- *(v1.6)* **The first order of a run completes rather than timing out.** Cold model load measured at 22–46 s against a 90 s timeout. Whether the adapter additionally warms the model at construction is a design call for this phase.
- H2 measured: prefix-stable vs perturbed-prefix p95 recorded, whatever the outcome. Note that Ollama's own prompt-cache behaviour, not just the model's KV reuse, is part of what is being measured.
- H1 assessed by a domain reviewer on paired commander-on / commander-off runs — `"Mariana Shield"` (stock Tier-1) against `"Mariana Shield AI"` (`RedSu35_01` commanded), same seed entry, same initial conditions.
- ✅ **OQ-5** and **OQ-6** resolved *at phase start rather than at the gate* — see §Open questions. *(OQ-1 and OQ-2 resolved in v1.6.)* Resolving them first was deliberate: OQ-6 gates what the smoke even runs against, and leaving a gating question open past the phase that depends on it is how OQ-4 sat unresolved through three revisions.
- *(v1.7)* The embedded schema is the `oneOf` document of AIC-ORD-1, asserted by unit test, and `reject.shape` over the soak is reported against the 0/12 spike baseline. A materially higher rate is a finding about the shipping system that the 12-order spike did not predict, and it is reported as such rather than absorbed.

#### Carried out of Phase 1b *(v1.7.6)*

The phase closes with these open, named, and owned — not folded into a gate result.

| # | Item | Why it is not a Phase 1b defect | Where it goes |
|---|---|---|---|
| C1 | **The model substitutes memorised real-world coordinates for waypoints**, reproducibly (three observations, two entities). Contained by Stage-B `geofence` | It is a property of the local 7B, not of this system. Every field it emits is well-formed and in range, so no schema, decoder, or prompt change reliably prevents it — only the validator catches it, and the validator does | Phase 2 comparison: whether a frontier model does the same is now a **measurable** question with a concrete benchmark behind it |
| C2 | **B8 has no live evidence.** No order in any run carried a target, so `engage`/`crank` never occurred and the winchester path was never reached | The logic is covered by five unit tests. What is missing is a scenario state — Tier 1 reporting tracks at the moment the rail is empty — not an implementation | Exercise opportunistically; not worth engineering a scenario for |
| C3 | **In-engine p95 is unmeasured.** Runs yield ~10 orders, and the observed figures (10.4 s, 4.9 s, 7.6 s) are the second-highest of five to seven samples | A ≤ 10-minute engagement cannot produce a percentile. The offline soak's 2,163 ms p95 over 200 orders stands as the latency number | Revisit only if a scenario whose subject survives longer is available |
| C4 | **`rawBody` records the whole Ollama envelope**, whose `context` token array consumes most of the 4 KB cap; and `sanitizeText` strips `"` and `\`, so the field is human-readable but not re-parseable JSON | The order survives because it sits at the front of the envelope, and the field did its job — it identified C1 | Recording the unwrapped `response` would be strictly more useful per byte. Small, deferred |
| C5 | **OQ-3 and OQ-8 remain open** | OQ-8's measurement half is done; its decision needs the hosted adapter to exist | Phase 2 start |

### Phase 2 — Claude backend

**Deliverables:** `claude` adapter over raw HTTPS with structured outputs; refusal handling; token accounting; the authorization gate and egress warning; the transmitted-field allowlist test.

*(v1.8 — the gate is partitioned by what authorization it needs, because the previous wording read as one undifferentiated list and the first four items are reachable with no network at all. That distinction is not bookkeeping: it decides what can be built and proven before the owner is asked for anything.)*

**Validation gate — reachable with no network and no authorization:**
- Allowlist test green; no key in any prompt, log, or record — asserted with a sentinel value placed in the environment, not by reading the code.
- Refusal, 429, 5xx, transport-`nullopt`, and TLS-unavailable paths each exercised and correct, against a fake transport through the same injectable-factory seam the local adapter's suite uses.
- *(v1.8)* The schema projection carries no unsupported keyword, preserves every pin as a `const`, and resolves the adversarial corpus identically to the canonical document (§Corrections item 19).
- Unit, ASan, and deployed-smoke suites still green and still requiring **no inference server and no network**; `tests/live/` and `run-live-scenario.ps1` still absent from CI and from `ai-commander-tests.vcxproj`.

**Validation gate — requires owner authorization for live egress:**
- Owner authorization recorded before the first live hosted request. **This item gates every item below it**, and it is the owner's to give rather than the implementer's to infer.

  *(v1.8.1 — recorded 2026-08-03.)* The platform owner granted a **narrow** authorization: a token-count probe against `POST /v1/messages/count_tokens` carrying the rendered **stable prefix only** — system prompt, posture and ROE vocabulary, order schema, and `data/doctrine.txt`. **No volatile suffix, and therefore no scenario state of any kind**: no position, velocity, heading, team, track, or loadout. Every request that carries a snapshot — the 200-order soak, the live-scenario run, and the memorised-coordinate comparison — remains gated and needs its own grant.

  The narrowness is the point rather than caution for its own sake. This probe resolves OQ-8, the question with the largest single effect on §Cost model and on whether §Success metrics' ≤ $1.10 target is reachable, and it resolves it **without moving one byte of proprietary scenario state off the machine**. §Tenet 4 asks for a positive act before scenario data egresses, not merely before an API key is first used, so the grant is recorded at the granularity the tenet is actually about.

  *(v1.8.3 — a second grant, recorded 2026-08-04, before the first request made under it.)* The prefix-only grant above resolved OQ-8 and expired with it. The owner has now authorized the **full Phase 2 measurement gate**, which is **snapshot-carrying**: the schema-acceptance probe, the ~240-order soak, the p95 sample, and the memorised-coordinate comparison. Requests under this grant transmit the rendered volatile suffix — **position, velocity, heading, team, reported tracks, and loadout** — and the grant is recorded with that enumerated rather than summarised, so a later reader sees what actually left the machine.

  **The grant has one boundary and it is deliberate.** It covers the **synthetic situation fixtures in `tests/live/LiveMain.cpp`** — six hand-authored situations written for the harness — and it does **not** cover `run-live-scenario.ps1` against `oppint_red_interceptor`, which would transmit real scenario state from a deployed mission file. The distinction is the one §Tenet 4 is actually about: the fixtures carry the same *field set* as a real snapshot, populated with values that were invented for a test rather than drawn from proprietary content. A phase that measures the hosted backend on fixtures and says so is honest; one that quietly upgraded a fixture grant into a scenario grant because "it is the same fields" would be inferring authorization, which is the thing this gate exists to prevent. **The in-engine live-scenario run therefore remains ungranted and is carried out of Phase 2.**

  Budget recorded against the corrected balance (§Corrections item 21): the gate is ~240 orders ≈ **$0.25 cached / $1.22 uncached** against **$5**, and the measurement of which of those two regimes actually applies is itself one of the gate's outputs.

  *(v1.8.5 — a third grant, recorded 2026-08-04 before the requests made under it, covering **Phase 3 diagnostic runs on the same synthetic fixtures**.)* Granted after Phase 2 reported two gate failures, to measure them rather than to fix them. It covers exactly two experiments, both on the `LiveMain` fixtures and **both inside the v1.8.3 boundary** — no real scenario state, `run-live-scenario.ps1` still ungranted:

  - **The latency decomposition (C2).** Two arms of ~60 orders differing only in doctrine size, to separate "the prefix is large" from "the output is long" as the cause of the 4,615 ms p95. Neither hypothesis is assumed; the point is that they predict different Phase 3 work and picking one without measuring is how §Corrections gets longer.
  - **The Sonnet order-quality comparison.** ~48 orders on `claude-sonnet-5`, the use §Cost model already reserves that model for. Time-boxed rather than open-ended: the introductory rate ends **2026-08-31**, after which the same run costs 50 % more.

  Estimated **≈ $0.31** against **$4.60** remaining. This grant authorizes **measurement only** — no change to `PromptRenderer`, `OrderSchema`, or the latency target follows from it, and §Carried out of Phase 2 C3's prose-schema removal stays unmade until order quality has been measured against its absence.

  *(v1.8.8 — a fourth grant, recorded 2026-08-04 **before the request made under it**, covering **one experiment: the ceiling-raised re-run C7 needs**.)* Asked for and granted because it is a **new experiment the v1.8.5 grant does not name** — that grant enumerated two experiments and both have run. Same boundary as its three predecessors: the **six synthetic `LiveMain` fixtures**, no real scenario state, **`run-live-scenario.ps1` against `oppint_red_interceptor` still ungranted** and C1 unchanged.

  - **48 orders on `claude-sonnet-5`, the same six fixtures × 8 repeats**, with `claude.maxTokens` raised to **8,192** — the bound v1.8.7 derived. **Paired with the censored 48-order run so the only difference is the ceiling.** A probe whose purpose is to observe a censored tail must not itself censor, and because billing is on *actual* output tokens rather than the ceiling, an unused ceiling costs nothing.
  - **The design carries its own control.** `max_tokens` is an **enforced ceiling the model is not aware of**, so raising it should leave the 44 already-completed responses' lengths *unchanged*. If they shift materially, the run has found something other than what §Corrections item 25 predicts, and it will say so.
  - **What it is not.** It authorizes **measurement**, not a config change. §Corrections item 25's refusal stands until this run reports: the default is 512 and no replacement value is chosen in advance of the data. The one thing already fixed is the *bound*, which was derived from Stage A's body cap rather than from any output-length figure (item 26).

  Estimated **≈ $0.38** against **$4.06** remaining. Time-boxed for the same reason the Sonnet comparison was: the introductory rate ends **2026-08-31**.

  *(v1.8.11 — a fifth grant, recorded 2026-08-05 **before any request made under it**, covering **three experiments — and, for the first time, crossing the boundary the previous four grants deliberately held**.)* Asked for and given after the C2 re-analysis (§Corrections item 28) established that the cheap version of the C2 experiment does not work.

  **Read the boundary change first, because it is the part that is not a continuation.** Grants one through four each carried the same sentence: the **six synthetic `LiveMain` fixtures**, no real scenario state, `run-live-scenario.ps1` against `oppint_red_interceptor` **ungranted**. That boundary was held four times, including once (v1.8.3) where it was explicitly argued that "it is the same fields" must not be allowed to upgrade a fixture grant into a scenario grant. **The owner has now granted it, as a decision rather than as an inference**, and this document records it that way: what changed is the owner's judgement, not an argument that made the boundary look already-crossed.

  - **① The C2 fixed term, via time-to-first-token.** The instrument §Corrections item 28(f) argues for, replacing the total-latency estimator whose residual SD is 1,482 ms. Two doctrine sizes on `claude-sonnet-5` and on `claude-haiku-4-5`, measuring **TTFT separately from total**, so the fixed per-request term is observed rather than inferred out of a mixture. **Streaming here is a measurement instrument under `tests/live/`, not a product path** — see §Out of scope, whose streaming row is unchanged and still governs the adapter. **The risk is stated in the grant rather than discovered after it:** the fixed term may still sit below the noise floor even in TTFT. **A null result is a real outcome, is to be reported as one, and is not grounds for a third pair of arms.** Estimated **≈ $0.50**.
  - **② The C3 quality half.** A paired arm — full prose schema against its absence — at n ≈ 120, the one open item whose result changes shipped code. The cost mechanism is already confirmed (−12 %); what has never been measured is whether removing the prose rendering costs order quality, and **one rejection at n=60 is not a measurement.** **A condition attaches to this one:** the arm's power — what accept-rate difference n ≈ 120 can actually resolve — **is to be written into this document before the run, not derived from it afterwards.** If n ≈ 120 cannot distinguish the hypotheses, that is to be said and the run not made. Estimated **≈ $0.40**.
  - **③ C1 — the in-engine live-scenario run.** `tests\smoke\run-live-scenario.ps1` on the **hosted** backend against `oppint_red_interceptor` in "Mariana Shield", with its paired commander-off control. **What leaves the machine is the same field set as every request since v1.8.3 — position, velocity, heading, team, reported tracks, loadout — drawn from real scenario state rather than from six hand-authored fixtures.** `docs/egress.md` is updated in the same revision as this grant and *before* the run, so the provenance change is on the record rather than in a commit message. Estimated **≈ $0.10** — ~30 orders per commanded entity over a 600-second run, on Haiku; the control run makes no request at all.

  **What this grant is not.** As with all four before it, it authorizes **measurement**. No latency remedy follows from ①, no prose-schema removal follows from ② unless ② measures that quality holds, and nothing about ③ changes a default. §Carried out of Phase 2 C8 is untouched: **no `claude.maxTokens` value is chosen for Sonnet**, and 673 remains a sample maximum.

  Estimated **≈ $1.00** against **$3.68** remaining (§Budget: $1.32 of $5 recorded at list). Time-boxed for the third time for the same reason: `claude-sonnet-5`'s introductory rate ends **2026-08-31**, after which every Sonnet figure in this document rises ~50 %.
- Measured cost per order within 20 % of the Cost model's Haiku row; if not, the model reconciles the difference before further runs. **❌ (v1.8.4 — MISSED, and the model has been reconciled.** Measured **$0.001464/order** over 240 orders against the v1.8.2 Haiku row of $0.00105 — **+39.4 %**, nearly double the tolerance. **This document's model was wrong, not the measurement**, for two reasons now recorded: §Corrections item 22, the order schema is transmitted twice per request and the model counted it once; and the output-token figure was assumed at 80 since v1.2 and measures 104.7, 31 % low. §Cost model is rebuilt on measured `usage` rather than adjusted to fit. The gate item is reported as failed rather than reinterpreted — the tolerance was there to catch exactly this, and it did.**)
- p95 latency ≤ 2.5 s. **❌ (v1.8.4 — MISSED. Measured p50/p95/p99 = 2,602 / 4,615 / 7,099 ms** over 240 orders — a real percentile over a real sample, which is what §Carried out of Phase 1b C3 asked for and Phase 1b could not supply. **p95 is 85 % over the 2.5 s target, and even p50 exceeds it.** The target was set in v1.2 against no hosted measurement of any kind. Nothing was tuned to chase it and the number is reported as it fell. What this does *not* establish is where the time goes: the 20 s order cadence is unaffected at p99, so the miss is a latency-target failure and not a control-loop failure, and no run in this phase was degraded by it. Carried to Phase 3 with a destination — see §Carried out of Phase 2 C2.)
- OQ-8 resolved. *(v1.3 — the measurement half is already done: 4,738 bytes ≈ 1,200 tokens, taken at the Phase 1a gate. What remains at this gate is the owner's judgement on whether ~2,900 tokens of genuine doctrine are worth writing to reach Haiku 4.5's cache minimum, against the recomputed regimes in §Cost model — $1.30/four-ship-hour unpadded-uncached vs $0.73 padded-and-cached.)* **(v1.8 — that measurement was superseded and the replacement is not free.** The 4,738-byte figure was taken doctrine-less, §Corrections item 16; the deployed prefix is 17,756 bytes. The token count that decides the question needs Anthropic's tokenizer, which is a live call — so this item is authorization-gated, not merely pending. See OQ-8.) **✅ (v1.8.2 — DONE, 2026-08-04. 4,489 tokens measured on `count_tokens`; clears the 4,096 minimum by 393; the prefix caches as written, the padding delta is zero, and the owner's judgement this item reserved never has to be made. §Cost model recomputed: $0.76/four-ship-hour cached against the ≤ $1.10 target — met.)**
- *(v1.8)* **The Phase 1b memorised-coordinate benchmark run against the hosted model, and reported whatever it shows.** §Carried out of Phase 1b C1 made this a measurable question with a concrete baseline; a phase that builds the hosted backend and does not answer it has left its predecessor's most valuable output on the floor. A negative result — the frontier model does the same thing — is as publishable here as a positive one, and neither changes the standing of Stage B, which catches it either way. **✅ (v1.8.4 — RUN, and the honest answer is two-part.)** **The Perth substitution does NOT reproduce**: 0 of 11 waypoint-carrying orders landed within 50 km of `−31.95, 115.86`. Over 11 orders that bounds the rate rather than establishing zero, and the detector looks for Perth specifically, so it is evidence about *that* coordinate and not about memorisation generally. **But the run found a different waypoint failure, and it reproduced within the sample.** Two of 11 orders — both on the `winchester, no threat (rtb)` situation, on consecutive repeats — placed the waypoint at longitude **exactly 140.000** where own-ship was at 145.000, putting it **540 km outside** the 200 km geofence. Not a memorised city: a **round-number longitude**. **The generalisation is the finding, and it is stronger than either result alone.** The local 7B substituted a memorised place; the frontier model substituted a rounded coordinate. Both emit a well-formed, in-range, entirely wrong waypoint that no schema, decoder, or prompt constraint rejects, and both are caught only by Stage-B `geofence`. **The specific wrong number is model-specific; the failure mode is not** — which is a considerably better argument for Stage B than "the small model hallucinates Perth" ever was. Note also that Stage A accepted all 240 soak orders while these geofence violations exist: the two stages measure different things and the 100 % Stage-A rate is not in tension with an 18 % geofence rate.
- *(v1.8)* `reject.shape` against the hosted backend **reported, not barred**, with the local 0/12 as the comparison. The reasoning is v1.7.5's: a rate needs a sample size, and the hosted encoding is a projection whose behaviour §Corrections item 19 predicts but has not measured. A materially non-zero rate is a finding about the projection, reported as such. **✅ (v1.8.4 — `reject.shape` is 0.00 % over 240 orders, and `reject.schema` is 0.00 %; acceptance 240/240 = 100 %.** The projection holds its constraints on the hosted path exactly as the canonical document does locally — 0/240 against the local baseline's 0/12, on a sample twenty times larger. Nothing to report as a finding, which is itself the result: the mechanical projection preserved the four-branch encoding's effect, and §Corrections item 19 confirms it had to exist for the request to be accepted at all.)
- *(v1.8.4)* **§Corrections item 19's prediction settled against the live API, in whichever direction it fell. ✅ CONFIRMED, on both halves, by three requests that isolate them.** The canonical document is rejected `400 — "Schema type 'oneOf' is not supported"`; a hybrid differing from it only in `oneOf`→`anyOf` is rejected `400 — "For 'integer' type, properties maximum, minimum are not supported"`; the shipped projection is accepted `200`. The prediction was made from documentation in v1.8 and explicitly labelled as not-yet-measured; it was right, and **the projection is load-bearing rather than insurance**. The canonical `orderJsonSchema()` remains unmodified (R4) — now with a *measured* reason to keep the projection rather than a predicted one.

#### Carried out of Phase 2 *(v1.8.4)*

The phase closes with these open, named, and owned — not folded into a gate result.

| # | Item | Why it is not a Phase 2 defect | Where it goes |
|---|---|---|---|
| C1 | **The in-engine live-scenario run against `oppint_red_interceptor` was not performed.** `run-live-scenario.ps1` is untouched on the hosted backend. **(v1.8.11 — AUTHORIZED. The run is now scheduled; it has not yet been made.)** | **Not a defect, an authorization boundary.** The v1.8.3 grant covers the synthetic `LiveMain` fixtures and explicitly not real scenario state from a deployed mission file. The distinction was drawn deliberately and the run was not made rather than the grant being read generously | **CLOSED (v1.8.15).** Granted v1.8.11 by an owner **decision**, not by an argument that made it look already-covered; **run 2026-08-05, 19 checks / 0 failed** (§Corrections item 32). Both entities commanded, 3 postures, 0 timeouts, `reject.schema`/`reject.shape` 0 %, frame cost max 2.87 ms, tree left clean, and **Stage B refused a real 450 m/s request against a 400 m/s bound** — Goal 3 on the product. **It supplies no rate and no percentile** (n≈13); those bars stay on the soak. What it buys is *"the backend works"* in place of *"the backend works on fixtures"* |
| C2 | **p95 latency is 4,615 ms against a ≤ 2.5 s target** — missed by 85 %, with p50 (2,602 ms) also over. **(v1.8.5 — decomposition run, INCONCLUSIVE. v1.8.9 — the *reason* for that verdict became suspect; see §Corrections item 27(f). v1.8.10 — re-analysed: the verdict stands, the reason is replaced, and one clause is struck; see item 28.)** | The target was set in v1.2 with no hosted measurement behind it, and this is the first real sample. Nothing was tuned to chase it. The 20 s cadence absorbs p99 (7,099 ms), so no run was degraded — it is a target failure, not a control-loop failure | Phase 3, **reopened**. Range restriction is now **demonstrated**, not argued: banding Sonnet's output to Haiku's width collapses its r² **0.734 → 0.036**, and the closed form predicts **0.019** on Haiku's spread. **But Haiku's arms remain uninformative in both directions** — arm A's slope CI includes zero and excludes Sonnet's 13.8 ms/tok — so nothing here says output length drives Haiku's latency. **(v1.8.11 — granted. v1.8.12 — RUN, and C2 CLOSES on a cause; see §Corrections item 29.)** The TTFT instrument was built and it does not decompose anything: TTFT correlates with total at **0.995**, because under structured outputs the first delta arrives after the generation. **But the run answers C2 by a route nobody was arguing about** — time to response *headers*, before one token exists, is **mean 3,157 ms / p95 6,917 ms**. **The fixed term alone exceeds the 2.5 s total-latency target**, so the target is unreachable by any prompt-side change and always was. The doctrine comparison is **null on all three estimators** (best: −103 ms [−940, +733] for 2,202 cached tokens). Remedy is a different model, a different target, or acceptance — **and the 20 s cadence already accepts it.** The target row stays **MISSED**; it is not moved to fit the measurement |
| C3 | **The order schema is transmitted twice per request. (v1.8.5 — the cost half is now confirmed by measurement; the quality half is not.)** | Cutting the prefix by 1,617 cached tokens moved cost from **$1.05 to $0.92 per four-ship-hour (−12 %)**, confirming the mechanism. But that arm also accepted **59/60** where the full-doctrine arm accepted 60/60 **CLOSED (v1.8.14).** Granted v1.8.11; power and decision rule pre-registered in **item 30** and pushed before the run; result in **item 31**. **Both arms accepted 120/120** with `reject.schema` and `reject.shape` at 0.00 %, bounding arm B's true acceptance at **≥ 97.53 %** against the ≥ 95 % gate. **The removal is MADE** — prefix **17,756 → 8,750 B**, cost **$1.05 → $0.88** per four-ship-hour. The residual is a **new constraint, not an open question**: the cached block is now 5,075 tokens against Haiku's 4,096 minimum, a **24 % margin where it was 86 %** |
| C3-note | *(superseded by the C3 row above, v1.8.5)* The schema is ~71 % of the cached prefix and ~52 % of the bill (§Corrections item 22) | The prose rendering predates structured outputs, when `format` was the only constraint | — |
| C4 | **`cache_creation_input_tokens` is captured by neither `ILlmClient` nor the harness**, so cache-write cost is invisible to the accounting | Quantified rather than ignored: $0.0095 per write, amortized to ~$0.00004/order over a 240-order run — under 0.1 % | Add alongside `lastCacheReadTokens()` when a cheaper reason to touch that interface arises |
| C5 | **The memorised-coordinate result is a bounded negative, not a zero**, and the round-number-longitude substitution it found instead has 2 observations on 1 situation | 11 waypoint-carrying orders cannot establish a rate. The detector looks for Perth specifically and would not see a different memorised coordinate | Phase 3, if order quality is measured at scale. The Stage-B conclusion does not depend on it |
| C6 | **OQ-3 remains open** | Unrelated to Phase 2; it turns on whether `n8ro-llm` is ever installed | v1.1 planning, unchanged |
| C7 | **`claude.maxTokens = 512` is Haiku-shaped and truncates Sonnet** (§Corrections items 24(b), 25, 26, 27). *(Opened v1.8.5, narrowed v1.8.6, config surface v1.8.7, **measured and CLOSED v1.8.9**.)* | Not a Phase 2 defect: 512 was chosen against the only model then measured, and Haiku uses 25 % of it at its worst over 120 orders. It became wrong when a second model was run, not when it was set | **CLOSED.** The config surface is model-shaped and bounded `[1, 8192]` (v1.8.7, derived from Stage A's body cap — item 26 also records the 4 KiB record cap that would have silently truncated the evidence). The measurement is done (v1.8.9, under the v1.8.8 grant): raising the ceiling took acceptance **91.7 % → 100.0 %** for **+0.5 % cost**, and Sonnet's true max is **673 tokens — not the ~2,000 that scaling by Haiku's headroom would have given**. **The default stays 512 because the default model is Haiku**; the Sonnet figures an operator needs are on the record and picking a headroom policy is the owner's call, not this document's. Residual → **C8** |
| C8 | **A ceiling sized from one 48-order run is sized against noise** (§Corrections item 27(e)). *(Opened v1.8.9.)* | Fell out of C7's own control rather than being assumed: per-fixture mean output moved by up to **71 tokens** between two identically-configured runs, and one fixture's max moved **478 → 193**. 673 is a sample maximum, not a bound | Only matters if a non-Haiku model is actually adopted. At that point the question is a **tail** question and needs repeats, not a larger single sample. Not scheduled, and no value is chosen in the meantime |

##### The C2 latency decomposition — run, and inconclusive *(v1.8.5; re-analysed and partly corrected v1.8.10 — see §Corrections item 28)*

Two arms of 60 orders differing **only** in doctrine size, to separate "the prefix is large" from "the output is long" as the cause of the p95 miss.

| | Arm A — full doctrine | Arm B — short doctrine |
|---|---|---|
| Prefix | 17,756 B · 7,608 cached tokens | 10,976 B · 5,991 cached tokens |
| p50 / p95 / p99 | 2,523 / **3,674** / 4,361 ms | 2,113 / **5,657** / 6,797 ms |
| latency vs output tokens, **r²** | **0.004** | **0.071** |
| Cost per four-ship-hour | $1.05 | $0.92 |

**Neither hypothesis survives.** Output-token count explains essentially **none** of the latency variance in either arm — 0.4 % and 7 % — and the intercept/slope split the regression produces (2,454 ms + 1.8 ms/token for A, 466 ms + 20.8 ms/token for B) must not be quoted as a decomposition.

**But the reason given here in v1.8.5 was wrong twice over, and §Corrections item 28 corrects both.** *(v1.8.10.)* This paragraph originally read the 1.8-vs-20.8 split as "two arms of the same system disagreeing by 5×, and that is the tell." **Their slope confidence intervals overlap** — [−5.63, +9.25] and [+1.49, +40.12] — so they are two estimates too imprecise to conflict, not two estimates that conflict. And the r² column, which this section leads with, is a property of how far the predictor was allowed to move: **restricting Sonnet's known-real relationship to Haiku's output range collapses its r² from 0.734 to 0.036**, and the closed form predicts **0.019** on Haiku's spread. The slope with its interval is the test; r² is not.

**The prefix reduction did not produce a consistent improvement either.** Cutting ~1,617 cached tokens moved p50 **down** 16 % and p95 **up** 54 %. A real effect on a fixed per-request cost would move both in the same direction.

**And the p95 itself is not stable at this sample size.** Arm A and the Phase 2 soak are the *same configuration*: p95 **3,674 ms** at n = 60 against **4,615 ms** at n = 240, 26 % apart. Some of what §Success metrics reports as a fixed miss is sampling.

**What this establishes** *(clause struck v1.8.10 — see §Corrections item 28(e))*: latency here is dominated by variance this instrument cannot resolve at n = 60 — run-to-run and within-run, consistent with service-side queueing and scheduling. **What it does not establish:** that the prefix is irrelevant, or that output length is. The original text continued *"…not by either quantity the product controls,"* and **that clause is withdrawn**: it asserts a negative about the two quantities the experiment had no power to detect. What the arms establish is a **noise floor**, which is a fact about the instrument and not an attribution.

**The next step is therefore a better instrument, not a fix.** Time-to-first-token separated from generation time would put the prefix term where it can actually be seen; the total-latency figure this harness records mixes it with everything else. Choosing a remedy on the strength of these numbers would be picking the arm that flatters a story, which is the error §Corrections is mostly a record of.

**And a bigger version of the same arms will not do instead** *(v1.8.10)*. At Sonnet's residual SD of 1,482 ms the intercept's 95 % interval at n=48 is **±776 ms**, so a two-arm doctrine comparison needs an intercept gap above **≈1,100 ms** to resolve — far more than ~2,200 cached tokens can plausibly cost — and closing that by sample size alone runs to roughly **$10**. The instrument is the cheaper instrument, not merely the better one. §Corrections item 28(f).

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
- [x] *(v1.7)* Every conditional rule in AIC-ORD-1 is expressed in a form the constrained decoder can enforce, verified by measurement rather than by assumption
- [x] *(v1.7.6)* Every Phase 1b gate item has a **result**, and every item the phase does not claim is enumerated in §Carried out of Phase 1b with a reason and a destination — rather than left implicit in a green summary
- [x] *(v1.7.6)* Every gate item names something the target can actually do, re-checked against a run rather than against intent — the live-smoke item was re-specified in v1.7.5 when the OQ-6 scenario turned out not to keep its subject alive for the duration the gate asked for
- [ ] OQ-3 and OQ-8 — **outstanding** *(v1.7 — OQ-5 and OQ-6 resolved 2026-08-02; OQ-1 and OQ-2 resolved 2026-08-02 in v1.6; OQ-4 resolved 2026-08-01; OQ-7 and OQ-9 resolved 2026-08-01. OQ-3 depends on a roadmap answer this document cannot produce; OQ-8 has its measurement but not its decision, which is the owner's at Phase 2 start)*

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
- **OQ-4 was answered, one revision late, and the delay cost nothing.** *(v1.4.)* It was scheduled at the Phase 1a gate so it would land before Phase 1b spent effort on the local adapter. Phase 1a closed without it — caught by the v1.3 PRD review, then resolved. The answer was "no", so nothing built in Phase 1a was wasted; had it been "yes", the slip would have been expensive, and that asymmetry is the argument for answering cheap-but-invalidating questions on schedule rather than when convenient. The investigation took under an hour: enumerate the tool handlers in the shipped binary.
- **The doctrine text is unwritten and is on the critical path for order quality.** It is the one Phase 1 deliverable this PRD does not specify in detail, because its content is domain expertise rather than engineering. Flagged as a rabbit hole with a one-day timebox; if it needs more, that is a signal to reconsider the RAG deferral.
- **v1.3 note — a validation gate must name a tool that exists on the target platform.** "TSAN clean" survived from the authoring brief through two revisions and a design pass without anyone checking whether a ThreadSanitizer runtime ships for Windows. It does not, and the PRD consequently specified a gate no build could ever pass. The lesson generalizes past this one item: a gate is a claim about *this* toolchain, and every gate item should be traceable to a command someone has run here, in the same way §Corrections requires every SDK fact to be traceable to a shipped header. The other gate items were checked against this when v1.3 was written — `dumpbin /exports`, `/fsanitize=address`, and the replay suite all exist and all run — but the check should be part of writing a gate, not part of repairing one.
- **v1.3 note — two Phase 1a gate items were assumptions wearing the costume of measurements.** "TSAN clean" was unrunnable and the Cost model's ~800-token prefix was a guess that undershot the measured 1,200 by a third, carrying every uncached figure in the table down with it. Both were phrased with the confidence of observed facts. The pattern to watch: a number with no units-and-source trace behind it (Appendix A's discipline) reads exactly like one that has been measured, and the Cost model — the one section whose whole output is arithmetic — had no such trace on its most load-bearing input. §Corrections item 9 now carries it.
- **v1.2 note — the snapshot was specified from the Lua surface, not the C++ one.** Every field in the original §Exactly what is transmitted named a Lua verb, and two of them turned out to have no C++ equivalent. The lesson generalizes: for a C++ plugin, "which verb returns this?" is the wrong traceability question — "which header or schema record exposes this to *the plugin*?" is the right one. Appendix A now carries the Lua/C++ split explicitly so the next field added is checked against both columns.

## Changelog

### v1.8.15 — 2026-08-05

**The run held across four grants was made, and the hosted backend works inside the engine.** `run-live-scenario.ps1 -Backend claude` against `oppint_red_interceptor` in the shipped "Mariana Shield", 600 s commander-on plus a 600 s commander-off control, on `claude-haiku-4-5`. **19 checks, 0 failed.** §Corrections item 32.

**Every hosted figure in this document before this one described six synthetic fixtures** — the same field set as a real snapshot, filled with values invented for a test. This one draws them from a shipped scenario. The distinction was never technical; it was whose decision it was, and it was the owner's.

**What worked, restricted to what a fixture cannot exercise.** Both entities commanded. Three distinct postures — `defend`, `engage`, `ingress`. **Zero timeouts**, including the first order of the run. `reject.schema` and `reject.shape` at **0 %**. Plugin frame cost p50 **0.0023 ms**, max **2.87 ms** across 12,001 frames, inside the 5 ms bound. Release tree left clean: config removed, `oppint_red_interceptor.lua` restored to 19,032 bytes.

**C3's change is confirmed on the product.** A real in-engine request reported **5,118 cached tokens** — the reduced prefix, caching in the engine, one commit after it was measured on fixtures at 5,075.

**And the most valuable single observation is a rejection.** One order asked for `cruiseSpeedMps` **450** against `safety.maxSpeedMps` **400**, and Stage B refused it. **Goal 3 — no unsafe order reaches a `request*` verb — demonstrated on the product rather than on an adversarial corpus**, with the full `rawBody` attached, which is what v1.7.5 added AIC-DET-1 for. **A clean run would have been weaker evidence than this one.**

**Then the two numbers the harness printed that are not what they look like, both now fixed.**

*The acceptance rate had the wrong denominator.* It printed "10 of 13 requested (76.9 %)". **Two of the thirteen were still in flight when the engine stopped**, issued in the last seconds of a fixed-length run — no verdict, because the run did not wait for one. Counting them against acceptance charges the backend for a decision it was never allowed to make. Resolved: **10 of 11**. And **neither figure is a rate at n=11**, which C5 already says in this document. **76.9 % reads exactly like an order-quality result and is not one** — the same shape as the 91.7 % configuration artifact in item 24(b).

*The "p95" was the maximum.* Printed over **ten** samples, where the 95th-percentile index lands on the last. The ten ran 2,816 → 5,552 ms with a **median of 3,137** and one outlier that became the headline. **v1.7.4's Finding 3 recorded this exact error — "the reported p95 is not a p95" — and the script went on printing one for four revisions afterwards.** A lesson that stayed in the changelog and never reached the instrument. It now reports min/median/max with n and refuses to print a percentile.

**What C1 establishes, narrowly.** The hosted path works end to end in the engine on real scenario state — real component reads, Stage B against real entity state, the Lua tier consuming published orders, the safety bound holding, the cache reading, the frame budget intact. **It supplies no rate and no percentile**; ~13 orders in ten minutes cannot, and the ≥ 95 % and latency bars stay on the soak. **What it buys is the sentence "the backend works" in place of "the backend works on fixtures."**

**C1 CLOSES.** ≈$0.02 against ≈$0.10 estimated. Running total ≈ **$2.50 of $5**.

### v1.8.14 — 2026-08-05

**The first Phase 3 diagnostic to change shipped code — and it changed it on a rule written down before the run.** 240 orders on `claude-haiku-4-5`, 120 per arm, under the fifth grant. §Corrections item 31.

**Both arms accepted 120/120**, with `reject.schema` and `reject.shape` at 0.00 % in each. Item 30's rule — *≥ 119/120 with both counters at zero → the removal is safe to make* — fires on the branch it named first. Arm B's 120/120 bounds its true acceptance at **≥ 97.53 %** (exact, one-sided 95 %) against the **≥ 95 %** gate. **The prose copy of the order schema is removed from `PromptRenderer::build`.**

**What it bought.** Prefix **17,756 → 8,750 bytes**. Cost **$0.001464 → $0.001220** per order, **$1.05 → $0.88** per four-ship-hour — **−16.6 %** — and headroom against the ≤ $1.10 target goes from **4 % to 20 %**. Larger than v1.8.5's 12 %, because that arm cut 6,780 bytes of doctrine and this one cuts **9,006 bytes of duplicated schema**.

**A prediction that landed.** §Corrections item 22 put the post-removal cached block at ~5,345 tokens. Measured **5,075** — **−5 %**, and **the first figure in this document's cost history to land inside 10 %.** It landed because it was computed from a measured `usage` figure rather than scaled from a ratio, which is precisely the distinction items 21, 22 and 24 were each written to record.

**What it does not license, restated because item 30 said it in advance.** The arms **cannot distinguish 100 % from 99 %** — the resolvable difference is ~2.5 points. And **acceptance is not quality**: an order that is well-formed but tactically worse passes Stage A and counts here as accepted. What is established is that the removal does not push acceptance through the gate. **Nothing here says the two prompts produce equally good orders.**

**And a margin that has now been spent.** The cached block falls to **5,075 tokens** against Haiku 4.5's **4,096** minimum: the margin was 3,512 tokens (86 %), it is now **979 (24 %)**. §Corrections item 22 concluded that item 21's cache-cliff warning was overstated — *"a page of doctrine can be deleted without consequence."* **That is no longer true.** It holds to roughly 980 tokens and fails after, and falling out of cache costs **5.7×** what this removal saved. The warning was overstated when it was written and is now approximately correct — which is what happens when the thing a margin protects gets spent.

**One byte.** The first cut of the change also deleted the blank line separating the old schema block from `DOCTRINE:`, making the shipped prefix 8,749 bytes against the 8,750 arm B measured. Invisible, harmless, and exactly the shape of every other item in §Corrections. **The newline is restored and the reason is written into the source**, so what ships is byte-identical to what was measured.

**C3 CLOSES.** Cost half measured in v1.8.5 and again here; quality half measured here; the change it proposed is made. ≈$0.32 against ≈$0.40 estimated. Running total ≈ **$2.48 of $5**.

### v1.8.13 — 2026-08-05

**A power statement, written before the run it is about.** No request has been made at the time of this revision. The fifth grant conditions C3's quality arm on exactly this ordering, and the reason is in this document's own record: **every prior arm here was designed, run, and only then assessed for whether it could have detected anything — and twice the answer was no.** The C2 doctrine arms could not (item 28(d)); the TTFT instrument could not (item 29(c)). **A power statement written after a null result is indistinguishable from an excuse**, so this one is written first.

**What is being tested.** The order schema is transmitted twice per request — once as prose in the prefix, once structurally as `output_config.format.schema`. The duplication is ~71 % of the cached prefix (item 22) and cutting it saves **12 %** per four-ship-hour, confirmed in v1.8.5. **What the prose copy is worth has never been measured.** The arm removes exactly that block, with structured outputs left on.

**What n=120 per arm can resolve, exactly.** Clopper–Pearson one-sided 95 % lower bounds on acceptance: **120/120 → ≥ 97.53 %**, **119/120 → ≥ 96.11 %**, **118/120 → 94.85 %**, 117/120 → 93.67 %. The gate is ≥ 95 %. Between arms, the resolvable difference is **≈ 2.5 percentage points**.

**So: this run can rule out a quality cost large enough to breach the gate, and it cannot distinguish 100 % from 99 %.** Stated now so that a null cannot later be reported as evidence of no difference — the error item 28(d) exists to record.

**The decision rule, fixed in advance.** ≥ 119/120 with `reject.schema` and `reject.shape` at 0.00 % → the removal is safe to make and C3's saving is available. ≤ 118/120 → not made; a 12 % saving does not buy a gate breach. **Any `schema` or `shape` rejection → not made regardless of count**, because that is the prose copy doing structural work the projection was assumed to cover, and one such rejection is a mechanism rather than a rate.

**And what the run is not.** Acceptance is not quality. A well-formed order that is tactically worse passes Stage A and counts as accepted. Acceptance is what the gate is written in and what the arms can compare; **a semantic difference would be invisible here.**

### v1.8.12 — 2026-08-05

**The instrument was built, it failed at its stated job, and it closed the question anyway.** 96 orders on `claude-sonnet-5` across two doctrine arms, under the fifth grant. §Corrections item 29.

**What was predicted.** Item 28(f): total latency cannot resolve the fixed term because its residual SD is 1,482 ms; TTFT will, because it removes generation, and generation — 57 % of the mean round trip — is where the scatter lives.

**What happened.** **TTFT correlates with total at 0.995 and 0.991.** Under structured outputs the service emits about six deltas per response, ~41–49 tokens each, and the entire per-token slope sits in TTFT while the segment after the first delta is flat. **The first delta arrives after the generation.** What the probe subtracts is a 2.2-second flush with an SD of 314 ms, not generation — so SD(TTFT) is 2,989 ms against SD(total) 2,878 ms. **Very slightly worse.**

**And the reason is a mistake worth naming.** Generation is 57 % of the mean and **SD(afterFirst) is 314 ms**. *A share of the mean is not a share of the variance.* Item 28(f) moved from one to the other in a single step, in a revision whose entire subject was the danger of reading one statistic as though it were another. **That is the third time this document has made that class of error** — item 25's r², item 28(a)'s r², and now this — which is why it is recorded as a class.

**The estimator that helps was already being recorded.** Time to **response headers**: SD 2,203 / 1,973 ms against total's 2,878 / 2,959, and a correlation with total of 0.63 / 0.78 rather than 0.995 — a genuinely different measurement rather than a shifted copy. It cost nothing; the probe timestamps it on the way past. **The useful cut point was one event earlier than the one that had a name.**

**The doctrine comparison is null on all three estimators.** Arm A minus arm B, for a prefix delta of 2,202 cached tokens: headers **−103 ms** [−940, +733], TTFT +549 [−660, +1,758], total +454 [−714, +1,621]. The best estimator resolves anything above 837 ms and puts the prefix's cost at approximately nothing — with the **short**-doctrine arm marginally slower. **That is the second time a prefix reduction has failed to move latency in the predicted direction**; v1.8.5 cut 1,617 cached tokens and moved p50 down 16 % while moving p95 up 54 %.

**And then the number that actually closes C2, which nobody was arguing about.** Time to response headers — **before a single token has been generated** — is mean **3,157 ms** and p95 **6,917 ms** on arm A, 3,260 / 7,357 on arm B. **The fixed term alone exceeds the ≤ 2.5 s total-latency target: by 26 % at the mean and 176 % at p95.** No reduction in prompt size, output length or schema can produce a 2.5 s p95 on a path whose pre-generation phase is already 6.9 s at p95. The target was set in v1.2 against no hosted measurement of any kind, and it was never achievable here.

**What does not follow.** §Success metrics' latency row is unchanged and still reads **MISSED**. Moving a target to match a measurement is what this document refused in v1.3 and it is not starting now. The remedy is a different model, a different target, or acceptance — and **the 20 s cadence already accepts it**, absorbing p99 with room, which is why no run in any phase has been degraded. **C2 closes with a cause, not a remedy.**

**Cost, with the overrun.** 109 requests ≈ **$0.84** against ≈$0.50 estimated, **+68 %**. Thirteen were instrument development, and one of those was worth the money on its own: the first smoke run produced two samples pinned at ~30,500 ms, which turned out to be .NET's `Expect100Continue` handshake rather than the service. **A probe measuring its own client stack and reporting it as latency** — caught only because 30,498 and 30,501 ms are not what variance looks like. Running total ≈**$2.16 of $5**.

### v1.8.11 — 2026-08-05

**A grant, recorded before the requests made under it, for the fifth time — and the first one that moves a boundary rather than restating it.** No request has been made under it at the time of this commit. That ordering is the whole convention: an authorization written after a result is an explanation, not a grant.

**Read the boundary change first.** Grants two, three and four each carried the same sentence — the six synthetic `LiveMain` fixtures, no real scenario state, `run-live-scenario.ps1` against `oppint_red_interceptor` **ungranted**. The v1.8.3 grant went further and argued the point explicitly: *"one that quietly upgraded a fixture grant into a scenario grant because 'it is the same fields' would be inferring authorization, which is the thing this gate exists to prevent."* **The owner has now granted it.** It is recorded as a decision. Nothing in the intervening measurements made the boundary look already-crossed, and this document does not claim otherwise.

**What the three experiments are, and what each one is not.**

**① The C2 fixed term, via time-to-first-token — ≈$0.50.** §Corrections item 28(f) established that the cheaper substitute does not work: at a residual SD of 1,482 ms the intercept's 95 % interval at n=48 is ±776 ms, a repeat two-arm comparison needs a ≈1,100 ms gap to resolve, and closing that with sample size costs ≈$10. TTFT is a **different estimator**, not a bigger sample — it removes generation from the measured quantity instead of averaging over it. **The risk is written into the grant rather than discovered afterwards: the fixed term may still sit below the noise floor even in TTFT, a null result is a real outcome, and it is not grounds for a third pair of arms.**

**② C3's quality half — ≈$0.40.** The only open item whose result changes shipped code. The cost mechanism is confirmed (−12 %); whether removing the prose schema costs order quality has never been measured, and one rejection at n=60 is not a measurement. **A condition attaches:** the arm's resolvable effect size goes into this document **before** the run. If n≈120 cannot distinguish the hypotheses, that gets said and the run does not happen — which is the same discipline ① just spent a revision establishing.

**③ C1 — the in-engine live-scenario run, authorized — ≈$0.10.** `run-live-scenario.ps1` on the hosted backend against `oppint_red_interceptor` in "Mariana Shield", with its paired commander-off control. Everything hosted in this document is currently characterised on **six synthetic fixtures**; that is stated wherever a hosted number appears and it is honest, but it is not the product. **`docs/egress.md` is revised in this same commit and before the run** — the volatile suffix's field set is unchanged and exhaustive as listed, and what changes is that the values in it are drawn from a shipped scenario rather than from a fixture someone wrote for a test.

**The streaming row in §Out of scope is not lifted.** An order is still atomic and the adapter still does not stream. The carve-out admits a streaming read inside `tests/live/` as a measurement instrument, which is the only place it could go: `n8ro::core::IHttpClient::send()` is blocking and returns a whole body with no chunk or headers-received hook, so **the product seam could not stream even if this document asked it to**. A probe that measures the fixed term is not a consumer of partial orders.

**Measurement only, for the fifth consecutive grant.** No latency remedy follows from ①. No prose-schema removal follows from ② unless ② measures that quality holds. Nothing about ③ changes a default. C8 is untouched — **no `claude.maxTokens` value is chosen for Sonnet**, and 673 remains a sample maximum from a run whose per-fixture means move by up to 71 tokens.

**Budget.** ≈$1.00 against $3.68 remaining of $5 at list. Time-boxed for the third time for the same reason: `claude-sonnet-5`'s introductory rate ends **2026-08-31**, after which every Sonnet figure in this document rises ~50 %.

### v1.8.10 — 2026-08-05

**A re-analysis that corrects this document twice, and costs nothing.** No request, no network, no key — the per-order rows from all four hosted runs were already on disk, written by `--csv` during measurements already paid for. §Corrections item 28 is the whole revision.

**Range restriction is now demonstrated rather than suspected.** v1.8.9 raised the possibility that Haiku's near-zero r² was an artifact of a flattened x-axis. You cannot widen Haiku's range after the fact, so the test runs the other way: **narrow Sonnet's known-real relationship to Haiku's width.** Restricted to 124–200 tokens, **Sonnet's r² falls 0.734 → 0.036 and its slope 13.80 → 4.20.** The closed form agrees without the banding — Sonnet's own slope and residual noise at Haiku's spread predict **r² = 0.019**, between the measured 0.004 and 0.071. A real 13.8 ms/token effect, seen through Haiku's window, is *predicted* to look exactly as absent as it looked.

**And in the same paragraph, two errors this document made itself.**

*The two Haiku arms never disagreed.* v1.8.5 read the 1.8-vs-20.8 slope split as "two arms of one system disagreeing by 5×, and that is the tell." **Their 95 % intervals overlap** — [−5.63, +9.25] against [+1.49, +40.12]. Two estimates too imprecise to conflict were reported as evidence of conflict; the 5× ratio is what dividing one noisy number by another produces.

*One row of item 27(f)'s table was fitted on a censored predictor.* Sonnet @ 512 was reported at n=48, r² 0.574, slope 10.1 — including the four truncated orders, whose `tokensOut` is **the cap, not a measurement**. Regressing latency on a value that four responses of unknown true length share inflates the fit. Accepted-only: **n=44, r² 0.499, slope 9.88.**

**The clause that is struck.** *"latency is dominated by service-side variance, not by either quantity the product controls."* The first half is supportable; the second **asserts a negative about the two quantities the experiment had no power to detect**, and is withdrawn from the live text. v1.8.5's *verdict* — INCONCLUSIVE, take no action, build a better instrument — stands and was right. **Its reason has been replaced, not its conclusion.**

**What the Haiku arms license, precisely: nothing, in either direction.** It is tempting to read (c) as "the effect was there and Haiku could not see it." Arm A's interval **includes zero and excludes 13.8**; arm B's includes both. Consistent with a Sonnet-sized effect *and* with none — uninformative, not hidden.

**The remedy is unchanged and now has a price on it.** Sonnet's residual SD is 1,482 ms, so the intercept's 95 % interval at n=48 is **±776 ms** — the size of the 3,472 → 2,788 ms drift observed between two *identical* runs. A repeat two-arm doctrine comparison needs a gap above **≈1,100 ms** to resolve, which ~2,200 cached tokens will not produce, and buying that resolution with sample size costs about **$10** against a $5 budget with $1.32 spent. **TTFT is not a bigger sample; it is a different estimator** — it removes generation from the measured quantity instead of averaging over it, and generation is where the 1,482 ms of scatter lives.

**The data is in the tree now.** The four per-order CSVs and the script that produces item 28's table are archived under `tests/live/data/`, so every figure in items 25, 27(f) and 28 is re-derivable offline. Until this revision they existed only in scratch — four paid runs' primary evidence, one sweep from gone.

**Two things this revision deliberately does not do.** It does not edit the v1.8.5 changelog entry, which is annotated instead: a changelog records what the document said at that revision, and rewriting it would delete the evidence that the error happened, which is the opposite of what §Corrections is for. And it does not touch the streaming row in §Out of scope — an order is still atomic, and the TTFT work is an instrument under `tests/live/`, not a product path.

### v1.8.9 — 2026-08-04

**The ceiling-raised re-run reported. Four results, and only two of them are the ones the run was built to get.**

48 orders on `claude-sonnet-5` at `maxTokens = 8192` under the v1.8.8 grant, paired with the 512 run: same model, same six fixtures, ceiling the only difference.

**The truncation thesis is confirmed outright.** Acceptance **48/48 (100.0 %)** against **44/48 (91.7 %)**, `reject.schema` and `reject.shape` still 0.00 %, and now no rejections of any kind. Item 24(b) inferred from reject-reason codes that "when Sonnet finishes a response, the order is well-formed every time." Removing the cut-off point removed every failure. **The 91.7 % was a configuration artifact end to end.**

**The tail is short, and v1.8.6's refusal was right by a factor of three.** Maximum output **673 tokens — 1.31× the old ceiling**, 7 of 48 above 512. Scaling Sonnet's ceiling by Haiku's 4.0× headroom would have said **~2,000**. It was refused as the same reasoning that missed by 79 % in item 24(a); the measured answer is **673**, so the refused estimate was **3× too high**. The principle held *and* the arithmetic would have hurt.

**The censored mean was nearly right, which is also worth saying.** 269.5 against **271.9** measured — **+0.9 %**. Item 25 was correct that 269.5 was a lower bound and correct that the tail could not be sized from it; the understatement turned out to be under one percent. That is reported because an honest "you could not have known" has to include the times the unknown was small. What the censoring actually cost was never the mean — it was **8.3 % of the orders**.

**The raise is nearly free, and truncation was pure waste.** Output tokens +0.9 %, cost **+0.5 %** ($5.65 → $5.67/four-ship-hour at list). Because `max_tokens` bills on *actual* output, an unused ceiling costs nothing — while at 512 the four truncated responses were **billed at 512 output tokens each and produced no order at all**.

**And now the two results the run did not go looking for.**

**The built-in control was swamped, and the noise is the more important number.** The design predicted the sub-512 population would be unchanged, because `max_tokens` is a ceiling the model cannot see. Per-fixture means moved by **−71.1, −41.4, −20.8, +13.9, +4.5, −1.3** tokens, and one fixture's maximum moved **478 → 193**. A ceiling raise can only *lengthen* a response; both material shifts are *shortenings*; so they are run-to-run sampling variance and the control is swamped rather than violated. **The consequence lands on the answer itself: 673 is a sample maximum, not a bound, and any ceiling sized from a single 48-order run is sized against noise.** Carried as **C8**.

**C2 reopens, from a run that was not about C2.** Latency-vs-output r² is **0.004 and 0.071 on the two Haiku arms** but **0.574 and 0.734 on the two Sonnet runs**. Haiku's output spans ~60–127 tokens; Sonnet's spans 124–673 — **five times the range** — and **restricted range mechanically suppresses r²**. So v1.8.5's "latency is dominated by service-side variance, not by either quantity the product controls" was concluded from two arms whose predictor barely varied. Where the predictor has room, output length explains **57–73 %** of the variance and generation is **57 %** of the mean round trip. **This does not establish that output length drives Haiku's latency** and must not be quoted as if it did; it establishes that C2's answer is **model-dependent** and that "INCONCLUSIVE" described a measurement with a flattened x-axis. The intercept also moved **3,472 → 2,788 ms** between two runs of the same configuration, so ±700 ms of it is noise at n=48 and it must not be quoted to three digits.

> **Annotation added v1.8.10 — one figure above is wrong and one argument is incomplete; the entry is left as written.** **(1)** *"0.574 … on the two Sonnet runs"* — the Sonnet @ 512 fit included the four truncated orders, whose `tokensOut` is the cap rather than a measurement. Accepted-only it is **n=44, r² 0.499, slope 9.88**. **(2)** The range-restriction argument stated here is correct and is now *demonstrated* rather than asserted (banding Sonnet to Haiku's width: r² **0.734 → 0.036**) — but it licenses **nothing** about whether the effect is present on Haiku, whose arms admit zero and a Sonnet-sized slope equally. See **§Corrections item 28**.

**No config value changes.** The default stays 512 because `claude.model` defaults to Haiku, for which 512 is 4.0× the measured worst case. What this revision adds is the number an operator switching to Sonnet was missing. **Choosing a headroom policy over a noisy sample maximum is the owner's call, not this document's. C7 closes.**

### v1.8.8 — 2026-08-04

**The fourth egress grant, recorded before the request made under it.** That ordering is the whole discipline: a grant written afterwards is a description, not an authorization.

It was **asked for** rather than inferred. The v1.8.5 grant enumerated two experiments and both had run, so a third experiment on the same fixtures, inside the same budget, against the same model is **not covered** — and "same fixtures, same budget, therefore covered" is exactly the inference §Tenet 4's gate exists to prevent. Same boundary as its three predecessors: the six synthetic `LiveMain` fixtures, no real scenario state, `run-live-scenario.ps1` against `oppint_red_interceptor` still ungranted, **C1 unchanged**.

**What it authorizes: 48 orders on `claude-sonnet-5`, the same six fixtures × 8 repeats, with `claude.maxTokens` raised to 8,192** — the bound derived in v1.8.7. **Paired with the censored run so the ceiling is the only difference**, which is what makes the comparison mean anything. A probe whose purpose is to observe a censored tail must not itself censor; and because billing is on **actual output tokens** rather than on the ceiling, an unused ceiling costs nothing, so there is no reason to probe at a value that might censor again.

**The design carries its own control.** `max_tokens` is an **enforced ceiling the model is not aware of** — it does not shorten what the model intends to write, it cuts what the model does write. So raising it should leave the 44 already-completed responses' lengths **unchanged**. If they shift materially, the run has found something §Corrections item 25 does not predict, and the report will say so rather than absorbing it.

**≈ $0.38 against $4.06 remaining**, time-boxed by the 2026-08-31 introductory rate. **Measurement only.** No config value is chosen in advance of the data and the default stays 512 until the run reports — the only thing v1.8.7 already fixed is the *bound*, and it came from Stage A's body cap rather than from any output-length figure.

### v1.8.7 — 2026-08-04

**The C7 config-surface revision. It changes the shape of `claude.maxTokens` and, on purpose, not its value.**

A config field is a contract, so it is revised here before any code touches it. What made that rule pay this time is that specifying the change required reading the surrounding code, and the reading found something neither v1.8.5 nor v1.8.6 knew.

**There are three response-size ceilings and none of them knows about the other two** (§Corrections item 26). `claude.maxTokens` is in **tokens**, is configurable, defaults to 512, and is validated `>= 1` **and nothing else**. Stage A's `kMaxResponseBodyBytes` is **64 KiB** and rejects an over-long body as `range` before the parse. `OrderRecorder`'s `kMaxRecordedBodyBytes` is **4 KiB** and truncates the recorded body **silently** — `sanitizeText` stops at the limit and writes no marker, so a truncated `rawBody` is indistinguishable from a short one.

**The lowest ceiling is the silent one, and it is the diagnostic.** At this project's own measured byte/token ratios — 17,756 bytes is 7,608 Haiku tokens and 10,493 Sonnet tokens, so 2.33 and 1.69 B/token — 4 KiB is roughly **1,000–2,400 tokens**. A raise of only 2–5× starts truncating `rawBody`, while the 64 KiB Stage-A cap (~16,000–39,000 tokens) never comes near. **The ceilings are ordered backwards for the purpose:** the first thing a raise would break is the record of exactly the long responses that motivated it.

**And that is observed rather than projected.** The single `rawBody` in this tree's order log is **exactly 4,096 bytes**, cut mid-way through a token array, on an `order.rejected` record. The cap has already truncated a diagnostic record here — on the one class of record that exists to be diagnosed — and nothing in the record says so.

**So the field gains a bound, derived rather than picked: `1 … 8192`.** 65,536 ÷ a deliberately generous 8 bytes/token guarantees a response *at* the configured ceiling cannot be rejected `range` for length; 8,192 also sits inside the ~16,000-token band a **non-streaming** request completes in, which matters because AIC-BE-2 has no streaming path and adding one is out of scope. The API's own limits — 64K output on Haiku 4.5, 128K on Sonnet 5 — are an order of magnitude above, so **the binding constraint is this product's, not the API's**. The field's documentation also becomes model-conditional, following the precedent `claude.effort` already set one row below it, and §Optimization approach's "a small cap bounds the worst case" is corrected: for Sonnet the cap does not bound the worst case, it **produces** one.

**The refusal, which is the point of the revision: the default stays 512.** A bound is not a value. 512 is measured-correct for the default model at 4.0× headroom, and it remains un-derivable for any other model because the run that exposed the problem is censored exactly where the answer lives (§Corrections item 25). **C7's config-surface half closes here. Its value half stays open, unguessed, and blocked on a grant that has not been asked for.**

### v1.8.6 — 2026-08-04

**No new measurement. The v1.8.5 runs' per-order rows were already on disk, and reading them properly settled one thing and closed off another.**

This revision exists because v1.8.5 shipped a conclusion — "`claude.maxTokens` is Haiku-shaped" — that was argued from *reject-reason codes* and a *mean*. Both were correct and neither was the evidence. The `--csv` rows committed in the same branch carry the evidence, and they also carry the reason the follow-up question cannot be answered from them.

**The truncation is exact, not approximate.** All **4** rejections have `tokensOut` of **exactly 512**; **0 of the 44** accepted responses do. The cap partitions the run without a single exception, which is a stronger statement than "the reject reasons look like truncation." The largest **completed** response is **502 tokens — 10 tokens of headroom** — and 3 of 44 completions land within 30 tokens of the ceiling.

**Truncation is concentrated, not diffuse.** Two of six fixtures produce every truncation: `winchester, one track close` at 3/8 and `no tracks at all` at 1/8; the other four never exceed 478, and four of those never exceed 396. Sonnet's verbosity is **situation-dependent**, so a single scalar ceiling is being asked to cover distributions that do not overlap.

**"Haiku-shaped" now has a number.** Across the 120 Haiku orders in both C2 arms, the largest output is **127 tokens** against the same 512 ceiling — **4.0× headroom, nothing at cap**. Sonnet's largest completed output leaves **1.02×**.

**And the part that matters more than any of it: the data is censored exactly where the open question lives.** The 4 truncated responses were counted at 512, but the model was *cut off* — their true lengths are **≥ 512 and were never observed**. So **269.5 is a lower bound on the mean, not the mean**; completed-only is **247.4**; the true value is above both. v1.8.5 said the right `maxTokens` was unknown "because nobody has measured where Sonnet's output length distributes." That is now half-wrong in a way worth stating precisely: the distribution **is** measured up to 512, **8.3 % of the mass lies above it**, and the part above the cap is exactly the part a ceiling has to be sized against.

**The obvious way to close the gap is the error this document paid for one item earlier.** Scaling Sonnet's ceiling by Haiku's 4.0× headroom gives ~2,000 — the same "scale one model's measurement by a ratio, hold the shape fixed" move that missed by **79 %** in item 24(a), two paragraphs above where it would be written. The tail is measured by re-running with the ceiling raised and by nothing else. Carried as **C7**. **No config field was touched and no value was chosen.**

### v1.8.5 — 2026-08-04

**Three diagnostic runs. Two negative results and one refuted assumption — no fixes.** The grant that authorized them (§Phase 2, v1.8.5) was explicitly for *measurement only*, because Phase 2 closed with two failures and the temptation was to start repairing them before knowing what they were.

**The C2 latency decomposition did not decompose anything, and that is the result.** Two arms of 60 orders differing only in doctrine size were meant to separate "the prefix is large" from "the output is long."

| | Arm A (full doctrine) | Arm B (short doctrine) |
|---|---|---|
| Cached prefix | 7,608 tok | 5,991 tok |
| p50 / p95 | 2,523 / **3,674** ms | 2,113 / **5,657** ms |
| r², latency vs output tokens | **0.004** | **0.071** |

Output length explains **essentially none** of the latency variance. The regression still emits an intercept and a slope — 2,454 ms + 1.8 ms/token for A, 466 ms + 20.8 ms/token for B — and those numbers are **noise wearing the shape of an answer**; two arms of the same system disagreeing by 5× is what that looks like. Shrinking the prefix moved p50 down 16 % and p95 **up** 54 %, which a genuine fixed-cost effect would not do. And the *same configuration* gave p95 3,674 ms at n = 60 versus 4,615 ms at n = 240 — **26 % apart**, so part of what §Success metrics reports as a fixed miss is sampling.

What that establishes is that latency is dominated by service-side variance rather than by anything the product controls at this sample size. What it does **not** establish is that the prefix is irrelevant — only that its effect is smaller than the noise floor of the instrument. **The next step is a better instrument, not a remedy:** time-to-first-token separated from generation time would put the prefix term somewhere it can be seen. Choosing a fix on these numbers would mean picking whichever arm flattered a story, which is most of what §Corrections is a record of.

> **Annotation added v1.8.10 — this entry is left as written, and two of its claims are withdrawn elsewhere.** A changelog records what the document said at a revision, so the text above is not edited. **(1)** *"two arms of the same system disagreeing by 5×"* — their slope confidence intervals **overlap** ([−5.63, +9.25] and [+1.49, +40.12]); they were two estimates too imprecise to conflict. **(2)** *"dominated by service-side variance rather than by anything the product controls"* — the second half is an assertion of a negative from arms with no power to detect it, and is **struck** from the live text in §Phase 2 carried items. The verdict this entry reached — INCONCLUSIVE, no action, build a better instrument — is unchanged and was correct. See **§Corrections item 28**.

**C3's cost half is confirmed; its quality half is why the change is still unmade.** The smaller prefix cost **$0.92 per four-ship-hour against $1.05** — a 12 % saving from 1,617 fewer cached tokens, confirming the mechanism §Corrections item 22 identified. That arm also accepted **59/60** where full doctrine accepted 60/60. One rejection at n = 60 is not significant and is *exactly* the quality cost C3 says must be measured before the prose schema copy is deleted. It remains deleted-in-theory only.

**§Corrections item 24 — the first non-Haiku run invalidated two things.**

*Tokenization is model-specific.* The identical 17,756-byte prefix is **7,608 tokens on Haiku and 10,493 on Sonnet — +37.9 %**. §Cost model's non-Haiku rows scaled Haiku's token counts by each model's published *rate* while holding the counts fixed, and v1.8.4 flagged that as an untested assumption costing "one request" to remove. It was one request. The assumption was false: measured Sonnet is **$0.00784/order against the $0.00439 predicted, +79 %**, or **$5.65 per four-ship-hour**. The Opus row was produced the same way and is now a floor, not a figure.

*And `claude.maxTokens = 512` is sized for Haiku.* Sonnet accepted **44/48 (91.7 %)**, under the ≥ 95 % gate — but `reject.schema` and `reject.shape` are both **0.00 %**, and all four failures read `parse: body is not well-formed JSON` or `content array carries no text block`. That is a **truncation** signature, not a bad-order one. Sonnet's mean output is **269.5 tokens** against a ceiling chosen when the only measured model wrote ~105, so its longer responses are cut off mid-JSON. **When Sonnet finishes a response, the order is well-formed every time.**

The conclusion to resist is *"Sonnet is worse."* What is measured is that a configuration value is Haiku-shaped. Raising it is the candidate fix and is **not made here** — it is a config-surface change needing a PRD revision, and the right value is unknown because nobody has measured where Sonnet's output length distributes. Recorded explicitly so that 91.7 % is never quoted as an order-quality result, which is exactly what it resembles in a table.

### v1.8.4 — 2026-08-04

**Phase 2 closes with results on every gate item, including the two it failed.**

| Gate item | Target | Measured | |
|---|---|---|---|
| Order acceptance | ≥ 95 % | **100 %** (240/240) | ✅ |
| `reject.schema` | < 1 % | **0.00 %** | ✅ |
| `reject.shape` vs local 0/12 | reported | **0.00 %** over 240 | ✅ |
| Cost per four-ship-hour | ≤ $1.10 | **$1.05** | ✅ |
| Cost/order vs §Cost model | ±20 % | **+39.4 %** | ❌ |
| Round-trip p95 | ≤ 2.5 s | **4,615 ms** | ❌ |

**The two failures are different in kind and the distinction matters.** The p95 miss is a *product* result: the hosted path is slower than a target set in v1.2 against no hosted measurement at all. The ±20 % miss is a *document* result: the product costs what it costs, and this PRD's model of that cost was wrong by 39 %. Netting them into "cost is fine, latency isn't" would lose the second, which is the more instructive one.

**§Corrections item 19 is confirmed, on both halves, and the projection is load-bearing.** Three requests, each differing from the last in one respect. The canonical document: `400 — "Schema type 'oneOf' is not supported"`. A hybrid identical to it but for `oneOf`→`anyOf`, all bounds retained: `400 — "For 'integer' type, properties maximum, minimum are not supported"`. The shipped projection: `200`. The hybrid is what makes this a full confirmation instead of half of one — the canonical fails on `oneOf` before any bound is evaluated, and the projection changes both things at once, so only a document differing in exactly one respect can separate them.

A methodological correction rides along, because this loop made the error before catching it: the first attempt tested item 19 by *driving the adapter*, which sends the projection — so the keywords the prediction is about were never in the request, and a `200` was printed as **"PREDICTION REFUTED"**. **When a mitigation is already deployed, exercising the mitigated path measures the mitigation, not the hazard.**

**§Corrections item 22 — the schema is sent twice, and OQ-8 counted one copy.** `usage` on the first real order reported `cache_creation_input_tokens: 7608` where OQ-8 had measured the prefix at **4,489**. The 3,119-token gap is the order schema, transmitted a second time in `output_config.format.schema`; `PromptRenderer` also renders it as prose into the prefix. Roughly **71 % of the cached prefix is the schema in two renderings**, and the cache read is **52 % of the per-order bill**. This is why §Cost model missed its own tolerance — and it is the highest-value cost lever the project has found: dropping the prose copy would give ~$0.89/four-ship-hour with the prefix still cache-eligible. Not done here; prefix optimization is out of scope and the prose copy has never been measured against its absence.

It also **withdraws a warning v1.8.2 made two revisions ago**. That revision reported a 9.6 % margin over the cache minimum and called AIC-BE-3's startup check the only guard on a 4.8× cost cliff a page of doctrine away. Against the block that actually caches the margin is **86 %**. The check is still the right guard; the alarm attached to it was wrong.

**The general form, now three items long.** `count_tokens` on a constructed string measures the string. Only `usage` on a real request measures the request. Item 21 was a number traced correctly to a superseded source; item 22 is a number traced correctly to *almost* the right object. Both read exactly like measurements, and §Cost model's assumptions table now carries a status column precisely so the next one is visible without re-deriving the chain.

**§Corrections item 23 — `input_tokens` excludes cached tokens, it does not contain them.** The harness's cost function subtracted reads from `input_tokens` to avoid double-counting and produced **`cost per order $-0.00614`**. A negative price is caught immediately; the instructive part is that had the two populations been similar in size rather than differing by 40×, the same wrong formula would have produced a plausible number and shipped. The information needed to prevent it was in the first response received.

**The memorised-coordinate comparison ran, and the negative result is the less interesting half.** Perth does **not** reproduce — 0 of 11 waypoint-carrying orders within 50 km — which over 11 orders bounds the rate rather than establishing zero. But two orders, both on `winchester, no threat (rtb)` and on consecutive repeats, placed the waypoint at longitude **exactly 140.000** with own-ship at 145.000: **540 km outside** the geofence. The local 7B substituted a memorised city; Haiku 4.5 substituted a round number. **Different wrong value, identical failure mode** — well-formed, in range, unconstrainable by schema or decoder, caught only by Stage-B `geofence`. That generalises the Phase 1b finding rather than refuting it, and it is a stronger argument for Stage B than the original.

**What Phase 2 does not claim** is in §Carried out of Phase 2: six items, each with a reason and a destination. The first is that **the in-engine live-scenario run was not performed** — the v1.8.3 grant covers the synthetic fixtures and not real scenario state, and the run was skipped rather than the grant read generously.

### v1.8.3 — 2026-08-04

**The second grant, recorded before the first request made under it.** v1.8.1's authorization was prefix-only; it did its job — OQ-8 is resolved — and it expired with the question. The owner has now granted the **full Phase 2 measurement gate**, and unlike the first grant this one is **snapshot-carrying**. §Phase 2 enumerates what that means rather than summarising it: position, velocity, heading, team, reported tracks, and loadout leave the machine on every request in the soak, the p95 sample, and the memorised-coordinate comparison.

**The grant has a boundary, and naming it is most of the value of writing this down.** It covers the six synthetic situations hand-authored in `tests/live/LiveMain.cpp`. It does **not** cover `run-live-scenario.ps1` against `oppint_red_interceptor`, which transmits real state from a deployed mission file. The two carry the *same field set*, which is precisely why the distinction had to be drawn explicitly — "it is the same fields either way" is the argument that would silently convert a fixture grant into a scenario grant, and inferring authorization is the thing §Tenet 4's gate exists to prevent. The in-engine live-scenario run is therefore **ungranted and carried out of Phase 2** with a destination, not marked green by omission.

**And a scoping fact the Phase 2 gate was written without.** `tests/live/LiveMain.cpp` constructs `LocalLlmClient` directly — the harness has **no Claude path at all**, so `--mode soak|cold|h2|geo` measure Ollama and nothing else. Every gated measurement in this phase needs a backend selector wired in first. `CommanderRuntime::runWorkerCall` already takes `ILlmClient&`, so the change is contained rather than structural, but the gate's implicit assumption that the measurements were one command away was wrong, and it was wrong in the direction that hides work.

### v1.8.2 — 2026-08-04

**OQ-8 is resolved, and the answer is no.** The deployed stable prefix measures **4,489 input tokens** against Anthropic's tokenizer — `POST /v1/messages/count_tokens`, `claude-haiku-4-5`, on the 17,756 bytes dumped from the deployed `PromptRenderer`, giving **3.955 bytes/token**. That **clears** Haiku 4.5's 4,096-token prompt-cache minimum by **393 tokens**. The prefix caches as written, **the padding delta is zero**, and the judgement this question has reserved for the owner since v1.2 — whether ~2,900 tokens of *genuine* doctrine were worth writing to earn the discount — **never has to be made.**

It is worth being precise about how that happened, because nobody decided it. The prefix crossed the threshold as a **side effect of a correctness fix**: v1.7.1's four-branch schema partition repeats shared field descriptions per branch, which grew the prefix from 4,738 to 14,074 bytes, and §Corrections item 16's doctrine-loading fix supplied the rest. Both were recorded at the time as *costs*. Neither was made for cost reasons, and the fact that they add up to a cache-eligible prefix is a coincidence this document should not dress up as foresight.

**Reconciling §Cost model to the measurement is where the real finding is.** Every uncached figure in that section from v1.3 through v1.8.1 was computed against a **1,200-token** prefix. The Haiku row read $0.00180/order and $1.30/four-ship-hour; the deployed prefix costs **$0.00509 and $3.66** — **2.8× higher**, in the row the default configuration uses.

What makes this §Corrections item 21 rather than a silent table update is that **the error survived a correction aimed directly at it.** Item 16 established in v1.7.1 that the 4,738-byte measurement was taken with no doctrine loaded and that the deployed prefix was 17,756 bytes. §Cost model — the one section whose entire output is arithmetic over that number — was not recomputed. The fact was corrected in the section that discovered it and left standing in the section that consumed it. §Corrections' own v1.3 note warns that *"a number with no units-and-source trace behind it reads exactly like one that has been measured"*; the sharper version this revision adds is that **a number with a correct trace to a superseded source reads exactly the same way.** §Cost model's inputs now carry a measured/assumed status column, so the next superseded input is visible without re-deriving the chain — and two inputs are marked **assumed** today, the ~200-token suffix and the ~80-token output, both carried since v1.2 and neither ever measured.

**The target is met, and the margin under it is thinner than the headline.** At **$0.76 per four-ship-hour cached** against §Success metrics' ≤ $1.10, Phase 2's cost target is met with 31 % headroom. The uncached figure is **$3.66**, which misses by 3.3×, and the two regimes differ by **4.8×**. Nothing in the product tells an operator which one they are in — and the prefix clears the minimum by only **9.6 %**, so deleting roughly **1,554 bytes** of `data/doctrine.txt` silently moves them from the first regime to the second with no error, no warning at the API, and no counter.

That inverts the purpose of AIC-BE-3's startup prefix/cache-minimum comparison. It was written as a standing reminder of a condition known to be unmet, expected to fire on every Haiku run until OQ-8 was decided. It is now expected **never** to fire — and it is the **only** detector of a silent 4.8× cost regression. Its criterion is rewritten to say so, to require a real token count rather than a byte-ratio estimate (3.955 bytes/token is a property of this corpus, not a constant), and to log the **margin** rather than a bare verdict, because *"393 tokens over the minimum"* tells someone editing doctrine how much room they have and *"clears the minimum"* tells them nothing until the day it stops being true.

**A budget figure that was wrong by 20× for seven revisions.** §Cost model said **$100 in held credit**; the balance is **$5**. It went unnoticed because "≈ 77 four-ship hours" is comfortable at either number, and nobody audits a figure that is not binding on any decision. It is binding now: at the measured cached rate the balance funds **6.6 four-ship-hours**, of which the Phase 2 gate at ~240 orders consumes about **$0.25**.

### v1.8.1 — 2026-08-03

**The authorization, recorded and scoped.** The platform owner granted hosted egress for a **prefix-only token count** and nothing else. The scoping is the substance of the entry, not a caveat on it: what the probe transmits is the system prompt, the posture and ROE vocabulary, the order schema, and `data/doctrine.txt` — the stable prefix, which by construction contains no volatile suffix and therefore no position, velocity, heading, team, track, or loadout. The 200-order soak, the live-scenario run, and the memorised-coordinate comparison all carry snapshots and all remain gated behind a separate grant.

That granularity is deliberate. §Tenet 4 asks for a positive act before **scenario data** egresses, not merely before an API key is first exercised, so a record that read "hosted backend authorized" would have been both true and useless — it would have licensed the runs the owner had not agreed to. The record in §Phase 2 names what may go and what may not.

**And a prediction this document made, refuted by the first request it authorized.** v1.8 framed the OQ-8 measurement as effectively free: `count_tokens` is a utility endpoint rather than an inference call, so an unfunded key should still reach it. The request was sent. It returned **HTTP 400 — *"Your credit balance is too low to access the Anthropic API."*** The endpoint may be unbilled; the gateway rejects on balance before that is consulted. *"Costs nothing"* and *"works on an empty account"* are separate claims and only the first held.

The correction is small in consequence and worth recording anyway, because the shape of the error is one this project keeps making: an assumption that was plausible, unmeasured, and load-bearing for a plan. Measuring it took one request. §Corrections item 20 carries it, along with the second-order finding that the probe script reported that 400 with an *empty message* — `$_.ErrorDetails.Message` is routinely blank on PowerShell 5.1 — which is the third time this project has been misled by a diagnostic channel that fails silently rather than loudly (§Corrections 16's `readDoctrine`, v1.7.5's block-buffered stdout, now this).

What the failed request did establish: the deployed prefix is **17,756 bytes** dumped from the real `PromptRenderer` with the real doctrine, it carries **no non-ASCII characters**, and the request reached the API and was refused on billing rather than on form. Shape, encoding, and credential path are all proven; the measurement is one command behind a funded account.

### v1.8 — 2026-08-03

**AIC-BE-2 reconciled against what a hosted adapter needs, before one was written.** The FR was drafted in v1.0 from the published API shape and had never been held against an implementation. Three things it could not have anticipated:

**1. Where the Claude envelope unwrap lives — resolved on the seam Phase 1b already built.** Phase 1b answered this question for Ollama by passing an `EnvelopeFormat` value into Stage A, precisely so that Stage A would stay a pure function of its arguments and never hold a client. The same seam carries Claude: a third enumerator, a sibling unwrap function beside `unwrapOllamaGenerate`, and one branch in check A2. `RejectReason::Refusal` — declared since v1.2, produced by nothing, and appearing in `toString()` and nowhere else — becomes the `stop_reason == "refusal"` outcome. The guard is on `stop_reason` and explicitly not on `stop_details`, which may be null on a refusal; branching on the latter would read `content` on exactly the responses that must not be read.

**2. The one-definition rule does not survive contact with the hosted backend as written — so the rule changed, not the document.** §Corrections item 19 records the finding: the hosted structured-output path does not accept `minimum`/`maximum`/`minLength`/`maxLength`, and §Corrections item 15's four-branch encoding is built out of exactly those — `targetEntityId` forced empty by a zero-length bound, `orbitRadiusM` forced to zero by a zero-width numeric range. Strip them and the mechanism that took `reject.shape` from 10/12 to 0/12 is gone.

The tempting fix was to re-express those pins as `const` in the canonical document, since `const` is supported on both paths. **It was rejected, and the reason is the lesson of v1.7.1 read in the other direction:** the canonical document measured 0/12 against the deployed local backend, and the hosted problem is *predicted from documentation, not measured*. Editing a measured-good artifact on the strength of an unmeasured story is how §Corrections entries get written. Instead AIC-BE-1's rule is restated — an adapter may send a **mechanical projection** of the canonical object, computed beside it, and still may not send a hand-authored variant. A projection cannot drift, because it is derived on every build; a copy drifts the first time someone edits one of the two. The projection is testable with no network: no unsupported keyword survives, every pin survives as a `const`, and the adversarial corpus resolves identically against both documents.

**3. The prompt cache needs a boundary the worker could not see.** `PromptRenderer` has held `prefix()` and `renderSuffix()` separately since Phase 0, but what crosses to the worker is one joined string — so an adapter had no way to place a cache breakpoint at the prefix/suffix boundary. AIC-BE-3 now requires that offset to travel with the request, as a length rather than a second string, so the prompt stays the single record of what was sent and `promptHash` is unaffected. This is not a detail: a breakpoint at the end of the whole prompt writes a fresh cache entry every request and reads none, which pays the write premium to receive nothing, and every cached figure in §Cost model assumes otherwise.

**And one correction to this document's own framing of the work.** OQ-8's remaining half was described as a measurement already substantially done, with only an owner judgement left. Both halves moved: the 4,738-byte figure was taken doctrine-less (§Corrections item 16) and the deployed prefix is 17,756 bytes; and the replacement measurement **is itself a live hosted request**, because Anthropic's tokenizer is reachable only through `POST /v1/messages/count_tokens`. There is no offline Claude tokenizer, and substituting another vendor's would reproduce the exact byte-ratio error OQ-8 already refuses. So the Phase 2 gate is partitioned by what authorization each item needs — four items are reachable with no network at all, and everything else waits on the owner. That partition is not bookkeeping: it is what lets the adapter, its failure paths, the allowlist assertion, and the schema projection all be proven before anyone is asked to authorize egress.

### v1.7.6 — 2026-08-03

Phase 1b closes.

- **The live-scenario smoke passes: 17 checks, 0 failed**, against the gate as re-specified in
  v1.7.5. Both items v1.7.1 recorded as unreachable have now run, and every Phase 1b gate item has
  a result. The failing runs and their causes stay in the record above; nothing was deleted to make
  this line true.
- **§Carried out of Phase 1b** enumerates the five things the phase does **not** claim, each with a
  reason it is not a Phase 1b defect and a place it goes. A phase that closes with an empty
  carried-forward list is usually one that stopped looking.
- **The memorised-coordinate substitution reproduces**, and that upgrades it from an anecdote to a
  characterised failure mode. A third run drew `−31.952247, 115.857309` on `RedSu35_02` — Perth
  again, agreeing with the first observation to four decimal places, same `posture: hold`, same
  `reason` string — against a coordinate that appears nowhere in the prompt. **This is the phase's
  most useful finding.** It is a well-formed, in-range, entirely wrong number that no schema,
  decoder, or prompt wording catches, and the Stage-B envelope catches it every time. It also turns
  "does a frontier model do this too?" into a measurable Phase 2 question with a concrete benchmark.

### v1.7.5 — 2026-08-03

Three items. The first is this document withdrawing its own explanation from one revision earlier.

- **v1.7.4's account of the `geofence` failure is REFUTED, by measurement.** It proposed that the
  doctrine says *"egress toward the home field"* while carrying no coordinates, so the model
  invents one. A `geo` probe added to the live harness ran nine situations against the live model
  through the shipping prompt — **three of them drawing `rtb`, the exact posture the hypothesis was
  about — and all ten waypoint-carrying orders put the waypoint on own-ship position at 0 m.** None
  outside the fence. The predicted failure mode does not occur.
- **AIC-DET-1's `rawBody` is delivered on Stage-B rejections, and it immediately produced the real
  answer.** It had been an empty string, so a `geofence` record carried the distance and never the
  waypoint — which is precisely why v1.7.4 had to guess. The candidate carries the body through
  Stage A and Stage B alike; the plugin was discarding it at the last step. Same truncation and
  charset filtering Stage-A rejections already had, so no new exposure.
- **The cause, once it could be read rather than inferred: `posture: hold` with
  `waypoint: −31.952876, 115.860450` — Perth, Western Australia**, against an own-ship position
  near Guam. The model substituted a memorised real-world coordinate for a waypoint whose correct
  value was own-ship position — the one case the offline probe got right ten times out of ten. It
  is a **low-rate hallucination of a well-formed, in-range, entirely wrong number**: not a doctrine
  gap, not schema-constrainable (every field was legal), and not something prompt wording reliably
  prevents. The geofence was the only thing between that order and an aircraft flying to Australia.
- **A second, independent lapse in the same run makes the point sharper.** `cruiseSpeedMps: 600`
  against a 400 m/s bound — with a waypoint of `13.484045, 144.991216`, correct and adjacent to
  own-ship. Geography right, kinematics wrong; and in the other order, the reverse. Two independent
  low-rate lapses rather than one systematic misunderstanding, which is why the answer is a
  validator and not a rewrite.
- **The lesson is the one §Corrections item 16 already recorded:** a channel that fails silently
  costs more than whatever it was hiding. Here it cost a wrong published explanation.
- **The live-smoke gate is re-specified against what the OQ-6 scenario can supply.** It now asserts
  over the **commanded window** rather than a wall-clock 10 minutes, because the Red flight is
  destroyed at ~85 s and no configuration makes it live longer. *"Entity completes the scenario"*
  is **withdrawn**: the commanded entity is opposed by a Blue CAP, and whether it survives is the
  scenario's outcome rather than the commander's correctness — asserting it would fail the gate
  whenever the opposition wins. Acceptance is **reported, not barred**, here.
- **On that last point, stated plainly because it deserves scrutiny:** this is not a bar lowered to
  turn a red run green. The failing run stays in §Phase 1b with its numbers and its cause. It is
  the acceptance measurement being assigned to the instrument with the sample size for it — ~10
  orders cannot distinguish a real regression from three unlucky draws, and **the ≥ 95 % bar
  remains unchanged on the 200-order soak**, where it has always lived. What the live smoke
  uniquely proves is that the pipeline works inside the engine, and that needs no rate.

### v1.7.4 — 2026-08-03

Written from the two gate runs v1.7.1 could not perform. The interesting content is a failure.

- **The in-engine live smoke ran, and failed one assertion: acceptance 50 % against ≥ 90 %.**
  Recorded as a failure with its cause, not restated into something that passes — the same
  treatment v1.7.1 gave the items it could not run at all.
- **All three rejections were the Stage-B safety envelope working**: two `geofence` on waypoints
  ~5,300 km away, one `clamp` on a 600 m/s cruise speed against a 400 m/s bound. Two independent
  orders seconds apart proposing the same wrong distance is systematic. The likely mechanism —
  labelled a hypothesis, because the log does not carry the coordinates to prove it — is that the
  doctrine says *"egress toward the home field"* and, by design, carries no coordinates, while the
  volatile suffix supplies no reference geography beyond own-ship position. Asked to go home with
  no home given, the model invents one.
- **Explicitly NOT resolved by widening `safety.geofenceRadiusM`.** Moving a bound to make a
  measurement pass erases the signal. The bound is not what is wrong.
- **The gate item's "10-minute run" does not fit the OQ-6 scenario.** Both commanded entities are
  destroyed at ~85 s by the Blue package, so the run commanded nothing for its remaining ~515 s and
  the whole measurement rests on 10 requests. That is a defect in how the gate item was phrased,
  recorded as such.
- **No p95 claim is made from this run.** The 10,363 ms figure is the second-highest of five
  samples. In-engine latency clearly exceeds the offline harness's 2,163 ms and there is a
  mechanism for it — `maxConcurrentRequests` is 1 and two entities share it — but five samples do
  not measure a percentile and this document will not pretend otherwise.
- **`reject.shape` held at 0 % against situations nobody authored.** The 200-order soak's 0.00 %
  was against six hand-written cases; this is the first evidence from outside that set. Small n,
  and said to be small.
- **Frame cost measured over 12,001 frames: p50 0.0018 ms, max 0.262 ms** against the 5 ms bar.
  This is the one number in the run with a large sample behind it.
- **Two gaps recorded rather than fixed.** B8 was never exercised — no order carried a target, so
  `engage`/`crank` never occurred and its evidence remains its unit tests. And **Stage-B rejections
  write an empty `rawBody`**, so a `geofence` rejection records how far the waypoint was but not
  where; AIC-DET-1 promises the raw body and Stage B does not deliver it, because by then the body
  has been parsed into an `Order`.

### v1.7.3 — 2026-08-03

One decision, taken because the alternatives were to accept a demo defect or to keep writing
doctrine the evidence says will not move it.

- **Stage-B check B8 and reject reason `loadout` added to AIC-VAL-1.** `engage`/`crank` is rejected
  when the Tier-1-reported loadout for the order's snapshot window is **entirely dry**. The Phase 1b
  gate's one standing order-quality miss — a winchester aircraft drawing `engage` where doctrine
  says `rtb` — becomes a counted rejection with a runbook row instead of an unmeasurable complaint.
- **Why the validator rather than more doctrine.** Two focused doctrine iterations moved the rate
  not at all. §Rabbit holes names that as the timebox signal. Restating the same instruction louder
  is not evidence-led, and the alternative it points to — reconsidering the RAG deferral — is a far
  larger commitment than the miss justifies.
- **Why a new reason rather than widening B3.** `reject.track` climbing and `reject.loadout`
  climbing have different diagnoses and different responses. `RejectReason.h` states that rationale
  in its own header comment; folding one into the other would destroy the information the
  fine-grained reason set exists to carry.
- **"Entirely dry" is defined so that absence is not evidence.** An empty reported loadout — no
  hardpoints at all — does **not** trigger B8. That case means the Tier-1 script does not report
  stores, which §Validation requires to keep receiving orders, and B3 already rejects its targeted
  orders because a script reporting no loadout reports no tracks either. This distinction is the
  part a naive reading of "reject when the loadout is empty" gets wrong, and it carries its own
  test for that reason.
- **The window rule matches B3's.** B8 validates the loadout that accompanied the order's
  *snapshot*, not the one current at publication. An aircraft that fired its last missile while
  inference was in flight must not retroactively invalidate an order that was correct when it was
  requested.
- **No FR surface is widened.** No Lua function, posture, order field, config field, or backend.

### v1.7.2 — 2026-08-03

Written at the Phase 1b close-out, before the code it authorizes. One decision, taken because the
alternative was to leave two gate items permanently unreachable and call that a property.

- **AIC-API-2 gains a second configuration source: `data/config/plugins/ai-commander.cfg`, read at
  `initialize()` as the deployed default.** Precedence is defined — an explicit `applyConfigFields()`
  wins, in either call order — and the file is applied through the *same* `tryParseConfigFields`
  all-or-nothing path, so there is one commit path rather than two. **No config field is added.**
  The FR's field table is unchanged; only how those fields arrive is widened.
- **§Corrections item 17 resolved**, and the reasoning recorded rather than just the outcome. The
  headless host's inability to apply per-plugin config was not a safety property — it was an
  asymmetry. The UI host has applied that exact file since Phase 0, and the shipped example config
  has told operators to put it there since Phase 0, so a stale file could **already** enable the
  commander on the interactive path. What v1.7.2 changes is that both hosts now behave the same way,
  and that the behaviour is logged.
- **Fail-closed is unchanged and now carries a test rather than an argument.** Absent file →
  compiled defaults → `commander.enabled == false`. A new acceptance criterion says so, and it is
  asserted by the suite rather than by inspection of the default struct.
- **The residual exposure is stated, not waved off.** A stale `ai-commander.cfg` in a release tree
  now takes effect on headless runs too. Two things bound it: the §Operational readiness deployment
  checklist, which gains an explicit row for that file, and a mandatory startup log line naming the
  file read and the field count — so an unexpected configuration is visible in the first ten lines
  of a log instead of inferred from behaviour. That line exists because §Corrections item 16 is this
  document's own record of what an unlogged, silently-unresolved path costs: it hid for two phases.
- **A missing file logs at INFO, not WARNING.** Absent-and-disabled is the state the deployment
  checklist *requires* of a default deploy. Warning on the correct state teaches operators to ignore
  warnings, which is how the next real warning gets missed.

### v1.7.1 — 2026-08-02

Written during Phase 1b implementation and at its gate, from live runs of the shipping adapter. v1.7
was right about the mechanism and wrong about the partition; three further findings came from
running the deployed artifact rather than the harness.

- **Phase 1b gate results recorded**, item by item, including the two that did not pass. Acceptance
  **100.0 %** over 200 orders, `reject.schema` **0.00 %**, `reject.shape` **0.00 %**, p95 **2,163 ms**,
  first-order-of-a-run **4,566 ms accepted** from a force-evicted model, unit suite **87/87** and
  **87/87 under ASan**, deployed smoke **25/25**. The live in-engine smoke and the H1 assessment are
  recorded as **not satisfied**, with the reason, rather than restated into something that passes.
- **H2 measured and NOT SUPPORTED.** Stable p95 2,291 ms vs perturbed 2,362 ms — a **3.1 %**
  difference against a predicted ≥ 30 %. On a GPU, prompt evaluation is a small share of a round trip
  spent mostly generating. The hypothesis's own "if wrong" clause governs: prefix discipline is kept
  because it costs nothing and because the hosted cache discount is a separate and larger prize.
- **§Corrections item 16: the doctrine block was never deployed.** `prompt.doctrinePath` resolves
  against the host's working directory and nothing put the file there, so every deployed run since
  Phase 0 rendered `"(none provided)"` — silently, with no counter moving and only order quality
  degrading. That includes the run behind item 9's `prefixBytes = 4,738`, which was therefore a
  doctrine-less prefix. Fixed with a startup warning naming the resolved path and a seed-if-absent
  deploy step.
- **§Corrections item 17: the headless host does not apply per-plugin configuration**, so the
  live-scenario smoke cannot be automated — the same shape as the v1.2 "TSAN clean" item, and
  recorded as unmet for the same reason. The two ways out are an owner decision, because one of them
  changes how configuration reaches a fail-closed switch.
- **§Corrections item 18: scenarios live in the database**, not in the seed JSON, as binary
  `.n8ro.instance` records. The live smoke swaps the mission script the shipped scenario points at,
  with backup and restore, rather than authoring a scenario variant.
- **OQ-8's premise may have dissolved.** With the doctrine actually loaded and the four-branch
  schema, the deployed prefix is **17,756 bytes ≈ 4,500 tokens** — above Haiku 4.5's 4,096-token
  minimum rather than below it. If that survives measurement against Anthropic's tokenizer, the
  prefix caches as written and the padding question resolves to "no padding needed".

- **§Corrections — new item 15: two branches were the wrong number.** v1.7 split the schema by
  *waypoint presence* and put `defend` with `engage` and `crank`. Those three agree about waypoints
  and disagree about targets — `engage` and `crank` require one, `defend` forbids one — so the
  branch could not state the target rule, and the first live soak measured 2/12 rejections reading
  *"posture 'defend' must not carry a targetEntityId"*. The partition is by **whole constraint
  profile**, which yields four branches: transit (`ingress`, `rtb`), hold, targeted (`engage`,
  `crank`), and defend. Re-measured: **0 rejections in 24 live orders**, `reject.schema` 0 %,
  `reject.shape` 0 %.
- **§FRs — AIC-ORD-1's encoding table** replaced with the four-branch table, and its acceptance
  criterion strengthened: the test asserts that **for every posture in every branch** the branch's
  bounds agree with the Stage-A A6 predicates, rather than that some branch exists. The weaker
  criterion v1.7 wrote would have passed the very split this revision corrects.
- **Prefix growth recorded, not hidden.** Repeating the shared field descriptions across four
  branches took the rendered prefix from 4,738 bytes to **14,074**. Measured p95 moved from ~1.7 s
  to ~2.2 s on the verification host — still far inside the ≤ 20 s local-7B target, and reported
  because it is a real cost of the encoding. It is also material to **OQ-8**: at ~3,500 tokens the
  prefix now sits near Haiku 4.5's 4,096-token cache minimum rather than well below it, which
  shrinks the padding delta that question turns on.

### v1.7 — 2026-08-02

Written at **Phase 1b start**, from measurements taken against the live inference server before any
adapter code was written. Two of this document's own predictions were tested; one held and one did
not, and the one that did not was blocking the phase.

- **§Corrections — new item 13: the conditional-presence failure is *omission*, and `oneOf` closes it.** v1.6's item 12 recorded two models *over-emitting* (`orbitRadiusM` on non-`hold`, `engage` with no target) and concluded that Stage-A A6 was the only enforcement and that shape rejections would fall as prompt wording improved. Measured against the real `PromptRenderer` prefix, the real doctrine, and the shipped A6 rules: `qwen2.5:7b-instruct-q8_0` emits **no optional field at all**, producing **10/12 `shape` rejections**. A prompt block stating the rules imperatively moved it **not at all**; worked examples took it to 2/12; `if`/`then`/`else` was confirmed unhonoured; a **`oneOf`** schema over two posture-discriminated branches took it to **0/12 with the prompt untouched**. The mechanism in item 12 is confirmed; its remedy is refuted.
- **§FRs — AIC-ORD-1 now specifies the `oneOf` encoding.** Two branches: `{ingress, hold, rtb}` with `waypoint` **required**, and `{engage, crank, defend}` declaring no `waypoint` property at all, both `additionalProperties: false`, both requiring `targetEntityId` and `orbitRadiusM` with the values the field table already gave them. **No field, posture, type, range, configuration value, verb, or backend changes** — the same contract, expressed so a decoder can hold it. A new acceptance criterion asserts the branch shapes, and states that a flat object with optional conditional fields does not satisfy it. Acceptance criterion (c) corrected from "generating the local GBNF grammar" to the `format` parameter, which OQ-1 settled in v1.6 and this criterion had not caught up with.
- **Stage-A A6 is unchanged, deliberately.** The decoder becomes a second line rather than a substitute. An adapter whose backend stops honouring `format` must still fail as a `shape` rejection, not as an accepted order.
- **§Corrections — new item 14: no warm-up at construction.** A `num_predict: 1` warm-up ping costs 2,503 ms and leaves the first real order at 1,432 ms — 3,935 ms against 3,860 ms without it. It relocates the cold cost rather than removing it, so the design option item 10 left open is closed **negatively**, on its own measurement. The same item records that a VRAM-evicted but page-cached load is **3.9 s**, not item 10's 22–46 s; item 10 measured a cold *disk* read and **stands as the worst case**.
- **§FRs — AIC-BE-1** gains the corresponding criteria: do not warm at construction, never block `initialize()` on a network call, and distinguish an unreachable server from a `local.model` tag the server does not have — reporting the latter as a configuration error naming the tag, which is what item 11 asked for and did not get.
- **OQ-5 resolved — No.** The entitlement subsystem reaches neither plugins nor the simulation: absent from `plugin/` headers and from `PluginContext`, absent from `lib/n8ro-core.lib` (so the proposed `initialize()` check would not have linked), and absent from every `n8ro-sim-*` binary — it lives in `N8RO.exe` and `LexActivator.dll` alone. The contingency is retired rather than carried.
- **OQ-6 resolved — `oppint_red_interceptor` / `RedSu35_01`**, in seed scenario "Mariana Shield". The live smoke runs against an **additive** `"Mariana Shield AI"` duplicate, so no shipped scenario record is modified and the commander-off control run has identical initial conditions.
- **§Risks** — the constrained-decoding row is largely retired and restated as a *regression* risk; **§Runbook** gains a `reject.shape` row that reads a climbing counter as "constrained decoding is not in force", explicitly warning against responding by relaxing A6; **§Phase 1b gate** restates the `reject.shape` note against the measurement instead of the v1.6 prediction; **§Cross-service impact** drops the retired llama.cpp/GGUF-import comparison from the `local` dependency row; **H3** corrected from "GBNF grammar locally".
- **§Review checklist** — OQ line rewritten: only OQ-3 and OQ-8 remain outstanding.

### v1.6 — 2026-08-02

**Topics in this revision:**
- **OQ-1 resolved — Ollama.** Not by preference but by what is installed plus a spike: Ollama 0.32.5 serving on `localhost:11434` with 14 instruct models already imported, so the shipped split GGUFs need no import. Its `format` parameter enforced the AIC-ORD-1 schema 3/3 across two models, retiring the GBNF argument for llama.cpp.
- **OQ-2 resolved — Yes, on the verification host.** RTX 4070 Ti SUPER, 16 GB. Warm round trips measured at ~1.7 s (qwen2.5 7B q8_0) and ~4.5 s (llama3.1 8B q4_K_M). Scoped explicitly to one host: a fleet inventory this is not.
- **Three findings from the spike**, all of which change a default or a stated expectation rather than an FR.

**Sections updated:**
- §Header — Status to Draft v1.6; revision-history entry.
- §Corrections verified in-tree — **items 10, 11, 12 added**: cold load exceeds the timeout; `local.model` named a file where Ollama needs a tag; JSON Schema cannot express the conditional-presence rules.
- §Performance → latency targets — **3 measured rows added** (warm 7B, warm 8B, cold load), each marked as a 3-sample observation rather than a distribution.
- §FRs — AIC-BE-1: the endpoint and payload are **pinned** (`POST /api/generate`, `format` = the embedded schema, order returned as a string in `response`), which v1.1 promised would happen once OQ-1 resolved. Three acceptance criteria added: send the same schema object AIC-ORD-1 embeds rather than a copy; do not assume `format` enforces A6; size the timeout for a cold load.
- §FRs — AIC-API-2: `commander.requestTimeoutS` 30 → **90**; `local.model` → **`qwen2.5:7b-instruct-q8_0`** (a tag, and the 7B now that a GPU is confirmed); `local.grammarEnabled`'s meaning pinned to Ollama's `format` rather than GBNF.
- §Risks — the unmet-dependency row rescoped to "not current on this host, still certain elsewhere"; **2 rows added** (cold-load timeout; constrained decoding does not enforce conditional rules).
- §Operational readiness → Dependencies — inference-server row rewritten to Installed/serving with the version and model count; **GPU row added**.
- §Open questions — **OQ-1 and OQ-2 marked Resolved 2026-08-02** with the evidence in their rationales.
- §Milestones → Phase 1b — deliverables made concrete rather than conditional; gate gains a first-order-completes item, separates `reject.shape` from the < 1 % `reject.schema` bar with the reason, and names the ~1.7 s baseline to beat.

**Sections explicitly verified no-change:**
- §One-liner · §Purpose and scope · §Problem statement · §Goals · §Success metrics · §Non-goals · §Key hypotheses · §Tenets · §Security posture · §Naming and path conventions · §Out of scope · **every FR except AIC-BE-1 and AIC-API-2** · §Scope authority · §Cross-service impact · §Source control · §Observability · §Rollback strategy · §Alternatives · §Rabbit holes · §Cost model · §Validation and test plan · §Review checklist · §Appendix A · §Appendix B · §Appendix C

**New OQ entries:** none.
**Resolved OQ entries:** **OQ-1**, **OQ-2**. Remaining open: OQ-3 (v1.1 planning), OQ-5 (Phase 1b deployment), OQ-6 (**Phase 1b start — the one item still gating**), OQ-8 (Phase 2 start).
**Out-of-Scope additions:** none.
**FR changes:** +0, ~2 modified (AIC-BE-1 pinned; AIC-API-2 defaults), −0.
**UAC changes:** none — no FR's observable contract changed, only the values and the mechanism behind it.

### v1.5 — 2026-08-01

**Topics in this revision:**
- **The CI split described since v1.1 was wrong about which side the tests land on.** Found while building the workflows. Hosted runners were expected to run "order-schema validity plus accept/reject fixture round-trips (AIC-ORD-1, AIC-VAL-1)"; in the built implementation zero of the 67 tests can run there, because the order schema and Stage-A validator are built on `n8ro::core::JsonValue` and the harness is `n8ro::core::TestRunner`. Both dependencies are deliberate and stand — `validateAgainstSchema` is what makes AIC-ORD-1's one-definition-three-consumers claim literal, and the SDK harness keeps the repository free of third-party test dependencies — so the paragraph was restated rather than the design changed.

**Sections updated:**
- §Header — Status to Draft v1.5; revision-history entry added.
- §Source control and repository → CI paragraph — rewritten. The split is restated as *compiles / does not compile* rather than *which FRs*, with a table naming both workflow files and the self-hosted runner labels. Added the badge obligation (a green hosted badge does not mean the plugin compiles), the shared-release-tree cleanup obligation, the rationale for the PRD lint (it exists because OQ-4's slip was caught late by a human), and the reason `clang-format` ships advisory.

**Sections explicitly verified no-change:**
- Everything else. No FR, UAC, OQ, metric, milestone, risk, or ADR was touched: this revision corrects a description of tooling, not a requirement.

**New OQ entries:** none.
**Resolved OQ entries:** none.
**Out-of-Scope additions:** none.
**FR changes:** +0, ~0, −0.
**UAC changes:** none.

### v1.4 — 2026-08-01

**Topics in this revision:**
- **OQ-4 resolved: No.** The MCP stack is not a sanctioned entity-control path. Enumerated from the shipped binaries rather than from documentation: `n8ro-sim-bot.exe` registers exactly two `IToolHandler` implementations — `WorkbookDescribeApiHandler` / `WorkbookEvalHandler`, surfaced as `workbook_describe_api` and `workbook_eval` over topics `sim/workbook/eval` and `sim/workbook/eval_response`. No entity-control tool exists. `n8ro-data-bot.exe` registers ~37 handlers, all authoring-domain create/edit/list/get against the database. Surfaced as overdue by the v1.3 `/prd-review` pass (its decision target, "Phase 1a end", had passed).

**Sections updated:**
- §Header — Status to Draft v1.4; revision-history entry added.
- §Prior art and lessons learned — the MCP bullet rewritten from "unverified" to the enumerated finding, with the lesson drawn: the tree's AI surface covers authoring and interactive debugging, and the autonomous per-entity path is the gap this PRD fills.
- §Out of scope — the MCP row moved **Deferred → Out of scope**, target `Pending OQ-4` → `N/A — verified absent, not deferred`, re-dated 2026-08-01. No strikethrough; the row is rewritten in place because its *status* changed rather than a new deferral being added.
- §Cross-service impact — the MCP row changed from "None in v1 / Pending OQ-4" to "None ever", with the note that the two systems coexist and only this one is autonomous.
- §Alternatives → Option 2 — Pros/Cons rewritten off evidence. Four concrete disqualifications replace the single "unverified" con: no entity-control tool; the only reaching path requires the model to emit Lua, forfeiting AIC-ORD-1's closed-schema guarantee; mutation is gated on human approval by design; and none of the surrounding machinery (roster, cadence, staleness, fratricide, ladder, order log) exists. Added a note recording `workbook_eval` as an adjacent LLM-facing arbitrary-Lua channel.
- §Open questions — OQ-4 marked **Resolved 2026-08-01 — No**, with the enumerated evidence in its rationale.
- §Quality gate notes — the OQ-4 bullet rewritten: it was answered a revision late, the delay happened to cost nothing because the answer was "no", and that asymmetry is the argument for answering cheap-but-invalidating questions on schedule.

**Sections explicitly verified no-change:**
- §One-liner · §Purpose and scope · §Problem statement · §Goals · §Success metrics · §Non-goals · §Key hypotheses · §Tenets · §Security posture · §Naming and path conventions · **every FR (AIC-ARCH-1/2/3/4, AIC-ORD-1/2, AIC-VAL-1/2, AIC-SEC-2, AIC-API-1/2, AIC-BE-1/2/3/4, AIC-DET-1/2)** · §Scope authority · §Performance requirements · §Configuration and deployment · §Observability · §Operational readiness · §Rollback strategy · §Alternatives Options 1/3/4/5 · §Risks · §Rabbit holes · §Cost model · §Validation and test plan · §Milestones · §Review checklist · §Appendix A · §Appendix B · §Appendix C ADR-1…7

**New OQ entries:** none — this revision resolves a question rather than raising one.
**Resolved OQ entries:** **OQ-4**
**Out-of-Scope additions:** 0 rows added; **1 row changed status** (MCP routing: Deferred → Out of scope, verified absent)
**FR changes:** none — +0, ~0, −0. The FR set is untouched, which is the point: the alternative that could have superseded it does not.
**UAC changes:** none.

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
