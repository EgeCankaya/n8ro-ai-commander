#!/usr/bin/env python3
"""The pre-registered outcome campaign, computed. PRD v1.8.42, §Corrections item 58.

WHY THIS EXISTS, AND WHY IT IS NOT tools/analyse-outcomes.py

`analyse-outcomes.py` reports launches, detonations, kills and losses. The planning analysis in
§Corrections item 58 found that NOT ONE of those four can answer "what is the commander worth" at
any n anyone will run - they need 17, 42, 140 runs, and `losses` has been identically 0 in all
three arms of every three-arm run since the reference-script fix, so it carries no information at
all. The signal is in the graded `pk` those four columns threshold away.

So this tool owns the campaign number, exactly as `tools/acceptance-report.py` owns the in-engine
acceptance figure and for the same reason: a quantity quoted from three documents drifts, and a
quantity nobody can regenerate goes stale. It reads ARCHIVED ENGINE LOGS ONLY - no engine run, no
inference server, no network, no cost.

WHAT IT REFUSES TO DO

It does not choose its own test. The endpoint, the arms, the n, the decision rule, the null wording
and the two forbidden claims are fixed in §Validation's pre-registration, which was committed
BEFORE the campaign ran. This tool applies that rule and prints the verdict the rule produces,
including when that verdict is NULL. A tool that could pick its analysis after seeing the data
would be the failure §Corrections items 40, 42, 47(b) and 50(a) record.

It also refuses to print anything until its own t-quantile reproduces a published value, on the
same footing as acceptance-report.py's interval self-test.

USAGE
    python tools/outcome-campaign.py [--archive "<path>"] [--since 20260809T190000Z]

The archive lives OUTSIDE the repository (run logs carry live scenario state - a security-relevant
ignore rule enforced by tools/check-artifacts.ps1). This tool reads it in place and writes nothing.
"""
import argparse
import glob
import math
import os
import re
import statistics as st
import sys

DEFAULT_ARCHIVE = os.path.expanduser(r"~\Documents\N8RO AI Commander logs")
PREFIX = "RedSu35"          # the commanded aircraft
CADENCE_WINDOW_S = 20.0

# The three arms, and which pair the pre-registration names as THE comparison.
ARMS = {
    "on": "commander-on-*.log",
    "script-only": "script-only-*.log",
    "off": "commander-off-*.log",
}
# ON vs SCRIPT-ONLY, and NOT ON vs OFF: `on` and `off` differ in TWO variables - the commander AND
# the mission script, because the harness restores the shipped script before the control arm.
# §Corrections item 45(b) records that the first version of that comparison was published before
# anyone noticed.
PRIMARY_PAIR = ("on", "script-only")

RE_SPAWN = re.compile(r"Weapon spawned: (\S+) profile \S+ owner (\S+) target (\S+)")
RE_DET = re.compile(r"Detonation: (\S+) outcome=detonation target=(\S+)")
RE_DMG = re.compile(
    r"Warhead damage applied: source=(\S+) target=(\S+) pk=([0-9.]+) cumPk=([0-9.]+) state=(\S+)")
RE_SEEKER_LOST = re.compile(r"\[seeker\] (\S+) lost")


# -------------------------------------------------------------------------------------------------
# Statistics, dependency-free. scipy is used when present and is not required: this tool has to run
# on a bare checkout, like every other gate in this repository.
# -------------------------------------------------------------------------------------------------
def _betacf(a, b, x):
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
    """Regularised incomplete beta I_x(a, b)."""
    if x <= 0.0:
        return 0.0
    if x >= 1.0:
        return 1.0
    lbeta = (math.lgamma(a + b) - math.lgamma(a) - math.lgamma(b)
             + a * math.log(x) + b * math.log(1.0 - x))
    if x < (a + 1.0) / (a + b + 2.0):
        return math.exp(lbeta) * _betacf(a, b, x) / a
    return 1.0 - math.exp(lbeta) * _betacf(b, a, 1.0 - x) / b


def t_cdf(t, df):
    x = df / (df + t * t)
    half = 0.5 * betainc(df / 2.0, 0.5, x)
    return 1.0 - half if t > 0 else half


def t_ppf(p, df):
    """Inverse Student-t CDF by bisection. Adequate and auditable; no dependency."""
    lo, hi = -400.0, 400.0
    for _ in range(300):
        mid = (lo + hi) / 2.0
        if t_cdf(mid, df) < p:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2.0


def self_test():
    """Two published Student-t quantiles. If these do not reproduce, every interval below is wrong.

    t(0.975, df=4) = 2.7764 and t(0.975, df=3) = 3.1824 - the values a four-pair and a three-pair
    campaign respectively depend on.
    """
    a, b = t_ppf(0.975, 4), t_ppf(0.975, 3)
    ok = abs(a - 2.7764) < 0.001 and abs(b - 3.1824) < 0.001
    return ok, a, b


# -------------------------------------------------------------------------------------------------
# Extraction
# -------------------------------------------------------------------------------------------------
def analyse(path):
    """Per-arm outcome record read off one archived engine log."""
    owner, seeker_lost = {}, set()
    launches = dets = kills = losses = 0
    dmg_dealt = dmg_absorbed = 0.0
    blue_shots_at_red = []        # (munition, victim) - Blue munitions fired AT a commanded aircraft
    hit_by = {}                   # munition -> pk it applied to a commanded aircraft

    for line in open(path, encoding="utf-8", errors="ignore"):
        m = RE_SPAWN.search(line)
        if m:
            mun, own, tgt = m.groups()
            owner[mun] = own
            if own.startswith(PREFIX):
                launches += 1
            elif tgt.startswith(PREFIX):
                blue_shots_at_red.append((mun, tgt))
            continue
        m = RE_DET.search(line)
        if m and owner.get(m.group(1), "").startswith(PREFIX):
            dets += 1
        m = RE_SEEKER_LOST.search(line)
        if m:
            seeker_lost.add(m.group(1))
        m = RE_DMG.search(line)
        if m:
            mun, victim, pk, _cum, state = m.groups()
            pk = float(pk)
            src = owner.get(mun, "")
            if src.startswith(PREFIX) and not victim.startswith(PREFIX):
                dmg_dealt += pk
                if state == "destroyed":
                    kills += 1
            if victim.startswith(PREFIX):
                # THE PRIMARY ENDPOINT. Every pk applied to a commanded aircraft, graded rather
                # than thresholded - which is the whole difference from the `losses` column.
                dmg_absorbed += pk
                hit_by[mun] = hit_by.get(mun, 0.0) + pk
                if state == "destroyed":
                    losses += 1

    defeated = [(mun, tgt) for mun, tgt in blue_shots_at_red if mun not in hit_by]
    return {
        "launches": launches, "detonations": dets, "kills": kills, "losses": losses,
        "damageDealt": round(dmg_dealt, 4), "damageAbsorbed": round(dmg_absorbed, 4),
        "blueShotsAtRed": len(blue_shots_at_red),
        "blueShotsDefeated": len(defeated),
        "defeatedWithSeekerLoss": len([m for m, _ in defeated if m in seeker_lost]),
        "defeatedMunitions": [m for m, _ in defeated],
    }


def load(archive, since):
    runs = []
    for name in sorted(os.listdir(archive)):
        d = os.path.join(archive, name)
        if not os.path.isdir(d) or name < since:
            continue
        rec = {"run": name}
        for arm, pat in ARMS.items():
            hits = [p for p in glob.glob(os.path.join(d, pat)) if not p.endswith(".err")]
            rec[arm] = analyse(hits[0]) if hits else None
        # A campaign observation is a run carrying BOTH primary arms. The pre-registration's design
        # is paired within a run; a run missing one arm is not half an observation.
        if rec[PRIMARY_PAIR[0]] and rec[PRIMARY_PAIR[1]]:
            runs.append(rec)
    return runs


def paired(runs, metric):
    return [r[PRIMARY_PAIR[0]][metric] - r[PRIMARY_PAIR[1]][metric] for r in runs]


def ci(diffs, alpha=0.05):
    n = len(diffs)
    if n < 2:
        return None
    mean = st.mean(diffs)
    sd = st.stdev(diffs)
    if sd == 0.0:
        # A zero-width "interval" is not an interval, and the decision rule below would read it
        # as "excludes zero" and print CLAIM SUPPORTED off a degenerate sample. Refuse instead.
        # Not reachable on the campaign data (sd 0.1552) and not as remote as it looks: the
        # control arm takes only two values across nine runs. §Corrections item 68(h).
        return None
    se = sd / math.sqrt(n)
    crit = t_ppf(1 - alpha / 2, n - 1)
    t_stat = mean / se
    p = 2 * (1 - t_cdf(abs(t_stat), n - 1))
    return mean, sd, mean - crit * se, mean + crit * se, t_stat, p


# The pre-registered secondary endpoints, with the n each would need. Printed so the campaign
# cannot report one as a finding - in either direction - without the number that says it cannot.
SECONDARY_N = {
    "damageDealt": 156, "kills": 140, "detonations": 42, "launches": 17,
}

MDE_80 = 0.284      # §Validation: minimum detectable paired difference at n = 4, 80 % power.


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--archive", default=DEFAULT_ARCHIVE)
    ap.add_argument("--since", default="",
                    help="skip runs whose folder name sorts before this (the campaign's start)")
    args = ap.parse_args()

    ok, a, b = self_test()
    print("=== t-quantile self-test (must give 2.7764 and 3.1824) ===")
    print("  t(0.975, 4) = %.4f   t(0.975, 3) = %.4f   ->  %s" % (a, b, "PASS" if ok else "FAIL"))
    if not ok:
        print("\n::error:: t-quantile implementation is wrong. Refusing to quote anything.")
        return 1

    if not os.path.isdir(args.archive):
        print("\n::error:: archive not found: %s" % args.archive)
        return 1

    runs = load(args.archive, args.since)
    if not runs:
        print("\n::error:: no runs carrying both %s arms under %s (since %r)"
              % (" and ".join(PRIMARY_PAIR), args.archive, args.since))
        return 1

    # THE POPULATION IS AN ARGUMENT, NOT A PROPERTY OF THIS TOOL, so it is printed with the
    # verdict. The pre-registration fixes n = 4 and excludes the five planning runs in PROSE;
    # nothing here enforces either, and a pasted verdict used to carry no trace of which runs
    # produced it. §Corrections item 68(h). NOTE that folder labels are NOT all in one time base
    # -- run-live-scenario.ps1 stamps LOCAL time with a literal 'Z' while the probes stamp real
    # UTC, so `--since` is a lexical comparison over mixed bases (C26, item 68(g)).
    print("\n=== population (an argument to this tool, printed so a verdict carries it) ===")
    print("  archive : %s" % args.archive)
    print("  --since : %s" % (args.since or "(none - EVERY run in the archive)"))
    print("  runs    : %d  ->  %s" % (len(runs), ", ".join(r["run"][:15] for r in runs)))

    print("\n=== campaign runs (both primary arms present) ===")
    print("  comparison: %s vs %s -- NOT on vs off, which moves the script as well" % PRIMARY_PAIR)
    hdr = "%-18s %-28s %-28s %-28s" % ("run", "ON absorbed/dealt", "SCRIPT-ONLY", "OFF")
    print("  " + hdr)
    print("  " + "-" * len(hdr))
    for r in runs:
        def cell(arm):
            x = r[arm]
            if not x:
                return "%-28s" % "-"
            return "%-28s" % ("%.4f / %.4f  L%d D%d K%d X%d" % (
                x["damageAbsorbed"], x["damageDealt"],
                x["launches"], x["detonations"], x["kills"], x["losses"]))
        print("  %-18s %s %s %s" % (r["run"][:15], cell("on"), cell("script-only"), cell("off")))

    # ---------------------------------------------------------------------------------------------
    # THE PRIMARY TEST, applied exactly as pre-registered.
    # ---------------------------------------------------------------------------------------------
    diffs = paired(runs, "damageAbsorbed")
    n = len(diffs)
    print("\n=== PRIMARY ENDPOINT: damageAbsorbed, paired ON - SCRIPT-ONLY ===")
    print("  per-run differences: [%s]" % ", ".join("%+.4f" % d for d in diffs))
    res = ci(diffs)
    if res is None:
        if n < 2:
            print("  n = %d - a paired interval needs at least 2 runs" % n)
        else:
            print("  n = %d, and every paired difference is identical (sd = 0)." % n)
            print("  REFUSING TO QUOTE. A zero-width interval is not an interval, and the")
            print("  decision rule would read it as 'excludes zero'. §Corrections item 68(h).")
        return 1
    mean, sd, lo, hi, t_stat, p = res
    print("  n = %d   mean %+.4f   sd %.4f" % (n, mean, sd))
    print("  95 %% CI on the mean paired difference: [%+.4f, %+.4f]" % (lo, hi))
    print("  t = %+.3f on %d df,  p = %.4f (two-sided)" % (t_stat, n - 1, p))

    negatives = sum(1 for d in diffs if d < 0)
    print("  sign consistency: %d of %d negative  (CORROBORATION ONLY - at n = 4 a sign test"
          % (negatives, n))
    print("                    cannot reach alpha = 0.05; 4 of 4 is p = 0.125)")

    # The three conjuncts, printed individually so a reader can see which one carried the verdict.
    c1 = (lo < 0 and hi < 0) or (lo > 0 and hi > 0)
    c2 = mean < 0
    c3 = negatives == n
    print("\n  the pre-registered decision rule, conjunct by conjunct:")
    print("    1. 95 %% CI excludes zero .................. %s" % ("YES" if c1 else "no"))
    print("    2. point estimate is negative ............. %s" % ("YES" if c2 else "no"))
    print("    3. sign negative in all %d runs ............ %s" % (n, "YES" if c3 else "no"))

    if c1 and c2 and c3:
        print("\n  VERDICT: CLAIM SUPPORTED.")
        print("  The commanded aircraft absorb less damage than the same script un-commanded.")
        print("  Quote the INTERVAL [%+.4f, %+.4f], never the point." % (lo, hi))
        print("  AND SAY THIS WHEREVER IT IS QUOTED: this design cannot separate 'the commander")
        print("  makes the aircraft safer' from 'the commander makes it fight less', because")
        print("  damageDealt is underpowered at this n (see below).")
    else:
        print("\n  VERDICT: NULL.")
        print("  The pre-registered wording, which is not 'the commander has no effect':")
        print("    any effect is smaller than %.3f in absolute value." % MDE_80)
        print("  Those two statements are different and only the first is supported by a")
        print("  bounded-power design.")

    # ---------------------------------------------------------------------------------------------
    # Secondary endpoints - declared underpowered IN ADVANCE and not claimable in either direction.
    # ---------------------------------------------------------------------------------------------
    print("\n=== SECONDARY ENDPOINTS - UNDERPOWERED BY PRE-REGISTRATION, NOT CLAIMABLE ===")
    print("  %-14s %-30s %10s %s" % ("endpoint", "paired differences", "mean", "runs needed"))
    for metric, need in SECONDARY_N.items():
        d = paired(runs, metric)
        print("  %-14s %-30s %+10.4f %d" % (
            metric, "[" + ", ".join("%+g" % x for x in d) + "]", st.mean(d), need))
    d = paired(runs, "losses")
    print("  %-14s %-30s %+10.4f %s" % (
        "losses", "[" + ", ".join("%+g" % x for x in d) + "]", st.mean(d),
        "DEGENERATE - identically 0 in every arm since the script fix"))
    print("  Reporting any of these as a finding - including as a reassuring null - would be")
    print("  reading a result off an instrument this table says cannot see it.")

    # ---------------------------------------------------------------------------------------------
    # The corroborating mechanism. Per-munition, not a rate, and reported separately.
    # ---------------------------------------------------------------------------------------------
    print("\n=== CORROBORATING MECHANISM (per-munition; NOT a rate; reported separately) ===")
    print("  %-18s %-24s %-24s %-24s" % ("run", "ON defeated/fired", "SCRIPT-ONLY", "OFF"))
    tot = {arm: [0, 0, 0] for arm in ARMS}
    for r in runs:
        row = "  %-18s" % r["run"][:15]
        for arm in ("on", "script-only", "off"):
            x = r[arm]
            if not x:
                row += " %-24s" % "-"
                continue
            tot[arm][0] += x["blueShotsDefeated"]
            tot[arm][1] += x["blueShotsAtRed"]
            tot[arm][2] += x["defeatedWithSeekerLoss"]
            row += " %-24s" % ("%d of %d  (seeker-lost %d)" % (
                x["blueShotsDefeated"], x["blueShotsAtRed"], x["defeatedWithSeekerLoss"]))
        print(row)
    print("  " + "-" * 90)
    for arm in ("on", "script-only", "off"):
        if tot[arm][1]:
            print("  %-12s POOLED %d of %d Blue munitions defeated, %d with a logged seeker loss"
                  % (arm, tot[arm][0], tot[arm][1], tot[arm][2]))
    print("\n  A defeated munition is one fired at a commanded aircraft that produced no damage")
    print("  event. This is a mechanism readable off recorded values, which this project's")
    print("  precedent (C5, C21, C22, C23) treats as the one category a small sample may close.")
    print("  THAT PRECEDENT IS NOT A WRITTEN RULE. Earlier versions of this line cited")
    print("  '§Scope authority rule 4', which does not exist and never has - §Corrections item")
    print("  68(a), open as C25. And the premise it was granted on is refuted: the denominator")
    print("  IS a sampling quantity (item 62(c)). It is NOT pooled into the primary endpoint -")
    print("  conflating a mechanism with a rate is §Corrections item 40.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
