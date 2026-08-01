#include "StubLlmClient.h"

#include "Order.h"
#include "OrderSchema.h"

#include <core/json/JsonValue.h>

#include <string>

namespace arkheon::aicommander {

namespace {

using n8ro::core::JsonValue;

// Renders an order document the same way a model would, so the stub exercises the real parse and
// validation path rather than short-circuiting it. Building it through JsonValue (rather than
// string concatenation) means an entity id carrying an awkward character produces a correctly
// escaped document — the stub must not be the one component that cannot represent what the others
// have to handle.
std::string renderOrder(
    const std::string& entityId,
    Posture posture,
    const std::string& targetEntityId,
    double latDeg, double lonDeg, double altM,
    double cruiseSpeedMps,
    double orbitRadiusM,
    Roe roe,
    const std::string& reason) {
    JsonValue doc = JsonValue::object();
    (void)doc.setInt64("schemaVersion", kOrderSchemaVersion);
    (void)doc.setString("entityId", entityId);
    (void)doc.setString("posture", toString(posture));
    (void)doc.setString("targetEntityId", targetEntityId);
    (void)doc.setDouble("cruiseSpeedMps", cruiseSpeedMps);
    (void)doc.setDouble("orbitRadiusM", orbitRadiusM);
    (void)doc.setString("roe", toString(roe));
    (void)doc.setString("reason", reason);

    if (postureRequiresWaypoint(posture)) {
        JsonValue waypoint = JsonValue::object();
        (void)waypoint.setDouble("latitudeDeg", latDeg);
        (void)waypoint.setDouble("longitudeDeg", lonDeg);
        (void)waypoint.setDouble("altitudeHaeM", altM);
        (void)doc.set("waypoint", waypoint);
    }

    return doc.toString();
}

} // namespace

LlmResult StubLlmClient::request(const LlmRequest& request) {
    ++invocations_;

    LlmResult result;
    result.latencyMs = 0;   // No I/O, so no latency to report. Honest zero, not a fabricated value.

    if (injectTransportFailure_) {
        injectTransportFailure_ = false;
        result.completed = false;
        result.transportDetail = "injected transport failure";
        return result;
    }

    result.completed = true;
    result.statusCode = 200;

    if (hasInjectedBody_) {
        hasInjectedBody_ = false;
        result.body = injectedBody_;
        injectedBody_.clear();
        return result;
    }

    const OrderSnapshot& snapshot = request.snapshot;

    // The fixed table. Cycles so a long run exercises every posture and both conditional shapes,
    // and keys off the snapshot so the order is always self-consistent with what was asked about.
    //
    // Where a target is needed, the FIRST REPORTED TRACK is used rather than an invented id. That
    // matters: a stub that named a target Tier 1 never reported would be rejected by Stage-B check
    // B3 on every cycle, and the integration test would be measuring the rejection path while
    // looking like it measured the acceptance path.
    const bool haveTrack = !snapshot.tracks.empty();
    const std::string target = haveTrack ? snapshot.tracks.front().targetEntityId : std::string();

    const std::int64_t phase = haveTrack ? (invocations_ % 5) : (invocations_ % 3);

    switch (phase) {
        case 0:
            result.body = renderOrder(snapshot.entityId, Posture::Ingress, "",
                snapshot.latitudeDeg + 0.05, snapshot.longitudeDeg + 0.05, 9000.0,
                240.0, 0.0, Roe::WeaponsTight,
                "Stub: pressing toward the assigned area.");
            break;
        case 1:
            result.body = renderOrder(snapshot.entityId, Posture::Hold, "",
                snapshot.latitudeDeg, snapshot.longitudeDeg, 9000.0,
                200.0, 8000.0, Roe::WeaponsTight,
                "Stub: holding station pending contact.");
            break;
        case 2:
            result.body = renderOrder(snapshot.entityId, Posture::Rtb, "",
                snapshot.latitudeDeg - 0.05, snapshot.longitudeDeg - 0.05, 6000.0,
                260.0, 0.0, Roe::WeaponsHold,
                "Stub: egressing to the recovery point.");
            break;
        case 3:
            result.body = renderOrder(snapshot.entityId, Posture::Engage, target,
                0.0, 0.0, 0.0, 300.0, 0.0, Roe::WeaponsFree,
                "Stub: committing on the nearest reported contact.");
            break;
        default:
            result.body = renderOrder(snapshot.entityId, Posture::Crank, target,
                0.0, 0.0, 0.0, 280.0, 0.0, Roe::WeaponsTight,
                "Stub: cranking while the shot is assessed.");
            break;
    }

    return result;
}

} // namespace arkheon::aicommander
