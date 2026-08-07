#include "OrderRecorder.h"

#include "OrderValidatorStageA.h"

#include <core/json/JsonValue.h>

#include <filesystem>
#include <string>

namespace arkheon::aicommander {

namespace {

using n8ro::core::JsonValue;

// Serializes an order into the record's `order` field. Field-for-field with the wire schema, so a
// recorded order can be replayed through the identical Stage-A path it originally took.
JsonValue orderToJson(const Order& order) {
    JsonValue node = JsonValue::object();
    (void)node.setInt64("schemaVersion", order.schemaVersion);
    (void)node.setString("entityId", order.entityId);
    (void)node.setString("posture", toString(order.posture));
    (void)node.setString("targetEntityId", order.targetEntityId);
    (void)node.setDouble("cruiseSpeedMps", order.cruiseSpeedMps);
    (void)node.setDouble("orbitRadiusM", order.orbitRadiusM);
    (void)node.setString("roe", toString(order.roe));
    (void)node.setString("reason", order.reason);
    if (order.hasWaypoint()) {
        JsonValue waypoint = JsonValue::object();
        (void)waypoint.setDouble("latitudeDeg", order.latitudeDeg);
        (void)waypoint.setDouble("longitudeDeg", order.longitudeDeg);
        (void)waypoint.setDouble("altitudeHaeM", order.altitudeHaeM);
        (void)node.set("waypoint", waypoint);
    }
    return node;
}

JsonValue baseRecord(double simTimeS, std::int64_t frame, OrderEvent event) {
    JsonValue record = JsonValue::object();
    (void)record.setDouble("t", simTimeS);
    (void)record.setInt64("frame", frame);
    (void)record.setString("event", toString(event));
    return record;
}

} // namespace

const char* toString(OrderEvent event) {
    switch (event) {
        case OrderEvent::CommanderEnabled:  return "commander.enabled";
        case OrderEvent::CommanderDisabled: return "commander.disabled";
        case OrderEvent::OrderRequested:    return "order.requested";
        case OrderEvent::OrderAccepted:     return "order.accepted";
        case OrderEvent::OrderRejected:     return "order.rejected";
        case OrderEvent::OrderTimeout:      return "order.timeout";
        case OrderEvent::FallbackStanding:  return "fallback.standing";
        case OrderEvent::FallbackReleased:  return "fallback.released";
        case OrderEvent::ReplayDivergence:  return "replay.divergence";
    }
    return "order.rejected";
}

OrderRecorder::~OrderRecorder() {
    close();
}

bool OrderRecorder::open(const std::string& directory, std::size_t maxBytesPerFile, int maxFiles) {
    close();

    directory_ = directory;
    maxBytesPerFile_ = maxBytesPerFile > 0 ? maxBytesPerFile : (16 * 1024 * 1024);
    maxFiles_ = maxFiles > 0 ? maxFiles : 4;

    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    if (ec) {
        // Best-effort by design: recording must never stall or fail a frame. A failure here
        // disables the log and the run continues without it.
        return false;
    }

    activePath_ = (std::filesystem::path(directory_) / "orders.jsonl").string();
    stream_.open(activePath_, std::ios::out | std::ios::app | std::ios::binary);
    if (!stream_.is_open()) {
        return false;
    }

    bytesWritten_ = static_cast<std::size_t>(std::filesystem::file_size(activePath_, ec));
    if (ec) {
        bytesWritten_ = 0;
    }
    return true;
}

void OrderRecorder::close() {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
    bytesWritten_ = 0;
}

void OrderRecorder::rotateIfNeeded() {
    if (bytesWritten_ < maxBytesPerFile_) {
        return;
    }

    stream_.flush();
    stream_.close();

    std::error_code ec;
    // Shift orders.N -> orders.N+1, dropping the oldest. Total on-disk size is bounded by
    // maxBytesPerFile * maxFiles, which is what keeps a long run from exhausting the disk through
    // a log that is on by default.
    const std::filesystem::path base(activePath_);
    for (int index = maxFiles_ - 1; index >= 1; --index) {
        const std::filesystem::path from =
            index == 1 ? base : std::filesystem::path(activePath_ + "." + std::to_string(index - 1));
        const std::filesystem::path to(activePath_ + "." + std::to_string(index));
        if (std::filesystem::exists(from, ec)) {
            std::filesystem::remove(to, ec);
            std::filesystem::rename(from, to, ec);
        }
    }

    stream_.open(activePath_, std::ios::out | std::ios::trunc | std::ios::binary);
    bytesWritten_ = 0;
}

void OrderRecorder::writeLine(const std::string& line) {
    if (!stream_.is_open()) {
        return;
    }
    stream_ << line << '\n';
    bytesWritten_ += line.size() + 1;
    // Flushed per record rather than buffered: the log's main job is post-mortem, and a crash is
    // exactly when the last few records matter most. One order every 20 s per entity makes the
    // cost irrelevant.
    stream_.flush();
    rotateIfNeeded();
}

void OrderRecorder::recordRequested(
    double simTimeS, std::int64_t frame, const OrderSnapshot& snapshot,
    const std::string& backend, const std::string& model,
    const std::string& snapshotHash, const std::string& promptHash) {
    JsonValue record = baseRecord(simTimeS, frame, OrderEvent::OrderRequested);
    (void)record.setString("entityId", snapshot.entityId);
    (void)record.setInt64("serial", snapshot.serial);
    (void)record.setString("backend", backend);
    (void)record.setString("model", model);
    (void)record.setString("snapshotHash", snapshotHash);
    (void)record.setString("promptHash", promptHash);
    (void)record.setInt64("tracksReported", static_cast<std::int64_t>(snapshot.tracks.size()));

    // The own-ship state the snapshot actually transmitted (AIC-DET-1, v1.8.27, C18).
    //
    // WHY, AND WHY THE HASH WAS NOT ENOUGH. This FR's Customer scenario has promised since v1.0 that
    // an engineer "reads the exact order, THE SNAPSHOT THAT PRODUCED IT, and the model's stated
    // reason" - and no record has ever carried a snapshot. `snapshotHash` cannot stand in for one:
    // it is a digest, it answers "did the picture change?" and never "what WAS the picture?", and
    // position moves every tick, so it changes whether or not any other field did.
    //
    // The concrete question it exists to answer: when an accepted order commands 1.5 m/s, was that
    // the model inventing a number or the snapshot reporting one? Before this block those were one
    // unresolved observation (PRD Corrections item 42(e)). `headingDeg` is here for the same reason
    // - the schema documents componentTransform/headingDeg as the heading at t=0, so whether it
    // tracks the physics pass is checkable only by watching it move across a run.
    //
    // OWN-SHIP ONLY. tracks[] and loadout[] are deliberately excluded: both are already
    // reconstructible from the Tier-1 ingress calls, and both would dominate the record's size for
    // a question nobody has asked of them.
    JsonValue own = JsonValue::object();
    (void)own.setDouble("latitudeDeg", snapshot.latitudeDeg);
    (void)own.setDouble("longitudeDeg", snapshot.longitudeDeg);
    (void)own.setDouble("altitudeHaeM", snapshot.altitudeHaeM);
    (void)own.setDouble("headingDeg", snapshot.headingDeg);
    (void)own.setDouble("speedMps", snapshot.speedMps);
    (void)own.setDouble("velN", snapshot.velNMps);
    (void)own.setDouble("velE", snapshot.velEMps);
    (void)own.setDouble("velD", snapshot.velDMps);
    (void)record.set("own", own);

    writeLine(record.toString());
}

namespace {

// Written unconditionally, zeros included (AIC-DET-1, v1.8.18). Omitting a zero would be the
// obvious economy and it is the wrong one: on the hosted path a zero in BOTH cache fields is not
// an absence of information, it is the observation AIC-BE-3's guard turns on — the block never
// cached. A field that disappears exactly when it carries the interesting value is worse than no
// field, and a reader diffing two order logs would see nothing where the regression is.
void writeUsage(JsonValue& record, const OrderRecorder::TokenUsage& usage) {
    (void)record.setInt64("tokensIn", usage.tokensIn);
    (void)record.setInt64("tokensOut", usage.tokensOut);
    (void)record.setInt64("cacheReadTokens", usage.cacheReadTokens);
    (void)record.setInt64("cacheCreationTokens", usage.cacheCreationTokens);
}

} // namespace

void OrderRecorder::recordAccepted(
    double publishedSimTimeS, std::int64_t frame, const Order& order, std::int64_t serial,
    std::int64_t latencyMs, const TokenUsage& usage) {
    JsonValue record = baseRecord(publishedSimTimeS, frame, OrderEvent::OrderAccepted);
    (void)record.setString("entityId", order.entityId);
    (void)record.setInt64("serial", serial);
    (void)record.setInt64("latencyMs", latencyMs);
    writeUsage(record, usage);
    (void)record.set("order", orderToJson(order));
    writeLine(record.toString());
}

void OrderRecorder::recordRejected(
    double simTimeS, std::int64_t frame, const std::string& entityId, std::int64_t serial,
    RejectReason reason, const std::string& detail, const std::string& rawBody,
    const TokenUsage& usage) {
    JsonValue record = baseRecord(simTimeS, frame, OrderEvent::OrderRejected);
    (void)record.setString("entityId", entityId);
    (void)record.setInt64("serial", serial);
    writeUsage(record, usage);
    (void)record.setString("reason", toString(reason));
    (void)record.setString("detail", detail);
    // Truncated and charset-filtered. The raw body is attacker-influenced text, and this file is
    // read back by replay and by humans — an unfiltered body could corrupt either.
    (void)record.setString("rawBody", sanitizeText(rawBody, kMaxRecordedBodyBytes));
    writeLine(record.toString());
}

void OrderRecorder::recordFallback(
    double simTimeS, std::int64_t frame, const std::string& entityId,
    FallbackLevel from, FallbackLevel to, double secondsSinceLastAccepted) {
    const OrderEvent event = (to == FallbackLevel::Released) ? OrderEvent::FallbackReleased
                                                             : OrderEvent::FallbackStanding;
    JsonValue record = baseRecord(simTimeS, frame, event);
    (void)record.setString("entityId", entityId);
    (void)record.setString("from", toString(from));
    (void)record.setString("to", toString(to));
    (void)record.setInt64("level", static_cast<std::int64_t>(to));
    (void)record.setDouble("sinceLastAcceptedS", secondsSinceLastAccepted);
    writeLine(record.toString());
}

void OrderRecorder::recordReplayDivergence(
    double simTimeS, std::int64_t frame, const std::string& entityId, std::int64_t serial,
    RejectReason failingCheck, const std::string& detail) {
    JsonValue record = baseRecord(simTimeS, frame, OrderEvent::ReplayDivergence);
    (void)record.setString("entityId", entityId);
    (void)record.setInt64("serial", serial);
    (void)record.setString("failingCheck", toString(failingCheck));
    (void)record.setString("detail", detail);
    writeLine(record.toString());
}

void OrderRecorder::recordCommanderState(double simTimeS, bool enabled, const std::string& detail) {
    JsonValue record = baseRecord(simTimeS, 0,
        enabled ? OrderEvent::CommanderEnabled : OrderEvent::CommanderDisabled);
    (void)record.setString("detail", detail);
    writeLine(record.toString());
}

} // namespace arkheon::aicommander
