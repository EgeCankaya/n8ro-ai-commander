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

// -- A2: the backend's response envelope --------------------------------------------------------
//
// Ollama's POST /api/generate answers with an object carrying the generated text as a JSON STRING
// in `response`, alongside timing and token counters. The order document is therefore one level
// down, and A3 must parse the unwrapped string rather than the body.
//
// Every failure here is `envelope`, never `parse`. The distinction is what the runbook reads: a
// `parse` rejection says the model produced something that is not an order, while an `envelope`
// rejection says the *backend* did not deliver one. Those need different responses, so they carry
// different reasons.
[[nodiscard]] bool unwrapOllamaGenerate(const std::string& body, std::string& out, std::string& error) {
    const std::optional<JsonValue> parsed = JsonValue::parse(body);
    if (!parsed.has_value() || !parsed->isObject()) {
        error = "backend response is not a JSON object";
        return false;
    }

    // Ollama reports a server-side problem as a 200 carrying an `error` string. Surfacing it
    // verbatim is the difference between "model 'qwen2.5:7b' not found" and a bare rejection.
    if (parsed->has("error")) {
        const JsonValue node = parsed->get("error");
        error = "backend reported an error: "
            + (node.isString() ? sanitizeText(node.asString(), 200) : std::string("(non-string)"));
        return false;
    }

    if (!parsed->has("response")) {
        error = "backend response has no 'response' field";
        return false;
    }
    const JsonValue node = parsed->get("response");
    if (!node.isString()) {
        error = "backend response's 'response' field is not a string";
        return false;
    }
    out = node.asString();
    if (out.empty()) {
        // A well-formed envelope that delivered no order. Not the model's failure, so not `parse`.
        error = "backend response's 'response' field is empty";
        return false;
    }
    return true;
}

// -- A2 (Claude): Anthropic's POST /v1/messages envelope -----------------------------------------
//
// `{ "id": …, "model": …, "content": [ {"type":"text","text":"<the order>"}, … ],
//    "stop_reason": …, "stop_details": …, "usage": {…} }`
//
// Returns false on any envelope failure and fills `error`. `refusalOut` is set to true for the ONE
// case that is not an envelope failure at all — the model declining — because that outcome has its
// own reject reason and its own runbook row, and folding it into `envelope` would lose the
// distinction between "the backend malfunctioned" and "the model said no".
[[nodiscard]] bool unwrapClaudeMessages(
    const std::string& body, std::string& out, std::string& error, bool& refusalOut) {
    refusalOut = false;

    const std::optional<JsonValue> parsed = JsonValue::parse(body);
    if (!parsed.has_value() || !parsed->isObject()) {
        error = "backend response is not a JSON object";
        return false;
    }

    // An API-level error is a 200-carrying-`error` shape here too, and naming it beats a bare
    // rejection for exactly the reason the Ollama path says so.
    if (parsed->get("type").asString() == "error" || parsed->has("error")) {
        const JsonValue node = parsed->get("error");
        const std::string message = node.isObject() ? node.get("message").asString()
                                  : node.isString() ? node.asString()
                                                    : std::string();
        error = "backend reported an error: "
            + (message.empty() ? std::string("(no message)") : sanitizeText(message, 200));
        return false;
    }

    // THE REFUSAL CHECK, AND IT COMES FIRST.
    //
    // Before `content` is touched, deliberately. On a refusal `content` is empty or holds a partial
    // answer, and reading it first is precisely how a partial gets mistaken for a whole one.
    //
    // The guard is on `stop_reason` and NOT on `stop_details`. `stop_details` may be null even on a
    // refusal, so a check written against it would fall through on some refusals and then read the
    // content this check exists to protect against — a bug that would present as an occasional
    // truncated order rather than as a failure, which is the worst way for it to present.
    if (parsed->get("stop_reason").asString() == "refusal") {
        refusalOut = true;
        // stop_details is advisory. Report the category when it is there, say so when it is not,
        // and never depend on it having been there.
        const JsonValue details = parsed->get("stop_details");
        const std::string category = details.isObject() ? details.get("category").asString()
                                                        : std::string();
        error = "the model declined the request (stop_reason 'refusal', category "
            + (category.empty() ? std::string("unreported") : sanitizeText(category, 64)) + ")";
        return false;
    }

    const JsonValue content = parsed->get("content");
    if (!content.isArray()) {
        error = "backend response has no 'content' array";
        return false;
    }

    // The order is the first block of type "text". Scanned rather than indexed at [0]: the block
    // list is documented as potentially carrying other block types, and hard-coding position zero
    // would turn a leading non-text block into a spurious `envelope` rejection.
    for (std::size_t i = 0; i < content.size(); ++i) {
        const JsonValue block = content.at(i);
        if (!block.isObject() || block.get("type").asString() != "text") {
            continue;
        }
        out = block.get("text").asString();
        if (out.empty()) {
            error = "backend response's first text block is empty";
            return false;
        }
        return true;
    }

    // A well-formed envelope that delivered no order document. Not the model producing something
    // malformed, so not `parse`.
    error = "backend response's 'content' array carries no text block";
    return false;
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

StageAOutcome validateStageA(
    const std::string& body, const std::string& requestingEntityId, EnvelopeFormat envelope,
    double defaultOrbitRadiusM) {
    // -- A0: size ------------------------------------------------------------------------------
    // Before the parse, deliberately. A 10 MB body is not an order and parsing it to discover that
    // spends the worker's time on the adversary's terms. Measured against the body as received,
    // envelope included — the cap is about what the worker was asked to chew on, not about what
    // survived unwrapping.
    if (body.size() > kMaxResponseBodyBytes) {
        return reject(RejectReason::Range,
            "response body is " + std::to_string(body.size()) + " bytes, over the "
                + std::to_string(kMaxResponseBodyBytes) + "-byte cap");
    }
    if (body.empty()) {
        return reject(RejectReason::Parse, "response body is empty");
    }

    // -- A2: unwrap the backend envelope --------------------------------------------------------
    std::string document = body;
    if (envelope == EnvelopeFormat::OllamaGenerate) {
        std::string envelopeError;
        if (!unwrapOllamaGenerate(body, document, envelopeError)) {
            return reject(RejectReason::Envelope, envelopeError);
        }
    } else if (envelope == EnvelopeFormat::ClaudeMessages) {
        std::string envelopeError;
        bool refused = false;
        if (!unwrapClaudeMessages(body, document, envelopeError, refused)) {
            // A refusal is not a malformed envelope, and giving it its own reason is the whole
            // point of having fine-grained reasons: a climbing `reject.envelope` means the backend
            // is broken, while a climbing `reject.refusal` means the prompt is asking for something
            // the model will not do. Those need different responses, so they need different
            // counters (PRD AIC-VAL-1, and RejectReason.h's own stated rationale).
            return reject(refused ? RejectReason::Refusal : RejectReason::Envelope, envelopeError);
        }
    }

    // -- A3: one well-formed JSON object -------------------------------------------------------
    const std::optional<JsonValue> parsed = JsonValue::parse(document);
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
    // The two directions of this rule are NOT symmetric, and the asymmetry is the requirement
    // (AIC-ORD-1, v1.8.30, closing C14). Below zero on `hold` is REPAIRED; above zero on a posture
    // that forbids a radius is still REJECTED.
    //
    // The reason is a property of the encoding rather than a preference. Three of the four-branch
    // schema's conditional rules are forced-to-a-constant rules and every layer holds them: the
    // local decoder honours `const`, and the hosted projection's pinToConst turns a zero-width
    // range into one. The hold branch's [1, 50000] is the only rule shaped "greater than zero", and
    // it is held by nothing anywhere - the local decoder was measured ignoring numeric bounds in
    // both directions on four models across three families, and the hosted projection strips the
    // keywords outright because pinToConst cannot rescue a range that is not zero-width.
    //
    // So a model emitting a positive radius on `defend` is SAYING SOMETHING WRONG against a rule
    // the runtime does enforce; a model emitting zero on `hold` is OMITTING SOMETHING the runtime
    // never compelled it to supply. Rejecting the second discards a posture, waypoint, speed and
    // ROE that are all sound, over one scalar - and AIC-VAL-2 then holds the PREVIOUS order in
    // force, which is strictly worse than a repaired one. That is C16's argument, unchanged, and
    // this was the largest rejection class in the archive at 16 of 55.
    bool orbitRadiusRepaired = false;
    if (order.posture == Posture::Hold) {
        if (order.orbitRadiusM <= 0.0) {
            order.orbitRadiusM = defaultOrbitRadiusM;
            orbitRadiusRepaired = true;
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
    // `reason` is bounded ASYMMETRICALLY: too short rejects, too long truncates (AIC-ORD-1,
    // PRD v1.8.25, closing C16). The reasoning is in OrderSchema.h next to the two constants;
    // what matters here is the ORDER of the two branches. The emptiness check runs first, so a
    // zero-length reason can never be mistaken for something that was truncated to nothing.
    if (order.reason.size() < kMinReasonChars) {
        return reject(RejectReason::Range, "'reason' must be at least "
            + std::to_string(kMinReasonChars) + " character, got "
            + std::to_string(order.reason.size()));
    }
    bool reasonTruncated = false;
    if (order.reason.size() > kMaxReasonChars) {
        // Marked, and marked INSIDE the cap - the marker replaces the last characters rather than
        // being appended past them, so the stored value satisfies the bound it was measured
        // against. A shortened record that does not say it was shortened is the defect
        // §Corrections item 26 already names; this must not add a second instance of it.
        const std::size_t markerLength = std::char_traits<char>::length(kReasonTruncationMarker);
        order.reason.resize(kMaxReasonChars - markerLength);
        order.reason += kReasonTruncationMarker;
        reasonTruncated = true;
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
    //
    // Validated against the ONE BRANCH the posture selects, not against the whole oneOf document.
    // Two reasons, and the second is the load-bearing one: the posture is already parsed by the
    // time we get here, so the branch is known for free; and the SDK does not document which JSON
    // Schema keywords its validator implements, so depending on `oneOf` support here would make
    // Stage A's backstop silently conditional on an undocumented feature. The branch is a sub-value
    // of the same built document — there is no second definition to drift.
    //
    // Validated against a NORMALIZED copy, and this is a deliberate asymmetry rather than a
    // loophole. The branch's `required` array exists to compel a constrained DECODER to emit every
    // conditional field; what Stage A ACCEPTS is governed by AIC-ORD-1's field table, which gives
    // both fields a value in the inapplicable case ("empty otherwise", "0 otherwise"). An order
    // that omits them means exactly that order. Holding acceptance to the emission rule instead
    // would reject every order the `stub` adapter produces and, worse, every order in a log
    // recorded by an earlier build — which would break AIC-DET-2's replay guarantee silently, at
    // the one moment nobody is watching for it. A6 above has already enforced the values.
    {
        JsonValue normalized = doc;
        if (!normalized.has("targetEntityId")) {
            (void)normalized.setString("targetEntityId", "");
        }
        if (!normalized.has("orbitRadiusM")) {
            (void)normalized.setDouble("orbitRadiusM", 0.0);
        }
        // The REPAIRED radius, not the one the model sent, and this line is load-bearing for
        // exactly the reason the `reason` line below is. The hold branch bounds orbitRadiusM to
        // [1, 50000], so validating the original here would fail every repaired order as `schema`
        // and give back the whole-order loss the repair exists to prevent - one check later, under
        // a less informative reason code. Stage A validates the order it is going to ACCEPT.
        if (orbitRadiusRepaired) {
            (void)normalized.setDouble("orbitRadiusM", order.orbitRadiusM);
        }
        // The TRUNCATED reason, not the one the model sent - and this line is load-bearing rather
        // than tidy. The branch document bounds `reason` by maxLength, so validating the original
        // here would fail every over-long order as `schema` and give back exactly the whole-order
        // loss C16 was about, one check later and under a less informative reason code. Stage A
        // validates the order it is going to ACCEPT, which is the same asymmetry the two lines
        // above already rest on.
        (void)normalized.setString("reason", order.reason);

        std::string schemaError;
        if (!normalized.validateAgainstSchema(orderSchemaBranch(order.posture), &schemaError)) {
            return reject(RejectReason::Schema,
                "failed schema validation: " + sanitizeText(schemaError, 200));
        }
    }

    StageAOutcome outcome;
    outcome.accepted = true;
    outcome.reason = RejectReason::None;
    outcome.reasonTruncated = reasonTruncated;
    outcome.orbitRadiusRepaired = orbitRadiusRepaired;
    outcome.order = std::move(order);
    return outcome;
}

} // namespace arkheon::aicommander
