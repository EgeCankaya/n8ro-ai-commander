#!/usr/bin/env python3
"""Outcome comparison for paired live runs: commander-on vs commander-off.

WHY THIS EXISTS. The live-smoke harness reports acceptance, postures, frame cost, timeouts and
reject rates - six quantities, none of which is an OUTCOME. A commander can therefore score green
on every published metric while making the entity it commands shoot less, hit nothing, and die.
That is not hypothetical: it is what PRD Corrections item 45 measured, from logs that had been on
disk since Phase 1b, using nothing but this script. Carried as C21.

It reads ARCHIVED ENGINE LOGS ONLY. No inference server, no engine run, no network, no cost.

The three line formats it depends on, quoted from the logs rather than assumed:

    Weapon spawned: <munitionId> profile <p> owner <OWNER> target <TARGET>
    Detonation: <munitionId> outcome=detonation target=<T> reason=... missDistanceM=...
    Warhead damage applied: source=<munitionId> target=<T> pk=... cumPk=... state=destroyed

A KILL is attributed to the munition's OWNER, resolved through the spawn line, because the warhead
line names the weapon and not the shooter. Getting that wrong would credit every kill to a missile.

A DETONATION IS NOT A KILL, and the distinction is load-bearing: PRD item 40 reported a "hits"
column that counted detonations, and 5 of them destroyed nothing. This script reports both, in
separate columns, so the two can never be conflated again.

Usage:
    python tools/analyse-outcomes.py <archive-root> [--since 20260806T171041Z]

`--since` exists because the fire path did not work before the named-hardpoint fix of PRD v1.8.22
(C12): earlier commanded launches drew no loadout slot, so pairs before it are confounded and
should be excluded from any launch or kill comparison.
"""
import argparse
import collections
import glob
import os
import re
import sys

RE_SPAWN = re.compile(r"Weapon spawned: (\S+) profile \S+ owner (\S+) target (\S+)")
RE_DETONATE = re.compile(r"Detonation: (\S+) outcome=detonation target=(\S+)")
RE_KILL = re.compile(r"Warhead damage applied: source=(\S+) target=(\S+) .*state=destroyed")

Tally = collections.namedtuple("Tally", "launches detonations kills losses")


def analyse(path, prefix):
    """Counts for entities whose id starts with `prefix` (the commanded aircraft)."""
    owner_of = {}
    launches = detonations = kills = losses = 0
    for line in open(path, encoding="utf-8", errors="ignore"):
        m = RE_SPAWN.search(line)
        if m:
            munition, owner, _target = m.groups()
            owner_of[munition] = owner
            if owner.startswith(prefix):
                launches += 1
            continue
        m = RE_DETONATE.search(line)
        if m and owner_of.get(m.group(1), "").startswith(prefix):
            detonations += 1
        m = RE_KILL.search(line)
        if m:
            munition, victim = m.groups()
            if owner_of.get(munition, "").startswith(prefix):
                kills += 1
            if victim.startswith(prefix):
                losses += 1
    return Tally(launches, detonations, kills, losses)


# The archived engine log for one arm of a run.
#
# THREE ARMS, NOT TWO (PRD v1.8.30, C22). `on` and `off` differ in TWO variables - the commander AND
# the mission script, because the harness restores the shipped script before the control run, which
# PRD item 45(b) records being published before anyone noticed. `script-only` is the arm that holds
# the script fixed and moves only the commander, and it is the one that says whether the reference
# Tier-1 script is as good as the stock one when nothing is commanding it.
ARM_GLOB = {
    "on": "commander-on-*.log",
    "off": "commander-off-*.log",
    "script-only": "script-only-*.log",
}


def arm_log(run_dir, arm):
    hits = [p for p in glob.glob(os.path.join(run_dir, ARM_GLOB[arm]))
            if not p.endswith(".err")]
    return hits[0] if hits else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("archive_root", help="directory holding the <stamp>-local run folders")
    ap.add_argument("--since", default="", help="skip runs whose folder name sorts before this")
    ap.add_argument("--prefix", default="RedSu35",
                    help="entity-id prefix of the commanded aircraft (default RedSu35)")
    args = ap.parse_args()

    if not os.path.isdir(args.archive_root):
        print("not a directory: %s" % args.archive_root)
        return 1

    arms = ("on", "script-only", "off")
    totals = {arm: [0, 0, 0, 0] for arm in arms}
    seen = {arm: 0 for arm in arms}
    pairs = 0

    header = "%-8s" % "pair"
    for arm in arms:
        header += " %-24s" % ("%s L/D/K/lost" % arm.upper())
    print(header)
    print("-" * len(header))

    for name in sorted(os.listdir(args.archive_root)):
        run_dir = os.path.join(args.archive_root, name)
        if not os.path.isdir(run_dir) or name < args.since:
            continue
        logs = {arm: arm_log(run_dir, arm) for arm in arms}
        # `on` and `off` are still what makes a run a PAIR: script-only is the third arm and is
        # reported when present rather than required, so every archived run from before v1.8.30
        # still reads correctly here instead of vanishing from the table.
        if not logs["on"] or not logs["off"]:
            continue
        pairs += 1
        row = "%-8s" % name[9:15]
        for arm in arms:
            if not logs[arm]:
                row += " %-24s" % "-"
                continue
            t = analyse(logs[arm], args.prefix)
            seen[arm] += 1
            for i, v in enumerate(t):
                totals[arm][i] += v
            row += " %-24s" % ("%2d / %2d / %2d / %2d" % t)
        print(row)

    if not pairs:
        print("no paired runs found under %s" % args.archive_root)
        return 1

    print("-" * len(header))
    pooled = "%-8s" % ("POOLED%d" % pairs)
    for arm in arms:
        pooled += " %-24s" % (("%2d / %2d / %2d / %2d" % tuple(totals[arm])) if seen[arm] else "-")
    print(pooled)
    for arm in arms:
        if seen[arm] != pairs:
            print("note: %s present in %d of %d runs" % (arm, seen[arm], pairs))
    print()
    print("launch = weapon spawned by a commanded aircraft")
    print("det    = one of those weapons detonated (NOT a kill - see the module docstring)")
    print("kill   = one of those weapons destroyed something")
    print("lost   = a commanded aircraft was destroyed")
    print()
    print("ON vs OFF differ in TWO variables - the commander AND the mission script, because the")
    print("harness restores the shipped script before the control arm. SCRIPT-ONLY holds the script")
    print("fixed and moves only the commander; it is the arm that says whether the reference Tier-1")
    print("script fights as well as the stock one. See PRD C21 and C22.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
