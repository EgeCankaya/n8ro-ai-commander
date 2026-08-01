#include "ReplayLlmClient.h"

#include "OrderRecorder.h"

#include <core/json/JsonValue.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

namespace arkheon::aicommander {

namespace {

using n8ro::core::JsonValue;

} // namespace

bool ReplayLlmClient::load(const std::string& path, std::string& error) {
    error.clear();
    entries_.clear();
    cursors_.clear();
    sourcePath_ = path;

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "cannot open replay log '" + path + "'";
        return false;
    }

    std::string line;
    std::size_t lineNumber = 0;
    std::size_t malformed = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        if (line.empty()) {
            continue;
        }
        const std::optional<JsonValue> parsed = JsonValue::parse(line);
        if (!parsed.has_value() || !parsed->isObject()) {
            // A single malformed line does not abandon the log: a run killed mid-write leaves a
            // truncated last line, and refusing to replay everything before it would throw away
            // the whole recording over its final byte.
            ++malformed;
            continue;
        }
        if (parsed->get("event").asString() != std::string(toString(OrderEvent::OrderAccepted))) {
            continue;
        }
        const JsonValue order = parsed->get("order");
        if (!order.isObject()) {
            ++malformed;
            continue;
        }

        ReplayEntry entry;
        entry.publishedSimTimeS = parsed->get("t").asDouble();
        entry.serial = parsed->get("serial").asInt64();
        entry.entityId = order.get("entityId").asString();
        // Re-serialize the recorded order back into a wire document, so replay feeds Stage A the
        // same shape a live backend would. Replay must not get a shortcut past validation.
        entry.orderBody = order.toString();

        if (entry.entityId.empty()) {
            ++malformed;
            continue;
        }
        entries_.push_back(std::move(entry));
    }

    // Stable sort by recorded publication time, so replay is driven by simulation time rather than
    // by the order lines happened to land in the file.
    std::stable_sort(entries_.begin(), entries_.end(),
                     [](const ReplayEntry& a, const ReplayEntry& b) {
                         if (a.publishedSimTimeS != b.publishedSimTimeS) {
                             return a.publishedSimTimeS < b.publishedSimTimeS;
                         }
                         return a.serial < b.serial;
                     });

    if (entries_.empty()) {
        error = "replay log '" + path + "' carries no order.accepted records ("
            + std::to_string(lineNumber) + " lines read, " + std::to_string(malformed)
            + " unusable)";
        return false;
    }
    return true;
}

LlmResult ReplayLlmClient::request(const LlmRequest& request) {
    LlmResult result;
    result.latencyMs = 0;

    const std::string& entityId = request.snapshot.entityId;
    const double nowS = request.snapshot.simTimeS;

    std::size_t& cursor = cursors_[entityId];

    for (std::size_t i = cursor; i < entries_.size(); ++i) {
        const ReplayEntry& entry = entries_[i];
        if (entry.entityId != entityId) {
            continue;
        }
        if (entry.publishedSimTimeS > nowS) {
            // Not yet due. Leave the cursor where it is so it is picked up on a later tick.
            break;
        }
        cursor = i + 1;
        result.completed = true;
        result.statusCode = 200;
        result.body = entry.orderBody;
        return result;
    }

    // Nothing due for this entity. Reported as a completed request with an empty body rather than
    // a transport failure: the log is intact and reachable, there is simply no order at this
    // simulation time. Calling that a transport failure would drive the fallback ladder on a
    // healthy replay.
    result.completed = true;
    result.statusCode = 204;
    return result;
}

} // namespace arkheon::aicommander
