#pragma once

#include <plugin/PluginConfigField.h>

#include <string>
#include <vector>

namespace arkheon::aicommander {

// Which ILlmClient adapter the commander drives. Selected at runtime by `commander.backend`
// (AIC-ARCH-3). `Stub` and `Replay` perform no I/O and construct no IHttpClient.
enum class Backend {
    Stub,
    Replay,
    Local,
    Claude,
};

[[nodiscard]] const char* toString(Backend backend);

// The plugin's full configuration surface (AIC-API-2), held as parsed values rather than as the
// canonical strings PluginConfigField carries on the wire.
//
// Application is all-or-nothing: `tryParse` validates every field into a scratch instance and only
// the caller's success path commits it, so a single bad field leaves every prior value untouched.
// That is a requirement, not a convenience — a partially applied config is a configuration nobody
// specified, and it is the same failure mode "reject, don't repair" rules out for orders.
struct CommanderConfig {
    // -- commander.* : master switch, cadence, and the bounds on outstanding work ----------------
    bool enabled = false;                 // Fail-closed master switch.
    Backend backend = Backend::Stub;
    double cadenceS = 20.0;               // Minimum simulation seconds between orders, per entity.
    int maxCommandedEntities = 4;         // Roster cap.
    int maxConcurrentRequests = 1;        // Worker fan-out; one IHttpClient per worker.
    int requestTimeoutS = 30;             // Written to HttpRequest::timeoutS (SDK default is 15).
    double maxOrderAgeS = 45.0;           // Stage-B staleness bound.
    double orderValidityS = 120.0;        // Fallback ladder step 1.
    double releaseAfterS = 300.0;         // Fallback ladder step 3.
    int maxTracksInPrompt = 8;            // Bounds the volatile suffix and the reported-track list.

    // -- prompt.* : the stable prefix's doctrine block (AIC-BE-3) --------------------------------
    // Read once at initialize(); a mid-run change does not take effect, which is what preserves
    // prefix byte-stability across a run.
    std::string doctrinePath = "data/doctrine.txt";

    // -- local.* : Phase 1b adapter (not exercised in Phase 1a) ---------------------------------
    std::string localBaseUrl = "http://localhost:11434";
    std::string localModel = "llama-3.2-3b-instruct-q4_k_m";
    double localTemperature = 0.0;        // Greedy decoding; lowest run-to-run variance.
    bool localGrammarEnabled = true;

    // -- claude.* : Phase 2 adapter, behind its own independent authorization gate ---------------
    bool claudeEnabled = false;
    std::string claudeBaseUrl = "https://api.anthropic.com";
    std::string claudeModel = "claude-haiku-4-5";
    int claudeMaxTokens = 512;
    // The NAME of an environment variable, never the value. See ADR-5: the value is read via
    // std::getenv at request time, never persisted, never logged, never returned by getConfigFields.
    std::string claudeApiKeyEnvVar = "ANTHROPIC_API_KEY";
    std::string claudeEffort;             // Empty for Haiku 4.5, which does not accept `effort`.

    // -- safety.* : the Stage-B clamp bounds -----------------------------------------------------
    double maxSpeedMps = 400.0;
    double minAltitudeHaeM = 100.0;
    double maxAltitudeHaeM = 20000.0;
    double geofenceRadiusM = 200000.0;
    double defaultOrbitRadiusM = 8000.0;

    // -- record.* / replay.* : the order log and its replay source (AIC-DET-1/2) ------------------
    bool recordEnabled = true;            // On by default — determinism depends on it.
    std::string recordPath = "logs/ai-commander/";
    std::string replayPath;               // Required when backend == Replay.
};

// Renders the config as the flat typed name/value descriptor list the host expects.
// Never emits a credential: `claude.apiKeyEnvVar` carries a variable name, and no field carries a
// variable value.
[[nodiscard]] std::vector<n8ro::core::PluginConfigField> toConfigFields(const CommanderConfig& config);

// Parses and validates `fields` on top of `base`, writing the result to `out` only when every field
// is valid. Returns false with a human-readable reason in `error` otherwise, leaving `out` untouched.
//
// Unknown field names are ignored rather than rejected, so a config persisted by a newer build does
// not brick an older one. Absent fields keep their value from `base`.
[[nodiscard]] bool tryParseConfigFields(
    const std::vector<n8ro::core::PluginConfigField>& fields,
    const CommanderConfig& base,
    CommanderConfig& out,
    std::string& error);

} // namespace arkheon::aicommander
