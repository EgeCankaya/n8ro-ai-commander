#include "TestSupport.h"

#include "CommanderConfig.h"

#include <string>
#include <vector>

using arkheon::aicommander::Backend;
using arkheon::aicommander::CommanderConfig;
using arkheon::aicommander::kDeployedConfigPath;
using arkheon::aicommander::parseConfigFileText;
using arkheon::aicommander::readDeployedConfigFile;
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

// AIC-API-2 (v1.8.7): claude.maxTokens is bounded at BOTH ends. The upper bound is the new half
// and it exists because the anticipated next act on this field is an operator raising it by hand:
// past kMaxClaudeMaxTokens a completed response can exceed Stage-A A0's kMaxResponseBodyBytes and
// be rejected `range`, which trades one silent length failure for a different one. The bound is
// not a recommendation — the default stays 512, which this also pins.
AIC_TEST(ConfigBoundsClaudeMaxTokensAtBothEnds) {
    AIC_EXPECT_EQ(CommanderConfig{}.claudeMaxTokens, 512,
                  "the default must stay 512 — no other value has been measured (PRD item 25)");

    struct Row {
        const char* value;
        bool expectAccepted;
        const char* why;
    };
    const Row rows[] = {
        {"0", false, "zero output tokens cannot carry an order"},
        {"-1", false, "a negative ceiling is meaningless"},
        {"1", true, "the lower bound itself is legal"},
        {"512", true, "the default must survive a round trip through the validator"},
        {"8192", true, "the upper bound itself is legal"},
        {"8193", false, "one over the bound must be rejected"},
        {"64000", false, "the model's own API maximum is not this product's bound"},
    };

    for (const Row& row : rows) {
        CommanderConfig parsed;
        std::string error;
        const bool applied = tryParseConfigFields(
            one("claude.maxTokens", PluginConfigFieldType::Int, row.value),
            CommanderConfig{}, parsed, error);

        AIC_EXPECT_EQ(applied, row.expectAccepted,
                      std::string("claude.maxTokens=") + row.value + ": " + row.why);
        if (!row.expectAccepted) {
            AIC_EXPECT_TRUE(error.find("claude.maxTokens") != std::string::npos,
                            "the reason must name the field, got: " + error);
        }
    }
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

// -- AIC-API-2 (v1.7.2): the deployed config file as a DEFAULT SOURCE -----------------------------
//
// These cover the parse and the compose. The end-to-end behaviour on the real headless host â€”
// fail-closed with no file, and the file actually taking effect with one â€” is asserted by the
// deployed-artifact smoke against n8ro-sim-local.exe, because that is the only place the claim
// "the headless host now honours this file" can actually be tested.

// The load-bearing property of the whole revision. An absent file must be distinguishable from an
// empty one, and must leave the compiled fail-closed defaults untouched.
AIC_TEST(AbsentDeployedConfigLeavesTheDefaultsFailClosed) {
    const auto absent = readDeployedConfigFile(
        "this/path/does/not/exist/ai-commander.cfg");
    AIC_EXPECT_FALSE(absent.has_value(),
                     "a missing config file must read back as nullopt, not as an empty field list");

    // What the plugin does when the read yields nothing: nothing. The compiled defaults stand.
    const CommanderConfig defaults;
    AIC_EXPECT_FALSE(defaults.enabled,
                     "with no deployed config the commander must remain disabled");
    AIC_EXPECT_TRUE(defaults.backend == Backend::Stub,
                    "with no deployed config the backend must remain stub");
    AIC_EXPECT_FALSE(defaults.claudeEnabled,
                     "with no deployed config the hosted-backend gate must remain closed");

    // And the path itself is pinned, so a rename cannot silently move the file the operator edits.
    AIC_EXPECT_EQ(std::string(kDeployedConfigPath),
                  std::string("data/config/plugins/ai-commander.cfg"),
                  "the deployed config path is a deployment contract, not an implementation detail");
    return true;
}

// The shipped example config is largely comment lines and has told operators to copy it into the
// plugins directory since Phase 0. A reader that choked on it would make this repository's own
// documented workflow produce a file the plugin refuses.
AIC_TEST(DeployedConfigParserSkipsCommentsAndBlanks) {
    const std::string text =
        "# Example deployed configuration for the AI Entity Commander.\n"
        "#\n"
        "\n"
        "   # an indented comment\n"
        "commander.enabled=true\n"
        "  commander.backend =  local  \n"
        "\n"
        "# -- safety envelope ---\n"
        "safety.maxSpeedMps=400\n"
        "a line with no equals sign\n";

    const std::vector<PluginConfigField> fields = parseConfigFileText(text);
    AIC_EXPECT_EQ(fields.size(), static_cast<std::size_t>(3),
                  "only the three name=value lines are fields");

    AIC_EXPECT_EQ(fields[0].name, std::string("commander.enabled"), "first field name");
    AIC_EXPECT_EQ(fields[0].value, std::string("true"), "first field value");

    // Surrounding whitespace is the operator's formatting, not part of the name or the value.
    AIC_EXPECT_EQ(fields[1].name, std::string("commander.backend"), "name is trimmed");
    AIC_EXPECT_EQ(fields[1].value, std::string("local"), "value is trimmed");

    // Every field is emitted as Text on purpose: tryParseConfigFields dispatches on the NAME and
    // parses the value itself, so a per-field type table here would be a second copy of a mapping
    // that already exists.
    for (const PluginConfigField& field : fields) {
        AIC_EXPECT_TRUE(field.type == PluginConfigFieldType::Text,
                        "every parsed field is Text; the value parser owns the typing");
    }
    return true;
}

// `replay.path=` and `claude.effort=` both ship empty in the example config and both MEAN empty.
// Dropping them would silently turn "set this to nothing" into "do not touch this".
AIC_TEST(DeployedConfigParserKeepsEmptyValues) {
    const std::vector<PluginConfigField> fields =
        parseConfigFileText("replay.path=\nclaude.effort=   \n");
    AIC_EXPECT_EQ(fields.size(), static_cast<std::size_t>(2), "both empty-valued fields survive");
    AIC_EXPECT_EQ(fields[0].name, std::string("replay.path"), "empty-valued field keeps its name");
    AIC_EXPECT_TRUE(fields[0].value.empty(), "an empty value is an empty string, not a dropped field");
    AIC_EXPECT_TRUE(fields[1].value.empty(), "trailing whitespace does not make a value non-empty");
    return true;
}

// The file goes through the SAME tryParseConfigFields path applyConfigFields uses, so all-or-nothing
// holds identically whichever source a value arrived from. One bad line applies zero fields â€”
// including the good ones that preceded it.
AIC_TEST(DeployedConfigApplicationIsAllOrNothing) {
    const std::string text =
        "commander.enabled=true\n"
        "commander.backend=local\n"
        "commander.cadenceS=not-a-number\n";

    const std::vector<PluginConfigField> fields = parseConfigFileText(text);
    AIC_EXPECT_EQ(fields.size(), static_cast<std::size_t>(3), "the parser reports all three lines");

    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_FALSE(tryParseConfigFields(fields, CommanderConfig{}, parsed, error),
                     "one invalid field must reject the whole file");
    AIC_EXPECT_TRUE(error.find("commander.cadenceS") != std::string::npos,
                    "the error must name the offending field so an operator can fix it");

    // The two valid lines preceding the bad one must NOT have taken effect. A partially applied
    // config is a configuration nobody specified.
    CommanderConfig live;
    AIC_EXPECT_FALSE(live.enabled, "a rejected file leaves the commander disabled");
    AIC_EXPECT_TRUE(live.backend == Backend::Stub, "a rejected file leaves the backend on stub");
    return true;
}

// The positive case: the file the live gate actually deploys turns the commander on.
AIC_TEST(DeployedConfigTurnsTheCommanderOn) {
    const std::string text =
        "# the file the live-scenario smoke writes\n"
        "commander.enabled=true\n"
        "commander.backend=local\n"
        "local.model=qwen2.5:7b-instruct-q8_0\n";

    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_TRUE(tryParseConfigFields(parseConfigFileText(text), CommanderConfig{}, parsed, error),
                    "a valid deployed config must apply");
    AIC_EXPECT_TRUE(parsed.enabled, "commander.enabled must be honoured from the file");
    AIC_EXPECT_TRUE(parsed.backend == Backend::Local, "commander.backend must be honoured from the file");

    // A tag, not a GGUF filename - the value survives the round trip through Text unchanged.
    AIC_EXPECT_EQ(parsed.localModel, std::string("qwen2.5:7b-instruct-q8_0"), "model tag round-trips");

    // Every field the file did NOT mention keeps its compiled default. Absent is not zero.
    AIC_EXPECT_EQ(parsed.requestTimeoutS, 90, "an unmentioned field keeps its compiled default");
    AIC_EXPECT_FALSE(parsed.claudeEnabled, "the hosted gate is not opened by a local-backend file");
    return true;
}

// The gates still hold when the values arrive from a file rather than from the config editor. This
// is the check that would catch a future refactor routing the file around tryParseConfigFields.
AIC_TEST(DeployedConfigCannotBypassTheHostedAuthorizationGate) {
    CommanderConfig parsed;
    std::string error;
    AIC_EXPECT_FALSE(
        tryParseConfigFields(parseConfigFileText("commander.backend=claude\n"),
                             CommanderConfig{}, parsed, error),
        "a deployed file must not be able to select the hosted backend with claude.enabled false");
    AIC_EXPECT_FALSE(
        tryParseConfigFields(parseConfigFileText("claude.baseUrl=http://api.anthropic.com\n"),
                             CommanderConfig{}, parsed, error),
        "a deployed file must not be able to downgrade the hosted backend to plain HTTP");
    return true;
}

