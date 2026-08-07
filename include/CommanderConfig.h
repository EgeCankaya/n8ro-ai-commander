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

// Upper bound on `claude.maxTokens`. Derived, not chosen (PRD §Corrections item 26).
//
// Three ceilings bound a response and none of them is aware of the others: this one, in TOKENS;
// Stage-A A0's kMaxResponseBodyBytes at 64 KiB, which rejects an over-long body as `range` before
// the parse; and OrderRecorder's kMaxRecordedBodyBytes at 4 KiB, which truncates the recorded body
// silently. 65,536 / 8 bytes-per-token — generous by 3-5x against this project's own measured
// ratios — is the largest ceiling at which a full-length response still cannot trip A0 for length.
// It is also inside the band a NON-STREAMING request completes in, and this adapter has no
// streaming path. The API's own maxima are an order of magnitude above; the binding constraint is
// ours.
//
// This is a BOUND, not a recommendation. The default stays 512 because no other value has been
// measured: the run that found 512 to be Haiku-shaped is right-censored at 512 (item 25).
inline constexpr int kMaxClaudeMaxTokens = 8192;

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
    // Model-shaped, like claudeEffort below. 512 is 4.0x Haiku 4.5's measured worst case over 120
    // orders and 1.02x Sonnet 5's, which truncated 4 of 48 at this value. Truncation arrives as
    // `parse`/`envelope`, never as a length error. Raising it for a verbose model needs that
    // model's output distribution measured first. Bounded above by kMaxClaudeMaxTokens.
    int claudeMaxTokens = 512;
    // The NAME of an environment variable, never the value. See ADR-5: the value is read via
    // std::getenv at request time, never persisted, never logged, never returned by getConfigFields.
    std::string claudeApiKeyEnvVar = "ANTHROPIC_API_KEY";
    std::string claudeEffort;             // Empty for Haiku 4.5, which does not accept `effort`.

    // -- safety.* : the Stage-B clamp bounds -----------------------------------------------------
    double maxSpeedMps = 400.0;

    // The floor B6 never had (PRD v1.8.27, AIC-VAL-1, closing C19).
    //
    // Until this field existed the accepted band was (0, maxSpeedMps]: Stage A's A7 refused <= 0,
    // B6 refused above the ceiling, and NOTHING refused between. Five orders at ~1.5 m/s were
    // accepted for a fighter, and every layer behaved as specified. The asymmetry was visible one
    // line below this one - altitude has carried BOTH bounds since v1.0, because too low is
    // dangerous, and the identical argument for speed was simply never made.
    //
    // `> 0` is not a floor. It is a meaninglessness check - the same role kMaxCruiseSpeedMps plays
    // at the other end ("a static ceiling, not an airframe limit").
    //
    // THE VALUE IS A POLICY CHOICE AND CANNOT BE DERIVED, which is why it is documented rather than
    // justified. No minimum, stall, or performance envelope exists anywhere in the shipped platform
    // data: componentPhysics carries mass and dragCoefficient, componentNavigation carries no speed
    // bound, the Su-35 profile overrides only mass and RCS, and the schema's only Mps leaves are
    // componentTransform/speedMps (initial) and waypoint/speed (commanded). So this sits on exactly
    // the same footing as maxSpeedMps = 400, which is also derived from nothing.
    //
    // 50 m/s is a factor of >= 4 below both the shipped scenario's 220 m/s spawn and the 400 m/s
    // ceiling, so it cannot refuse a legitimate cruise order in the platform class the rest of this
    // block already assumes - while sitting far above the 1.5-27 m/s band actually observed.
    // A DEPLOYMENT COMMANDING ROTARY-WING OR LOITERING PLATFORMS MUST LOWER IT, exactly as it must
    // lower minAltitudeHaeM. That is a property of this block, not a new burden from this field.
    double minSpeedMps = 50.0;

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
