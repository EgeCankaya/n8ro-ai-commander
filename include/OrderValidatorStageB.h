#pragma once

#include "CommanderConfig.h"
#include "Order.h"
#include "RejectReason.h"

#include <cstdint>
#include <string>
#include <vector>

namespace arkheon::aicommander {

// The live facts Stage B needs, behind an interface.
//
// This exists so the semantic checks are unit-testable without an engine. The production
// implementation reads IEntityManager on the simulation thread; the suite substitutes a fake and
// exercises fratricide, hallucinated targets, and geofence violations directly — which is the only
// way that corpus can run in CI with no scenario loaded.
//
// Every method here is called on the simulation thread only. Nothing in this interface may be
// invoked from a worker: IEntityManager is single-thread-only, and that constraint is the whole
// reason validation is split in two.
class StageBWorldView {
public:
    virtual ~StageBWorldView() = default;

    // B1. IEntityManager::getEntity(id) != nullptr in production.
    [[nodiscard]] virtual bool entityExists(const std::string& entityId) const = 0;

    // B4. IEntity::getTeam(). Empty when the entity is unknown.
    [[nodiscard]] virtual std::string teamOf(const std::string& entityId) const = 0;

    // B5. The commanded entity's current geodetic position, from componentTransform's authored
    // schema leaves. False when unavailable — in which case the geofence cannot be evaluated and
    // the order is refused rather than waved through.
    [[nodiscard]] virtual bool positionOf(
        const std::string& entityId, double& latDeg, double& lonDeg, double& altHaeM) const = 0;
};

// The per-order facts the simulation thread carries alongside the world view.
struct StageBRequest {
    // The entity this order was requested for, and whether it is still commanded.
    std::string commandedEntityId;
    bool onRoster = false;

    // B2. Simulation time when the snapshot was taken, and now.
    double snapshotSimTimeS = 0.0;
    double currentSimTimeS = 0.0;

    // B3. The track picture Tier 1 reported for the window this order's snapshot came from.
    //
    // Validated against the list that ACCOMPANIED the snapshot, not the list current at
    // publication: a target legitimately visible when the order was requested must not be rejected
    // merely because inference outlived the cadence window (ADR-3).
    std::vector<std::string> reportedTrackIds;

    // B7. The serial of the order currently published for this entity, and this order's serial.
    std::int64_t publishedSerial = -1;
    std::int64_t candidateSerial = 0;
};

struct StageBOutcome {
    bool accepted = false;
    RejectReason reason = RejectReason::None;
    std::string detail;
};

// Stage B — semantic validation against live state, simulation thread (AIC-VAL-1).
//
// Runs entirely within onTickFrame and must stay inside the per-frame budget, so every check is
// O(1) or O(reported tracks), which is capped at commander.maxTracksInPrompt.
[[nodiscard]] StageBOutcome validateStageB(
    const Order& order,
    const StageBRequest& request,
    const CommanderConfig& config,
    const StageBWorldView& world);

// Great-circle distance in metres between two geodetic points, plus the altitude delta.
// Spherical-earth approximation, matching the idiom the reference Tier-1 script already uses for
// its own range checks — the geofence is a coarse safety bound, not a navigation computation.
[[nodiscard]] double slantRangeM(
    double latADeg, double lonADeg, double altAM,
    double latBDeg, double lonBDeg, double altBM);

} // namespace arkheon::aicommander
