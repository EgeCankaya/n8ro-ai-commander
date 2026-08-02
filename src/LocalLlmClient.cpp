#include "LocalLlmClient.h"

#include "OrderSchema.h"

#include <core/json/JsonValue.h>
#include <core/net/HttpClientFactory.h>
#include <core/net/IHttpClient.h>

#include <chrono>
#include <string>
#include <utility>

namespace arkheon::aicommander {

namespace {

using n8ro::core::HttpMethod;
using n8ro::core::HttpRequest;
using n8ro::core::HttpResponse;
using n8ro::core::JsonValue;

// Trims one trailing slash so a baseUrl written either way produces the same URL. Config is edited
// by hand in a UI field, and "http://localhost:11434/" is not a configuration error worth failing.
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

} // namespace

LocalLlmClient::LocalLlmClient(LocalClientConfig config)
    : config_(std::move(config)),
      factory_([] { return n8ro::core::HttpClientFactory::create(); }) {
    // Nothing else. No client, no socket, no model load: this constructor runs on the simulation
    // thread and every one of those belongs to the worker.
}

LocalLlmClient::~LocalLlmClient() = default;

void LocalLlmClient::setHttpClientFactory(HttpClientFactory factory) {
    factory_ = std::move(factory);
    http_.reset();
    preflightPassed_ = false;
}

bool LocalLlmClient::runPreflight(std::string& detail) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = joinUrl(config_.baseUrl, "/api/tags");
    request.timeoutS = config_.timeoutS;

    const std::optional<HttpResponse> response = http_->send(request);
    if (!response.has_value()) {
        detail = "inference server unreachable at " + config_.baseUrl
            + " (no HTTP response; check that it is running and that the URL is right)";
        return false;
    }
    if (response->statusCode < 200 || response->statusCode >= 300) {
        detail = "inference server at " + config_.baseUrl + " answered /api/tags with HTTP "
            + std::to_string(response->statusCode);
        return false;
    }

    const std::optional<JsonValue> parsed = JsonValue::parse(response->body);
    if (!parsed.has_value() || !parsed->isObject() || !parsed->get("models").isArray()) {
        // The server answered but not in Ollama's shape. Say so rather than guessing: this is what
        // pointing local.baseUrl at some other HTTP service looks like.
        detail = "server at " + config_.baseUrl
            + " answered /api/tags without a 'models' array; is it Ollama?";
        return false;
    }

    const JsonValue models = parsed->get("models");
    std::string available;
    bool found = false;
    for (std::size_t i = 0; i < models.size(); ++i) {
        const std::string name = models.at(i).get("name").asString();
        if (name.empty()) {
            continue;
        }
        if (name == config_.model) {
            found = true;
        }
        if (!available.empty()) {
            available += ", ";
        }
        available += name;
    }

    if (!found) {
        // The failure §Corrections item 11 was written about. Naming the tag and listing what the
        // server actually has turns a generic rejection into a one-line fix.
        detail = "configuration error: local.model '" + config_.model
            + "' is not a tag on the server at " + config_.baseUrl
            + ". It must be an Ollama TAG, not a GGUF filename. Available: "
            + (available.empty() ? std::string("(none)") : available);
        return false;
    }

    return true;
}

LlmResult LocalLlmClient::request(const LlmRequest& request) {
    const auto start = std::chrono::steady_clock::now();

    // One IHttpClient per adapter, created on the thread that uses it. The header documents the
    // class as single-thread-only, and this adapter is only ever driven from a worker.
    if (http_ == nullptr) {
        http_ = factory_ ? factory_() : nullptr;
        if (http_ == nullptr) {
            return transportFailure("could not create an HTTP client", elapsedMs(start));
        }
    }

    // Preflight once. On failure it runs again next call, so a server that comes up later recovers
    // without a restart.
    if (!preflightPassed_) {
        std::string detail;
        if (!runPreflight(detail)) {
            return transportFailure(std::move(detail), elapsedMs(start));
        }
        preflightPassed_ = true;
    }

    JsonValue body = JsonValue::object();
    (void)body.setString("model", config_.model);
    (void)body.setString("prompt", request.prompt);
    (void)body.setBool("stream", false);

    JsonValue options = JsonValue::object();
    (void)options.setDouble("temperature", config_.temperature);
    (void)body.set("options", options);

    if (config_.grammarEnabled) {
        // THE SAME OBJECT AIC-ORD-1 embeds, not a copy of it. One definition, three consumers holds
        // here or it holds nowhere — a hand-built variant here would drift from the prompt and the
        // validator silently, and the drift would look like a model failure.
        (void)body.set("format", orderJsonSchema());
    }

    HttpRequest http;
    http.method = HttpMethod::Post;
    http.url = joinUrl(config_.baseUrl, "/api/generate");
    http.body = body.toString();
    http.contentType = "application/json";
    // Sized for a cold model load, not for steady state (PRD §Corrections item 10).
    http.timeoutS = config_.timeoutS;

    const std::optional<HttpResponse> response = http_->send(http);

    LlmResult result;
    result.latencyMs = elapsedMs(start);

    if (!response.has_value()) {
        // A transport failure is std::nullopt, NOT statusCode == 0. The sentinel documented on
        // HttpResponse is not what a caller observes (PRD §Corrections item 5).
        result.completed = false;
        result.statusCode = 0;
        result.transportDetail = "no HTTP response from " + http.url
            + " (transport failure, or the request exceeded " + std::to_string(http.timeoutS) + " s)";
        // A transport failure may equally mean the server went away since the preflight. Re-running
        // it on the next call costs one cheap GET and re-earns the diagnosis if the cause is the
        // model rather than the socket.
        preflightPassed_ = false;
        return result;
    }

    // A completed request carrying a non-2xx status is NOT a transport failure. runWorkerCall maps
    // it to the same reject reason with a different detail, and keeping them distinct here is what
    // lets the runbook tell "server refused" from "server unreachable".
    result.completed = true;
    result.statusCode = response->statusCode;
    result.body = response->body;

    // tokensIn / tokensOut stay 0 by design. The envelope carries prompt_eval_count and eval_count,
    // but reading them here means parsing the envelope in the adapter and again in Stage-A check A2
    // — and ILlmClient documents those fields as Phase-2 cost accounting, which the local backend
    // does not have.
    return result;
}

} // namespace arkheon::aicommander
