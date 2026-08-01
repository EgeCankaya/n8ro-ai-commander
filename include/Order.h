#pragma once

#include <string>

namespace arkheon::aicommander {

// The posture vocabulary (AIC-ORD-1). Six values, taken from the reference Tier-1 behaviour's own
// posture ladder in oppint_red_interceptor.lua rather than invented — every one of them already
// has a meaning a script author recognizes, and AIC-ORD-2 maps each to existing verbs.
//
// Adding a posture means adding a row to AIC-ORD-2's mapping table first: a posture with no verb
// mapping is an order nobody can execute.
enum class Posture {
    Ingress,
    Engage,
    Crank,
    Defend,
    Hold,
    Rtb,
};

// Rules of engagement. Orthogonal to posture: ROE constrains firing regardless of what the entity
// is flying toward.
enum class Roe {
    WeaponsFree,
    WeaponsTight,
    WeaponsHold,
};

[[nodiscard]] const char* toString(Posture posture);
[[nodiscard]] const char* toString(Roe roe);
[[nodiscard]] bool tryParsePosture(const std::string& text, Posture& out);
[[nodiscard]] bool tryParseRoe(const std::string& text, Roe& out);

// True when the posture requires a target. The model supplies WHICH target; the script computes
// the geometry (AIC-ORD-2).
[[nodiscard]] bool postureRequiresTarget(Posture posture);

// True when the posture carries a waypoint. For engage / crank / defend the script computes the
// steer point itself, so a waypoint would be ignored — and an ignored field in an accepted order
// is a field nobody specified, so the schema forbids it rather than dropping it.
[[nodiscard]] bool postureRequiresWaypoint(Posture posture);

// A validated order. Reaching this type means the document passed Stage A; it does NOT mean the
// order is safe to publish — Stage B still has to check it against live state.
struct Order {
    int schemaVersion = 1;
    std::string entityId;
    Posture posture = Posture::Hold;
    std::string targetEntityId;   // Empty unless the posture requires a target.

    // Waypoint. Meaningful only when postureRequiresWaypoint(posture).
    double latitudeDeg = 0.0;     // Deg, geodetic WGS-84.
    double longitudeDeg = 0.0;    // Deg, geodetic WGS-84.
    double altitudeHaeM = 0.0;    // M, height above the WGS-84 ellipsoid. NOT AGL, NOT MSL.

    double cruiseSpeedMps = 0.0;  // Mps, ground speed.
    double orbitRadiusM = 0.0;    // M. Non-zero only when posture == Hold.
    Roe roe = Roe::WeaponsHold;

    // Advisory only, never parsed. It exists so a human reading the order log can see what the
    // model thought it was doing; nothing downstream branches on it.
    std::string reason;

    [[nodiscard]] bool hasWaypoint() const { return postureRequiresWaypoint(posture); }
};

} // namespace arkheon::aicommander
