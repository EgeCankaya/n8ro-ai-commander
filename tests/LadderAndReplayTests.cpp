#include "TestSupport.h"

#include "CommanderConfig.h"
#include "FallbackLadder.h"
#include "Order.h"
#include "OrderRecorder.h"
#include "OrderValidatorStageA.h"
#include "ReplayLlmClient.h"
#include "Snapshot.h"
#include "StubLlmClient.h"

#include <core/json/JsonValue.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace arkheon::aicommander;

namespace {

Order makeIngressOrder() {
    Order order;
    order.entityId = "RedSu35_01";
    order.posture = Posture::Ingress;
    order.latitudeDeg = 13.60;
    order.longitudeDeg = 144.90;
    order.altitudeHaeM = 9000.0;
    order.cruiseSpeedMps = 240.0;
    order.roe = Roe::WeaponsTight;
    order.reason = "Pressing toward the area.";
    return order;
}

EntityPosition knownPosition() {
    EntityPosition position;
    position.known = true;
    position.latitudeDeg = 13.50;
    position.longitudeDeg = 144.80;
    position.altitudeHaeM = 9200.0;
    return position;
}

std::filesystem::path scratchDir(const char* name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "aic-tests" / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

} // namespace

// UAC-AIC-VAL-2: the ladder must step through its levels at the configured boundaries, and each
// transition must be reported exactly once so the recorder does not emit a line per frame.
AIC_TEST(FallbackLadderWalksItsLevels) {
    CommanderConfig config;      // cadence 20, validity 120, release 300
    LadderState state;
    const EntityPosition position = knownPosition();

    // Before any order: released, no order published.
    AIC_EXPECT_TRUE(state.level == FallbackLevel::Released, "initial level is released");
    AIC_EXPECT_FALSE(state.published.valid, "nothing is published before the first order");

    acceptOrder(state, makeIngressOrder(), 1, 100.0);
    AIC_EXPECT_TRUE(state.level == FallbackLevel::Live, "accepting an order goes Live");
    AIC_EXPECT_TRUE(state.published.valid, "the order is published");

    // Inside cadence: still Live, no transition reported.
    AIC_EXPECT_FALSE(advanceFallbackLadder(state, 110.0, config, position),
                     "no transition inside the cadence window");
    AIC_EXPECT_TRUE(state.level == FallbackLevel::Live, "still Live");

    // Past cadence, inside validity: Retained, and the ORIGINAL order still stands.
    AIC_EXPECT_TRUE(advanceFallbackLadder(state, 100.0 + config.cadenceS + 1.0, config, position),
                    "crossing the cadence boundary reports a transition");
    AIC_EXPECT_TRUE(state.level == FallbackLevel::Retained, "level is retained");
    AIC_EXPECT_TRUE(state.published.valid, "a retained order is still published");
    AIC_EXPECT_TRUE(state.published.order.posture == Posture::Ingress,
                    "retention must not alter the order - only the level moves");
    AIC_EXPECT_EQ(state.published.order.latitudeDeg, 13.60, "retained waypoint is unchanged");

    // A second call at the same level reports no transition - the recorder must not spam.
    AIC_EXPECT_FALSE(advanceFallbackLadder(state, 100.0 + config.cadenceS + 2.0, config, position),
                     "staying at a level reports no further transition");

    // Past validity: Standing. Hold, at the entity's position, weaponsTight, configured radius.
    AIC_EXPECT_TRUE(advanceFallbackLadder(state, 100.0 + config.orderValidityS + 1.0, config, position),
                    "crossing the validity boundary reports a transition");
    AIC_EXPECT_TRUE(state.level == FallbackLevel::Standing, "level is standing");
    AIC_EXPECT_TRUE(state.published.valid, "a standing order is published");
    AIC_EXPECT_TRUE(state.published.order.posture == Posture::Hold, "standing order holds");
    AIC_EXPECT_TRUE(state.published.order.roe == Roe::WeaponsTight, "standing order is weapons tight");
    AIC_EXPECT_EQ(state.published.order.orbitRadiusM, config.defaultOrbitRadiusM,
                  "standing order uses the configured orbit radius");
    AIC_EXPECT_EQ(state.published.order.latitudeDeg, position.latitudeDeg,
                  "the standing order holds over the entity's position at expiry");
    AIC_EXPECT_TRUE(state.published.order.targetEntityId.empty(),
                    "a standing order carries no target");

    // Past release: Released, nothing published.
    AIC_EXPECT_TRUE(advanceFallbackLadder(state, 100.0 + config.releaseAfterS + 1.0, config, position),
                    "crossing the release boundary reports a transition");
    AIC_EXPECT_TRUE(state.level == FallbackLevel::Released, "level is released");
    AIC_EXPECT_FALSE(state.published.valid,
                     "release publishes nothing - the reference script reads this as "
                     "'commander absent' and resumes waypoint following");
    return true;
}

// The standing order must be synthesized ONCE, on the transition. Rebuilding it per frame would
// let the hold point chase the drifting entity, quietly turning "hold where you were when the link
// died" into "hold wherever you happen to be now".
AIC_TEST(StandingOrderIsPinnedAtExpiryNotRecomputed) {
    CommanderConfig config;
    LadderState state;
    acceptOrder(state, makeIngressOrder(), 1, 100.0);

    EntityPosition position = knownPosition();
    (void)advanceFallbackLadder(state, 100.0 + config.orderValidityS + 1.0, config, position);
    const double pinnedLat = state.published.order.latitudeDeg;
    AIC_EXPECT_EQ(pinnedLat, 13.50, "pinned at the position at expiry");

    // The entity drifts a long way. The standing order must not follow it.
    position.latitudeDeg = 14.90;
    (void)advanceFallbackLadder(state, 100.0 + config.orderValidityS + 30.0, config, position);
    AIC_EXPECT_EQ(state.published.order.latitudeDeg, pinnedLat,
                  "the standing hold point must not chase the entity");
    return true;
}

// With no known position there is nowhere legitimate to hold, so the ladder must release rather
// than publish a hold over a fabricated waypoint.
AIC_TEST(StandingOrderReleasesWhenPositionIsUnknown) {
    CommanderConfig config;
    LadderState state;
    acceptOrder(state, makeIngressOrder(), 1, 100.0);

    EntityPosition unknown;   // known == false
    (void)advanceFallbackLadder(state, 100.0 + config.orderValidityS + 1.0, config, unknown);

    AIC_EXPECT_TRUE(state.level == FallbackLevel::Released,
                    "with no position, the ladder releases instead of inventing a hold point");
    AIC_EXPECT_FALSE(state.published.valid, "nothing is published");
    return true;
}

// A scenario reload runs simulation time backwards. The ladder must treat the history as gone
// rather than computing a negative age and concluding the order is fresh.
AIC_TEST(FallbackLadderHandlesTimeGoingBackwards) {
    CommanderConfig config;
    LadderState state;
    acceptOrder(state, makeIngressOrder(), 1, 400.0);

    AIC_EXPECT_TRUE(advanceFallbackLadder(state, 10.0, config, knownPosition()),
                    "time going backwards reports a transition");
    AIC_EXPECT_TRUE(state.level == FallbackLevel::Released, "released after a reload");
    AIC_EXPECT_FALSE(state.published.valid, "nothing published after a reload");
    return true;
}

// A new accepted order at any level must restore Live and replace the published order wholesale.
AIC_TEST(AcceptingAnOrderRecoversFromAnyLevel) {
    CommanderConfig config;
    LadderState state;
    acceptOrder(state, makeIngressOrder(), 1, 100.0);
    (void)advanceFallbackLadder(state, 100.0 + config.releaseAfterS + 1.0, config, knownPosition());
    AIC_EXPECT_TRUE(state.level == FallbackLevel::Released, "released");

    Order fresh = makeIngressOrder();
    fresh.posture = Posture::Rtb;
    fresh.reason = "Recovered link; egressing.";
    acceptOrder(state, fresh, 2, 500.0);

    AIC_EXPECT_TRUE(state.level == FallbackLevel::Live, "a fresh order restores Live");
    AIC_EXPECT_TRUE(state.published.valid, "and republishes");
    AIC_EXPECT_TRUE(state.published.order.posture == Posture::Rtb, "with the new order's content");
    AIC_EXPECT_EQ(state.published.serial, static_cast<std::int64_t>(2), "and the new serial");
    return true;
}

// AIC-DET-1 / AIC-DET-2: a stub run is recorded, then replayed, and the replayed orders must be
// byte-identical to what was recorded - twice.
AIC_TEST(RecordThenReplayReproducesTheOrderSequence) {
    const std::filesystem::path dir = scratchDir("record-replay");

    // -- record a stub run -----------------------------------------------------------------------
    std::vector<std::string> recordedBodies;
    {
        OrderRecorder recorder;
        AIC_EXPECT_TRUE(recorder.open(dir.string(), 16 * 1024 * 1024, 4), "recorder must open");

        StubLlmClient client;
        OrderSnapshot snapshot;
        snapshot.entityId = "RedSu35_01";
        snapshot.latitudeDeg = 13.50;
        snapshot.longitudeDeg = 144.80;
        snapshot.tracks.push_back(TrackReport{"BlueF18_02", 42000.0, 18.5});

        for (int i = 0; i < 8; ++i) {
            snapshot.serial = i + 1;
            snapshot.simTimeS = 100.0 + 20.0 * i;

            LlmRequest request;
            request.snapshot = snapshot;
            const LlmResult result = client.request(request);

            const StageAOutcome outcome = validateStageA(result.body, snapshot.entityId);
            AIC_EXPECT_TRUE(outcome.accepted,
                            "recorded order " + std::to_string(i) + " must be valid: "
                                + outcome.detail);
            recorder.recordAccepted(snapshot.simTimeS, i, outcome.order, snapshot.serial, 0, {});
            recordedBodies.push_back(std::string(toString(outcome.order.posture)) + "|"
                + outcome.order.targetEntityId + "|"
                + std::to_string(static_cast<int>(outcome.order.cruiseSpeedMps)));
        }
    }

    const std::string logPath = (dir / "orders.jsonl").string();
    AIC_EXPECT_TRUE(std::filesystem::exists(logPath), "the order log must exist");

    // -- replay it twice --------------------------------------------------------------------------
    std::vector<std::string> firstReplay;
    std::vector<std::string> secondReplay;

    for (int pass = 0; pass < 2; ++pass) {
        ReplayLlmClient replay;
        std::string error;
        AIC_EXPECT_TRUE(replay.load(logPath, error), "replay must load the log: " + error);
        AIC_EXPECT_EQ(replay.entryCount(), static_cast<std::size_t>(8), "8 recorded orders");

        std::vector<std::string>& into = (pass == 0) ? firstReplay : secondReplay;
        OrderSnapshot snapshot;
        snapshot.entityId = "RedSu35_01";

        for (int i = 0; i < 8; ++i) {
            snapshot.simTimeS = 100.0 + 20.0 * i;
            LlmRequest request;
            request.snapshot = snapshot;
            const LlmResult result = replay.request(request);
            AIC_EXPECT_TRUE(result.completed, "replay performs no I/O and always completes");
            AIC_EXPECT_FALSE(result.body.empty(),
                             "an order was due at t=" + std::to_string(snapshot.simTimeS));

            const StageAOutcome outcome = validateStageA(result.body, snapshot.entityId);
            AIC_EXPECT_TRUE(outcome.accepted,
                            "a REPLAYED order must survive the same Stage A a live one does: "
                                + outcome.detail);
            into.push_back(std::string(toString(outcome.order.posture)) + "|"
                + outcome.order.targetEntityId + "|"
                + std::to_string(static_cast<int>(outcome.order.cruiseSpeedMps)));
        }
    }

    AIC_EXPECT_EQ(firstReplay.size(), recordedBodies.size(), "replay yields the recorded count");
    for (std::size_t i = 0; i < recordedBodies.size(); ++i) {
        AIC_EXPECT_EQ(firstReplay[i], recordedBodies[i],
                      "replayed order " + std::to_string(i) + " differs from the recorded one");
        AIC_EXPECT_EQ(secondReplay[i], firstReplay[i],
                      "the two replays differ at order " + std::to_string(i)
                          + " - replay is not deterministic");
    }
    return true;
}

// Replay is keyed on recorded SIMULATION time. An order must not be handed out before its recorded
// publication time, or the whole sequence shifts relative to the run it is meant to reproduce.
AIC_TEST(ReplayHonoursRecordedSimulationTime) {
    const std::filesystem::path dir = scratchDir("replay-timing");
    const std::string logPath = (dir / "orders.jsonl").string();
    {
        OrderRecorder recorder;
        AIC_EXPECT_TRUE(recorder.open(dir.string(), 1024 * 1024, 2), "recorder opens");
        Order order = makeIngressOrder();
        recorder.recordAccepted(500.0, 0, order, 1, 0, {});
    }

    ReplayLlmClient replay;
    std::string error;
    AIC_EXPECT_TRUE(replay.load(logPath, error), "load: " + error);

    OrderSnapshot snapshot;
    snapshot.entityId = "RedSu35_01";

    snapshot.simTimeS = 499.0;
    LlmRequest early;
    early.snapshot = snapshot;
    const LlmResult notYet = replay.request(early);
    AIC_EXPECT_TRUE(notYet.completed, "a replay with nothing due still completes");
    AIC_EXPECT_TRUE(notYet.body.empty(), "an order must not be issued before its recorded time");
    AIC_EXPECT_EQ(notYet.statusCode, 204,
                  "nothing-due is reported as 204, NOT as a transport failure - calling it a "
                  "transport failure would drive the fallback ladder on a healthy replay");

    snapshot.simTimeS = 500.0;
    LlmRequest due;
    due.snapshot = snapshot;
    const LlmResult issued = replay.request(due);
    AIC_EXPECT_FALSE(issued.body.empty(), "the order is due at exactly its recorded time");
    return true;
}

// A run killed mid-write leaves a truncated final line. Refusing to replay everything before it
// would discard the whole recording over its last byte.
AIC_TEST(ReplayToleratesATruncatedFinalLine) {
    const std::filesystem::path dir = scratchDir("replay-truncated");
    const std::string logPath = (dir / "orders.jsonl").string();
    {
        OrderRecorder recorder;
        AIC_EXPECT_TRUE(recorder.open(dir.string(), 1024 * 1024, 2), "recorder opens");
        recorder.recordAccepted(100.0, 0, makeIngressOrder(), 1, 0, {});
        recorder.recordAccepted(120.0, 1, makeIngressOrder(), 2, 0, {});
    }
    {
        std::ofstream stream(logPath, std::ios::app | std::ios::binary);
        stream << R"({"t":140.0,"frame":2,"event":"order.accepted","seri)";
    }

    ReplayLlmClient replay;
    std::string error;
    AIC_EXPECT_TRUE(replay.load(logPath, error),
                    "a truncated final line must not abandon the log: " + error);
    AIC_EXPECT_EQ(replay.entryCount(), static_cast<std::size_t>(2),
                  "the two complete records must still be replayable");
    return true;
}

// C18 (AIC-DET-1, v1.8.27). `order.requested` must carry the own-ship state the snapshot
// transmitted, not merely a digest of it.
//
// THE PROPERTY THIS PROTECTS is that two questions stay distinguishable: "the model emitted 1.5 m/s"
// and "the snapshot said 1.5 m/s". Before this block they were one unresolved observation, because
// snapshotHash answers "did the picture change?" and never "what WAS the picture?" - and position
// moves every tick, so the hash changes whether or not any other field did. The distinct sentinel
// values below are what make a transposed or duplicated field visible.
AIC_TEST(RequestedRecordCarriesTheOwnShipSnapshot) {
    const std::filesystem::path dir = scratchDir("requested-own");
    const std::string logPath = (dir / "orders.jsonl").string();

    OrderSnapshot snapshot;
    snapshot.entityId = "RedSu35_01";
    snapshot.serial = 7;
    snapshot.latitudeDeg = 13.49;
    snapshot.longitudeDeg = 144.83;
    snapshot.altitudeHaeM = 10000.0;
    snapshot.headingDeg = 271.5;
    snapshot.speedMps = 319.75;
    snapshot.velNMps = 0.5;
    snapshot.velEMps = -319.75;
    snapshot.velDMps = 0.25;

    {
        OrderRecorder recorder;
        AIC_EXPECT_TRUE(recorder.open(dir.string(), 1024 * 1024, 2), "recorder opens");
        recorder.recordRequested(100.0, 0, snapshot, "local", "qwen2.5:7b",
                                 "fnv1a64:aa", "fnv1a64:bb");
    }

    std::ifstream stream(logPath, std::ios::binary);
    std::string line;
    AIC_EXPECT_TRUE(static_cast<bool>(std::getline(stream, line)), "a record was written");

    for (const char* needle : {"\"own\"", "\"headingDeg\":271.5", "\"speedMps\":319.75",
                               "\"velN\":0.5", "\"velE\":-319.75", "\"velD\":0.25",
                               "\"latitudeDeg\":13.49", "\"longitudeDeg\":144.83"}) {
        AIC_EXPECT_TRUE(line.find(needle) != std::string::npos,
                        std::string("order.requested must carry ") + needle + "; got: " + line);
    }

    // Own-ship only, deliberately: tracks and loadout are reconstructible from the Tier-1 ingress
    // calls and would dominate the record. If someone later widens this, they should have to say so.
    AIC_EXPECT_TRUE(line.find("\"tracks\"") == std::string::npos,
                    "the requested record carries own-ship state only, not the track list");
    return true;
}

// A replay backend with nothing to replay is a configuration error, not an empty run.
AIC_TEST(ReplayRejectsALogWithNoOrders) {
    const std::filesystem::path dir = scratchDir("replay-empty");
    const std::string logPath = (dir / "orders.jsonl").string();
    {
        OrderRecorder recorder;
        AIC_EXPECT_TRUE(recorder.open(dir.string(), 1024 * 1024, 2), "recorder opens");
        // Rejections only - no accepted orders.
        recorder.recordRejected(100.0, 0, "RedSu35_01", 1, RejectReason::Track,
                                "hallucinated target", "{}", {});
    }

    ReplayLlmClient replay;
    std::string error;
    AIC_EXPECT_FALSE(replay.load(logPath, error), "a log with no accepted orders must not load");
    AIC_EXPECT_TRUE(error.find("order.accepted") != std::string::npos,
                    "the error must say what was missing, got: " + error);
    return true;
}

// The recorder must neutralize an attacker-influenced raw body before it reaches the log, because
// that file is read back by replay and by humans. A body carrying a newline could otherwise forge
// an entire additional record.
AIC_TEST(RecorderSanitizesRawRejectedBodies) {
    const std::filesystem::path dir = scratchDir("recorder-sanitize");
    const std::string logPath = (dir / "orders.jsonl").string();
    {
        OrderRecorder recorder;
        AIC_EXPECT_TRUE(recorder.open(dir.string(), 1024 * 1024, 2), "recorder opens");
        recorder.recordRejected(100.0, 0, "RedSu35_01", 1, RejectReason::Schema, "bad body",
                                "junk\n{\"event\":\"order.accepted\",\"forged\":true}", {});
    }

    std::ifstream stream(logPath, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    // Exactly one line: the forged record must not have become a line of its own.
    const std::size_t lineCount = static_cast<std::size_t>(
        std::count(content.begin(), content.end(), '\n'));
    AIC_EXPECT_EQ(lineCount, static_cast<std::size_t>(1),
                  "a raw body carrying a newline must not forge an additional log record");

    // And the log must still parse as JSONL.
    const std::optional<n8ro::core::JsonValue> parsed = n8ro::core::JsonValue::parse(content);
    AIC_EXPECT_TRUE(parsed.has_value(), "the single record must still be valid JSON");
    return true;
}
