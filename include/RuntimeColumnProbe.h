#pragma once

#include <string>

namespace n8ro::sim {
class IEntityManager;
}

namespace arkheon::aicommander {

// Result of the AIC-ARCH-4 startup probe over the componentTransform runtime columns.
//
// Why this exists at all: every *authored* schema field fails loudly — readComponentFieldReal
// returns std::nullopt and the snapshot aborts. The transform's runtime columns do not.
// TransformRuntimeColumns.h states that resolving a name which does not exist yields a handle that
// "reads back 0 WITHOUT an error, so a typo here is silent — an entity simply never moves." A
// mistyped velocity path therefore produces a plausible stationary own-ship rather than a
// diagnosable failure, and every order downstream is computed from that fiction.
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

    // OQ-9 evidence. Populated on every probe attempt regardless of outcome, because the question
    // "can readComponentFieldReal address a runtime column at all, and does a bad path really
    // return 0 rather than nullopt?" is answered by the shape of these three reads, not by the
    // pass/fail verdict.
    bool dotPathResolved = false;      // "velocityNed.x"  — the form TransformRuntimeColumns.h uses.
    bool slashPathResolved = false;    // "velocityNed/x"  — the form the schema uses.
    bool bogusPathResolved = false;    // "velocityNed.q"  — deliberately not a column.
    double dotPathValue = 0.0;
    double bogusPathValue = 0.0;
};

// Runs the probe against `manager`, picking a commanded-eligible entity that should be moving.
//
// The probe cannot simply ask "does this column exist?" — the SDK exposes no such predicate, which
// is precisely the gap OQ-9 tracks. It instead reads three paths and reasons about the pattern:
//
//   * if the bogus path resolves to a value, a bad path is silent and only the moving-entity
//     heuristic can distinguish a real reading from a fabricated zero;
//   * if the bogus path returns nullopt, failures are loud after all, and a plain resolve-check
//     is sufficient — which would let AIC-ARCH-4 relax in a later revision.
//
// Either way the observation is written into the report so OQ-9 is answered from evidence.
[[nodiscard]] ProbeReport probeRuntimeColumns(const n8ro::sim::IEntityManager& manager);

} // namespace arkheon::aicommander
