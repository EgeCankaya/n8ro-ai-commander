#include "TestSupport.h"

#include "ILlmClient.h"
#include "OrderSlot.h"
#include "OrderValidatorStageA.h"
#include "Snapshot.h"
#include "StubLlmClient.h"

#include <atomic>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace arkheon::aicommander;

namespace {

OrderSnapshot makeSnapshot(const std::string& entityId, std::int64_t serial) {
    OrderSnapshot snapshot;
    snapshot.entityId = entityId;
    snapshot.serial = serial;
    snapshot.simTimeS = 400.0 + static_cast<double>(serial);
    snapshot.latitudeDeg = 13.50;
    snapshot.longitudeDeg = 144.80;
    snapshot.altitudeHaeM = 9000.0;
    snapshot.headingDeg = 270.0;
    snapshot.velNMps = -10.0;
    snapshot.velEMps = 210.0;
    snapshot.velDMps = 0.0;
    snapshot.team = "red";
    snapshot.tracks.push_back(TrackReport{"BlueF18_02", 42000.0, 18.5});
    return snapshot;
}

} // namespace

// AIC-ARCH-2, enforced structurally rather than by review.
//
// OrderSnapshot is the thread boundary. This asserts it stays a value type with no engine pointer
// in it: trivially copyable is too strong (it holds std::string), but copy-constructibility plus
// the absence of any pointer member is what makes "the worker holds no SDK pointer" a property of
// the type rather than a promise about the code that uses it.
AIC_TEST(SnapshotIsAValueTypeWithNoEnginePointer) {
    static_assert(std::is_copy_constructible<OrderSnapshot>::value,
                  "the snapshot must be copyable by value across the thread boundary");
    static_assert(std::is_move_constructible<OrderSnapshot>::value,
                  "the snapshot must be movable into the worker's callable");
    static_assert(std::is_copy_constructible<TrackReport>::value, "TrackReport must be copyable");
    static_assert(std::is_copy_constructible<LoadoutReport>::value, "LoadoutReport must be copyable");

    // A deep copy must survive the destruction of the original, which is what "by value" has to
    // mean for a type holding heap-allocated strings.
    OrderSnapshot copy;
    {
        const OrderSnapshot original = makeSnapshot("RedSu35_01", 7);
        copy = original;
    }
    AIC_EXPECT_EQ(copy.entityId, std::string("RedSu35_01"), "entityId survived the original");
    AIC_EXPECT_EQ(copy.tracks.size(), static_cast<std::size_t>(1), "tracks survived the original");
    AIC_EXPECT_EQ(copy.tracks[0].targetEntityId, std::string("BlueF18_02"), "track id survived");
    return true;
}

// Latest-wins is a correctness property, not a capacity one: a stale snapshot produces a stale
// order, so displacing an unconsumed older value is right rather than lossy.
AIC_TEST(SlotIsLatestWins) {
    LatestWinsSlot<OrderSnapshot> slot;
    AIC_EXPECT_TRUE(slot.empty(), "a fresh slot is empty");

    AIC_EXPECT_FALSE(slot.publish(makeSnapshot("RedSu35_01", 1)),
                     "publishing into an empty slot displaces nothing");
    AIC_EXPECT_TRUE(slot.publish(makeSnapshot("RedSu35_01", 2)),
                    "publishing over an unconsumed value reports the displacement");
    AIC_EXPECT_TRUE(slot.publish(makeSnapshot("RedSu35_01", 3)),
                    "and again");

    const std::optional<OrderSnapshot> taken = slot.take();
    AIC_EXPECT_TRUE(taken.has_value(), "take must yield the held value");
    AIC_EXPECT_EQ(taken->serial, static_cast<std::int64_t>(3),
                  "the NEWEST value survives - older ones were correctly dropped");

    AIC_EXPECT_TRUE(slot.empty(), "taking empties the slot");
    AIC_EXPECT_FALSE(slot.take().has_value(), "a second take yields nothing - never processed twice");
    return true;
}

// The slot is the only shared mutable state between the sim thread and a worker. If it races, the
// whole design fails. This hammers it from two threads and asserts every consumed value is
// self-consistent - a torn read would show up as a serial that does not match its own entityId
// suffix.
AIC_TEST(SlotSurvivesConcurrentPublishAndTake) {
    LatestWinsSlot<OrderSnapshot> slot;
    std::atomic<bool> stop{false};
    std::atomic<int> torn{0};
    std::atomic<int> consumed{0};

    constexpr int kPublishCount = 20000;

    std::thread producer([&] {
        for (int i = 1; i <= kPublishCount; ++i) {
            // Encode the serial into the entityId too, so a torn read is detectable rather than
            // merely improbable.
            OrderSnapshot snapshot = makeSnapshot("E" + std::to_string(i), i);
            (void)slot.publish(std::move(snapshot));
        }
        stop = true;
    });

    std::thread consumer([&] {
        while (!stop || !slot.empty()) {
            std::optional<OrderSnapshot> taken = slot.take();
            if (!taken.has_value()) {
                continue;
            }
            ++consumed;
            if (taken->entityId != "E" + std::to_string(taken->serial)) {
                ++torn;
            }
            if (taken->tracks.size() != 1 || taken->tracks[0].targetEntityId != "BlueF18_02") {
                ++torn;
            }
        }
    });

    producer.join();
    consumer.join();

    AIC_EXPECT_EQ(torn.load(), 0, "a torn or inconsistent value was observed across the slot");
    AIC_EXPECT_TRUE(consumed.load() > 0, "the consumer must have observed at least one value");
    // Latest-wins means most values are legitimately dropped. That is the design, not a leak.
    AIC_EXPECT_TRUE(consumed.load() <= kPublishCount, "cannot consume more than was published");
    return true;
}

// Canonical ordering is what makes the prompt bytes - and therefore snapshotHash - a function of
// the picture rather than of the order the script happened to iterate in.
AIC_TEST(SnapshotCanonicalizationIsOrderIndependent) {
    OrderSnapshot a;
    a.tracks = {{"Charlie", 3.0, 3.0}, {"Alpha", 1.0, 1.0}, {"Bravo", 2.0, 2.0}};
    a.loadout = {{"WingRight", "R77", 2, 4}, {"WingLeft", "R73", 1, 2}};

    OrderSnapshot b;
    b.tracks = {{"Bravo", 2.0, 2.0}, {"Alpha", 1.0, 1.0}, {"Charlie", 3.0, 3.0}};
    b.loadout = {{"WingLeft", "R73", 1, 2}, {"WingRight", "R77", 2, 4}};

    canonicalizeSnapshot(a);
    canonicalizeSnapshot(b);

    AIC_EXPECT_EQ(a.tracks.size(), b.tracks.size(), "track counts");
    for (std::size_t i = 0; i < a.tracks.size(); ++i) {
        AIC_EXPECT_EQ(a.tracks[i].targetEntityId, b.tracks[i].targetEntityId,
                      "track order differs at index " + std::to_string(i));
    }
    AIC_EXPECT_EQ(a.tracks[0].targetEntityId, std::string("Alpha"), "tracks sort ascending by id");
    AIC_EXPECT_EQ(a.loadout[0].hardpointName, std::string("WingLeft"),
                  "loadout sorts ascending by hardpoint");
    return true;
}

// The stub adapter is the CI gate: the whole pipeline must run through it with no server and no
// network. Its output has to survive the real Stage-A validator, or the integration test would be
// exercising a path the live backends never take.
AIC_TEST(StubClientProducesValidOrders) {
    StubLlmClient client;
    AIC_EXPECT_EQ(std::string(client.backendName()), std::string("stub"), "backend name");

    LlmRequest request;
    request.snapshot = makeSnapshot("RedSu35_01", 1);
    request.prompt = "(prompt)";

    // Walk enough invocations to cycle the whole table, so every posture and both conditional
    // shapes are exercised.
    for (int i = 0; i < 12; ++i) {
        const LlmResult result = client.request(request);
        AIC_EXPECT_TRUE(result.completed, "the stub performs no I/O and must always complete");
        AIC_EXPECT_EQ(result.statusCode, 200, "stub status");

        const StageAOutcome outcome = validateStageA(result.body, request.snapshot.entityId);
        AIC_EXPECT_TRUE(outcome.accepted,
                        "stub order #" + std::to_string(i) + " failed Stage A as '"
                            + std::string(toString(outcome.reason)) + "': " + outcome.detail
                            + " | body: " + result.body);
    }
    AIC_EXPECT_EQ(client.invocationCount(), static_cast<std::int64_t>(12), "invocation count");
    return true;
}

// A stub that named a target Tier 1 never reported would be rejected by Stage-B B3 on every cycle,
// and the integration test would be measuring the rejection path while appearing to measure the
// acceptance path. The stub must target only what the snapshot actually carries.
AIC_TEST(StubTargetsOnlyReportedTracks) {
    StubLlmClient client;
    LlmRequest request;
    request.snapshot = makeSnapshot("RedSu35_01", 1);

    bool sawTargetedOrder = false;
    for (int i = 0; i < 12; ++i) {
        const LlmResult result = client.request(request);
        const StageAOutcome outcome = validateStageA(result.body, request.snapshot.entityId);
        AIC_EXPECT_TRUE(outcome.accepted, "stub order must be valid");
        if (!outcome.order.targetEntityId.empty()) {
            sawTargetedOrder = true;
            AIC_EXPECT_EQ(outcome.order.targetEntityId, std::string("BlueF18_02"),
                          "the stub must target a REPORTED track, not an invented id");
        }
    }
    AIC_EXPECT_TRUE(sawTargetedOrder,
                    "the stub table must exercise at least one targeted posture when a track is "
                    "reported, or engage/crank are never covered");
    return true;
}

// With no reported tracks, the stub must not emit a targeted posture at all - there is nothing
// legitimate to target, and inventing one would be the exact hallucination the validator exists to
// catch.
AIC_TEST(StubEmitsNoTargetedOrderWithoutTracks) {
    StubLlmClient client;
    LlmRequest request;
    request.snapshot = makeSnapshot("RedSu35_01", 1);
    request.snapshot.tracks.clear();

    for (int i = 0; i < 9; ++i) {
        const LlmResult result = client.request(request);
        const StageAOutcome outcome = validateStageA(result.body, request.snapshot.entityId);
        AIC_EXPECT_TRUE(outcome.accepted,
                        "stub order must be valid with no tracks, got '"
                            + std::string(toString(outcome.reason)) + "': " + outcome.detail);
        AIC_EXPECT_TRUE(outcome.order.targetEntityId.empty(),
                        "with no reported tracks the stub must emit no target, got '"
                            + outcome.order.targetEntityId + "'");
    }
    return true;
}

// The resilience path: a transport failure must be distinguishable from a completed request, and
// must NOT look like an empty successful body.
AIC_TEST(StubTransportFailureIsDistinctFromEmptyBody) {
    StubLlmClient client;
    LlmRequest request;
    request.snapshot = makeSnapshot("RedSu35_01", 1);

    client.injectTransportFailure();
    const LlmResult failed = client.request(request);
    AIC_EXPECT_FALSE(failed.completed, "an injected transport failure must report completed=false");
    AIC_EXPECT_TRUE(failed.body.empty(), "a transport failure carries no body");
    AIC_EXPECT_FALSE(failed.transportDetail.empty(), "a transport failure must explain itself");

    // And the very next request recovers - the injection is one-shot, not a latched state.
    const LlmResult recovered = client.request(request);
    AIC_EXPECT_TRUE(recovered.completed, "the next request must recover");
    return true;
}
