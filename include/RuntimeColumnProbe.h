#pragma once

#include <string>

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
};

// Verifies that the componentTransform runtime columns the snapshot depends on
// (velocityNed.{x,y,z}) actually resolve, and refuses to let the commander enable if they do not.
//
// Why a probe is needed at all: these columns are declared in TransformRuntimeColumns.h rather
// than in the schema, and they use dot-joined paths where every authored schema leaf uses slashes.
// A path that silently read back a plausible zero instead of failing would produce a stationary
// own-ship in every snapshot, degrading orders with no failing test and no log line.
//
// OQ-9 (resolved 2026-08-01, observed on a live run) established that this failure is in fact
// LOUD, not silent: readComponentFieldReal validates the path through DynamicLayout::handle and
// returns std::nullopt for a name that is not a column. The probe is therefore a plain
// resolve-check — no moving-entity heuristic is required, and a stationary entity is a valid
// subject. The same run also showed both path forms resolve ("velocityNed.x" and "velocityNed/x"),
// so the dot form is used here only because TransformRuntimeColumns.h is its declared authority.
[[nodiscard]] ProbeReport probeRuntimeColumns(const n8ro::sim::IEntityManager& manager);

} // namespace arkheon::aicommander
