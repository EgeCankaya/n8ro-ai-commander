#pragma once

#include "CommanderConfig.h"
#include "OrderSlot.h"

namespace arkheon::aicommander {

// Where an entity sits on the degradation ladder (AIC-VAL-2).
//
// The point of naming the levels is that a backend outage produces one documented behaviour
// instead of per-script improvisation — some entities holding the last order forever, others
// reverting to spawn behaviour, none of it written down.
enum class FallbackLevel {
    Live = 0,       // A currently valid accepted order is published.
    Retained = 1,   // Past cadence but inside orderValidityS: the last accepted order still stands.
    Standing = 2,   // Past orderValidityS: the configured standing order (hold where it is).
    Released = 3,   // Past releaseAfterS: no order at all. Tier 1 takes the entity back.
};

[[nodiscard]] const char* toString(FallbackLevel level);

// Per-entity ladder state, kept on the simulation thread.
struct LadderState {
    PublishedOrder published;
    double lastAcceptedSimTimeS = -1.0;   // -1 means no order has ever been accepted.
    FallbackLevel level = FallbackLevel::Released;
};

// The entity's own position, needed to synthesize the standing order's waypoint.
struct EntityPosition {
    bool known = false;
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeHaeM = 0.0;
};

// Advances `state` to the level implied by the elapsed time since the last accepted order, and
// rewrites the published order when a transition demands it.
//
// Returns true when the level CHANGED, which is the caller's cue to write exactly one record and
// one log line — the ladder must not spam a record per frame while sitting at a level.
//
// Replacement is wholesale: the published order is swapped in its entirety or left alone. A
// rejected or expired order never partially overwrites a standing one, because a half-applied
// order is an order nobody issued.
bool advanceFallbackLadder(
    LadderState& state,
    double nowSimTimeS,
    const CommanderConfig& config,
    const EntityPosition& position);

// Records a freshly accepted order, resetting the ladder to Live.
void acceptOrder(LadderState& state, const Order& order, std::int64_t serial, double nowSimTimeS);

} // namespace arkheon::aicommander
