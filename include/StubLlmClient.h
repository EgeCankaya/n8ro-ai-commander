#pragma once

#include "ILlmClient.h"

#include <cstdint>
#include <string>
#include <vector>

namespace arkheon::aicommander {

// The `stub` adapter (AIC-ARCH-3): performs no I/O and returns from a fixed order table.
//
// This is what makes Phases 0 and 1a fully deliverable and testable with no inference server —
// which matters because no inference server ships in this tree. It is the CI gate, not a
// convenience: the entire pipeline (render, validate, record, publish, consume from Lua) runs
// through it with zero network and zero external dependency.
//
// Deterministic by construction: the response for a given (entity, invocation index) is a pure
// function of the table, so a `stub` run replays identically without needing the replay adapter at
// all. The tactical content is deliberately dull — it exercises every posture and both conditional
// shapes, and is not trying to be good tactics.
class StubLlmClient final : public ILlmClient {
public:
    StubLlmClient() = default;

    [[nodiscard]] const char* backendName() const override { return "stub"; }
    [[nodiscard]] std::string modelName() const override { return "stub-order-table"; }

    [[nodiscard]] LlmResult request(const LlmRequest& request) override;

    // Forces the next response to be a transport failure, so the resilience path can be exercised
    // without unplugging anything. Cleared after one use.
    void injectTransportFailure() { injectTransportFailure_ = true; }

    // Forces the next response body, so the corpus can be driven through the real pipeline rather
    // than only through validateStageA in isolation.
    void injectBody(std::string body) { injectedBody_ = std::move(body); hasInjectedBody_ = true; }

    [[nodiscard]] std::int64_t invocationCount() const { return invocations_; }

private:
    std::int64_t invocations_ = 0;
    bool injectTransportFailure_ = false;
    bool hasInjectedBody_ = false;
    std::string injectedBody_;
};

} // namespace arkheon::aicommander
