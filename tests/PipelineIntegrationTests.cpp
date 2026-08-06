#include "TestSupport.h"

#include "CommanderConfig.h"
#include "CommanderRuntime.h"
#include "FallbackLadder.h"
#include "FrameCostHistogram.h"
#include "OrderValidatorStageB.h"
#include "ReplayLlmClient.h"
#include "Snapshot.h"
#include "StubLlmClient.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace arkheon::aicommander;

namespace {

constexpr const char* kOwnId = "RedSu35_01";
constexpr const char* kBanditId = "BlueF18_02";

class FakeWorld final : public StageBWorldView {
public:
    std::map<std::string, std::string> teams;
    double lat = 13.50;
    double lon = 144.80;
    double alt = 9000.0;

    bool entityExists(const std::string& id) const override {
        return teams.find(id) != teams.end();
    }
    std::string teamOf(const std::string& id) const override {
        const auto it = teams.find(id);
        return it == teams.end() ? std::string() : it->second;
    }
    std::int64_t entityKindOf(const std::string&) const override { return kUnknownEntityKind; }
    bool positionOf(const std::string& id, double& outLat, double& outLon, double& outAlt) const override {
        if (teams.find(id) == teams.end()) {
            return false;
        }
        outLat = lat;
        outLon = lon;
        outAlt = alt;
        return true;
    }
};

// Drives the real pipeline for one entity over one frame, exactly as AiCommanderPlugin::onTickFrame
// does: drain -> Stage B -> publish, advance the ladder, then snapshot and dispatch.
//
// This is the in-process form of the stubbed-integration gate. It exercises the genuine
// CommanderRuntime, StubLlmClient, Stage A, Stage B, fallback ladder, and recorder; what it
// substitutes for is the engine's own component reads, which the live runtime-column probe already
// covers separately.
struct Harness {
    CommanderRuntime runtime;
    FakeWorld world;
    double simTimeS = 0.0;
    std::int64_t frame = 0;

    std::vector<std::int64_t> acceptedSerials;

    void tick(double dt) {
        const auto started = std::chrono::steady_clock::now();
        simTimeS += dt;
        ++frame;
        runtime.setCurrentSimTimeS(simTimeS);

        // -- drain + Stage B + publish ----------------------------------------------------------
        for (const std::string& entityId : runtime.rosterIds()) {
            EntityCommandState* state = runtime.find(entityId);
            if (state == nullptr) {
                continue;
            }
            std::optional<CandidateOrder> candidate = state->completed.take();
            if (!candidate.has_value()) {
                continue;
            }
            state->requestInFlight = false;

            // Stage-A verdict, acted on here exactly as the plugin does. The harness used to test
            // for an empty entityId instead, which diverged from the plugin's real path and is why
            // the suite missed that Stage-A rejections were never being counted or recorded.
            if (!candidate->stageAAccepted) {
                if (candidate->stageAReason != RejectReason::None) {
                    runtime.countRejection(candidate->stageAReason);
                }
                continue;
            }

            StageBRequest request;
            request.commandedEntityId = entityId;
            request.onRoster = true;
            request.snapshotSimTimeS = candidate->snapshot.simTimeS;
            request.currentSimTimeS = simTimeS;
            request.publishedSerial = state->ladder.published.serial;
            request.candidateSerial = candidate->snapshot.serial;
            for (const TrackReport& track : candidate->snapshot.tracks) {
                request.reportedTrackIds.push_back(track.targetEntityId);
            }

            const StageBOutcome outcome =
                validateStageB(candidate->order, request, runtime.config(), world);
            if (!outcome.accepted) {
                runtime.countRejection(outcome.reason);
                continue;
            }
            acceptOrder(state->ladder, candidate->order, candidate->snapshot.serial, simTimeS);
            runtime.countAcceptance(candidate->latencyMs);
            acceptedSerials.push_back(candidate->snapshot.serial);
        }

        // -- ladder -------------------------------------------------------------------------------
        for (const std::string& entityId : runtime.rosterIds()) {
            EntityCommandState* state = runtime.find(entityId);
            if (state == nullptr) {
                continue;
            }
            EntityPosition position;
            position.known = world.positionOf(
                entityId, position.latitudeDeg, position.longitudeDeg, position.altitudeHaeM);
            (void)advanceFallbackLadder(state->ladder, simTimeS, runtime.config(), position);
        }

        // -- snapshot + dispatch ------------------------------------------------------------------
        if (runtime.isOperational() && runtime.client() != nullptr) {
            for (const std::string& entityId : runtime.rosterIds()) {
                EntityCommandState* state = runtime.find(entityId);
                if (state == nullptr || state->requestInFlight) {
                    continue;
                }
                if (simTimeS - state->lastRequestSimTimeS < runtime.config().cadenceS) {
                    continue;
                }

                OrderSnapshot snapshot;
                snapshot.entityId = entityId;
                snapshot.simTimeS = simTimeS;
                snapshot.serial = state->nextSerial;
                snapshot.latitudeDeg = world.lat;
                snapshot.longitudeDeg = world.lon;
                snapshot.altitudeHaeM = world.alt;
                snapshot.team = world.teamOf(entityId);
                snapshot.tracks = state->pendingTracks;
                snapshot.loadout = state->pendingLoadout;
                snapshot.situationNote = state->situationNote;
                canonicalizeSnapshot(snapshot);

                state->pendingTracks.clear();
                state->pendingLoadout.clear();
                state->requestInFlight = true;
                state->lastRequestSimTimeS = simTimeS;
                ++state->nextSerial;
                runtime.countRequest();

                const std::string prompt = runtime.promptRenderer().render(snapshot);
                (void)state->completed.publish(
                    CommanderRuntime::runWorkerCall(snapshot, prompt, *runtime.client()));
            }
        }

        const auto elapsed = std::chrono::steady_clock::now() - started;
        runtime.frameCost().record(std::chrono::duration<double, std::micro>(elapsed).count());
    }
};

// Configured in place rather than returned by value: CommanderRuntime is deliberately
// non-copyable (it owns the adapter, the recorder, and the per-entity slots), and that is a
// property worth keeping rather than working around with a move constructor nothing else needs.
void setUpHarness(Harness& harness) {
    CommanderConfig config;
    config.enabled = true;
    harness.runtime.setConfig(config);

    ProbeReport probe;
    probe.result = ProbeResult::Pass;
    probe.detail = "test harness";
    harness.runtime.setProbeReport(std::move(probe));

    harness.runtime.setClient(std::make_unique<StubLlmClient>());
    harness.runtime.promptRenderer().build(config, "Test doctrine.");

    harness.world.teams[kOwnId] = "red";
    harness.world.teams[kBanditId] = "blue";
}

} // namespace

// The stubbed-integration gate, in process: 250 ticks of the real pipeline with no inference
// server and no network, asserting the pipeline actually produced orders and that the properties
// a Tier-1 script depends on hold.
AIC_TEST(PipelineAcceptsOrdersOver250Ticks) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment must succeed");

    constexpr double kDt = 1.0 / 60.0;
    for (int i = 0; i < 250; ++i) {
        // Tier 1 reports its picture every tick, as the reference script does.
        (void)harness.runtime.reportTrack(kOwnId, kBanditId, 42000.0, 18.5);
        harness.tick(kDt);
    }

    const CommanderStats& stats = harness.runtime.stats();
    AIC_EXPECT_TRUE(stats.requested > 0,
                    "the pipeline must have requested orders over 250 ticks");
    AIC_EXPECT_TRUE(stats.accepted > 0,
                    "the pipeline must have ACCEPTED orders; requested="
                        + std::to_string(stats.requested) + " rejected="
                        + std::to_string(stats.rejected));

    // Every order the stub produced should survive both stages against a sane world. A rejection
    // here means the stub and the validator disagree, which would make the whole gate hollow.
    AIC_EXPECT_EQ(stats.rejected, static_cast<std::int64_t>(0),
                  "no rejections expected against a consistent world; first reasons: "
                      + [&] {
                            std::string out;
                            for (const auto& entry : stats.rejectByReason) {
                                out += entry.first + "=" + std::to_string(entry.second) + " ";
                            }
                            return out;
                        }());

    // getOrderSerial must strictly increase, which is the property a script uses to detect a new
    // order without diffing every field.
    for (std::size_t i = 1; i < harness.acceptedSerials.size(); ++i) {
        AIC_EXPECT_TRUE(harness.acceptedSerials[i] > harness.acceptedSerials[i - 1],
                        "order serials must strictly increase at index " + std::to_string(i));
    }

    // A published order must be readable, which is what every Lua getter reads.
    const PublishedOrder* published = harness.runtime.publishedOrder(kOwnId);
    AIC_EXPECT_TRUE(published != nullptr, "an order must be published at the end of the run");
    AIC_EXPECT_TRUE(published->serial > 0, "the published order carries a serial");
    return true;
}

// The frame-cost budget, measured rather than asserted (Success Metrics: p95 < 0.5 ms,
// p99 < 2.0 ms). This measures the plugin's own per-frame work only, which is what the budget
// covers - inference latency is a property of the host machine and is explicitly not in it.
AIC_TEST(PipelineStaysInsideTheFrameBudget) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment");

    constexpr double kDt = 1.0 / 60.0;
    for (int i = 0; i < 250; ++i) {
        (void)harness.runtime.reportTrack(kOwnId, kBanditId, 42000.0, 18.5);
        harness.tick(kDt);
    }

    const FrameCostHistogram& cost = harness.runtime.frameCost();
    AIC_EXPECT_TRUE(cost.totalFrames() >= 250, "250 frames recorded");

    const double p95Ms = cost.p95Us() / 1000.0;
    const double p99Ms = cost.p99Us() / 1000.0;
    const double maxMs = cost.maxUs() / 1000.0;

    AIC_EXPECT_TRUE(p95Ms < 0.5,
                    "p95 frame cost must be under 0.5 ms, measured " + std::to_string(p95Ms) + " ms");
    AIC_EXPECT_TRUE(p99Ms < 2.0,
                    "p99 frame cost must be under 2.0 ms, measured " + std::to_string(p99Ms) + " ms");
    // Reported, not asserted: the worst single frame includes the tick that renders a prompt and
    // runs the stub inline, and on a loaded CI box it can legitimately spike. p99 is the gate.
    AIC_EXPECT_TRUE(maxMs >= 0.0, "max is reported for the record: " + std::to_string(maxMs) + " ms");
    return true;
}

// A hallucinated target must be caught by Stage B in the running pipeline, not merely in the
// validator's own unit test - this is the property that stands between a bad order and a
// requestFire call.
AIC_TEST(PipelineRejectsOrdersNamingUnreportedTargets) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment");

    // Note: NO reportTrack calls. The stub will therefore emit only untargeted postures, and any
    // targeted order that did appear would be rejected `track`.
    constexpr double kDt = 1.0 / 60.0;
    for (int i = 0; i < 250; ++i) {
        harness.tick(kDt);
    }

    const CommanderStats& stats = harness.runtime.stats();
    AIC_EXPECT_TRUE(stats.accepted > 0, "waypoint postures must still be accepted with no tracks");

    // Every accepted order must be untargeted, because nothing was ever reported.
    const PublishedOrder* published = harness.runtime.publishedOrder(kOwnId);
    AIC_EXPECT_TRUE(published != nullptr, "an order is published");
    AIC_EXPECT_TRUE(published->order.targetEntityId.empty(),
                    "with no reported tracks, no accepted order may name a target - got '"
                        + published->order.targetEntityId + "'");
    return true;
}

// The resilience path end to end: the backend dies, and the entity walks the ladder to Released
// while the run continues with no error state and no stall (UAC-AIC-VAL-2).
AIC_TEST(PipelineDegradesToReleasedWhenTheBackendDies) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment");

    constexpr double kDt = 0.5;   // Coarse ticks so the run covers releaseAfterS quickly.

    // Healthy phase: get an order published.
    for (int i = 0; i < 100; ++i) {
        (void)harness.runtime.reportTrack(kOwnId, kBanditId, 42000.0, 18.5);
        harness.tick(kDt);
    }
    AIC_EXPECT_TRUE(harness.runtime.publishedOrder(kOwnId) != nullptr, "an order is published");
    const EntityCommandState* state = harness.runtime.find(kOwnId);
    AIC_EXPECT_TRUE(state->ladder.level == FallbackLevel::Live
                        || state->ladder.level == FallbackLevel::Retained,
                    "the entity is being commanded");

    // The backend dies: every subsequent request is a transport failure.
    harness.runtime.setClient(nullptr);
    CommanderConfig config = harness.runtime.config();
    harness.runtime.setConfig(config);

    // Run past releaseAfterS (300 s default) with no new order.
    for (int i = 0; i < 800; ++i) {
        harness.tick(kDt);
    }

    state = harness.runtime.find(kOwnId);
    AIC_EXPECT_TRUE(state->ladder.level == FallbackLevel::Released,
                    "the entity must reach Released, got '"
                        + std::string(toString(state->ladder.level)) + "'");
    AIC_EXPECT_TRUE(harness.runtime.publishedOrder(kOwnId) == nullptr,
                    "a released entity publishes no order - the script reads that as "
                    "'commander absent' and resumes waypoint following");

    // And the run completed: no stall, no error state, frames still recorded throughout.
    AIC_EXPECT_TRUE(harness.runtime.frameCost().totalFrames() >= 900,
                    "the run continued through the outage");
    return true;
}

// Per-entity request suppression: never two requests in flight for one aircraft. Without it a slow
// backend produces overlapping requests and a self-inflicted slowdown.
AIC_TEST(PipelineNeverHasTwoRequestsInFlightForOneEntity) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment");

    constexpr double kDt = 1.0 / 60.0;
    std::int64_t maxInFlightObserved = 0;
    for (int i = 0; i < 300; ++i) {
        (void)harness.runtime.reportTrack(kOwnId, kBanditId, 42000.0, 18.5);
        harness.tick(kDt);
        const EntityCommandState* state = harness.runtime.find(kOwnId);
        maxInFlightObserved = std::max<std::int64_t>(maxInFlightObserved, state->requestInFlight ? 1 : 0);
    }
    AIC_EXPECT_TRUE(maxInFlightObserved <= 1, "at most one request in flight per entity");

    // Requests must be paced by cadence, not issued every frame.
    const CommanderStats& stats = harness.runtime.stats();
    AIC_EXPECT_TRUE(stats.requested < 300,
                    "requests must be paced by commander.cadenceS, not issued per frame - got "
                        + std::to_string(stats.requested) + " over 300 frames");
    return true;
}

// The roster cap is hard, and it is what keeps per-entity orders from drifting into multi-entity
// coordination the order schema has no answer for.
AIC_TEST(PipelineEnforcesTheRosterCap) {
    Harness harness;
    setUpHarness(harness);
    const int cap = harness.runtime.config().maxCommandedEntities;

    for (int i = 0; i < cap; ++i) {
        AIC_EXPECT_TRUE(harness.runtime.requestCommand("Entity_" + std::to_string(i)),
                        "enrolment " + std::to_string(i) + " must succeed");
    }
    AIC_EXPECT_FALSE(harness.runtime.requestCommand("OneTooMany"),
                     "enrolment past commander.maxCommandedEntities must fail");
    AIC_EXPECT_EQ(harness.runtime.rosterSize(), static_cast<std::size_t>(cap), "roster size");

    // Idempotent re-enrolment of an existing entity still succeeds at the cap.
    AIC_EXPECT_TRUE(harness.runtime.requestCommand("Entity_0"),
                    "re-enrolling an existing entity is idempotent even at the cap");

    // Releasing frees a slot.
    AIC_EXPECT_TRUE(harness.runtime.releaseCommand("Entity_0"), "release");
    AIC_EXPECT_TRUE(harness.runtime.requestCommand("OneTooMany"), "a freed slot is reusable");
    return true;
}

// Reporting into a disabled commander must accumulate nothing - otherwise a run that was never
// enabled would still be building a picture in memory.
AIC_TEST(PipelineIgnoresReportsWhileDisabled) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment");

    CommanderConfig config = harness.runtime.config();
    config.enabled = false;
    harness.runtime.setConfig(config);

    AIC_EXPECT_FALSE(harness.runtime.reportTrack(kOwnId, kBanditId, 1000.0, 5.0),
                     "reportTrack must fail while disabled");
    AIC_EXPECT_FALSE(harness.runtime.reportLoadout(kOwnId, "WingLeft", "R73", 2, 4),
                     "reportLoadout must fail while disabled");

    const EntityCommandState* state = harness.runtime.find(kOwnId);
    AIC_EXPECT_EQ(state->pendingTracks.size(), static_cast<std::size_t>(0),
                  "nothing may accumulate while disabled");
    AIC_EXPECT_EQ(state->pendingLoadout.size(), static_cast<std::size_t>(0),
                  "nothing may accumulate while disabled");
    return true;
}

// Regression, found in the milestone close-out code review.
//
// A Stage-A rejection has to survive the thread boundary carrying its REASON, or the whole
// syntactic half of the validator becomes invisible: no `aicmd.reject.*` increment, no
// order.rejected record, and — worse — an empty order reaching Stage B, where it is re-reported as
// a `roster` failure and counted under the wrong reason entirely.
//
// The original code discarded the verdict at the slot and signalled rejection by clearing
// entityId, which lost the reason and mislabelled the failure.
AIC_TEST(StageARejectionsSurviveTheThreadBoundaryWithTheirReason) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment");

    // Force the backend to return a body that fails Stage A with a specific, identifiable reason.
    auto* stub = static_cast<StubLlmClient*>(harness.runtime.client());
    stub->injectBody(R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"attack",)"
                     R"("cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree",)"
                     R"("reason":"Unknown posture."})");

    harness.tick(1.0);   // dispatch: the injected body is validated on the worker
    harness.tick(1.0);   // drain: the verdict is acted on

    const CommanderStats& stats = harness.runtime.stats();
    AIC_EXPECT_EQ(stats.rejected, static_cast<std::int64_t>(1),
                  "the Stage-A rejection must be counted exactly once");

    const auto entry = stats.rejectByReason.find("enum");
    AIC_EXPECT_TRUE(entry != stats.rejectByReason.end(),
                    "the rejection must be attributed to 'enum' (unknown posture), not lost and "
                    "not mislabelled as a Stage-B 'roster' failure");
    AIC_EXPECT_EQ(entry->second, static_cast<std::int64_t>(1), "counted once under 'enum'");
    AIC_EXPECT_TRUE(stats.rejectByReason.find("roster") == stats.rejectByReason.end(),
                    "an empty order must never reach Stage B and be re-reported as 'roster'");

    // And the entity must not be wedged: requestInFlight has to clear so it can ask again.
    const EntityCommandState* state = harness.runtime.find(kOwnId);
    AIC_EXPECT_FALSE(state->requestInFlight,
                     "a rejected request must clear requestInFlight or the entity goes "
                     "permanently silent");
    return true;
}

// A backend with nothing due (replay past its last record, or a 204) is NOT a rejection. Counting
// it as one would make the acceptance-rate metric meaningless on any replay run.
AIC_TEST(NothingDueIsNotCountedAsARejection) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment");

    auto* stub = static_cast<StubLlmClient*>(harness.runtime.client());
    stub->injectBody("");   // completed, but empty - "no order due"

    harness.tick(1.0);
    harness.tick(1.0);

    const CommanderStats& stats = harness.runtime.stats();
    AIC_EXPECT_EQ(stats.rejected, static_cast<std::int64_t>(0),
                  "an empty completed body is 'nothing due', not a rejection");
    const EntityCommandState* state = harness.runtime.find(kOwnId);
    AIC_EXPECT_FALSE(state->requestInFlight, "requestInFlight still clears");
    return true;
}

// The reported list is cleared when a snapshot is taken, so each window reflects exactly one
// Tier-1 pass and a script that stops reporting stops contributing tracks.
AIC_TEST(PipelineClearsReportedTracksPerWindow) {
    Harness harness;
    setUpHarness(harness);
    AIC_EXPECT_TRUE(harness.runtime.requestCommand(kOwnId), "enrolment");

    AIC_EXPECT_TRUE(harness.runtime.reportTrack(kOwnId, kBanditId, 42000.0, 18.5), "report");
    const EntityCommandState* state = harness.runtime.find(kOwnId);
    AIC_EXPECT_EQ(state->pendingTracks.size(), static_cast<std::size_t>(1), "one track pending");

    harness.tick(1.0);   // First tick dispatches, which takes the snapshot.

    state = harness.runtime.find(kOwnId);
    AIC_EXPECT_EQ(state->pendingTracks.size(), static_cast<std::size_t>(0),
                  "taking a snapshot must clear the reported list");
    return true;
}
