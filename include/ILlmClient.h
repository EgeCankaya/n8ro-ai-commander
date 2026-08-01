#pragma once

#include "Snapshot.h"

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

    [[nodiscard]] virtual LlmResult request(const LlmRequest& request) = 0;
};

} // namespace arkheon::aicommander
