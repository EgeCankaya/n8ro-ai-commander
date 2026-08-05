#!/usr/bin/env python3
"""Reproduces the C2 time-to-first-token analysis in PRD §Corrections item 29.

Runs offline against the archived per-order CSVs beside this file. No network, no API key,
stdlib only.

    python analyse-ttft.py

WHAT THE RUN WAS. Two arms of 48 orders on `claude-sonnet-5`, differing ONLY in doctrine size --
the same paired shape as the v1.8.5 C2 arms, with the same six fixtures -- measured through a
streaming probe that timestamps the response headers and the first `content_block_delta`
separately from the completed response.

THE QUESTION IT WAS BUILT TO ANSWER was whether the fixed per-request term differs between
doctrine sizes. §Corrections item 28(f) argued that total latency could not resolve it and that
TTFT could, on the reasoning that TTFT removes generation and "generation is where the scatter
lives."

THE QUESTION IT ACTUALLY ANSWERED is whether that reasoning was right. It was not, and this
script's job is to show why with the interval attached rather than to assert it.
"""

import csv
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))

ARM_A = "c2-ttft-armA-full-doctrine.csv"    # 17,756 B prefix -> 10,493 cached tokens
ARM_B = "c2-ttft-armB-short-doctrine.csv"   # 10,976 B prefix ->  8,291 cached tokens


def load(name):
    with open(os.path.join(HERE, name), newline="", encoding="utf-8-sig") as f:
        return [{k: v for k, v in r.items()} for r in csv.DictReader(f)]


def col(rows, name):
    return [float(r[name]) for r in rows]


def mean(xs):
    return sum(xs) / len(xs)


def sd(xs):
    m = mean(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def fit(xs, ys):
    """OLS ys ~ a + b*xs, with the slope's 95% interval. Same estimator as analyse-latency-vs-output.py."""
    n = len(xs)
    mx, my = mean(xs), mean(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    a = my - b * mx
    ss_res = sum((y - (a + b * x)) ** 2 for x, y in zip(xs, ys))
    s = math.sqrt(ss_res / (n - 2))
    se_b = s / math.sqrt(sxx)
    return dict(n=n, a=a, b=b, r2=1.0 - ss_res / syy, s=s,
                lo=b - 1.96 * se_b, hi=b + 1.96 * se_b)


def diff(xs, ys):
    """Welch difference of means, with a 95% interval. Two independent arms, unequal variance."""
    d = mean(xs) - mean(ys)
    se = math.sqrt(sd(xs) ** 2 / len(xs) + sd(ys) ** 2 / len(ys))
    return d, se, d - 1.96 * se, d + 1.96 * se


def rule(text=""):
    print("=" * 104)
    if text:
        print(text)
        print("=" * 104)


A, B = load(ARM_A), load(ARM_B)

rule("THE THREE ESTIMATORS, SIDE BY SIDE. The question is not which mean is smallest -- it is\n"
     "which SD is smallest, because the SD is what decides whether an arm comparison can resolve\n"
     "anything at all.")
print(f"{'':<14}{'arm A (full doctrine)':>32}{'arm B (short doctrine)':>32}")
print(f"{'':<14}{'mean':>10}{'SD':>10}{'n':>6}   {'mean':>10}{'SD':>10}{'n':>6}")
for label, key in [("headers", "headersMs"), ("TTFT", "ttftMs"),
                   ("afterFirst", "afterFirstMs"), ("total", "totalMs"),
                   ("tokensOut", "tokensOut"), ("deltas", "deltas")]:
    a, b = col(A, key), col(B, key)
    print(f"{label:<14}{mean(a):>10,.0f}{sd(a):>10,.0f}{len(a):>6}   "
          f"{mean(b):>10,.0f}{sd(b):>10,.0f}{len(b):>6}")

print()
rule("(1) IS TTFT ACTUALLY A LOWER-VARIANCE ESTIMATOR THAN TOTAL?\n"
     "That was the entire premise of item 28(f). It is a comparison of SD columns, not of means.")
for name, rows in [("arm A", A), ("arm B", B)]:
    t, tot = col(rows, "ttftMs"), col(rows, "totalMs")
    ratio = sd(t) / sd(tot)
    verdict = "LOWER" if ratio < 1 else "NOT LOWER"
    print(f"  {name}:  SD(TTFT) {sd(t):>7,.0f} ms   vs   SD(total) {sd(tot):>7,.0f} ms"
          f"   ratio {ratio:.2f}   -> {verdict}")
print()
print("  And the reason, in one column: how much variance does generation actually contribute?")
for name, rows in [("arm A", A), ("arm B", B)]:
    print(f"  {name}:  SD(afterFirst) {sd(col(rows, 'afterFirstMs')):>7,.0f} ms"
          f"   against SD(total) {sd(col(rows, 'totalMs')):>7,.0f} ms")
print("  Removing a component that contributes almost no variance removes almost no variance.")

print()
rule("(2) THE COMPARISON THE INSTRUMENT WAS BUILT FOR: does the fixed term differ by doctrine size?\n"
     "Prefix delta between the arms: 10,493 - 8,291 = 2,202 CACHED tokens.")
for label, key in [("headers", "headersMs"), ("TTFT", "ttftMs"), ("total", "totalMs")]:
    d, se, lo, hi = diff(col(A, key), col(B, key))
    zero = "INCLUDES ZERO" if lo <= 0 <= hi else "excludes zero"
    print(f"  {label:<10} A - B = {d:>8,.0f} ms   SE {se:>6,.0f}   95% [{lo:>9,.0f}, {hi:>8,.0f}]   {zero}")
print()
print("  Detectable difference at this n and this SD (1.96 x SE), per estimator:")
for label, key in [("headers", "headersMs"), ("TTFT", "ttftMs"), ("total", "totalMs")]:
    _, se, _, _ = diff(col(A, key), col(B, key))
    print(f"    {label:<10} anything smaller than {1.96 * se:>6,.0f} ms is invisible to this run")

print()
rule("(3) WHERE THE VARIANCE ACTUALLY IS, AND WHY TTFT DID NOT HELP.\n"
     "If generation drove the scatter, afterFirst would track tokensOut. Does it?")
for name, rows in [("arm A", A), ("arm B", B)]:
    f_after = fit(col(rows, "tokensOut"), col(rows, "afterFirstMs"))
    f_ttft = fit(col(rows, "tokensOut"), col(rows, "ttftMs"))
    f_tot = fit(col(rows, "tokensOut"), col(rows, "totalMs"))
    print(f"  {name}")
    print(f"    afterFirst ~ tokensOut   slope {f_after['b']:>6.2f} [{f_after['lo']:>6.2f}, {f_after['hi']:>6.2f}] ms/tok   r2 {f_after['r2']:.3f}   resid SD {f_after['s']:>6,.0f} ms")
    print(f"    TTFT       ~ tokensOut   slope {f_ttft['b']:>6.2f} [{f_ttft['lo']:>6.2f}, {f_ttft['hi']:>6.2f}] ms/tok   r2 {f_ttft['r2']:.3f}   resid SD {f_ttft['s']:>6,.0f} ms")
    print(f"    total      ~ tokensOut   slope {f_tot['b']:>6.2f} [{f_tot['lo']:>6.2f}, {f_tot['hi']:>6.2f}] ms/tok   r2 {f_tot['r2']:.3f}   resid SD {f_tot['s']:>6,.0f} ms")

print()
rule("(4) THE GRANULARITY CAVEAT, STATED RATHER THAN BURIED.\n"
     "Under structured outputs the service does not emit one delta per token. If it emitted few,\n"
     "then the FIRST delta already carries a large share of the generation, and TTFT is an upper\n"
     "bound on the fixed term rather than the fixed term.")
for name, rows in [("arm A", A), ("arm B", B)]:
    d = col(rows, "deltas")
    t = col(rows, "tokensOut")
    per = [ti / di for ti, di in zip(t, d)]
    print(f"  {name}:  deltas per response {min(d):.0f}-{max(d):.0f} (mean {mean(d):.1f})"
          f"   -> ~{mean(per):,.0f} tokens per delta on average")
print("  So TTFT contains roughly one delta's worth of generation. It is an UPPER BOUND on the")
print("  fixed term. `headers` is the estimator with no generation in it at all -- and it is the")
print("  one with the smallest SD, which is the same conclusion arriving by a second route.")
