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
// Deliberately not a CommanderConfig reference: the adapter lives on the worker side of the design
// and must not hold anything the simulation thread can mutate underneath it. A config change
// rebuilds the client (AiCommanderPlugin::rebuildClient), which is also what re-reads these.
struct LocalClientConfig {
    std::string baseUrl = "http://localhost:11434";

    // An OLLAMA TAG (`qwen2.5:7b-instruct-q8_0`), never a GGUF filename. Naming a file here fails
    // with model-not-found, which is exactly the failure the preflight below exists to name.
    std::string model = "qwen2.5:7b-instruct-q8_0";

    double temperature = 0.0;
    bool grammarEnabled = true;
    int timeoutS = 90;
};

// The `local` adapter (AIC-BE-1): POST {baseUrl}/api/generate against Ollama.
//
// Thread affinity: request() runs ON THE WORKER. It touches no engine state and holds no SDK
// pointer beyond its own IHttpClient, which it creates on first use — on the worker — because that
// class is documented single-thread-only and the constructor runs on the simulation thread.
//
// Three things this class deliberately does NOT do:
//
//   - It does not retry. §Rabbit holes: a timeout plus retries against a slow server produces
//     overlapping requests and a self-inflicted slowdown. The fallback ladder handles failure.
//   - It does not warm the model at construction. Measured: a warm-up ping costs 2,503 ms and
//     leaves the first real order at 1,432 ms — 3,935 ms against 3,860 ms without it. It relocates
//     the cold cost rather than removing it (PRD §Corrections item 14).
//   - It does not parse the order out of the response. The envelope is Stage-A check A2's job, and
//     it is reported through envelopeFormat() so that check stays in the one place reject reasons
//     are produced.
//
// What it does do beyond the request is one preflight, on the first call only: ask the server what
// models it has, so a mistyped `local.model` reads as a configuration error naming the tag rather
// than as the generic transport failure §Corrections item 11 complained about.
class LocalLlmClient final : public ILlmClient {
public:
    explicit LocalLlmClient(LocalClientConfig config);
    ~LocalLlmClient() override;

    LocalLlmClient(const LocalLlmClient&) = delete;
    LocalLlmClient& operator=(const LocalLlmClient&) = delete;

    [[nodiscard]] const char* backendName() const override { return "local"; }
    [[nodiscard]] std::string modelName() const override { return config_.model; }
    [[nodiscard]] EnvelopeFormat envelopeFormat() const override {
        return EnvelopeFormat::OllamaGenerate;
    }

    [[nodiscard]] LlmResult request(const LlmRequest& request) override;

    [[nodiscard]] const LocalClientConfig& config() const { return config_; }

    // Test seam. Defaults to n8ro::core::HttpClientFactory::create.
    //
    // The unit suite has to exercise this adapter with no server and no network — the PRD's CI
    // requirement is explicit about that — so the transport is injectable. Production never calls
    // this, and the default factory is the only thing that ever constructs a real client.
    using HttpClientFactory = std::function<std::unique_ptr<n8ro::core::IHttpClient>()>;
    void setHttpClientFactory(HttpClientFactory factory);

    // For tests and the startup log: whether the one-shot preflight has succeeded yet.
    [[nodiscard]] bool preflightPassed() const { return preflightPassed_; }

private:
    // Returns false and fills `detail` when the server is unreachable or does not carry the tag.
    [[nodiscard]] bool runPreflight(std::string& detail);

    LocalClientConfig config_;
    HttpClientFactory factory_;
    std::unique_ptr<n8ro::core::IHttpClient> http_;
    bool preflightPassed_ = false;
};

} // namespace arkheon::aicommander
