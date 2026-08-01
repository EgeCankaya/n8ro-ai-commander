#include "RuntimeColumnProbe.h"

#include <component/ComponentFieldAccess.h>
#include <component/ComponentTypeNames.h>
#include <entity/IEntity.h>
#include <entity/IEntityManager.h>
#include <entity/TransformRuntimeColumns.h>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace arkheon::aicommander {

namespace {

// The three velocity leaves the snapshot depends on, in the dot-joined form
// TransformRuntimeColumns.h declares. Composed from the header's own constants rather than
// re-spelled here, so a rename in the SDK breaks the build instead of the runtime.
constexpr std::string_view kVelocityNorth = n8ro::sim::kTransformColumnVelocityNorth;
constexpr std::string_view kVelocityEast = n8ro::sim::kTransformColumnVelocityEast;
constexpr std::string_view kVelocityDown = n8ro::sim::kTransformColumnVelocityDown;

// A path that is deliberately not a column. Reading it is how the probe learns whether an
// unresolvable name is silent (returns a value) or loud (returns nullopt) — the OQ-9 question.
constexpr std::string_view kBogusVelocityLeaf = "velocityNed.q";

// The schema's slash form of the same leaf. If this resolves, the two conventions are
// interchangeable after all and §Naming and path conventions can be simplified.
constexpr std::string_view kSlashVelocityNorth = "velocityNed/x";

[[nodiscard]] std::string formatDouble(double value) {
    std::string text = std::to_string(value);
    // Trim the trailing zeros std::to_string always emits, so the log line reads as a number a
    // human wrote rather than as a fixed-point artifact.
    const std::size_t lastNonZero = text.find_last_not_of('0');
    if (lastNonZero != std::string::npos && text[lastNonZero] == '.') {
        text.erase(lastNonZero + 2);
    } else if (lastNonZero != std::string::npos) {
        text.erase(lastNonZero + 1);
    }
    return text;
}

// Picks the entity most likely to be moving: the one with the greatest authored speedMps. An
// entity authored at rest cannot distinguish "column missing" from "genuinely zero", so probing
// against it would prove nothing.
[[nodiscard]] const n8ro::sim::IEntity* pickFastestEntity(
    const n8ro::sim::IEntityManager& manager,
    double& outAuthoredSpeedMps) {
    const std::vector<const n8ro::sim::IEntity*> entities = manager.getAllEntities();
    const n8ro::sim::IEntity* best = nullptr;
    double bestSpeed = 0.0;

    for (const n8ro::sim::IEntity* entity : entities) {
        if (entity == nullptr || !entity->hasComponent(n8ro::sim::kComponentTransform)) {
            continue;
        }
        const std::string id = entity->getScenarioEntityName();
        const std::optional<double> speed = n8ro::sim::readComponentFieldReal(
            manager, id, n8ro::sim::kComponentTransform, "speedMps");
        const double authored = speed.value_or(0.0);
        if (best == nullptr || authored > bestSpeed) {
            best = entity;
            bestSpeed = authored;
        }
    }

    outAuthoredSpeedMps = bestSpeed;
    return best;
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

    double authoredSpeedMps = 0.0;
    const n8ro::sim::IEntity* subject = pickFastestEntity(manager, authoredSpeedMps);
    if (subject == nullptr) {
        report.result = ProbeResult::NotRun;
        report.detail = "no entity with a componentTransform is loaded yet";
        return report;
    }

    const std::string entityId = subject->getScenarioEntityName();
    report.probedEntityId = entityId;

    const std::optional<double> north = n8ro::sim::readComponentFieldReal(
        manager, entityId, n8ro::sim::kComponentTransform, kVelocityNorth);
    const std::optional<double> east = n8ro::sim::readComponentFieldReal(
        manager, entityId, n8ro::sim::kComponentTransform, kVelocityEast);
    const std::optional<double> down = n8ro::sim::readComponentFieldReal(
        manager, entityId, n8ro::sim::kComponentTransform, kVelocityDown);
    const std::optional<double> bogus = n8ro::sim::readComponentFieldReal(
        manager, entityId, n8ro::sim::kComponentTransform, kBogusVelocityLeaf);
    const std::optional<double> slash = n8ro::sim::readComponentFieldReal(
        manager, entityId, n8ro::sim::kComponentTransform, kSlashVelocityNorth);

    report.dotPathResolved = north.has_value() && east.has_value() && down.has_value();
    report.slashPathResolved = slash.has_value();
    report.bogusPathResolved = bogus.has_value();
    report.dotPathValue = north.value_or(0.0);
    report.bogusPathValue = bogus.value_or(0.0);

    if (!report.dotPathResolved) {
        report.result = ProbeResult::Fail;
        report.detail = "componentTransform runtime column '" + std::string(kVelocityNorth)
            + "' did not resolve (entity '" + entityId
            + "'); check include/n8ro-sim/entity/TransformRuntimeColumns.h against this release";
        return report;
    }

    const double speedNed = std::sqrt(
        north.value() * north.value() + east.value() * east.value() + down.value() * down.value());

    // The heuristic AIC-ARCH-4 specifies. An entity authored with a real ground speed cannot
    // legitimately read back an all-zero NED velocity once it is moving, so an all-zero reading
    // here is the signature of a resolved-but-fabricated column rather than of a stationary jet.
    //
    // The generous margin matters: this runs at startup, possibly on the very first frame, where
    // the physics model may not have integrated a velocity yet. A false FAIL would disable the
    // commander on a healthy tree, which is worse than a late-caught typo.
    constexpr double kAuthoredSpeedFloorMps = 1.0;
    constexpr double kObservedSpeedFloorMps = 0.01;
    if (authoredSpeedMps > kAuthoredSpeedFloorMps && speedNed < kObservedSpeedFloorMps) {
        report.result = ProbeResult::Fail;
        report.detail = "entity '" + entityId + "' is authored at " + formatDouble(authoredSpeedMps)
            + " m/s but its velocityNed columns read all-zero; the columns resolve but carry no data";
        return report;
    }

    report.result = ProbeResult::Pass;
    report.detail = "velocityNed.{x,y,z} resolved on '" + entityId + "'; |v| = "
        + formatDouble(speedNed) + " m/s, authored speedMps = " + formatDouble(authoredSpeedMps);
    return report;
}

} // namespace arkheon::aicommander
