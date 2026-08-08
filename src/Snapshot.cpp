#include "Snapshot.h"

#include <algorithm>
#include <cmath>

namespace arkheon::aicommander {

const char* toString(TrackKind kind) {
    switch (kind) {
        case TrackKind::Air:      return "air";
        case TrackKind::Ground:   return "ground";
        case TrackKind::Surface:  return "surface";
        case TrackKind::Munition: return "munition";
        case TrackKind::Other:    break;
    }
    return "other";
}

const char* toString(TrackTeam team) {
    switch (team) {
        case TrackTeam::Hostile:  return "hostile";
        case TrackTeam::Friendly: return "friendly";
        case TrackTeam::Unknown:  break;
    }
    return "unknown";
}

// Both parsers CLAMP rather than fail - see the header for why that is the requirement and not a
// convenience. An exact, case-sensitive match is required: accepting "AIR" or " air " would put a
// normalization step between Tier 1 and the prompt, and the point of a closed vocabulary is that
// there is nothing between them.
TrackKind parseTrackKind(const std::string& text) {
    if (text == "air")      return TrackKind::Air;
    if (text == "ground")   return TrackKind::Ground;
    if (text == "surface")  return TrackKind::Surface;
    if (text == "munition") return TrackKind::Munition;
    return TrackKind::Other;
}

TrackTeam parseTrackTeam(const std::string& text) {
    if (text == "hostile")  return TrackTeam::Hostile;
    if (text == "friendly") return TrackTeam::Friendly;
    return TrackTeam::Unknown;
}

double groundSpeedMps(double velNMps, double velEMps, double velDMps) {
    // std::sqrt of the sum of squares, not std::hypot: hypot's three-argument overload is C++17 and
    // is the better choice for extreme magnitudes, but the inputs here are aircraft velocities in
    // m/s and cannot approach the overflow range hypot exists to protect. The plain form is what a
    // reader checking this against the PRD's "||velocityNed||" can verify at a glance.
    return std::sqrt(velNMps * velNMps + velEMps * velEMps + velDMps * velDMps);
}

double courseOverGroundDeg(double velNMps, double velEMps) {
    // atan2(East, North), not atan2(y, x) with the usual mathematical convention. NED puts North on
    // x and East on y, and compass bearings run clockwise from North - so North is the FIRST
    // argument's axis and East the second's, which is the transposition this call would otherwise
    // invite. Getting it backwards yields a bearing mirrored about the 45-degree line, which reads
    // plausibly on a due-north or due-east sample and is wrong everywhere else.
    constexpr double kRadToDeg = 57.295779513082320876798154814105;
    const double deg = std::atan2(velEMps, velNMps) * kRadToDeg;
    // atan2 returns (-180, 180]; compass bearings are [0, 360).
    return deg < 0.0 ? deg + 360.0 : deg;
}

void canonicalizeSnapshot(OrderSnapshot& snapshot) {
    // Deterministic order, not Lua call order. Two snapshots holding the same picture must render
    // byte-identically or `snapshotHash` becomes a function of how the script happened to iterate,
    // which would make prompt-drift detection and replay comparison meaningless.
    std::sort(snapshot.tracks.begin(), snapshot.tracks.end(),
              [](const TrackReport& a, const TrackReport& b) {
                  return a.targetEntityId < b.targetEntityId;
              });
    std::sort(snapshot.loadout.begin(), snapshot.loadout.end(),
              [](const LoadoutReport& a, const LoadoutReport& b) {
                  return a.hardpointName < b.hardpointName;
              });
}

} // namespace arkheon::aicommander
