#include "TestSupport.h"

#include "CommanderConfig.h"

#include <string>
#include <vector>

using arkheon::aicommander::Backend;
using arkheon::aicommander::CommanderConfig;
using arkheon::aicommander::toConfigFields;
using arkheon::aicommander::toString;
using arkheon::aicommander::tryParseConfigFields;
using n8ro::core::PluginConfigField;
using n8ro::core::PluginConfigFieldType;

namespace {

// Builds a field list carrying a single override, which is how an editor applies one edit.
std::vector<PluginConfigField> one(std::string name, PluginConfigFieldType type, std::string value) {
    return {PluginConfigField{std::move(name), type, std::move(value)}};
}

} // namespace

// AIC-API-2: the two authorization gates default closed. This is the single most important
// property of the config surface — everything else is tuning.
AIC_TEST(ConfigDefaultsAreFailClosed) {
    const CommanderConfig defaults;
    AIC_EXPECT_FALSE(defaults.enabled, "commander.enabled must default false");
    AIC_EXPECT_FALSE(defaults.claudeEnabled, "claude.enabled must default false");
    AIC_EXPECT_TRUE(defaults.backend == Backend::Stub, "commander.backend must default to stub");
    AIC_EXPECT_TRUE(defaults.recordEnabled, "record.enabled must default true - determinism needs it");
    return true;
}

// getConfigFields() must never carry a credential. The redaction is structural rather than a
// filter: the config holds the env-var NAME, and no field holds a value (ADR-5).
AIC_TEST(ConfigNeverEmitsACredential) {
    CommanderConfig config;
    config.claudeApiKeyEnvVar = "ANTHROPIC_API_KEY";

    const std::vector<PluginConfigField> fields = toConfigFields(config);

    bool sawEnvVarName = false;
    for (const PluginConfigField& field : fields) {
        AIC_EXPECT_FALSE(field.name == "claude.apiKey", "there must be no claude.apiKey field at all");
        if (field.name == "claude.apiKeyEnvVar") {
            sawEnvVarName = true;
            AIC_EXPECT_EQ(field.value, std::string("ANTHROPIC_API_KEY"),
                          "claude.apiKeyEnvVar must carry the variable NAME");
        }
    }
    AIC_EXPECT_TRUE(sawEnvVarName, "claude.apiKeyEnvVar must be present");
    return true;
}

// Round-tripping must be lossless, or an operator who opens and saves the config without editing
// anything would silently change it.
AIC_TEST(ConfigRoundTripsWithoutDrift) {
    CommanderConfig original;
    original.enabled = true;
    original.cadenceS = 12.5;
    original.maxCommandedEntities = 3;
    original.maxTracksInPrompt = 6;
    original.maxSpeedMps = 375.5;
    original.recordPath = "logs/custom/";

    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_TRUE(tryParseConfigFields(toConfigFields(original), CommanderConfig{}, parsed, error),
                    "round-trip must parse: " + error);

    AIC_EXPECT_EQ(parsed.enabled, original.enabled, "enabled drifted");
    AIC_EXPECT_EQ(parsed.cadenceS, original.cadenceS, "cadenceS drifted");
    AIC_EXPECT_EQ(parsed.maxCommandedEntities, original.maxCommandedEntities, "roster cap drifted");
    AIC_EXPECT_EQ(parsed.maxTracksInPrompt, original.maxTracksInPrompt, "maxTracksInPrompt drifted");
    AIC_EXPECT_EQ(parsed.maxSpeedMps, original.maxSpeedMps, "maxSpeedMps drifted");
    AIC_EXPECT_EQ(parsed.recordPath, original.recordPath, "recordPath drifted");
    return true;
}

// UAC-AIC-API-2: selecting the hosted backend without its independent authorization gate is a
// configuration error, and it must NOT silently fall back to local - a silent fallback teaches
// operators that the switch does not matter.
AIC_TEST(ConfigRejectsClaudeBackendWithoutItsGate) {
    CommanderConfig parsed;
    std::string error;
    const bool applied = tryParseConfigFields(
        one("commander.backend", PluginConfigFieldType::Text, "claude"),
        CommanderConfig{}, parsed, error);

    AIC_EXPECT_FALSE(applied, "commander.backend=claude must be rejected while claude.enabled=false");
    AIC_EXPECT_TRUE(error.find("claude.enabled") != std::string::npos,
                    "the rejection reason must name the gate that blocked it, got: " + error);
    return true;
}

// A plain-HTTP hosted endpoint would put the API key and scenario state in cleartext on the wire.
AIC_TEST(ConfigRejectsPlainHttpClaudeBaseUrl) {
    CommanderConfig parsed;
    std::string error;
    const bool applied = tryParseConfigFields(
        one("claude.baseUrl", PluginConfigFieldType::Text, "http://api.anthropic.com"),
        CommanderConfig{}, parsed, error);

    AIC_EXPECT_FALSE(applied, "a non-https claude.baseUrl must be rejected");
    AIC_EXPECT_TRUE(error.find("https") != std::string::npos,
                    "the reason must mention https, got: " + error);
    return true;
}

// The load-bearing property of applyConfigFields: application is all-or-nothing. A batch with one
// bad field leaves EVERY prior value untouched, including the good fields in the same batch.
// A partially applied config is a configuration nobody specified.
AIC_TEST(ConfigApplicationIsAllOrNothing) {
    CommanderConfig base;
    base.cadenceS = 20.0;
    base.maxCommandedEntities = 4;

    // A batch where the first field is valid and the second is not.
    const std::vector<PluginConfigField> batch{
        PluginConfigField{"commander.cadenceS", PluginConfigFieldType::Real, "5.0"},
        PluginConfigField{"commander.maxCommandedEntities", PluginConfigFieldType::Int, "not-a-number"},
    };

    CommanderConfig parsed = base;
    std::string error;
    AIC_EXPECT_FALSE(tryParseConfigFields(batch, base, parsed, error),
                     "a batch containing an invalid field must be rejected wholesale");

    // The valid field from the rejected batch must NOT have leaked through.
    AIC_EXPECT_EQ(parsed.cadenceS, 20.0,
                  "cadenceS must retain its prior value after a rejected batch");
    AIC_EXPECT_EQ(parsed.maxCommandedEntities, 4,
                  "maxCommandedEntities must retain its prior value after a rejected batch");
    return true;
}

// Non-finite bounds would silently disable whichever clamp they back, because every comparison
// against NaN is false. They must never reach the validator.
AIC_TEST(ConfigRejectsNonFiniteBounds) {
    struct Row {
        const char* name;
        const char* value;
    };
    const Row rows[] = {
        {"safety.maxSpeedMps", "nan"},
        {"safety.maxSpeedMps", "inf"},
        {"safety.maxAltitudeHaeM", "NaN"},
        {"commander.cadenceS", "infinity"},
    };

    for (const Row& row : rows) {
        CommanderConfig parsed;
        std::string error;
        const bool applied = tryParseConfigFields(
            one(row.name, PluginConfigFieldType::Real, row.value), CommanderConfig{}, parsed, error);
        AIC_EXPECT_FALSE(applied,
                         std::string("non-finite value '") + row.value + "' for " + row.name
                             + " must be rejected");
    }
    return true;
}

// replay.path is not optional when the replay backend is selected - a replay with no log is a
// backend that can never produce an order.
AIC_TEST(ConfigRequiresReplayPathForReplayBackend) {
    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_FALSE(
        tryParseConfigFields(one("commander.backend", PluginConfigFieldType::Text, "replay"),
                             CommanderConfig{}, parsed, error),
        "replay backend without replay.path must be rejected");

    const std::vector<PluginConfigField> withPath{
        PluginConfigField{"commander.backend", PluginConfigFieldType::Text, "replay"},
        PluginConfigField{"replay.path", PluginConfigFieldType::Text, "logs/ai-commander/run1.jsonl"},
    };
    AIC_EXPECT_TRUE(tryParseConfigFields(withPath, CommanderConfig{}, parsed, error),
                    "replay backend with replay.path must apply: " + error);
    AIC_EXPECT_TRUE(parsed.backend == Backend::Replay, "backend must be replay");
    return true;
}

// The ladder's steps must be observable in order: an order cannot be released before it has had a
// chance to expire into the standing order.
AIC_TEST(ConfigRejectsLadderOrderingInversion) {
    const std::vector<PluginConfigField> inverted{
        PluginConfigField{"commander.orderValidityS", PluginConfigFieldType::Real, "300"},
        PluginConfigField{"commander.releaseAfterS", PluginConfigFieldType::Real, "120"},
    };
    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_FALSE(tryParseConfigFields(inverted, CommanderConfig{}, parsed, error),
                     "releaseAfterS < orderValidityS must be rejected");
    return true;
}

// An unknown key must not brick the plugin: a config persisted by a newer build has to load in an
// older one, and an unknown key cannot change behaviour anyway.
AIC_TEST(ConfigIgnoresUnknownFields) {
    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_TRUE(
        tryParseConfigFields(one("commander.futureSetting", PluginConfigFieldType::Text, "whatever"),
                             CommanderConfig{}, parsed, error),
        "an unknown field must be ignored, not rejected: " + error);
    return true;
}

// Backend names are an enum on the wire; anything outside the four adapters is a rejection rather
// than a silent fall back to the default.
AIC_TEST(ConfigRejectsUnknownBackend) {
    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_FALSE(
        tryParseConfigFields(one("commander.backend", PluginConfigFieldType::Text, "gpt"),
                             CommanderConfig{}, parsed, error),
        "an unknown backend name must be rejected");

    AIC_EXPECT_EQ(std::string(toString(Backend::Stub)), std::string("stub"), "stub name");
    AIC_EXPECT_EQ(std::string(toString(Backend::Replay)), std::string("replay"), "replay name");
    AIC_EXPECT_EQ(std::string(toString(Backend::Local)), std::string("local"), "local name");
    AIC_EXPECT_EQ(std::string(toString(Backend::Claude)), std::string("claude"), "claude name");
    return true;
}

// Altitude bounds must describe a non-empty interval, or every altitude check is vacuously false.
AIC_TEST(ConfigRejectsInvertedAltitudeBounds) {
    const std::vector<PluginConfigField> inverted{
        PluginConfigField{"safety.minAltitudeHaeM", PluginConfigFieldType::Real, "20000"},
        PluginConfigField{"safety.maxAltitudeHaeM", PluginConfigFieldType::Real, "100"},
    };
    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_FALSE(tryParseConfigFields(inverted, CommanderConfig{}, parsed, error),
                     "min >= max altitude must be rejected");
    return true;
}
