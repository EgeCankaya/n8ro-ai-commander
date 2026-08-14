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

// The geodetic triple the snapshot and Stage B both depend on (v1.8.55). Unlike the velocity columns
// above, these are authored SCHEMA leaves - slash-joined, and spelled here because no SDK header
// owns these names. The source is the schema reference,
// /datablocks/componentTransform/positionGeodetic/... , with the /datablocks/<componentType>/
// prefix dropped per the field-path convention.
constexpr std::array<std::string_view, 3> kRequiredGeodeticLeaves{
    "positionGeodetic/latitudeDeg",
    "positionGeodetic/longitudeDeg",
    "positionGeodetic/altitudeHaeM",
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

ProbeReport probeSnapshotLeavesWith(
    const std::string& entityId, const ColumnReader& read, const HealthLeafProbe& probeHealth) {
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

    for (const std::string_view leaf : kRequiredGeodeticLeaves) {
        if (!read(entityId, leaf).has_value()) {
            report.result = ProbeResult::Fail;
            report.detail = "componentTransform schema leaf '" + std::string(leaf)
                + "' did not resolve on entity '" + entityId
                + "'; reconcile against dev/ai-coding/schema-reference/schema-reference.json";
            return report;
        }
    }

    // Health is checked AFTER the fatal leaves and cannot change the verdict. An unreadable tier
    // mutes the C27 uncommandable guard, and a muted guard is worth a warning - but never worth
    // disabling a commander that is otherwise fully able to run. See the header for the argument.
    if (probeHealth && !probeHealth(entityId)) {
        report.warning = "componentLifecycle/health did not resolve on entity '" + entityId
            + "'; the commander is ENABLED and running, but the guard that stops it commanding a "
              "wrecked airframe cannot read the tier and will treat every aircraft as commandable "
              "(PRD C27). Reconcile against schema-reference.json";
    }

    report.result = ProbeResult::Pass;
    report.detail = "velocityNed.{x,y,z} and positionGeodetic.{lat,lon,altHae} resolved on '"
        + entityId + "'";
    return report;
}

ProbeReport probeRuntimeColumnsWith(const std::string& entityId, const ColumnReader& read) {
    return probeSnapshotLeavesWith(entityId, read, nullptr);
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

    return probeSnapshotLeavesWith(
        subjectId,
        [&manager](const std::string& entityId, std::string_view fieldPath) {
            return n8ro::sim::readComponentFieldReal(
                manager, entityId, n8ro::sim::kComponentTransform, fieldPath);
        },
        [&manager](const std::string& entityId) {
            // Resolve-check only. A tier this build cannot interpret is still a tier that RESOLVED,
            // so the question here is narrower than readHealthTier's: does the leaf exist at all?
            return n8ro::sim::readComponentFieldInt(
                       manager, entityId, n8ro::sim::kComponentLifecycle, "health").has_value()
                || n8ro::sim::readComponentFieldText(
                       manager, entityId, n8ro::sim::kComponentLifecycle, "health").has_value();
        });
}

} // namespace arkheon::aicommander
