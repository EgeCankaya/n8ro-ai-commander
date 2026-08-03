#include "OrderValidatorStageB.h"

#include <cmath>
#include <string>

namespace arkheon::aicommander {

namespace {

StageBOutcome reject(RejectReason reason, std::string detail) {
    StageBOutcome outcome;
    outcome.accepted = false;
    outcome.reason = reason;
    outcome.detail = std::move(detail);
    return outcome;
}

[[nodiscard]] std::string formatMetres(double value) {
    return std::to_string(static_cast<long long>(value)) + " m";
}

} // namespace

double slantRangeM(
    double latADeg, double lonADeg, double altAM,
    double latBDeg, double lonBDeg, double altBM) {
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    constexpr double kEarthRadiusM = 6371000.0;

    const double phi1 = latADeg * kDegToRad;
    const double phi2 = latBDeg * kDegToRad;
    const double deltaPhi = (latBDeg - latADeg) * kDegToRad;
    const double deltaLambda = (lonBDeg - lonADeg) * kDegToRad;

    const double a = std::sin(deltaPhi / 2.0) * std::sin(deltaPhi / 2.0)
        + std::cos(phi1) * std::cos(phi2) * std::sin(deltaLambda / 2.0) * std::sin(deltaLambda / 2.0);
    const double groundM = 2.0 * kEarthRadiusM * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    const double deltaAlt = altBM - altAM;
    return std::sqrt(groundM * groundM + deltaAlt * deltaAlt);
}

StageBOutcome validateStageB(
    const Order& order,
    const StageBRequest& request,
    const CommanderConfig& config,
    const StageBWorldView& world) {

    // -- B1: roster and liveness ---------------------------------------------------------------
    if (!request.onRoster) {
        return reject(RejectReason::Roster,
            "entity '" + order.entityId + "' is not on the commander roster");
    }
    if (order.entityId != request.commandedEntityId) {
        return reject(RejectReason::Roster,
            "order names '" + order.entityId + "' but was requested for '"
                + request.commandedEntityId + "'");
    }
    if (!world.entityExists(order.entityId)) {
        // The entity was destroyed while the order was in flight. Common at the end of an
        // engagement, and not an error — just no longer actionable.
        return reject(RejectReason::Roster,
            "entity '" + order.entityId + "' no longer exists");
    }

    // -- B2: staleness -------------------------------------------------------------------------
    const double ageS = request.currentSimTimeS - request.snapshotSimTimeS;
    if (ageS > config.maxOrderAgeS) {
        return reject(RejectReason::Stale,
            "order derives from a snapshot " + std::to_string(static_cast<int>(ageS))
                + " s old, over the " + std::to_string(static_cast<int>(config.maxOrderAgeS))
                + " s bound");
    }
    if (ageS < 0.0) {
        // Simulation time ran backwards — a scenario reload while a request was in flight. The
        // order describes a world that no longer exists.
        return reject(RejectReason::Stale,
            "snapshot simulation time is in the future; the scenario was reloaded mid-request");
    }

    // -- B7: supersession ----------------------------------------------------------------------
    // Checked before the target work below because it is the cheapest way to discard an order that
    // a newer one has already overtaken, which happens whenever inference outlives the cadence.
    if (request.candidateSerial <= request.publishedSerial) {
        return reject(RejectReason::Superseded,
            "order serial " + std::to_string(request.candidateSerial)
                + " is not newer than the published serial "
                + std::to_string(request.publishedSerial));
    }

    // -- B3 / B4: the target ---------------------------------------------------------------------
    if (!order.targetEntityId.empty()) {
        // B3. Validated against what Tier 1 actually reported, not against an engine-side track
        // query — the plugin has no C++ read seam for sensor tracks (PRD Corrections, item 4).
        // An empty reported list therefore rejects every targeted order, which is the intended
        // degradation: an unreported track cannot be engaged on model initiative.
        bool reported = false;
        for (const std::string& trackId : request.reportedTrackIds) {
            if (trackId == order.targetEntityId) {
                reported = true;
                break;
            }
        }
        if (!reported) {
            return reject(RejectReason::Track,
                "targetEntityId '" + order.targetEntityId
                    + "' is not in the reported track list ("
                    + std::to_string(request.reportedTrackIds.size()) + " tracks reported)");
        }

        if (!world.entityExists(order.targetEntityId)) {
            return reject(RejectReason::Track,
                "targetEntityId '" + order.targetEntityId + "' does not exist");
        }

        // B4. Fratricide. The teams must be known AND different — an unknown team is a rejection,
        // not a pass, because "we could not tell whose side it is on" is not a basis for shooting.
        const std::string ownTeam = world.teamOf(order.entityId);
        const std::string targetTeam = world.teamOf(order.targetEntityId);
        if (ownTeam.empty() || targetTeam.empty()) {
            return reject(RejectReason::Fratricide,
                "team unknown for '" + (ownTeam.empty() ? order.entityId : order.targetEntityId)
                    + "'; refusing to assume hostility");
        }
        if (ownTeam == targetTeam) {
            return reject(RejectReason::Fratricide,
                "target '" + order.targetEntityId + "' is on own team '" + ownTeam + "'");
        }
    }

    // -- B8: an offensive posture on an aircraft with nothing left to shoot ----------------------
    // Deliberately AFTER B3/B4: a hallucinated or friendly target on a dry aircraft is a more
    // specific complaint than a dry rail, and the more specific diagnosis should win the record.
    //
    // The asymmetry with B3 is the whole point of this check. An EMPTY reportedAmmoCounts means
    // Tier 1 reported no stores at all, which is a script that does not call reportLoadout — not
    // an aircraft that is out. That script is required to keep receiving orders, and its targeted
    // orders are already refused by B3, because a script reporting no loadout reports no tracks
    // either. Absence of information is not evidence, and the validator does not invent it.
    if (order.posture == Posture::Engage || order.posture == Posture::Crank) {
        bool anyRoundsRemaining = false;
        for (const int ammoCount : request.reportedAmmoCounts) {
            if (ammoCount > 0) {
                anyRoundsRemaining = true;
                break;
            }
        }
        if (!request.reportedAmmoCounts.empty() && !anyRoundsRemaining) {
            return reject(RejectReason::Loadout,
                std::string("posture ") + toString(order.posture) + " ordered with every reported "
                    + "hardpoint dry (" + std::to_string(request.reportedAmmoCounts.size())
                    + " reported, 0 rounds remaining)");
        }
    }

    // -- B6: the safety envelope ----------------------------------------------------------------
    // Reject, do not clamp. A speed outside the envelope is not repaired down to the bound: the
    // model asked for something the operator did not authorize, and quietly granting a different
    // order than the one issued is exactly what "reject, don't repair" forbids.
    if (order.cruiseSpeedMps > config.maxSpeedMps) {
        return reject(RejectReason::Clamp,
            "cruiseSpeedMps " + std::to_string(static_cast<int>(order.cruiseSpeedMps))
                + " exceeds safety.maxSpeedMps "
                + std::to_string(static_cast<int>(config.maxSpeedMps)));
    }

    if (order.hasWaypoint()) {
        if (order.altitudeHaeM < config.minAltitudeHaeM
            || order.altitudeHaeM > config.maxAltitudeHaeM) {
            return reject(RejectReason::Clamp,
                "waypoint altitude " + std::to_string(static_cast<int>(order.altitudeHaeM))
                    + " m HAE is outside the configured envelope ["
                    + std::to_string(static_cast<int>(config.minAltitudeHaeM)) + ", "
                    + std::to_string(static_cast<int>(config.maxAltitudeHaeM)) + "]");
        }

        // -- B5: geofence ------------------------------------------------------------------------
        double ownLat = 0.0;
        double ownLon = 0.0;
        double ownAlt = 0.0;
        if (!world.positionOf(order.entityId, ownLat, ownLon, ownAlt)) {
            return reject(RejectReason::Geofence,
                "cannot read the commanded entity's position; the geofence is unevaluable");
        }
        const double distanceM = slantRangeM(
            ownLat, ownLon, ownAlt, order.latitudeDeg, order.longitudeDeg, order.altitudeHaeM);
        if (distanceM > config.geofenceRadiusM) {
            return reject(RejectReason::Geofence,
                "waypoint is " + formatMetres(distanceM) + " away, beyond safety.geofenceRadiusM "
                    + formatMetres(config.geofenceRadiusM));
        }
    }

    StageBOutcome outcome;
    outcome.accepted = true;
    outcome.reason = RejectReason::None;
    return outcome;
}

} // namespace arkheon::aicommander
