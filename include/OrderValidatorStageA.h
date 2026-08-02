#pragma once

#include "EnvelopeFormat.h"
#include "Order.h"
#include "RejectReason.h"

#include <cstddef>
#include <string>

namespace arkheon::aicommander {

// A body larger than this is refused before any parse is attempted. An order is ~200 bytes; a
// megabyte of "order" is a malfunctioning backend or a hostile one, and parsing it to find out
// costs the worker real time.
inline constexpr std::size_t kMaxResponseBodyBytes = 64 * 1024;

// What Stage A concluded. On acceptance `order` is fully populated and every static invariant
// holds; it is NOT yet safe to publish, because Stage B has not seen it.
struct StageAOutcome {
    bool accepted = false;
    RejectReason reason = RejectReason::None;
    std::string detail;   // Human-readable, written to the order record alongside the reason.
    Order order;
};

// Stage A — syntactic validation, worker thread, **no SDK access** (AIC-VAL-1).
//
// Everything here is a pure function of the response body and the requesting entity's id. It
// touches no IEntityManager, no IScenarioManager, no MessageBusPacked, no ISimulationEngine, and
// no ScriptingApiContext — those are single-thread-only and live on the update thread. That
// constraint is why validation is two-stage rather than one.
//
// Check order is deliberate, and it is *not* simply the order the FR table lists. The table
// enumerates what must be checked; this orders those checks so the reason code is as specific as
// the failure allows. Running the JSON-Schema pass first would collapse an out-of-range latitude,
// an unknown posture, and a missing field all into `schema`, and the runbook needs to tell them
// apart. So: size, envelope, parse, unknown properties, version, required/typed fields, enums,
// conditional shape, ranges — and only then the schema document as a structural backstop for
// anything the explicit checks did not think to look at.
//
// `envelope` names how the backend wrapped the order (check A2). It arrives as a value so this
// function keeps holding nothing: with EnvelopeFormat::Raw — the default, and what `stub` and
// `replay` return — `body` is the order document itself and A2 is a no-op.
[[nodiscard]] StageAOutcome validateStageA(
    const std::string& body,
    const std::string& requestingEntityId,
    EnvelopeFormat envelope = EnvelopeFormat::Raw);

// Charset filter applied to every string that can reach a prompt or an order record.
//
// Permits printable ASCII minus the characters that would let a value break out of its context:
// no control characters, no quote, no backslash. This is defence in depth rather than the primary
// control - the validator is the real defence - but a `reason` string carrying a newline and a
// forged log prefix would corrupt the JSONL order log, and that log is what replay reads.
[[nodiscard]] bool isSanitizedText(const std::string& text);

// Truncates and strips a string to the sanitized charset, for values the plugin accepts but must
// neutralize rather than reject (e.g. a Tier-1 situation note).
[[nodiscard]] std::string sanitizeText(const std::string& text, std::size_t maxChars);

} // namespace arkheon::aicommander
