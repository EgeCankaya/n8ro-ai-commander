#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace n8ro::sim {
class IEntityManager;
}

namespace arkheon::aicommander {

// Result of the AIC-ARCH-4 startup probe over the componentTransform runtime columns.
enum class ProbeResult {
    NotRun,
    Pass,
    Fail,
};

[[nodiscard]] const char* toString(ProbeResult result);

// What the probe observed, kept for the startup log and for aiCommander.getStats().
struct ProbeReport {
    ProbeResult result = ProbeResult::NotRun;
    std::string detail;          // Human-readable; names the failing path when result == Fail.
    std::string probedEntityId;  // The entity the probe ran against, for reproducibility.

    // Non-fatal findings, names the leaf. A warning NEVER changes `result` and never disables the
    // commander — see the `health` note on probeSnapshotLeavesWith. Empty when there are none.
    std::string warning;
};

// Reads one componentTransform field of one entity. The seam that makes the probe testable: the
// production caller binds it to readComponentFieldReal, and the suite binds it to a fake that can
// be made to fail on demand — which is the only way to cover the branch that DISABLES the
// commander, and that branch is the entire reason the probe exists.
using ColumnReader = std::function<std::optional<double>(
    const std::string& entityId, std::string_view fieldPath)>;

// Resolve-check for `componentLifecycle/health`, which is an ENUM leaf and so cannot go through
// ColumnReader — nothing specifies whether an enum column reads as an int or as text, and
// HealthTier owns that question. Returns false when the leaf does not resolve at all.
using HealthLeafProbe = std::function<bool(const std::string& entityId)>;

// The pure form. Verifies every leaf the plugin reads, in two severities.
//
// FAILS the probe:
//   - `velocityNed.x` / `.y` / `.z` — runtime columns declared in TransformRuntimeColumns.h, on
//     DOT-joined paths where every authored schema leaf uses slashes. OQ-9 (resolved 2026-08-01,
//     observed on a live run) established that an unresolvable column returns std::nullopt rather
//     than a fabricated zero: readComponentFieldReal validates the path through
//     DynamicLayout::handle and reports. So this is a plain resolve-check — no moving-entity
//     heuristic, and a stationary entity is a valid subject.
//   - `positionGeodetic/latitudeDeg` / `longitudeDeg` / `altitudeHaeM` — slash-joined SCHEMA leaves
//     (v1.8.55). These already fail loudly, so the check buys EARLY, NAMED diagnosis rather than
//     rescue from silence: without it the symptom is buildSnapshot returning false on every entity
//     every tick, which presents as "the commander does nothing" and names no path.
//
// WARNS ONLY, and deliberately does not fail:
//   - `componentLifecycle/health` (v1.8.55). It feeds exactly one consumer, the C27 uncommandable
//     guard, whose specified behaviour on an unreadable tier is to treat the airframe as
//     COMMANDABLE (PRD §Corrections item 70(d)). Failing here would disable the entire commander
//     over a leaf whose absence costs four wasted orders per lost airframe — strictly worse than
//     the defect that guard fixes, and the opposite of the trade the guard itself makes. But it is
//     not silent either: an unreadable `health` is AIC-BE-3's muted-guard shape exactly, so it is
//     named in `warning` and surfaces as `probe.warning` in aiCommander.getStats().
//
// This list is the SINGLE place these paths are spelled for verification (PRD AIC-ARCH-4). A leaf
// added to the snapshot or to a guard belongs here in the same revision: a subset probe that reads
// like a whole one is worse than no probe, because it converts "unverified" into "verified" in a
// reader's mind at no cost to the defect.
[[nodiscard]] ProbeReport probeSnapshotLeavesWith(
    const std::string& entityId, const ColumnReader& read, const HealthLeafProbe& probeHealth);

// Retained spelling for the transform-only probe: `probeHealth` absent means the health leaf is not
// checked and no warning is produced.
[[nodiscard]] ProbeReport probeRuntimeColumnsWith(
    const std::string& entityId, const ColumnReader& read);

// The engine-facing form: picks any entity carrying a componentTransform and probes it.
[[nodiscard]] ProbeReport probeRuntimeColumns(const n8ro::sim::IEntityManager& manager);

} // namespace arkheon::aicommander
