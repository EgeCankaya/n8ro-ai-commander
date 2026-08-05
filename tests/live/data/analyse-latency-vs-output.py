#!/usr/bin/env python3
"""Reproduces the latency-vs-output analysis in PRD §Corrections item 28.

Runs offline against the archived per-order CSVs beside this file. No network, no API key,
no release tree, no compiler -- stdlib only. If this script and those four CSVs are present,
every number in §Corrections items 25, 27(f) and 28 is re-derivable.

    python analyse-latency-vs-output.py

WHY IT EXISTS. Item 27(f) reopened C2 on the suspicion that Haiku's near-zero r-squared was an
artifact of a flattened x-axis rather than evidence about the system. Item 28 turns that into a
demonstration and then narrows what it licenses. Both steps are arithmetic over rows already on
disk, and arithmetic that lives only in a transcript is not evidence.

THE POINT THE SCRIPT EXISTS TO MAKE. r-squared answers "how much of THIS SAMPLE's scatter does the
predictor explain", which depends on how far the predictor was allowed to move. The slope answers
"how many milliseconds per token", which does not. Restricting the range attenuates the first and
leaves the second unbiased. A table that leads with r-squared reports a property of the sampling
as though it were a property of the system -- which is what v1.8.5 and item 27(f) both did.
"""

import csv
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))

# The four hosted runs this project has, each written by `ai-commander-live-tests.exe --csv`.
# Provenance is in README.md beside this file: which grant paid for each, and what it measured.
ARM_A = "c2-armA-haiku-full-doctrine.csv"   # v1.8.5, third egress grant
ARM_B = "c2-armB-haiku-short-doctrine.csv"  # v1.8.5, third egress grant
SONNET_512 = "sonnet-maxtokens-512.csv"     # v1.8.5, third egress grant
SONNET_8192 = "sonnet-maxtokens-8192.csv"   # v1.8.9, fourth egress grant


def load(name, accepted_only=False):
    rows = []
    with open(os.path.join(HERE, name), newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            if accepted_only and r["accepted"] != "1":
                continue
            rows.append((int(r["tokensOut"]), float(r["latencyMs"]), r["situation"]))
    return rows


def fit(rows):
    """OLS latencyMs ~ a + b*tokensOut, carrying the slope's standard error and 95% interval.

    The interval is the whole point. A slope reported without one invites exactly the reading
    v1.8.5 gave two of them: that 1.8 and 20.8 are different numbers, therefore the system
    disagrees with itself.
    """
    n = len(rows)
    if n < 3:
        return None
    xs = [r[0] for r in rows]
    ys = [r[1] for r in rows]
    mx = sum(xs) / n
    my = sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    if sxx == 0 or syy == 0:
        return None
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    a = my - b * mx
    ss_res = sum((y - (a + b * x)) ** 2 for x, y in zip(xs, ys))
    s = math.sqrt(ss_res / (n - 2))                       # residual SD
    se_b = s / math.sqrt(sxx)
    se_a = s * math.sqrt(1.0 / n + mx * mx / sxx)         # the intercept's SE -- item 28(f)
    return dict(n=n, a=a, b=b, r2=1.0 - ss_res / syy, s=s, se_b=se_b, se_a=se_a,
                sd_x=math.sqrt(sxx / (n - 1)), lo=b - 1.96 * se_b, hi=b + 1.96 * se_b,
                xmin=min(xs), xmax=max(xs), mean_x=mx)


def show(name, f):
    print(f"{name:<26} n{f['n']:<4} x: {f['xmin']:>4}-{f['xmax']:<4} SD {f['sd_x']:>6.1f}   "
          f"slope {f['b']:>6.2f} +/- {1.96 * f['se_b']:>5.2f} ms/tok  "
          f"[{f['lo']:>7.2f}, {f['hi']:>6.2f}]   r2 {f['r2']:.3f}   resid SD {f['s']:>6.0f} ms")


def rule(text=""):
    print("=" * 110)
    if text:
        print(text)
        print("=" * 110)


runs = [
    ("C2 arm A  Haiku", fit(load(ARM_A))),
    ("C2 arm B  Haiku", fit(load(ARM_B))),
    # Accepted-only, and NOT n=48. The four truncated orders have tokensOut == the cap: that is
    # not a measurement of the predictor, it is four responses of unknown and differing true
    # length recorded at the same x. Item 28(a); the censoring itself is item 25(d).
    ("Sonnet @512 (acc.)", fit(load(SONNET_512, accepted_only=True))),
    ("Sonnet @8192", fit(load(SONNET_8192))),
]

rule("THE SLOPE IS THE TEST, NOT r2. Range restriction attenuates r2; it does NOT bias the slope.")
for name, f in runs:
    show(name, f)

A, B, S512, S8192 = (f for _, f in runs)

print()
print("--- (b) Do the two Haiku arms even disagree, once their uncertainty is shown? ---")
print(f"  arm A slope 95% CI [{A['lo']:.2f}, {A['hi']:.2f}] ms/tok")
print(f"  arm B slope 95% CI [{B['lo']:.2f}, {B['hi']:.2f}] ms/tok")
print(f"  overlap? {'YES' if A['hi'] >= B['lo'] and B['hi'] >= A['lo'] else 'NO'}"
      "   -> two estimates too imprecise to conflict, reported by v1.8.5 as a conflict")

print()
print("--- (d) And what do they license in the OTHER direction? ---")
print(f"  Sonnet's measured slope ({S8192['b']:.1f} ms/tok) inside arm A's CI? "
      f"{'YES' if A['lo'] <= S8192['b'] <= A['hi'] else 'NO'}"
      f"   inside arm B's CI? {'YES' if B['lo'] <= S8192['b'] <= B['hi'] else 'NO'}")
print(f"  zero inside arm A's CI? {'YES' if A['lo'] <= 0 <= A['hi'] else 'NO'}"
      f"   inside arm B's CI? {'YES' if B['lo'] <= 0 <= B['hi'] else 'NO'}")
print("  arm A admits zero and excludes Sonnet's slope; arm B admits both. Consistent with a")
print("  Sonnet-sized effect AND with none -- uninformative, not 'hidden'.")

print()
rule("(c) THE DECISIVE DIRECTION: you cannot widen Haiku's range, so NARROW Sonnet's to Haiku's\n"
     "width and see whether a KNOWN-REAL relationship collapses into a Haiku-looking r2.")
haiku_sd = (A["sd_x"] + B["sd_x"]) / 2
print(f"  Haiku's mean output-token SD across both arms: {haiku_sd:.1f}")
for lo, hi in [(124, 200), (124, 250), (300, 420), (124, 300)]:
    band = [r for r in load(SONNET_8192) if lo <= r[0] <= hi]
    g = fit(band)
    if g:
        print(f"  Sonnet restricted to [{lo},{hi}]  n{g['n']:<3} SD {g['sd_x']:>5.1f}  "
              f"slope {g['b']:>7.2f}  r2 {g['r2']:.3f}")

print()
rule("WHAT r2 WOULD SONNET'S OWN RELATIONSHIP PRODUCE ON HAIKU'S NARROW RANGE?\n"
     "r2 = b^2*SDx^2 / (b^2*SDx^2 + s^2)  -- Sonnet's slope and residual noise, Haiku's spread.\n"
     "(An ASSUMPTION, not a measurement: it supposes Haiku's per-token cost and noise resemble\n"
     " Sonnet's. It answers only 'is Haiku's near-zero r2 CONSISTENT with a hidden real effect?')")
for label, f in [("Sonnet @8192", S8192), ("Sonnet @512 ", S512)]:
    num = (f["b"] * haiku_sd) ** 2
    print(f"  using {label}  slope {f['b']:>5.1f} ms/tok, resid SD {f['s']:>5.0f} ms, "
          f"x-SD {haiku_sd:.1f}  ->  predicted r2 {num / (num + f['s'] ** 2):.3f}")
print(f"  MEASURED on Haiku: arm A r2 {A['r2']:.3f}   arm B r2 {B['r2']:.3f}")

print()
rule("(f) WHY A BIGGER VERSION OF THE SAME ARMS IS THE WRONG PURCHASE.\n"
     "The doctrine comparison tests the INTERCEPT -- the fixed per-request term. So the question\n"
     "is how precisely this instrument can locate an intercept, not a slope.")
se_a = S8192["se_a"]
gap = 1.96 * se_a * math.sqrt(2)
print(f"  Sonnet @8192: intercept {S8192['a']:.0f} ms, SE {se_a:.0f} ms, 95% interval +/- {1.96 * se_a:.0f} ms")
print(f"  observed drift between two IDENTICALLY configured runs: 3,472 -> 2,788 ms  (684 ms)")
print(f"  -> a two-arm intercept gap must exceed ~{gap:.0f} ms to resolve at n={S8192['n']}/arm.")
for target in (300.0,):
    n_needed = S8192["n"] * (gap / target) ** 2
    print(f"  -> reaching a {target:.0f} ms resolvable gap by sample size alone: "
          f"~{n_needed:.0f} orders/arm, {2 * n_needed:.0f} orders, "
          f"~${2 * n_needed * 0.00788:,.2f} at Sonnet's measured $0.00788/order")
print("  The prefix delta at issue is ~2,200 CACHED tokens and will not cost anything like that.")
print("  TTFT is not a bigger sample; it is a different estimator -- it removes generation from")
print(f"  the measured quantity, and generation is where the {S8192['s']:.0f} ms of scatter lives.")
