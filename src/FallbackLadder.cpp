#include "FallbackLadder.h"

namespace arkheon::aicommander {

const char* toString(FallbackLevel level) {
    switch (level) {
        case FallbackLevel::Live:     return "live";
        case FallbackLevel::Retained: return "retained";
        case FallbackLevel::Standing: return "standing";
        case FallbackLevel::Released: return "released";
    }
    return "released";
}

void acceptOrder(LadderState& state, const Order& order, std::int64_t serial, double nowSimTimeS) {
    state.published.valid = true;
    state.published.order = order;
    state.published.serial = serial;
    state.published.acceptedSimTimeS = nowSimTimeS;
    state.lastAcceptedSimTimeS = nowSimTimeS;
    state.level = FallbackLevel::Live;
}

bool advanceFallbackLadder(
    LadderState& state,
    double nowSimTimeS,
    const CommanderConfig& config,
    const EntityPosition& position) {

    const FallbackLevel previous = state.level;

    // Never had an order. Nothing to degrade from; the entity is Tier 1's until one arrives.
    if (state.lastAcceptedSimTimeS < 0.0) {
        state.level = FallbackLevel::Released;
        state.published.valid = false;
        return previous != state.level;
    }

    const double sinceAcceptedS = nowSimTimeS - state.lastAcceptedSimTimeS;

    // Simulation time ran backwards — a scenario reload. Treat the history as gone rather than
    // computing a negative age and concluding the order is fresh.
    if (sinceAcceptedS < 0.0) {
        state.lastAcceptedSimTimeS = -1.0;
        state.level = FallbackLevel::Released;
        state.published.valid = false;
        return previous != state.level;
    }

    if (sinceAcceptedS >= config.releaseAfterS) {
        // Step 3: release to Tier 1. Every getter reports "no order", which the reference script
        // reads as "commander absent" and answers with navigation.resumeWaypointFollowing.
        state.level = FallbackLevel::Released;
        state.published.valid = false;
        state.published.serial = -1;
    } else if (sinceAcceptedS >= config.orderValidityS) {
        // Step 2: the standing order. Synthesized once, on the transition into Standing — not
        // rebuilt per frame, or the hold point would chase the entity as it drifted and the
        // "hold where you were when the link died" contract would quietly become "hold wherever
        // you are now".
        if (previous != FallbackLevel::Standing) {
            Order standing;
            standing.schemaVersion = state.published.order.schemaVersion;
            standing.entityId = state.published.order.entityId;
            standing.posture = Posture::Hold;
            standing.targetEntityId.clear();
            standing.cruiseSpeedMps = state.published.order.cruiseSpeedMps;
            standing.orbitRadiusM = config.defaultOrbitRadiusM;
            // weaponsFree, not weaponsTight. AIC-VAL-2 specified this in PRD v1.8.30 and the code
            // did not follow until v1.8.34 (§Corrections item 50(d)). weaponsTight permits fire
            // ONLY against the ordered targetEntityId, and this order carries no target because
            // `hold` carries none - so the rung was a state in which the entity could neither
            // shoot nor be permitted to choose something to shoot at. It is not a widening of
            // authority: weaponsFree is the script's own target selection, which is exactly what
            // an entity with no commander installed already does, and rung 3 degrades to that
            // outright two minutes later.
            standing.roe = Roe::WeaponsFree;
            standing.reason = "Standing order: last accepted order expired.";

            if (position.known) {
                standing.latitudeDeg = position.latitudeDeg;
                standing.longitudeDeg = position.longitudeDeg;
                standing.altitudeHaeM = position.altitudeHaeM;
            } else {
                // No position to hold over. Releasing is the honest answer — a hold order with a
                // fabricated waypoint would send the entity somewhere nobody chose.
                state.level = FallbackLevel::Released;
                state.published.valid = false;
                state.published.serial = -1;
                return previous != state.level;
            }

            state.published.order = standing;
            state.published.valid = true;
        }
        state.level = FallbackLevel::Standing;
    } else if (sinceAcceptedS >= config.cadenceS) {
        // Step 1: retain. The last accepted order stands unchanged; only the level moves, so a
        // script watching getOrderSerial sees no new order and keeps executing the current one.
        state.level = FallbackLevel::Retained;
    } else {
        state.level = FallbackLevel::Live;
    }

    return previous != state.level;
}

} // namespace arkheon::aicommander
