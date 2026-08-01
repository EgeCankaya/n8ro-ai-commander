#pragma once

#include "ILlmClient.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace arkheon::aicommander {

// One recorded order, recovered from an `order.accepted` line.
struct ReplayEntry {
    double publishedSimTimeS = 0.0;
    std::string entityId;
    std::int64_t serial = 0;
    std::string orderBody;   // The order re-serialized as a wire document.
};

// The `replay` adapter (AIC-DET-2): sources orders from a recorded log instead of any model.
//
// It is an ILlmClient rather than a special mode, which is the point — a replayed order goes
// through the same Stage A, the same Stage B, the same recorder, and the same slots as a live one.
// A "replay mode" that bypassed validation would reproduce the decisions while proving nothing
// about the pipeline that made them.
//
// Performs no network I/O and constructs no IHttpClient at all.
//
// Stage B still runs, and that is deliberate: if live state has diverged from the recorded run —
// the target no longer exists, the geofence no longer contains the waypoint — the order is
// rejected and a `replay.divergence` record is written. A replay that quietly diverged would be
// worse than one that failed, because it would look like a reproduction.
class ReplayLlmClient final : public ILlmClient {
public:
    [[nodiscard]] const char* backendName() const override { return "replay"; }
    [[nodiscard]] std::string modelName() const override { return "replay:" + sourcePath_; }

    // Loads `order.accepted` records from a JSONL log. Returns false with a reason when the file
    // cannot be read or carries no usable records — a replay backend with nothing to replay is a
    // configuration error, not an empty run.
    bool load(const std::string& path, std::string& error);

    [[nodiscard]] std::size_t entryCount() const { return entries_.size(); }

    // Returns the next unconsumed recorded order for the requesting entity whose recorded
    // publication time is at or before the snapshot's simulation time.
    //
    // Keyed on recorded SIMULATION time, never on wall time and never on frame number: frame
    // numbering depends on the engine's timing configuration, and wall time is not reproducible at
    // all. Simulation time is the one clock the commander actually owns a guarantee about.
    [[nodiscard]] LlmResult request(const LlmRequest& request) override;

private:
    std::string sourcePath_;
    std::vector<ReplayEntry> entries_;
    // Per-entity cursor into `entries_`, so each entity replays its own sequence in order and one
    // entity running ahead cannot consume another's orders.
    std::map<std::string, std::size_t> cursors_;
};

} // namespace arkheon::aicommander
