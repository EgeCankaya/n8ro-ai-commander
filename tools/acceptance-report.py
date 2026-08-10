#!/usr/bin/env python3
"""Pooled in-engine order acceptance, recomputed from the archive, as an interval.

WHY THIS EXISTS (PRD v1.8.36, C17, §Corrections item 52)

C17 is not a dispute about which number is right. Both instruments are honest: the
synthetic-fixture soak measures 100 % and the engine measures far less, and they are two
instruments rather than two estimates of one quantity. The owner decided on 2026-08-06 that
the gate stays on the fixtures and that decision is not reopened here.

What C17 actually costs this project is STALENESS. The in-engine figure has gone stale four
times - 86.2 % -> 84.3 % -> 79.8 % -> 69.6 % - and every time for the same reason: a run was
archived and nobody recomputed. Each restatement made the old number look more settled
(§Corrections items 46(f), 48, 50, 51). The figure is quoted in three documents and there has
never been a way to regenerate it other than by hand.

So this tool owns the number. Run it, paste the sentinel it prints into the three documents,
and `tools/lint-prd.ps1` will fail the build if they ever disagree again.

DELIBERATELY NOT IN THIS TOOL: any verdict. It prints a rate and an interval. Whether a bar is
met is §Success metrics' business, and per §Scope authority a single run may not close a
question. It also prints no acceptance figure without its interval and its n, because a bare
decimal to one place is the form in which 86.2 % survived its own successor run.

USAGE
    python tools/acceptance-report.py ["<archive path>"]

The archive lives OUTSIDE this repository (order logs carry live scenario state and are a
security-relevant ignore rule - see §Configuration and tools/check-artifacts.ps1). This tool
reads it in place and writes nothing.
"""
import glob
import json
import os
import sys
from collections import Counter

DEFAULT_ARCHIVE = os.path.expanduser(r"~\Documents\N8RO AI Commander logs")

# Rejection reasons that are NOT model failures. Named rather than inferred, each with the row
# that establishes it. This is a REPORTING split, not the normative taxonomy - that is C17's
# item 3 and belongs in the PRD before it belongs here.
ENVIRONMENT_ARTIFACT = {
    "roster": "order resolved after its aircraft was destroyed (C21/C23 artifact, item 50(f))",
}
# The C23 stall floor is a `clamp` rejection, so it cannot be split off by reason alone - it is
# identified by its detail string. Item 51(a)-(c): the aircraft is parked, the snapshot reports
# the parked speed, and the model copies it. The model is exonerated by the record.
C23_DETAIL_MARKERS = ("below safety.minspeedmps", "is below safety.minspeedmps")


def clopper_pearson(k, n, alpha=0.05):
    """95 % Clopper-Pearson (exact) interval. Binding method per open-issues-review.md §8.

    Wilson is NOT the method this project uses and gives visibly different bounds; the
    self-test below discriminates them, which is why it is a test and not a comment.
    """
    if n == 0:
        return (0.0, 1.0)
    try:
        from scipy.stats import beta  # noqa
        lo = 0.0 if k == 0 else beta.ppf(alpha / 2, k, n - k + 1)
        hi = 1.0 if k == n else beta.isf(alpha / 2, k + 1, n - k)
        return (float(lo), float(hi))
    except ImportError:
        pass
    # No scipy: bisect the regularised incomplete beta via its continued fraction. Kept
    # dependency-free on purpose - this tool has to run on a bare checkout.
    from math import lgamma, exp

    def betacf(a, b, x):
        tiny, eps = 1e-30, 3e-16
        qab, qap, qam = a + b, a + 1.0, a - 1.0
        c, d = 1.0, 1.0 - qab * x / qap
        if abs(d) < tiny:
            d = tiny
        d = 1.0 / d
        h = d
        for m in range(1, 300):
            m2 = 2 * m
            aa = m * (b - m) * x / ((qam + m2) * (a + m2))
            d = 1.0 + aa * d
            c = 1.0 + aa / c
            if abs(d) < tiny:
                d = tiny
            if abs(c) < tiny:
                c = tiny
            d = 1.0 / d
            h *= d * c
            aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2))
            d = 1.0 + aa * d
            c = 1.0 + aa / c
            if abs(d) < tiny:
                d = tiny
            if abs(c) < tiny:
                c = tiny
            d = 1.0 / d
            delta = d * c
            h *= delta
            if abs(delta - 1.0) < eps:
                break
        return h

    def betainc(a, b, x):
        if x <= 0.0:
            return 0.0
        if x >= 1.0:
            return 1.0
        lbeta = lgamma(a + b) - lgamma(a) - lgamma(b) + a * __import__("math").log(x) \
            + b * __import__("math").log(1.0 - x)
        if x < (a + 1.0) / (a + b + 2.0):
            return exp(lbeta) * betacf(a, b, x) / a
        return 1.0 - exp(lbeta) * betacf(b, a, 1.0 - x) / b

    def solve(target, a, b):
        lo, hi = 0.0, 1.0
        for _ in range(200):
            mid = (lo + hi) / 2
            if betainc(a, b, mid) < target:
                lo = mid
            else:
                hi = mid
        return (lo + hi) / 2

    low = 0.0 if k == 0 else solve(alpha / 2, k, n - k + 1)
    high = 1.0 if k == n else solve(1 - alpha / 2, k + 1, n - k)
    return (low, high)


def self_test():
    """Validated against the PRD's own published figure before it may quote anything.

    §Success metrics publishes 50/58 -> [74.6 %, 93.9 %]. If this does not reproduce, the
    interval code is wrong and every number below it is wrong too.
    """
    lo, hi = clopper_pearson(50, 58)
    ok = abs(lo * 100 - 74.6) < 0.1 and abs(hi * 100 - 93.9) < 0.1
    return ok, lo * 100, hi * 100


def collect(archive):
    runs, accepted, rejected = 0, 0, 0
    reasons, artifact = Counter(), Counter()
    for path in sorted(glob.glob(os.path.join(archive, "*", "orders.jsonl"))):
        runs += 1
        for line in open(path, encoding="utf-8", errors="ignore"):
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except Exception:
                continue
            ev = rec.get("event")
            if ev == "order.accepted":
                accepted += 1
            elif ev == "order.rejected":
                rejected += 1
                reason = str(rec.get("reason", "?"))
                reasons[reason] += 1
                detail = str(rec.get("detail", "")).lower()
                if reason in ENVIRONMENT_ARTIFACT:
                    artifact["roster"] += 1
                elif any(m in detail for m in C23_DETAIL_MARKERS):
                    artifact["c23-stall-floor"] += 1
    return runs, accepted, rejected, reasons, artifact


def main():
    archive = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ARCHIVE
    ok, lo, hi = self_test()
    print("=== interval self-test (50/58 must give [74.6, 93.9] - the PRD's published figure) ===")
    print(f"  computed [{lo:.1f}, {hi:.1f}]  ->  {'PASS' if ok else 'FAIL'}")
    if not ok:
        print("\n::error:: interval implementation does not reproduce the published figure. "
              "Refusing to quote anything.")
        return 1

    if not os.path.isdir(archive):
        print(f"\n::error:: archive not found: {archive}")
        return 1

    runs, accepted, rejected, reasons, artifact = collect(archive)
    resolved = accepted + rejected
    if resolved == 0:
        print(f"\n::error:: no resolved orders under {archive}")
        return 1

    lo, hi = clopper_pearson(accepted, resolved)
    print(f"\n=== pooled in-engine acceptance ===")
    print(f"  archive        : {archive}")
    print(f"  runs with logs : {runs}")
    print(f"  resolved orders: {resolved}  (accepted {accepted}, rejected {rejected})")
    print(f"  acceptance     : {accepted/resolved*100:.1f} % [{lo*100:.1f}, {hi*100:.1f}]")

    print(f"\n=== rejection census ===")
    for reason, count in reasons.most_common():
        print(f"  {reason:<12} {count:>4}  ({count/rejected*100:.1f} % of rejections)")

    art_total = sum(artifact.values())
    if art_total:
        print(f"\n=== not model failures (reported, NOT excluded from the figure above) ===")
        for k, v in artifact.most_common():
            print(f"  {k:<16} {v:>4}")
        clean = accepted + (rejected - art_total)
        clo, chi = clopper_pearson(accepted, accepted + (rejected - art_total))
        print(f"  excluding both : {accepted/clean*100:.1f} % [{clo*100:.1f}, {chi*100:.1f}] "
              f"(n = {clean})")
        print("  (a split for reading, not a normative taxonomy - that is C17's remaining item "
              "and belongs in the PRD first)")

    sentinel = (f"<!-- in-engine-acceptance: {accepted/resolved*100:.1f} "
                f"[{lo*100:.1f}, {hi*100:.1f}] n={resolved} runs={runs} -->")
    print(f"\n=== paste this line into docs/prd.md, docs/summary.md and README.md ===")
    print(sentinel)
    print("\ntools/lint-prd.ps1 fails when the three copies disagree. That is the whole point:")
    print("the figure has gone stale four times and every time it was found by a reader.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
