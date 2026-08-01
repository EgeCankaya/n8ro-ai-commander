#include "OrderValidatorStageA.h"

#include "OrderSchema.h"

#include <core/json/JsonValue.h>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace arkheon::aicommander {

namespace {

using n8ro::core::JsonValue;

StageAOutcome reject(RejectReason reason, std::string detail) {
    StageAOutcome outcome;
    outcome.accepted = false;
    outcome.reason = reason;
    outcome.detail = std::move(detail);
    return outcome;
}

// The properties the document may carry. Anything else is a rejection under
// additionalProperties:false — checked explicitly here so the detail can name the offending
// property, which the schema validator's message would not.
const std::vector<std::string>& allowedTopLevelProperties() {
    static const std::vector<std::string> allowed{
        "schemaVersion", "entityId", "posture", "targetEntityId",
        "waypoint", "cruiseSpeedMps", "orbitRadiusM", "roe", "reason",
    };
    return allowed;
}

const std::vector<std::string>& allowedWaypointProperties() {
    static const std::vector<std::string> allowed{"latitudeDeg", "longitudeDeg", "altitudeHaeM"};
    return allowed;
}

[[nodiscard]] bool contains(const std::vector<std::string>& haystack, const std::string& needle) {
    for (const std::string& candidate : haystack) {
        if (candidate == needle) {
            return true;
        }
    }
    return false;
}

// JsonValue::asDouble() returns 0.0 on a type mismatch, so a missing or non-numeric field would
// silently read as a valid zero. Every numeric read goes through this instead.
[[nodiscard]] bool readNumber(const JsonValue& object, const char* key, double& out) {
    if (!object.has(key)) {
        return false;
    }
    const JsonValue node = object.get(key);
    if (!node.isNumber()) {
        return false;
    }
    const std::optional<double> value = node.tryGetDouble();
    if (!value.has_value()) {
        return false;
    }
    out = *value;
    return true;
}

[[nodiscard]] bool readString(const JsonValue& object, const char* key, std::string& out) {
    if (!object.has(key)) {
        return false;
    }
    const JsonValue node = object.get(key);
    if (!node.isString()) {
        return false;
    }
    out = node.asString();
    return true;
}

} // namespace

bool isSanitizedText(const std::string& text) {
    for (const char raw : text) {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (c < 0x20 || c > 0x7E) {
            return false;  // control character or non-ASCII
        }
        if (raw == '"' || raw == '\\') {
            return false;  // would need escaping to survive the JSONL order log
        }
    }
    return true;
}

std::string sanitizeText(const std::string& text, std::size_t maxChars) {
    std::string out;
    out.reserve(text.size() < maxChars ? text.size() : maxChars);
    for (const char raw : text) {
        if (out.size() >= maxChars) {
            break;
        }
        const unsigned char c = static_cast<unsigned char>(raw);
        if (c < 0x20 || c > 0x7E || raw == '"' || raw == '\\') {
            continue;
        }
        out.push_back(raw);
    }
    return out;
}

StageAOutcome validateStageA(const std::string& body, const std::string& requestingEntityId) {
    // -- A0: size ------------------------------------------------------------------------------
    // Before the parse, deliberately. A 10 MB body is not an order and parsing it to discover that
    // spends the worker's time on the adversary's terms.
    if (body.size() > kMaxResponseBodyBytes) {
        return reject(RejectReason::Range,
            "response body is " + std::to_string(body.size()) + " bytes, over the "
                + std::to_string(kMaxResponseBodyBytes) + "-byte cap");
    }
    if (body.empty()) {
        return reject(RejectReason::Parse, "response body is empty");
    }

    // -- A3: one well-formed JSON object -------------------------------------------------------
    const std::optional<JsonValue> parsed = JsonValue::parse(body);
    if (!parsed.has_value()) {
        // Also where bare NaN / Infinity land: neither is valid JSON, so they never reach the
        // numeric checks at all.
        return reject(RejectReason::Parse, "body is not well-formed JSON");
    }
    if (!parsed->isObject()) {
        return reject(RejectReason::Parse, "body parsed, but is not a JSON object");
    }
    const JsonValue& doc = *parsed;

    // -- A4a: no unknown properties ------------------------------------------------------------
    for (const std::string& key : doc.keys()) {
        if (!contains(allowedTopLevelProperties(), key)) {
            return reject(RejectReason::Schema, "unknown top-level property '" + key + "'");
        }
    }

    // -- A5: schemaVersion ---------------------------------------------------------------------
    if (!doc.has("schemaVersion")) {
        return reject(RejectReason::Schema, "missing required property 'schemaVersion'");
    }
    {
        const JsonValue node = doc.get("schemaVersion");
        if (!node.isNumber()) {
            return reject(RejectReason::Schema, "'schemaVersion' must be an integer");
        }
        const std::int64_t version = node.asInt64();
        if (version != kOrderSchemaVersion) {
            // Distinct from `schema`: a well-formed order of the wrong vintage is a different
            // problem from a malformed one, and it is the signal that the model's prompt and this
            // build have diverged.
            return reject(RejectReason::Version,
                "schemaVersion is " + std::to_string(version) + ", expected "
                    + std::to_string(kOrderSchemaVersion));
        }
    }

    // -- required fields, with types -----------------------------------------------------------
    Order order;
    order.schemaVersion = kOrderSchemaVersion;

    if (!readString(doc, "entityId", order.entityId)) {
        return reject(RejectReason::Schema, "'entityId' is missing or not a string");
    }
    if (order.entityId.empty()) {
        return reject(RejectReason::Schema, "'entityId' must not be empty");
    }

    std::string postureText;
    if (!readString(doc, "posture", postureText)) {
        return reject(RejectReason::Schema, "'posture' is missing or not a string");
    }
    std::string roeText;
    if (!readString(doc, "roe", roeText)) {
        return reject(RejectReason::Schema, "'roe' is missing or not a string");
    }
    if (!doc.has("cruiseSpeedMps")) {
        return reject(RejectReason::Schema, "missing required property 'cruiseSpeedMps'");
    }
    if (!readNumber(doc, "cruiseSpeedMps", order.cruiseSpeedMps)) {
        return reject(RejectReason::Schema, "'cruiseSpeedMps' must be a number");
    }
    if (!readString(doc, "reason", order.reason)) {
        return reject(RejectReason::Schema, "'reason' is missing or not a string");
    }

    // -- A5: enum membership -------------------------------------------------------------------
    if (!tryParsePosture(postureText, order.posture)) {
        return reject(RejectReason::Enum, "'posture' is not a member of the vocabulary: '"
            + sanitizeText(postureText, 32) + "'");
    }
    if (!tryParseRoe(roeText, order.roe)) {
        return reject(RejectReason::Enum, "'roe' is not a member of the vocabulary: '"
            + sanitizeText(roeText, 32) + "'");
    }

    // -- entity correspondence -----------------------------------------------------------------
    // The worker holds the snapshot it asked about, so this is a pure comparison — no roster and
    // no SDK needed. An order for a different entity is not "the wrong entity is commanded", it is
    // a response that does not answer the request.
    if (order.entityId != requestingEntityId) {
        return reject(RejectReason::Shape,
            "order names entity '" + sanitizeText(order.entityId, 64) + "' but the request was for '"
                + sanitizeText(requestingEntityId, 64) + "'");
    }

    // -- A6: conditional presence --------------------------------------------------------------
    const bool needsTarget = postureRequiresTarget(order.posture);
    const bool needsWaypoint = postureRequiresWaypoint(order.posture);

    if (doc.has("targetEntityId")) {
        if (!readString(doc, "targetEntityId", order.targetEntityId)) {
            return reject(RejectReason::Schema, "'targetEntityId' must be a string");
        }
    }
    if (needsTarget && order.targetEntityId.empty()) {
        return reject(RejectReason::Shape,
            std::string("posture '") + toString(order.posture) + "' requires a non-empty targetEntityId");
    }
    if (!needsTarget && !order.targetEntityId.empty()) {
        return reject(RejectReason::Shape,
            std::string("posture '") + toString(order.posture)
                + "' must not carry a targetEntityId; the script selects its own");
    }

    if (needsWaypoint) {
        if (!doc.has("waypoint")) {
            return reject(RejectReason::Shape,
                std::string("posture '") + toString(order.posture) + "' requires a waypoint");
        }
        const JsonValue waypoint = doc.get("waypoint");
        if (!waypoint.isObject()) {
            return reject(RejectReason::Schema, "'waypoint' must be an object");
        }
        for (const std::string& key : waypoint.keys()) {
            if (!contains(allowedWaypointProperties(), key)) {
                return reject(RejectReason::Schema, "unknown property 'waypoint." + key + "'");
            }
        }
        if (!readNumber(waypoint, "latitudeDeg", order.latitudeDeg)
            || !readNumber(waypoint, "longitudeDeg", order.longitudeDeg)
            || !readNumber(waypoint, "altitudeHaeM", order.altitudeHaeM)) {
            return reject(RejectReason::Schema,
                "'waypoint' must carry numeric latitudeDeg, longitudeDeg, and altitudeHaeM");
        }
    } else if (doc.has("waypoint")) {
        // Not merely ignored. An accepted order carrying a field nothing reads is an order whose
        // author and reader disagree about what was commanded.
        return reject(RejectReason::Shape,
            std::string("posture '") + toString(order.posture)
                + "' must not carry a waypoint; the script computes the geometry");
    }

    if (doc.has("orbitRadiusM") && !readNumber(doc, "orbitRadiusM", order.orbitRadiusM)) {
        return reject(RejectReason::Schema, "'orbitRadiusM' must be a number");
    }
    if (order.posture == Posture::Hold) {
        if (order.orbitRadiusM <= 0.0) {
            return reject(RejectReason::Shape, "posture 'hold' requires orbitRadiusM > 0");
        }
    } else if (order.orbitRadiusM != 0.0) {
        return reject(RejectReason::Shape,
            std::string("posture '") + toString(order.posture) + "' requires orbitRadiusM == 0");
    }

    // -- A7: finiteness, static range, string caps and charset ---------------------------------
    if (!std::isfinite(order.cruiseSpeedMps)) {
        return reject(RejectReason::Range, "'cruiseSpeedMps' is not finite");
    }
    if (order.cruiseSpeedMps <= 0.0) {
        return reject(RejectReason::Range, "'cruiseSpeedMps' must be > 0");
    }
    // A static sanity ceiling, distinct from safety.maxSpeedMps. The configured envelope is a
    // Stage-B concern; this only rejects values no airframe in any configuration could mean, so
    // that an absurd speed reads as `range` rather than reaching the configured-clamp counter and
    // making it look like an envelope-tuning problem.
    if (order.cruiseSpeedMps > kMaxCruiseSpeedMps) {
        return reject(RejectReason::Range, "'cruiseSpeedMps' exceeds the static ceiling of "
            + std::to_string(static_cast<long long>(kMaxCruiseSpeedMps)) + " m/s");
    }
    if (!std::isfinite(order.orbitRadiusM)) {
        return reject(RejectReason::Range, "'orbitRadiusM' is not finite");
    }
    if (order.orbitRadiusM > kMaxOrbitRadiusM) {
        return reject(RejectReason::Range, "'orbitRadiusM' exceeds "
            + std::to_string(static_cast<long long>(kMaxOrbitRadiusM)) + " m");
    }

    if (needsWaypoint) {
        if (!std::isfinite(order.latitudeDeg) || !std::isfinite(order.longitudeDeg)
            || !std::isfinite(order.altitudeHaeM)) {
            return reject(RejectReason::Range, "waypoint carries a non-finite coordinate");
        }
        if (order.latitudeDeg < kMinLatitudeDeg || order.latitudeDeg > kMaxLatitudeDeg) {
            return reject(RejectReason::Range, "waypoint.latitudeDeg is outside [-90, 90]");
        }
        if (order.longitudeDeg < kMinLongitudeDeg || order.longitudeDeg > kMaxLongitudeDeg) {
            return reject(RejectReason::Range, "waypoint.longitudeDeg is outside [-180, 180]");
        }
        // Altitude's operating envelope is a safety.* bound and belongs to Stage B, where the live
        // configuration is available. Only the physically meaningless is rejected here.
    }

    if (order.entityId.size() > kMaxEntityIdChars || order.targetEntityId.size() > kMaxEntityIdChars) {
        return reject(RejectReason::Range, "an entity id exceeds "
            + std::to_string(kMaxEntityIdChars) + " characters");
    }
    if (order.reason.size() < kMinReasonChars || order.reason.size() > kMaxReasonChars) {
        return reject(RejectReason::Range, "'reason' must be 1-"
            + std::to_string(kMaxReasonChars) + " characters, got "
            + std::to_string(order.reason.size()));
    }
    if (!isSanitizedText(order.entityId) || !isSanitizedText(order.targetEntityId)) {
        return reject(RejectReason::Range, "an entity id carries characters outside the permitted set");
    }
    if (!isSanitizedText(order.reason)) {
        return reject(RejectReason::Range, "'reason' carries characters outside the permitted set");
    }

    // -- A4b: schema document as a structural backstop -------------------------------------------
    // The explicit checks above produce the precise reason codes the runbook needs. This catches
    // anything they did not think to look at, so the embedded schema stays a real validator rather
    // than decoration that only the model ever sees.
    {
        std::string schemaError;
        if (!doc.validateAgainstSchema(orderJsonSchema(), &schemaError)) {
            return reject(RejectReason::Schema,
                "failed schema validation: " + sanitizeText(schemaError, 200));
        }
    }

    StageAOutcome outcome;
    outcome.accepted = true;
    outcome.reason = RejectReason::None;
    outcome.order = std::move(order);
    return outcome;
}

} // namespace arkheon::aicommander
