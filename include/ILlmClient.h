#pragma once

#include "EnvelopeFormat.h"
#include "Snapshot.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace arkheon::aicommander {

// What the worker hands an adapter: the rendered prompt plus the snapshot it was rendered from.
//
// The snapshot travels alongside the prompt because the `stub` and `replay` adapters key off the
// entity and simulation time rather than parsing the prompt back apart. A network adapter ignores
// everything but the prompt.
struct LlmRequest {
    std::string prompt;
    OrderSnapshot snapshot;

    // Byte offset into `prompt` at which the stable prefix ends and the volatile suffix begins
    // (AIC-BE-3, v1.8). Zero means "no boundary declared"; every adapter without a cache concept
    // ignores it and sends `prompt` whole.
    //
    // A length rather than a second string, deliberately. `prompt` stays the single record of what
    // was sent — so promptHash is unaffected and the order record's format does not move — and an
    // offset cannot desynchronise from the text the way a parallel copy of the prefix could.
    //
    // What it buys: a hosted adapter can place its cache breakpoint exactly at the boundary. A
    // breakpoint at the END of the whole prompt would write a distinct cache entry per request and
    // read none — paying the write premium to receive nothing, silently, with no counter moving.
    std::size_t prefixLength = 0;
};

// What an adapter returns.
//
// `completed == false` is a transport failure — the request never produced an HTTP response at
// all. That is the shape IHttpClient::send() actually has: it returns std::optional<HttpResponse>
// and yields std::nullopt on a transport or TLS failure. The `statusCode == 0` sentinel documented
// on HttpResponse is NOT what a caller observes on failure, which is why this struct carries a
// separate flag rather than overloading the status code (PRD Corrections, item 5).
struct LlmResult {
    bool completed = false;
    int statusCode = 0;
    std::string body;
    std::string transportDetail;   // Populated only when completed == false.
    std::int64_t latencyMs = 0;

    // Token accounting, populated by adapters that report it (Phase 2). Zero elsewhere.
    int tokensIn = 0;
    int tokensOut = 0;

    // Prompt-cache accounting (AIC-BE-2, v1.8.18). Zero on every adapter with no cache concept,
    // exactly as tokensIn/tokensOut already are on LocalLlmClient.
    //
    // BOTH fields, and the reason is that ONE OF THEM IS NOT ENOUGH TO READ THE OTHER. A prompt
    // block below the model's cache minimum does not cache and says nothing about it: no error, no
    // rejection, no counter — it comes back with cacheCreationTokens == 0. A cold but eligible
    // FIRST request of a run comes back with cacheCreationTokens > 0 (it wrote the block) and
    // cacheReadTokens == 0 (there was nothing yet to read). So:
    //
    //     creation > 0, read == 0     cold start        correct, expected once per run
    //     creation == 0, read >= min  steady-state hit  the 119-of-120 case in C3's arms
    //     creation == 0, read == 0    NEVER CACHED      the failure the guard exists for
    //
    // Read `cacheReadTokens` alone and the first and third rows are the same observation. That is
    // why PRD C4 stopped being "add it when a cheaper reason to touch this interface arrives" and
    // became a precondition of C9 (§Corrections item 35(b)): a guard that cannot tell those apart
    // either warns on the first request of every run — and gets muted, which is worse than absent
    // because a muted guard still reads as a guard — or skips the first response and so cannot see
    // a shortfall that begins there, which is the only way this failure ever begins.
    //
    // These are ints rather than an optional pair because "the adapter reported zero" and "the
    // adapter has no cache concept" are separated at the CALL SITE by the backend, not here. See
    // AiCommanderPlugin's guard call, which is reached only on the hosted path.
    int cacheReadTokens = 0;
    int cacheCreationTokens = 0;
};

// The single seam every backend sits behind (AIC-ARCH-3).
//
// Thread affinity: request() is called ON THE WORKER and must not touch engine state. An
// implementation may own an IHttpClient, but never one shared with another thread — the header
// documents that class as single-thread-only.
//
// Validation, recording, and Lua publication are adapter-independent: all four adapters go through
// the same Stage A, the same Stage B, the same order log, and the same slots. That is what makes
// the Phase-2 backend a config change rather than a fork, and what lets CI run the entire pipeline
// with no server and no network.
class ILlmClient {
public:
    virtual ~ILlmClient() = default;

    // For the order record and the startup log.
    [[nodiscard]] virtual const char* backendName() const = 0;

    // For the order record's `model` field. Empty when the adapter has no model concept.
    [[nodiscard]] virtual std::string modelName() const { return {}; }

    // How this backend wraps the order document, for Stage-A check A2. Defaults to Raw, which is
    // what `stub` and `replay` return: the body is the order. A networked adapter that wraps it
    // overrides this, and the worker passes the answer to validateStageA - so the envelope knowledge
    // travels as a value and Stage A never holds a client.
    [[nodiscard]] virtual EnvelopeFormat envelopeFormat() const { return EnvelopeFormat::Raw; }

    [[nodiscard]] virtual LlmResult request(const LlmRequest& request) = 0;
};

} // namespace arkheon::aicommander
