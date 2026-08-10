#pragma once

#include "CacheMinimumGuard.h"
#include "CommanderConfig.h"
#include "FallbackLadder.h"
#include "FrameCostHistogram.h"
#include "ILlmClient.h"
#include "OrderRecorder.h"
#include "OrderSlot.h"
#include "PromptRenderer.h"
#include "RejectReason.h"
#include "RuntimeColumnProbe.h"
#include "Snapshot.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace arkheon::aicommander {

// Per-entity command state, owned by the simulation thread.
//
// The one exception is `completed`, which a worker publishes into — that is the entire shared
// surface between the two threads, and it is a mutex-guarded latest-wins slot.
struct EntityCommandState {
    LadderState ladder;

    // What Tier 1 has reported since the last snapshot was taken. Cleared when a snapshot is taken,
    // so a window's report reflects exactly one Tier-1 pass and a script that stops reporting stops
    // contributing tracks rather than leaving a stale picture in place.
    std::vector<TrackReport> pendingTracks;
    std::vector<LoadoutReport> pendingLoadout;
    std::string situationNote;

    // Request pacing. A pending request suppresses new ones for this entity: never two in flight
    // for one aircraft, which is what stops a slow backend producing overlapping requests and a
    // self-inflicted slowdown.
    bool requestInFlight = false;
    double lastRequestSimTimeS = -1.0e30;
    std::int64_t nextSerial = 1;

    // Where the worker publishes its candidate. Read and drained on the simulation thread.
    LatestWinsSlot<CandidateOrder> completed;
};

// Per-reason rejection counters plus the pipeline totals behind aiCommander.getStats().
struct CommanderStats {
    std::int64_t requested = 0;
    std::int64_t accepted = 0;
    std::int64_t rejected = 0;
    std::int64_t timeouts = 0;
    std::int64_t droppedSnapshots = 0;
    std::int64_t lastLatencyMs = 0;
    // Orders whose `reason` Stage A shortened rather than rejecting the order for length (C16,
    // v1.8.25). Deliberately NOT in rejectByReason: nothing was rejected, and folding it in would
    // make the acceptance rate read as a rejection rate. It is a separate line because it answers a
    // separate question - "is the configured model outgrowing the cap?" - and because a truncation
    // nobody counts is the silent shortening §Corrections item 26 records as a defect.
    std::int64_t reasonTruncated = 0;
    // `hold` orders whose orbitRadiusM Stage A substituted rather than rejecting the order (C14,
    // v1.8.30). Same placement and the same argument as reasonTruncated above - and it answers the
    // question the rejection counter used to answer: how often does the model omit a bound no
    // decoder on either backend can enforce? reject.shape keeps its own counter, so the repair
    // makes the failure visible in a new place rather than deleting it from the old one.
    std::int64_t orbitRadiusRepaired = 0;
    std::map<std::string, std::int64_t> rejectByReason;
};

// The plugin's simulation-thread state.
//
// Thread affinity: everything here is touched only on the host's single update thread — the frame
// callbacks and the Lua callbacks both run there (IPlugin.h:36) — except each entity's `completed`
// slot, which is the deliberate, mutex-guarded exchange point with a worker.
class CommanderRuntime {
public:
    CommanderRuntime() = default;
    ~CommanderRuntime();

    CommanderRuntime(const CommanderRuntime&) = delete;
    CommanderRuntime& operator=(const CommanderRuntime&) = delete;

    // -- configuration and health ---------------------------------------------------------------
    [[nodiscard]] const CommanderConfig& config() const { return config_; }
    void setConfig(const CommanderConfig& config);

    [[nodiscard]] const ProbeReport& probeReport() const { return probeReport_; }
    void setProbeReport(ProbeReport report) { probeReport_ = std::move(report); }

    // True when the commander may issue orders: the master switch is on, a backend is constructed,
    // and the runtime columns the snapshot depends on are known good. A failed probe disables the
    // commander rather than letting it run on fabricated zeros (AIC-ARCH-4).
    [[nodiscard]] bool isOperational() const;

    [[nodiscard]] std::string statsJson() const;
    [[nodiscard]] const CommanderStats& stats() const { return stats_; }

    // The simulation clock, refreshed once per frame. Held here so the Lua getters can answer
    // "how old is this order?" without each one reaching for a clock of its own — and so they
    // read the same instant the frame's Stage-B checks used.
    void setCurrentSimTimeS(double simTimeS) { currentSimTimeS_ = simTimeS; }
    [[nodiscard]] double currentSimTimeS() const { return currentSimTimeS_; }

    // The plugin's own per-frame cost. Measured, not asserted: the p95/p99 budget is a Success
    // Metric and it needs a number behind it.
    [[nodiscard]] FrameCostHistogram& frameCost() { return frameCost_; }
    [[nodiscard]] const FrameCostHistogram& frameCost() const { return frameCost_; }

    // -- roster (AIC-API-1) ---------------------------------------------------------------------
    [[nodiscard]] bool requestCommand(const std::string& entityId);
    [[nodiscard]] bool releaseCommand(const std::string& entityId);
    [[nodiscard]] bool isOnRoster(const std::string& entityId) const;
    [[nodiscard]] std::size_t rosterSize() const { return entities_.size(); }

    [[nodiscard]] EntityCommandState* find(const std::string& entityId);
    [[nodiscard]] const EntityCommandState* find(const std::string& entityId) const;

    // Roster ids in deterministic (sorted) order. Iterating a std::map already gives that, but
    // returning a snapshot of the ids lets the caller mutate the roster mid-loop — which
    // releaseCommand from a Lua callback can do — without invalidating its own iterator.
    [[nodiscard]] std::vector<std::string> rosterIds() const;

    // -- Tier-1 ingress (AIC-API-1, v1.2) -------------------------------------------------------
    // `kind` and `team` are clamped to their closed vocabularies rather than validated (v1.8.30):
    // an unrecognised or empty value becomes Other / Unknown and the call still succeeds. See
    // Snapshot.h for why that is the requirement.
    [[nodiscard]] bool reportTrack(
        const std::string& entityId, const std::string& targetEntityId, double rangeM, double snrDb,
        TrackKind kind = TrackKind::Other, TrackTeam team = TrackTeam::Unknown);
    [[nodiscard]] bool reportLoadout(
        const std::string& entityId, const std::string& hardpointName,
        const std::string& weaponProfileName, int ammoCount, int ammoMax);
    [[nodiscard]] bool setSituationNote(const std::string& entityId, const std::string& text);

    // -- published-order reads ------------------------------------------------------------------
    [[nodiscard]] const PublishedOrder* publishedOrder(const std::string& entityId) const;

    // -- backend lifecycle ----------------------------------------------------------------------
    void setClient(std::unique_ptr<ILlmClient> client);
    [[nodiscard]] ILlmClient* client() { return client_.get(); }
    [[nodiscard]] const ILlmClient* client() const { return client_.get(); }

    [[nodiscard]] PromptRenderer& promptRenderer() { return promptRenderer_; }
    [[nodiscard]] OrderRecorder& recorder() { return recorder_; }

    // AIC-BE-3's runtime half. Simulation-thread state, latched per run, reset() on scenario stop
    // along with everything else here. Lives on the runtime rather than in the plugin so the check
    // can be exercised without an engine, an entity manager, or a host.
    [[nodiscard]] CacheMinimumGuard& cacheGuard() { return cacheGuard_; }

    // Runs one worker call synchronously. Used by the owned-thread fallback and by tests; the
    // production path hands this to IThreadRunner::submitBackgroundTask.
    //
    // Deliberately static and free of runtime state: it takes only the snapshot BY VALUE and a
    // client pointer, which is what makes "the worker captures no SDK pointer" structural rather
    // than a convention. It touches no IEntityManager, no recorder, and no roster.
    //
    // `prefixLength` is the byte offset at which the stable prefix ends (AIC-BE-3, v1.8). Defaulted
    // rather than required because only a hosted adapter has a use for it, and making it mandatory
    // would force every existing caller — the replay suite, the integration suite, the live harness
    // — to restate a value they have no concept of. Zero means "no boundary declared", which is
    // exactly correct for all of them.
    //
    // `defaultOrbitRadiusM` is safety.defaultOrbitRadiusM, forwarded to Stage A's `hold`-radius
    // repair (AIC-ORD-1, v1.8.30). It is a PARAMETER rather than a read of config_ because this
    // function is static on purpose - see the first comment in its body - and a worker that could
    // reach the runtime's config would be a worker that could reach the runtime. Defaulted for the
    // same reason prefixLength is: the offline suites have no CommanderConfig in hand, and the
    // compiled-in default is the one their fixtures already assume.
    [[nodiscard]] static CandidateOrder runWorkerCall(
        OrderSnapshot snapshot, const std::string& prompt, ILlmClient& client,
        bool& outAccepted, RejectReason& outReason, std::string& outDetail, std::string& outRawBody,
        std::size_t prefixLength = 0, double defaultOrbitRadiusM = kDefaultOrbitRadiusM);

    // The form the worker actually uses: packs the Stage-A verdict into the returned candidate so
    // it survives the thread boundary. Prefer this over the out-parameter overload — a caller that
    // drops those out-parameters silently loses every syntactic rejection from the counters and
    // the order log.
    [[nodiscard]] static CandidateOrder runWorkerCall(
        OrderSnapshot snapshot, const std::string& prompt, ILlmClient& client,
        std::size_t prefixLength = 0, double defaultOrbitRadiusM = kDefaultOrbitRadiusM);

    void countRejection(RejectReason reason);
    void countAcceptance(std::int64_t latencyMs);
    void countRequest();
    void countReasonTruncation();
    void countOrbitRadiusRepair();
    void countDroppedSnapshot() { ++stats_.droppedSnapshots; }

    // Clears all per-entity state. Called on scenario stop.
    void reset();

private:
    CommanderConfig config_;
    ProbeReport probeReport_;
    CommanderStats stats_;
    FrameCostHistogram frameCost_;
    double currentSimTimeS_ = 0.0;

    std::map<std::string, std::unique_ptr<EntityCommandState>> entities_;
    std::unique_ptr<ILlmClient> client_;
    PromptRenderer promptRenderer_;
    OrderRecorder recorder_;
    CacheMinimumGuard cacheGuard_;
};

} // namespace arkheon::aicommander
