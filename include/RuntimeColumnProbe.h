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
};

// Reads one componentTransform field of one entity. The seam that makes the probe testable: the
// production caller binds it to readComponentFieldReal, and the suite binds it to a fake that can
// be made to fail on demand — which is the only way to cover the branch that DISABLES the
// commander, and that branch is the entire reason the probe exists.
using ColumnReader = std::function<std::optional<double>(
    const std::string& entityId, std::string_view fieldPath)>;

// The pure form. Verifies that velocityNed.{x,y,z} resolve for `entityId`.
//
// Why a probe is needed at all: these columns are declared in TransformRuntimeColumns.h rather
// than in the schema, and they use dot-joined paths where every authored schema leaf uses slashes.
//
// OQ-9 (resolved 2026-08-01, observed on a live run) established that an unresolvable column
// returns std::nullopt rather than a fabricated zero: readComponentFieldReal validates the path
// through DynamicLayout::handle and reports. The probe is therefore a plain resolve-check — no
// moving-entity heuristic, and a stationary entity is a valid subject.
[[nodiscard]] ProbeReport probeRuntimeColumnsWith(
    const std::string& entityId, const ColumnReader& read);

// The engine-facing form: picks any entity carrying a componentTransform and probes it.
[[nodiscard]] ProbeReport probeRuntimeColumns(const n8ro::sim::IEntityManager& manager);

} // namespace arkheon::aicommander
