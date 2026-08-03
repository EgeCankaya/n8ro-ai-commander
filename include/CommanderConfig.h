#pragma once

#include <plugin/PluginConfigField.h>

#include <optional>
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
    // Written to HttpRequest::timeoutS (SDK default is 15). Sized for a COLD MODEL LOAD, not for
    // steady state: a warm 7B answers in ~1.7 s, but the first request after Ollama evicts the
    // model measured 22-46 s. At 30 s the first order of every run timed out, reliably.
    int requestTimeoutS = 90;
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
    // An OLLAMA TAG, not a GGUF filename. The shipped .gguf files need no import, and naming one
    // here fails with model-not-found — which surfaces as a generic transport rejection rather
    // than as the configuration error it is.
    std::string localModel = "qwen2.5:7b-instruct-q8_0";
    double localTemperature = 0.0;        // Greedy decoding; lowest run-to-run variance.
    // Sends the embedded order schema as Ollama's `format` parameter. Not GBNF. It secures types,
    // required fields and enums — but NOT the conditional-presence rules, which Stage-A A6 owns.
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

// The fixed path of the deployed configuration file (AIC-API-2, v1.7.2), relative to the HOST's
// working directory — the release root, which setup.cmd establishes. Deliberately not configurable:
// a config path that is itself configurable has no bootstrap.
inline constexpr const char* kDeployedConfigPath = "data/config/plugins/ai-commander.cfg";

// Parses a deployed .cfg into the flat field list applyConfigFields already takes.
//
// Format is the one the release tree already uses for per-plugin configuration — flat `name=value`,
// one per line, dotted names, no sections, empty values legal (data/config/plugins/n8ro-skyfeed.cfg
// is the shipped exemplar) — extended to tolerate `#` comment lines, blank lines, and surrounding
// whitespace, because docs/example-config/ai-commander.cfg is largely comments and has instructed
// operators to copy it into that directory since Phase 0. Rejecting it would have made this
// repository's own documented workflow produce a file the plugin refuses.
//
// Every field is emitted as Text. That is not laziness: tryParseConfigFields dispatches on the
// field NAME and parses the value itself, never reading field.type — so a per-field type table here
// would be a second copy of a mapping that already exists, and a second thing to drift.
//
// A line with no '=' is skipped rather than failing the parse. Validation is tryParseConfigFields's
// job, and it is the one place that should refuse anything.
[[nodiscard]] std::vector<n8ro::core::PluginConfigField> parseConfigFileText(const std::string& text);

// Reads the deployed config at `path` and returns the fields it carries, or std::nullopt when no
// file exists there.
//
// The nullopt-versus-empty distinction is load-bearing rather than stylistic: "there is no config
// file" and "there is a config file that sets nothing" are different operator situations and log
// differently. Collapsing them is how a typo'd path becomes indistinguishable from a deliberate
// default deploy — which is exactly the failure §Corrections item 16 records for the doctrine path.
[[nodiscard]] std::optional<std::vector<n8ro::core::PluginConfigField>> readDeployedConfigFile(
    const std::string& path);

} // namespace arkheon::aicommander
