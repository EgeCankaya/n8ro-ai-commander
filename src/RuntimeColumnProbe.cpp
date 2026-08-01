#include "RuntimeColumnProbe.h"

#include <component/ComponentFieldAccess.h>
#include <component/ComponentTypeNames.h>
#include <entity/IEntity.h>
#include <entity/IEntityManager.h>
#include <entity/TransformRuntimeColumns.h>

#include <array>
#include <string>
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

} // namespace

const char* toString(ProbeResult result) {
    switch (result) {
        case ProbeResult::NotRun: return "notRun";
        case ProbeResult::Pass:   return "pass";
        case ProbeResult::Fail:   return "fail";
    }
    return "notRun";
}

ProbeReport probeRuntimeColumnsWith(const std::string& entityId, const ColumnReader& read) {
    ProbeReport report;
    report.probedEntityId = entityId;

    if (entityId.empty() || !read) {
        report.result = ProbeResult::NotRun;
        report.detail = "no entity to probe";
        return report;
    }

    for (const std::string_view column : kRequiredVelocityColumns) {
        if (!read(entityId, column).has_value()) {
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

ProbeReport probeRuntimeColumns(const n8ro::sim::IEntityManager& manager) {
    // Any entity with a transform will do. Since an unresolvable column returns nullopt rather
    // than a fabricated zero (OQ-9), the subject does not need to be moving — a stationary entity
    // proves resolution just as well, and insisting on a moving one risks a false FAIL on the
    // first frame before physics has integrated a velocity.
    std::string subjectId;
    for (const n8ro::sim::IEntity* entity : manager.getAllEntities()) {
        if (entity != nullptr && entity->hasComponent(n8ro::sim::kComponentTransform)) {
            subjectId = entity->getScenarioEntityName();
            break;
        }
    }

    if (subjectId.empty()) {
        ProbeReport report;
        report.result = ProbeResult::NotRun;
        report.detail = "no entity with a componentTransform is loaded yet";
        return report;
    }

    return probeRuntimeColumnsWith(
        subjectId,
        [&manager](const std::string& entityId, std::string_view fieldPath) {
            return n8ro::sim::readComponentFieldReal(
                manager, entityId, n8ro::sim::kComponentTransform, fieldPath);
        });
}

} // namespace arkheon::aicommander
