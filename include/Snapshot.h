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
    // Course over ground, degrees clockwise from true north, in [0, 360).
    //
    // DERIVED from velocityNed, and this replaced a read of the componentTransform/headingDeg schema
    // leaf in v1.8.28 (C18). That leaf is documented as the heading "at t=0" on a component whose
    // runtime motion is "thereafter owned by the physics pass, not by this datablock" - and measured
    // over a 600 s run it never moved: 270.000 on both entities, one distinct value across fourteen
    // samples, while velN swung from -280 to +304. At t=40.1 the prompt reported the aircraft flying
    // WEST while it was flying EAST, 178.4 degrees out.
    //
    // NAMED `course`, NOT `heading`, on purpose. atan2(velE, velN) yields the direction of travel,
    // not nose attitude; true heading would need the orientationBodyToNedQuat runtime columns, which
    // AIC-ARCH-4 does not probe. In level unaccelerated flight they coincide. Giving it the accurate
    // name is the point - the defect being fixed here IS a quantity that carried a name it had not
    // earned, and repeating that in the replacement would be the same mistake twice.
    double courseDeg = 0.0;
    double velNMps = 0.0;         // m/s North (runtime column velocityNed.x).
    double velEMps = 0.0;         // m/s East  (runtime column velocityNed.y).
    double velDMps = 0.0;         // m/s Down  (runtime column velocityNed.z).

    // Ground speed as a MAGNITUDE, m/s: ||velocityNed|| over the three signed components above
    // (PRD v1.8.25, §Exactly what is transmitted, §Corrections item 41(e), closing C15).
    //
    // DERIVED, NOT READ, and that is the decision rather than an implementation detail.
    // componentTransform DOES carry a `speedMps` schema leaf, and reading it would be the obvious
    // implementation and the wrong one: schema-reference.json documents it as "Initial ground speed
    // at t=0", on a component whose own description says runtime pose and motion are "thereafter
    // owned by the physics pass, not by this datablock". Reading it would put a SPAWN-TIME CONSTANT
    // in the snapshot under a name that reads as current. TransformRuntimeColumns.h declares no
    // runtime speed or heading column, so velocityNed.* is the only live motion state there is and
    // a live ground speed must be computed from it.
    //
    // WHY IT EXISTS AT ALL. Its absence was C15. The model is asked for a cruise speed and was given
    // three SIGNED components and no scalar; in four of four archived runs it copied velEMps -
    // correct in magnitude, wrong in sign - and the whole order was rejected for it. It transmits
    // nothing new: it is a function of three fields already on the allowlist, so AIC-SEC-2's
    // boundary does not move.
    double speedMps = 0.0;

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

// Ground speed as a magnitude: ||(velN, velE, velD)||, m/s (PRD v1.8.25, C15).
//
// A free function rather than three lines inside buildSnapshot, so the definition the plugin ships
// is the definition a test can call. buildSnapshot needs an IEntityManager and is therefore not
// reachable from the offline suite; a test that re-implemented the norm beside it would assert its
// own arithmetic and pin nothing.
[[nodiscard]] double groundSpeedMps(double velNMps, double velEMps, double velDMps);

// Course over ground in degrees clockwise from true north, normalized to [0, 360) (PRD v1.8.28,
// C18). Free-standing for the same reason groundSpeedMps is: buildSnapshot needs an IEntityManager
// and cannot be reached from the offline suite, so the shipped definition has to be callable.
//
// A stationary entity has no course. This returns 0.0 there, which is indistinguishable from due
// north - deliberately not papered over with a sentinel, because `speedMps` travels beside it in
// every consumer and a reader who sees a zero speed already knows the course means nothing.
[[nodiscard]] double courseOverGroundDeg(double velNMps, double velEMps);

} // namespace arkheon::aicommander
