#include "RuntimeColumnProbe.h"

#include <component/ComponentFieldAccess.h>
#include <component/ComponentTypeNames.h>
#include <entity/IEntity.h>
#include <entity/IEntityManager.h>
#include <entity/TransformRuntimeColumns.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arkheon::aicommander {

namespace {

// The three velocity leaves the snapshot depends on, composed from TransformRuntimeColumns.h's own
// constants rather than re-spelled here — so a rename in the SDK breaks the build rather than the
// runtime, which is the whole point of that header being their single source of truth.
constexpr std::array<std::string_view, 3> kRequiredVelocityColumns{
    n8ro::sim::kTransformColumnVelocityNorth,
    n8ro::sim::kTransformColumnVelocityEast,
    n8ro::sim::kTransformColumnVelocityDown,
};

// Picks any entity carrying a transform. Since OQ-9 established that an unresolvable column
// returns std::nullopt rather than a fabricated zero, the subject does not need to be moving —
// a stationary entity proves resolution just as well.
[[nodiscard]] const n8ro::sim::IEntity* pickSubject(const n8ro::sim::IEntityManager& manager) {
    for (const n8ro::sim::IEntity* entity : manager.getAllEntities()) {
        if (entity != nullptr && entity->hasComponent(n8ro::sim::kComponentTransform)) {
            return entity;
        }
    }
    return nullptr;
}

} // namespace

const char* toString(ProbeResult result) {
    switch (result) {
        case ProbeResult::NotRun: return "notRun";
        case ProbeResult::Pass:   return "pass";
        case ProbeResult::Fail:   return "fail";
    }
    return "notRun";
}

ProbeReport probeRuntimeColumns(const n8ro::sim::IEntityManager& manager) {
    ProbeReport report;

    const n8ro::sim::IEntity* subject = pickSubject(manager);
    if (subject == nullptr) {
        // Nothing to probe yet. Deliberately NOT a failure — the caller retries on a later frame
        // rather than recording a verdict the scenario has not had a chance to earn.
        report.result = ProbeResult::NotRun;
        report.detail = "no entity with a componentTransform is loaded yet";
        return report;
    }

    const std::string entityId = subject->getScenarioEntityName();
    report.probedEntityId = entityId;

    for (const std::string_view column : kRequiredVelocityColumns) {
        const std::optional<double> value = n8ro::sim::readComponentFieldReal(
            manager, entityId, n8ro::sim::kComponentTransform, column);
        if (!value.has_value()) {
            report.result = ProbeResult::Fail;
            report.detail = "componentTransform runtime column '" + std::string(column)
                + "' did not resolve on entity '" + entityId
                + "'; reconcile against include/n8ro-sim/entity/TransformRuntimeColumns.h";
            return report;
        }
    }

    report.result = ProbeResult::Pass;
    report.detail = "velocityNed.{x,y,z} resolved on '" + entityId + "'";
    return report;
}

} // namespace arkheon::aicommander
