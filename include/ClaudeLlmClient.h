#pragma once

#include "ILlmClient.h"

#include <functional>
#include <memory>
#include <string>

namespace n8ro::core {
class IHttpClient;
}

namespace arkheon::aicommander {

// What the adapter needs from CommanderConfig, copied by value.
//
// Note what is NOT here: the API key. `apiKeyEnvVar` is the NAME of an environment variable, and
// the value is read with std::getenv at request time into a local (ADR-5). That is structural
// rather than procedural — a member holding the key could be formatted into a diagnostic string by
// some later edit, and there is no edit that can make a local outlive the call that read it.
struct ClaudeClientConfig {
    std::string baseUrl = "https://api.anthropic.com";
    std::string model = "claude-haiku-4-5";
    int maxTokens = 512;
    std::string apiKeyEnvVar = "ANTHROPIC_API_KEY";

    // Empty for Haiku 4.5. See the suppression in request(): for that model this is a prohibition
    // rather than an omission.
    std::string effort;

    int timeoutS = 90;

    // -- The spend ceiling (AIC-BE-2, v1.8.47, C24) -----------------------------------------------
    //
    // WHY THIS EXISTS. Until v1.8.47 there was no per-run, per-day or cumulative cost bound anywhere
    // in this plugin, and no counter that could enforce one. That was safe for six grants for a
    // reason that has now gone away: every hosted run was individually authorized, costed and
    // recorded, so a HUMAN CHECKPOINT sat in front of every request this project ever made. The
    // owner's standing authorization removes exactly that checkpoint - which is its purpose - so
    // something has to take its place. Derived exposure without one: ~$0.44/hour of two-entity
    // commanding, so a scenario left running overnight exceeds this project's whole budget about
    // twice over, with nothing in the code noticing.
    //
    // FAIL CLOSED, and this is the design decision worth arguing. `maxSpendUsd <= 0` DISABLES the
    // hosted backend; it does NOT mean "unlimited". The opposite convention is the more natural one
    // to write and is what most configuration does, and it is the wrong one here: the failure mode
    // of an unset, zeroed or mis-parsed field must be "sends nothing", never "sends without limit".
    // A typo must not become unbounded egress.
    //
    // PER-PROCESS, and that is deliberate rather than a gap. It bounds any single engine session -
    // the exposure above - and not running the engine a hundred times. Starting a run is a human
    // act and that checkpoint covers repetition; a cross-process ceiling would need durable state
    // this plugin has no home for, and a spend counter in a file an operator can delete is worse
    // than none because it reads as a control.
    double maxSpendUsd = 1.00;

    // THE PRICES ARE CONFIGURATION AND NOT CONSTANTS, and §Corrections item 54(e) is why.
    // `claude.model` is operator-settable, so a price compiled in here is a number nothing keeps in
    // step with the model it prices - the identical shape to the doctrine block that stated a
    // rejection threshold as "at or below zero" while the shipped bound had been 50.0 for nine
    // revisions. Defaults are claude-haiku-4-5's rates as used throughout §Cost model: $1/MTok in,
    // $5/MTok out, cache read at 0.1x input, cache write at 1.25x input.
    double priceInPerMTok = 1.00;
    double priceOutPerMTok = 5.00;
    double priceCacheReadPerMTok = 0.10;
    double priceCacheWritePerMTok = 1.25;
};

// The `claude` adapter (AIC-BE-2): POST {baseUrl}/v1/messages over HTTPS.
//
// Thread affinity: identical to LocalLlmClient. request() runs ON THE WORKER, touches no engine
// state, and owns one IHttpClient created on the worker on first use, because that class is
// documented single-thread-only and this constructor runs on the simulation thread.
//
// What it sends, and why each piece is the way it is:
//
//   - `x-api-key`, not `Authorization: Bearer`. Anthropic authenticates API keys on that header.
//   - `anthropic-version: 2023-06-01`, the pinned wire version.
//   - `output_config.format` carrying the SCHEMA PROJECTION, not the canonical document. The hosted
//     structured-output path does not accept the bound keywords the canonical four-branch encoding
//     is built out of (PRD v1.8, §Corrections item 19). The projection is computed from the
//     canonical document, never authored beside it, so the two cannot drift.
//   - Two content blocks, with `cache_control` on the FIRST. The prompt arrives as one string with
//     a declared prefix boundary (LlmRequest::prefixLength), and the breakpoint goes at that
//     boundary. Placing it at the end of the whole prompt instead would write a fresh cache entry
//     every request and read none — the silent way to pay the write premium and receive nothing.
//   - No assistant prefill. It returns 400 on every current model.
//
// What it deliberately does NOT do:
//
//   - It does not parse the order out of the response. That is Stage-A check A2's job, reached
//     through envelopeFormat(), so `refusal` and `envelope` are produced in the one place reject
//     reasons are produced.
//   - It does not retry more than once. AIC-BE-2 allows exactly one retry on 429/5xx; §Rabbit holes
//     explains what a retry loop against a slow endpoint does to a fixed cadence.
//   - It does not send `effort` on Haiku 4.5, which REJECTS that parameter. A configured value is
//     suppressed rather than forwarded.
class ClaudeLlmClient final : public ILlmClient {
public:
    explicit ClaudeLlmClient(ClaudeClientConfig config);
    ~ClaudeLlmClient() override;

    ClaudeLlmClient(const ClaudeLlmClient&) = delete;
    ClaudeLlmClient& operator=(const ClaudeLlmClient&) = delete;

    [[nodiscard]] const char* backendName() const override { return "claude"; }
    [[nodiscard]] std::string modelName() const override { return config_.model; }
    [[nodiscard]] EnvelopeFormat envelopeFormat() const override {
        return EnvelopeFormat::ClaudeMessages;
    }

    [[nodiscard]] LlmResult request(const LlmRequest& request) override;

    [[nodiscard]] const ClaudeClientConfig& config() const { return config_; }

    // Test seam, identical in shape and purpose to LocalLlmClient's. The unit suite exercises every
    // failure path — refusal, 429, 5xx, transport nullopt, TLS-unavailable — with no network and no
    // API key, which is what lets those paths be proven before egress is ever authorized.
    using HttpClientFactory = std::function<std::unique_ptr<n8ro::core::IHttpClient>()>;
    void setHttpClientFactory(HttpClientFactory factory);

    // Cache-read tokens from the last completed response. Zero when the prefix did not cache.
    //
    // Recorded because it is the only way to tell a cache hit from a miss, and whether the prefix
    // caches is the whole of OQ-8. A prefix over the model's minimum that still reports zero reads
    // means something is invalidating it, which is a different finding than OQ-8 asks about — and
    // an unobservable one without this number.
    [[nodiscard]] int lastCacheReadTokens() const { return lastCacheReadTokens_; }

    // The spend ceiling's observable state (AIC-BE-2, v1.8.47, C24). Both exist so the unit suite
    // can assert the ceiling without a network, and so an operator can see how close a session is.
    [[nodiscard]] double spentUsd() const { return spentUsd_; }
    [[nodiscard]] bool budgetExhausted() const { return budgetExhausted_; }

    // What one response cost, from the four token counts AIC-BE-2 has recorded since v1.8.18.
    // Static and pure so the arithmetic is testable on its own rather than only through a request.
    [[nodiscard]] static double costOf(const ClaudeClientConfig& config, const LlmResult& result);

private:
    // Builds the request body. Separated so a test can assert what would be sent without sending.
    [[nodiscard]] std::string buildRequestBody(const LlmRequest& request) const;

    // AIC-BE-4. Called only after an https attempt returned std::nullopt. Returns true when a
    // plain-http control request to the same host DOES get a response, which is what isolates a
    // missing TLS backend from a network outage.
    //
    // The control request carries NO prompt, NO key, and NO body. What it discloses is that this
    // host is being contacted, which DNS disclosed already; what it must never disclose is scenario
    // state, and carrying nothing is the structural guarantee of that rather than a promise.
    [[nodiscard]] bool tlsLikelyUnavailable();

    ClaudeClientConfig config_;
    HttpClientFactory factory_;
    std::unique_ptr<n8ro::core::IHttpClient> http_;
    int lastCacheReadTokens_ = 0;

    // Cumulative cost for the life of this adapter, and the latch it trips (C24).
    //
    // Plain members rather than atomics, on the same thread-affinity argument that already governs
    // `lastCacheReadTokens_` and `http_`: request() runs on the worker, one adapter instance is
    // owned by one worker, and IHttpClient is documented single-thread-only, so this class is
    // already confined to one thread by construction.
    //
    // THE LATCH IS THE POINT, not the counter. A ceiling that is re-checked but never latched
    // spends one more request every cadence after it reports being exhausted - 180 an hour at the
    // 20 s default - so once this is set no later request touches the network for the life of the
    // process.
    double spentUsd_ = 0.0;
    bool budgetExhausted_ = false;
    bool budgetLogged_ = false;
};

} // namespace arkheon::aicommander
