#include "ClaudeLlmClient.h"

#include "EnvVar.h"
#include "OrderSchema.h"

#include <core/json/JsonValue.h>
#include <core/net/HttpClientFactory.h>
#include <core/net/IHttpClient.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace arkheon::aicommander {

namespace {

using n8ro::core::HttpHeader;
using n8ro::core::HttpMethod;
using n8ro::core::HttpRequest;
using n8ro::core::HttpResponse;
using n8ro::core::JsonValue;

constexpr const char* kAnthropicVersion = "2023-06-01";

// One retry, and only for these. AIC-BE-2 allows exactly one; §Rabbit holes explains why more is a
// self-inflicted slowdown at a fixed cadence.
constexpr int kRetryBackoffMs = 500;

[[nodiscard]] std::string joinUrl(const std::string& baseUrl, const char* path) {
    std::string base = baseUrl;
    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    return base + path;
}

[[nodiscard]] std::int64_t elapsedMs(const std::chrono::steady_clock::time_point& start) {
    const auto delta = std::chrono::steady_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();
}

[[nodiscard]] LlmResult transportFailure(std::string detail, std::int64_t latencyMs) {
    LlmResult result;
    result.completed = false;
    result.statusCode = 0;
    result.transportDetail = std::move(detail);
    result.latencyMs = latencyMs;
    return result;
}

// Six decimals, because a single order costs about $0.0012 and a detail string that rounded to
// cents would report every refusal as "$0.00 of $1.00" (v1.8.47, C24). std::to_string on a double
// gives six by default, which is exactly what is wanted here and is why it is not iostreams.
[[nodiscard]] std::string formatUsd(double usd) {
    std::string text = std::to_string(usd);
    return text;
}

[[nodiscard]] bool isRetryable(int statusCode) {
    return statusCode == 429 || (statusCode >= 500 && statusCode < 600);
}

// Haiku 4.5 REJECTS `effort`. Suppressing a configured value there is a correctness requirement,
// not a tidy-up: forwarding it turns every request into a 400, which would present as a total
// backend outage rather than as a configuration problem.
[[nodiscard]] bool modelAcceptsEffort(const std::string& model) {
    return model.find("haiku") == std::string::npos;
}

} // namespace

ClaudeLlmClient::ClaudeLlmClient(ClaudeClientConfig config)
    : config_(std::move(config)),
      factory_([] { return n8ro::core::HttpClientFactory::create(); }) {
    // Nothing else. No client, no socket, no key: this constructor runs on the simulation thread
    // and every one of those belongs to the worker.
}

ClaudeLlmClient::~ClaudeLlmClient() = default;

void ClaudeLlmClient::setHttpClientFactory(HttpClientFactory factory) {
    factory_ = std::move(factory);
    http_.reset();
}

std::string ClaudeLlmClient::buildRequestBody(const LlmRequest& request) const {
    JsonValue body = JsonValue::object();
    (void)body.setString("model", config_.model);
    (void)body.setInt64("max_tokens", config_.maxTokens);

    // -- the prompt, split at the declared prefix boundary ---------------------------------------
    //
    // Two text blocks with the cache breakpoint on the first. This is the documented
    // shared-prefix/varying-suffix shape, and it is the only placement that earns the discount
    // §Cost model computes: a breakpoint at the end of the whole prompt writes a distinct entry per
    // request and reads none.
    //
    // prefixLength == 0 means no boundary was declared. Then the prompt goes as one unmarked block
    // — correct, uncached, and honest, rather than a guess at where the boundary might be.
    JsonValue blocks = JsonValue::array();
    const std::size_t split =
        (request.prefixLength > 0 && request.prefixLength < request.prompt.size())
            ? request.prefixLength
            : 0;

    if (split > 0) {
        JsonValue prefixBlock = JsonValue::object();
        (void)prefixBlock.setString("type", "text");
        (void)prefixBlock.setString("text", request.prompt.substr(0, split));
        JsonValue cacheControl = JsonValue::object();
        (void)cacheControl.setString("type", "ephemeral");
        (void)prefixBlock.set("cache_control", cacheControl);
        (void)blocks.pushBack(prefixBlock);

        JsonValue suffixBlock = JsonValue::object();
        (void)suffixBlock.setString("type", "text");
        (void)suffixBlock.setString("text", request.prompt.substr(split));
        (void)blocks.pushBack(suffixBlock);
    } else {
        JsonValue whole = JsonValue::object();
        (void)whole.setString("type", "text");
        (void)whole.setString("text", request.prompt);
        (void)blocks.pushBack(whole);
    }

    JsonValue userMessage = JsonValue::object();
    (void)userMessage.setString("role", "user");
    (void)userMessage.set("content", blocks);

    JsonValue messages = JsonValue::array();
    (void)messages.pushBack(userMessage);
    (void)body.set("messages", messages);

    // -- structured output ------------------------------------------------------------------------
    //
    // The PROJECTION, not the canonical document (PRD v1.8, §Corrections item 19). Computed from
    // the canonical one in OrderSchema.cpp, so the two cannot drift; the hosted path does not accept
    // the bound keywords the canonical encoding uses.
    JsonValue format = JsonValue::object();
    (void)format.setString("type", "json_schema");
    (void)format.set("schema", orderJsonSchemaForStructuredOutputs());

    JsonValue outputConfig = JsonValue::object();
    (void)outputConfig.set("format", format);
    if (!config_.effort.empty() && modelAcceptsEffort(config_.model)) {
        (void)outputConfig.setString("effort", config_.effort);
    }
    (void)body.set("output_config", outputConfig);

    // No assistant prefill: it returns 400 on every current model. The schema above is what forces
    // the shape that a prefill used to force.
    return body.toString();
}

bool ClaudeLlmClient::tlsLikelyUnavailable() {
    // Same host, plain http, and nothing else. No prompt, no key, no body.
    std::string controlUrl = config_.baseUrl;
    if (controlUrl.rfind("https://", 0) == 0) {
        controlUrl = "http://" + controlUrl.substr(std::string("https://").size());
    }

    HttpRequest control;
    control.method = HttpMethod::Get;
    control.url = controlUrl;
    control.timeoutS = config_.timeoutS;

    // A response of ANY status means the network reached the host and only the TLS layer failed.
    // No response means the host was unreachable either way, which is a network outage and must not
    // be reported as a TLS diagnosis — claiming a missing OpenSSL build during an outage would send
    // an operator to reinstall a runtime that was never the problem.
    return http_->send(control).has_value();
}

// AIC-BE-2's spend ceiling (v1.8.47, C24). Static and pure so the arithmetic can be asserted on its
// own rather than only through a request, and so the four prices are visibly its only inputs.
//
// The four counts are DISJOINT on this API - `input_tokens` excludes both cache figures - so
// summing all four is the whole cost of the response and not a double count.
double ClaudeLlmClient::costOf(const ClaudeClientConfig& config, const LlmResult& result) {
    constexpr double kPerMillion = 1000000.0;
    return (static_cast<double>(result.tokensIn) * config.priceInPerMTok
            + static_cast<double>(result.tokensOut) * config.priceOutPerMTok
            + static_cast<double>(result.cacheReadTokens) * config.priceCacheReadPerMTok
            + static_cast<double>(result.cacheCreationTokens) * config.priceCacheWritePerMTok)
        / kPerMillion;
}

LlmResult ClaudeLlmClient::request(const LlmRequest& request) {
    const auto start = std::chrono::steady_clock::now();
    lastCacheReadTokens_ = 0;

    // -- THE SPEND CEILING, CHECKED BEFORE ANYTHING ELSE (AIC-BE-2, v1.8.47, C24) -----------------
    //
    // Before the HTTP client is created and before the key is read out of the environment, because
    // a request that must not be sent must not open a socket or touch a credential. request() is
    // the only egress point in this plugin, so a guard here cannot be bypassed by another call site.
    //
    // FAIL CLOSED: `maxSpendUsd <= 0` disables the hosted backend and does NOT mean "unlimited".
    if (config_.maxSpendUsd <= 0.0) {
        return transportFailure(
            "hosted backend refused: claude.maxSpendUsd is at or below zero, which disables it "
            "rather than meaning unlimited (fail-closed by design, AIC-BE-2). Set a positive "
            "ceiling to enable the hosted path.",
            elapsedMs(start));
    }
    if (budgetExhausted_ || spentUsd_ >= config_.maxSpendUsd) {
        // THE LATCH. Once set, no later request touches the network for the life of the process. A
        // ceiling that is re-checked but never latched spends one more request every cadence after
        // it reports being exhausted - 180 an hour at the 20 s default.
        budgetExhausted_ = true;
        if (!budgetLogged_) {
            budgetLogged_ = true;
            // Logged exactly ONCE. Every refusal below still names the budget in its own detail, so
            // nothing is silent; what is suppressed is one identical line per cadence forever.
            std::fprintf(stderr,
                         "ai-commander: hosted spend ceiling reached - %.6f of %.6f USD. The claude "
                         "backend will not send again for the life of this process.\n",
                         spentUsd_, config_.maxSpendUsd);
        }
        return transportFailure(
            "hosted spend ceiling reached: " + formatUsd(spentUsd_) + " of "
                + formatUsd(config_.maxSpendUsd)
                + " USD (claude.maxSpendUsd). The backend is latched off for the life of this "
                  "process.",
            elapsedMs(start));
    }

    if (http_ == nullptr) {
        http_ = factory_ ? factory_() : nullptr;
        if (http_ == nullptr) {
            return transportFailure("could not create an HTTP client", elapsedMs(start));
        }
    }

    // ADR-5: the key is read into a LOCAL at request time. It is never a member, never logged, and
    // never written to the order record. `apiKey` dies with this function.
    //
    // Through tryReadEnvVar rather than std::getenv: these projects build with SDLCheck, which
    // promotes MSVC's C4996 deprecation of getenv to an error, and that wrapper is the one place
    // that knows it — the alternative is a _CRT_SECURE_NO_WARNINGS that would switch the whole
    // check off for every file.
    std::string apiKey;
    if (!tryReadEnvVar(config_.apiKeyEnvVar.c_str(), apiKey) || apiKey.empty()) {
        // Named as a configuration error rather than surfacing as a 401 the operator has to decode.
        // The VARIABLE NAME is safe to print; its value is what must never appear.
        return transportFailure(
            "configuration error: environment variable '" + config_.apiKeyEnvVar
                + "' (claude.apiKeyEnvVar) is unset or empty, so no API key is available",
            elapsedMs(start));
    }

    HttpRequest http;
    http.method = HttpMethod::Post;
    http.url = joinUrl(config_.baseUrl, "/v1/messages");
    http.body = buildRequestBody(request);
    http.contentType = "application/json";
    http.timeoutS = config_.timeoutS;
    http.headers.push_back(HttpHeader{"x-api-key", apiKey});
    http.headers.push_back(HttpHeader{"anthropic-version", kAnthropicVersion});

    // -- send, with at most one retry on 429/5xx -------------------------------------------------
    std::optional<HttpResponse> response = http_->send(http);
    bool retried = false;

    if (response.has_value() && isRetryable(response->statusCode)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kRetryBackoffMs));
        response = http_->send(http);
        retried = true;
    }

    LlmResult result;
    result.latencyMs = elapsedMs(start);

    if (!response.has_value()) {
        // A transport failure is std::nullopt, NOT statusCode == 0 (PRD §Corrections item 5).
        result.completed = false;
        result.statusCode = 0;

        // AIC-BE-4, and only on this path — the diagnosis is meaningless unless https already
        // failed to produce a response at all.
        if (tlsLikelyUnavailable()) {
            result.transportDetail =
                "TLS appears unavailable: https to " + http.url + " produced no response, but "
                "plain http to the same host did. The OpenSSL runtime "
                "(libssl-3-x64.dll / libcrypto-3-x64.dll) is likely missing from the process's "
                "search path. This is a runtime problem, not a network one.";
        } else {
            result.transportDetail = "no HTTP response from " + http.url
                + " (transport failure, or the request exceeded " + std::to_string(http.timeoutS)
                + " s)";
        }
        return result;
    }

    result.completed = true;
    result.statusCode = response->statusCode;
    result.body = response->body;

    if (isRetryable(result.statusCode) && retried) {
        // The retry was spent and the second attempt failed the same way. Say so: "429 twice" and
        // "429 once" are different operational stories, and the second one is invisible unless the
        // detail says the budget was used.
        result.transportDetail = "HTTP " + std::to_string(result.statusCode)
            + " on both the initial request and the single permitted retry";
    }

    // -- token accounting -------------------------------------------------------------------------
    //
    // Parsed here rather than in Stage A because these are cost figures, not validation input, and
    // AIC-DET-1 wants them on the record whether or not the order survives validation. A malformed
    // body simply leaves them zero — this must never be the thing that fails a request.
    const std::optional<JsonValue> parsed = JsonValue::parse(result.body);
    if (parsed.has_value() && parsed->isObject()) {
        const JsonValue usage = parsed->get("usage");
        if (usage.isObject()) {
            result.tokensIn = static_cast<int>(usage.get("input_tokens").asInt64());
            result.tokensOut = static_cast<int>(usage.get("output_tokens").asInt64());
            // The number that answers OQ-8's second half. A prefix over the model's cache minimum
            // that still reports zero reads means something is invalidating it — a different
            // finding, and an unobservable one without this.
            //
            // It goes onto the RESULT now, not just into the member (AIC-BE-2, v1.8.18). Until
            // then it stopped here: LlmResult carried no cache field, so the value never crossed
            // runWorkerCall's boundary, never reached OrderRecorder, and never appeared in an order
            // log — which meant every cached-token figure in the PRD came from tests/live and none
            // from the product. PRD §Corrections item 33(f), closed as C9 in item 35.
            result.cacheReadTokens =
                static_cast<int>(usage.get("cache_read_input_tokens").asInt64());

            // Parsed by nothing at all before v1.8.18 (PRD C4). It is NOT here for the write cost,
            // which item 33 quantified at under 0.1 % of a run — it is here because it is the only
            // thing that distinguishes a cold start from a block that never cached. Both states
            // report cacheReadTokens == 0; only this field tells them apart. See the state table on
            // LlmResult.
            result.cacheCreationTokens =
                static_cast<int>(usage.get("cache_creation_input_tokens").asInt64());

            // Retained alongside the result field rather than replaced by it. tests/live reads it
            // through the concrete pointer for the per-order CSV, and that path predates the
            // widening; removing it would break the offline harness to tidy a duplicate.
            lastCacheReadTokens_ = result.cacheReadTokens;
        }
    }

    // -- accumulate against the ceiling (AIC-BE-2, v1.8.47, C24) ----------------------------------
    //
    // AFTER the token accounting above, because that is what supplies the four counts, and
    // unconditionally on any response that produced a body - a 429 or a 5xx still bills for the
    // tokens the service processed, and a ceiling that only counted successes would undercount
    // exactly the runs that are going wrong.
    //
    // The latch is not tripped here. It is evaluated at the TOP of the next request, so the request
    // that crosses the ceiling is allowed to complete and be recorded rather than being abandoned
    // half-spent; what the ceiling promises is that no request is STARTED beyond it.
    spentUsd_ += costOf(config_, result);

    return result;
}

} // namespace arkheon::aicommander
