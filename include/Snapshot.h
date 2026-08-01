#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arkheon::aicommander {

// One reported sensor track. Pushed in by Tier 1 via aiCommander.reportTrack, because the plugin
// has no C++ read seam for sensor detections (PRD Corrections, item 4).
struct TrackReport {
    std::string targetEntityId;
    double rangeM = 0.0;   // metres, slant range (sensor stub's range_m)
    double snrDb = 0.0;    // decibels (sensor stub's snr_DB)
};

// One hardpoint's remaining stores. Pushed in by Tier 1 via aiCommander.reportLoadout.
struct LoadoutReport {
    std::string hardpointName;
    std::string weaponProfileName;
    int ammoCount = 0;
    int ammoMax = 0;
};

// Everything the worker is allowed to see about an entity, copied by value (AIC-ARCH-2).
//
// This type is the thread boundary. It holds no pointer, no reference, and no handle into engine
// state — only owned strings and scalars — so a worker holding one cannot reach IEntityManager,
// IScenarioManager, MessageBusPacked, ISimulationEngine, or ScriptingApiContext even by accident.
// Every one of those is single-thread-only, and the alternative to copying is undefined behaviour
// rather than a slower design.
//
// It is deliberately cheap: ~4 KB at the default eight tracks, which is why latest-wins slots can
// simply overwrite rather than queue.
struct OrderSnapshot {
    // Identity and time.
    std::string entityId;
    double simTimeS = 0.0;
    std::int64_t serial = 0;      // Monotonic per entity; becomes the order's serial if accepted.

    // Own-ship state, read on the simulation thread from componentTransform.
    double latitudeDeg = 0.0;     // Deg, geodetic WGS-84 (schema leaf).
    double longitudeDeg = 0.0;    // Deg, geodetic WGS-84 (schema leaf).
    double altitudeHaeM = 0.0;    // M, height above the WGS-84 ellipsoid (schema leaf).
    double headingDeg = 0.0;      // Deg clockwise from true north (schema leaf).
    double velNMps = 0.0;         // m/s North (runtime column velocityNed.x).
    double velEMps = 0.0;         // m/s East  (runtime column velocityNed.y).
    double velDMps = 0.0;         // m/s Down  (runtime column velocityNed.z).
    std::string team;

    // What Tier 1 reported for this cadence window. Rendered in a deterministic order, not in Lua
    // call order, so two snapshots holding the same picture produce identical prompt bytes and the
    // same snapshotHash (AIC-BE-3, AIC-DET-1).
    std::vector<TrackReport> tracks;
    std::vector<LoadoutReport> loadout;

    // Deterministic Tier-1 context, already sanitized and capped on ingress.
    std::string situationNote;
};

// Sorts the reported lists into their canonical order: tracks ascending by targetEntityId, loadout
// ascending by hardpointName. Called once when the snapshot is taken, so every downstream consumer
// — prompt renderer, snapshot hash, order record — sees the same bytes for the same picture.
void canonicalizeSnapshot(OrderSnapshot& snapshot);

} // namespace arkheon::aicommander
