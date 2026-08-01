#include "Snapshot.h"

#include <algorithm>

namespace arkheon::aicommander {

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
