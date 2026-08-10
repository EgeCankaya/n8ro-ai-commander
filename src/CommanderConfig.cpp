#include "CommanderConfig.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>

namespace arkheon::aicommander {

namespace {

using n8ro::core::PluginConfigField;
using n8ro::core::PluginConfigFieldType;

constexpr std::string_view kHttpsPrefix = "https://";

void pushBool(std::vector<PluginConfigField>& out, std::string name, bool value) {
    out.push_back(PluginConfigField{std::move(name), PluginConfigFieldType::Bool, value ? "true" : "false"});
}

void pushInt(std::vector<PluginConfigField>& out, std::string name, int value) {
    out.push_back(PluginConfigField{std::move(name), PluginConfigFieldType::Int, std::to_string(value)});
}

// Real values round-trip through the shortest representation that reads back identically, so a
// value the operator never touched does not drift its own text between getConfigFields calls.
void pushReal(std::vector<PluginConfigField>& out, std::string name, double value) {
    char buffer[64] = {};
    const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    std::string text = (result.ec == std::errc{}) ? std::string(buffer, result.ptr) : std::string("0");
    out.push_back(PluginConfigField{std::move(name), PluginConfigFieldType::Real, std::move(text)});
}

void pushText(std::vector<PluginConfigField>& out, std::string name, std::string value) {
    out.push_back(PluginConfigField{std::move(name), PluginConfigFieldType::Text, std::move(value)});
}

[[nodiscard]] std::string_view trim(std::string_view text) {
    const auto isSpace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] bool parseBool(std::string_view text, bool& out) {
    const std::string_view value = trim(text);
    if (value == "true" || value == "1") {
        out = true;
        return true;
    }
    if (value == "false" || value == "0") {
        out = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool parseInt(std::string_view text, int& out) {
    const std::string_view value = trim(text);
    int parsed = 0;
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last) {
        return false;
    }
    out = parsed;
    return true;
}

// Rejects NaN and infinity outright. A non-finite bound would silently disable whichever clamp it
// backs — every comparison against NaN is false — so it must never reach the validator.
[[nodiscard]] bool parseReal(std::string_view text, double& out) {
    const std::string_view value = trim(text);
    double parsed = 0.0;
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last || !std::isfinite(parsed)) {
        return false;
    }
    out = parsed;
    return true;
}

[[nodiscard]] bool parseBackend(std::string_view text, Backend& out) {
    const std::string_view value = trim(text);
    if (value == "stub") {
        out = Backend::Stub;
        return true;
    }
    if (value == "replay") {
        out = Backend::Replay;
        return true;
    }
    if (value == "local") {
        out = Backend::Local;
        return true;
    }
    if (value == "claude") {
        out = Backend::Claude;
        return true;
    }
    return false;
}

[[nodiscard]] bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

const char* toString(Backend backend) {
    switch (backend) {
        case Backend::Stub:   return "stub";
        case Backend::Replay: return "replay";
        case Backend::Local:  return "local";
        case Backend::Claude: return "claude";
    }
    return "stub";
}

std::vector<PluginConfigField> toConfigFields(const CommanderConfig& config) {
    std::vector<PluginConfigField> fields;
    fields.reserve(29);

    pushBool(fields, "commander.enabled", config.enabled);
    pushText(fields, "commander.backend", toString(config.backend));
    pushReal(fields, "commander.cadenceS", config.cadenceS);
    pushInt(fields, "commander.maxCommandedEntities", config.maxCommandedEntities);
    pushInt(fields, "commander.maxConcurrentRequests", config.maxConcurrentRequests);
    pushInt(fields, "commander.requestTimeoutS", config.requestTimeoutS);
    pushReal(fields, "commander.maxOrderAgeS", config.maxOrderAgeS);
    pushReal(fields, "commander.orderValidityS", config.orderValidityS);
    pushReal(fields, "commander.releaseAfterS", config.releaseAfterS);
    pushInt(fields, "commander.maxTracksInPrompt", config.maxTracksInPrompt);

    pushText(fields, "prompt.doctrinePath", config.doctrinePath);

    pushText(fields, "local.baseUrl", config.localBaseUrl);
    pushText(fields, "local.model", config.localModel);
    pushReal(fields, "local.temperature", config.localTemperature);
    pushBool(fields, "local.grammarEnabled", config.localGrammarEnabled);

    pushBool(fields, "claude.enabled", config.claudeEnabled);
    pushText(fields, "claude.baseUrl", config.claudeBaseUrl);
    pushText(fields, "claude.model", config.claudeModel);
    pushInt(fields, "claude.maxTokens", config.claudeMaxTokens);
    // The NAME only. There is deliberately no field carrying the key value, so there is nothing to
    // redact here — the redaction is structural (ADR-5).
    pushText(fields, "claude.apiKeyEnvVar", config.claudeApiKeyEnvVar);
    pushText(fields, "claude.effort", config.claudeEffort);
    // The spend ceiling and its prices (AIC-BE-2, v1.8.47, C24). Published like every other field
    // so an operator can see the ceiling a session is running under without reading the source.
    pushReal(fields, "claude.maxSpendUsd", config.claudeMaxSpendUsd);
    pushReal(fields, "claude.priceInPerMTok", config.claudePriceInPerMTok);
    pushReal(fields, "claude.priceOutPerMTok", config.claudePriceOutPerMTok);
    pushReal(fields, "claude.priceCacheReadPerMTok", config.claudePriceCacheReadPerMTok);
    pushReal(fields, "claude.priceCacheWritePerMTok", config.claudePriceCacheWritePerMTok);

    pushReal(fields, "safety.maxSpeedMps", config.maxSpeedMps);
    pushReal(fields, "safety.minSpeedMps", config.minSpeedMps);
    pushReal(fields, "safety.minAltitudeHaeM", config.minAltitudeHaeM);
    pushReal(fields, "safety.maxAltitudeHaeM", config.maxAltitudeHaeM);
    pushReal(fields, "safety.geofenceRadiusM", config.geofenceRadiusM);
    pushReal(fields, "safety.defaultOrbitRadiusM", config.defaultOrbitRadiusM);

    pushBool(fields, "record.enabled", config.recordEnabled);
    pushText(fields, "record.path", config.recordPath);
    pushText(fields, "replay.path", config.replayPath);

    return fields;
}

bool tryParseConfigFields(
    const std::vector<PluginConfigField>& fields,
    const CommanderConfig& base,
    CommanderConfig& out,
    std::string& error) {
    error.clear();

    // Scratch copy: nothing reaches `out` until every field has validated. This is what makes
    // application all-or-nothing rather than best-effort.
    CommanderConfig draft = base;

    const auto fail = [&error](std::string_view name, std::string_view why) {
        error = std::string(name) + ": " + std::string(why);
        return false;
    };

    for (const PluginConfigField& field : fields) {
        const std::string& name = field.name;

        if (name == "commander.enabled") {
            if (!parseBool(field.value, draft.enabled)) return fail(name, "expected true or false");
        } else if (name == "commander.backend") {
            if (!parseBackend(field.value, draft.backend)) {
                return fail(name, "expected one of stub | replay | local | claude");
            }
        } else if (name == "commander.cadenceS") {
            if (!parseReal(field.value, draft.cadenceS)) return fail(name, "expected a finite number");
            if (draft.cadenceS <= 0.0) return fail(name, "must be > 0");
        } else if (name == "commander.maxCommandedEntities") {
            if (!parseInt(field.value, draft.maxCommandedEntities)) return fail(name, "expected an integer");
            if (draft.maxCommandedEntities < 1) return fail(name, "must be >= 1");
        } else if (name == "commander.maxConcurrentRequests") {
            if (!parseInt(field.value, draft.maxConcurrentRequests)) return fail(name, "expected an integer");
            if (draft.maxConcurrentRequests < 1) return fail(name, "must be >= 1");
        } else if (name == "commander.requestTimeoutS") {
            if (!parseInt(field.value, draft.requestTimeoutS)) return fail(name, "expected an integer");
            if (draft.requestTimeoutS < 1) return fail(name, "must be >= 1");
        } else if (name == "commander.maxOrderAgeS") {
            if (!parseReal(field.value, draft.maxOrderAgeS)) return fail(name, "expected a finite number");
            if (draft.maxOrderAgeS <= 0.0) return fail(name, "must be > 0");
        } else if (name == "commander.orderValidityS") {
            if (!parseReal(field.value, draft.orderValidityS)) return fail(name, "expected a finite number");
            if (draft.orderValidityS <= 0.0) return fail(name, "must be > 0");
        } else if (name == "commander.releaseAfterS") {
            if (!parseReal(field.value, draft.releaseAfterS)) return fail(name, "expected a finite number");
            if (draft.releaseAfterS <= 0.0) return fail(name, "must be > 0");
        } else if (name == "commander.maxTracksInPrompt") {
            if (!parseInt(field.value, draft.maxTracksInPrompt)) return fail(name, "expected an integer");
            if (draft.maxTracksInPrompt < 0) return fail(name, "must be >= 0");
        } else if (name == "prompt.doctrinePath") {
            draft.doctrinePath = field.value;
        } else if (name == "local.baseUrl") {
            draft.localBaseUrl = field.value;
        } else if (name == "local.model") {
            draft.localModel = field.value;
        } else if (name == "local.temperature") {
            if (!parseReal(field.value, draft.localTemperature)) return fail(name, "expected a finite number");
            if (draft.localTemperature < 0.0 || draft.localTemperature > 2.0) {
                return fail(name, "must be within [0, 2]");
            }
        } else if (name == "local.grammarEnabled") {
            if (!parseBool(field.value, draft.localGrammarEnabled)) return fail(name, "expected true or false");
        } else if (name == "claude.enabled") {
            if (!parseBool(field.value, draft.claudeEnabled)) return fail(name, "expected true or false");
        } else if (name == "claude.baseUrl") {
            draft.claudeBaseUrl = field.value;
        } else if (name == "claude.model") {
            draft.claudeModel = field.value;
        } else if (name == "claude.maxTokens") {
            if (!parseInt(field.value, draft.claudeMaxTokens)) return fail(name, "expected an integer");
            if (draft.claudeMaxTokens < 1 || draft.claudeMaxTokens > kMaxClaudeMaxTokens) {
                return fail(name, "must be within [1, " + std::to_string(kMaxClaudeMaxTokens) + "]");
            }
        } else if (name == "claude.apiKeyEnvVar") {
            draft.claudeApiKeyEnvVar = field.value;
        } else if (name == "claude.maxSpendUsd") {
            // NOT range-checked against zero here, and that is deliberate: at or below zero is a
            // MEANINGFUL value - it disables the hosted backend - rather than an error to reject.
            // Rejecting it would turn "operator switched egress off" into a config-load failure.
            if (!parseReal(field.value, draft.claudeMaxSpendUsd)) {
                return fail(name, "expected a finite number");
            }
        } else if (name == "claude.priceInPerMTok") {
            if (!parseReal(field.value, draft.claudePriceInPerMTok) || draft.claudePriceInPerMTok < 0.0) {
                return fail(name, "expected a finite number at or above zero");
            }
        } else if (name == "claude.priceOutPerMTok") {
            if (!parseReal(field.value, draft.claudePriceOutPerMTok) || draft.claudePriceOutPerMTok < 0.0) {
                return fail(name, "expected a finite number at or above zero");
            }
        } else if (name == "claude.priceCacheReadPerMTok") {
            if (!parseReal(field.value, draft.claudePriceCacheReadPerMTok)
                || draft.claudePriceCacheReadPerMTok < 0.0) {
                return fail(name, "expected a finite number at or above zero");
            }
        } else if (name == "claude.priceCacheWritePerMTok") {
            if (!parseReal(field.value, draft.claudePriceCacheWritePerMTok)
                || draft.claudePriceCacheWritePerMTok < 0.0) {
                return fail(name, "expected a finite number at or above zero");
            }
        } else if (name == "claude.effort") {
            const std::string_view value = trim(field.value);
            if (!value.empty() && value != "low" && value != "medium" && value != "high") {
                return fail(name, "expected empty, low, medium, or high");
            }
            draft.claudeEffort = std::string(value);
        } else if (name == "safety.maxSpeedMps") {
            if (!parseReal(field.value, draft.maxSpeedMps)) return fail(name, "expected a finite number");
            if (draft.maxSpeedMps <= 0.0) return fail(name, "must be > 0");
        } else if (name == "safety.minSpeedMps") {
            if (!parseReal(field.value, draft.minSpeedMps)) return fail(name, "expected a finite number");
            // Zero is legal and means "no floor" - the pre-v1.8.27 behaviour, which a rotary-wing or
            // loitering deployment may legitimately want back. Negative is not: a speed floor below
            // zero cannot bind on any value Stage A's A7 lets through, so it is a configuration
            // mistake rather than a permissive setting, and a bound that silently cannot fire is
            // exactly the failure mode this field exists to remove.
            if (draft.minSpeedMps < 0.0) return fail(name, "must be >= 0");
        } else if (name == "safety.minAltitudeHaeM") {
            if (!parseReal(field.value, draft.minAltitudeHaeM)) return fail(name, "expected a finite number");
        } else if (name == "safety.maxAltitudeHaeM") {
            if (!parseReal(field.value, draft.maxAltitudeHaeM)) return fail(name, "expected a finite number");
        } else if (name == "safety.geofenceRadiusM") {
            if (!parseReal(field.value, draft.geofenceRadiusM)) return fail(name, "expected a finite number");
            if (draft.geofenceRadiusM <= 0.0) return fail(name, "must be > 0");
        } else if (name == "safety.defaultOrbitRadiusM") {
            if (!parseReal(field.value, draft.defaultOrbitRadiusM)) return fail(name, "expected a finite number");
            if (draft.defaultOrbitRadiusM <= 0.0) return fail(name, "must be > 0");
        } else if (name == "record.enabled") {
            if (!parseBool(field.value, draft.recordEnabled)) return fail(name, "expected true or false");
        } else if (name == "record.path") {
            draft.recordPath = field.value;
        } else if (name == "replay.path") {
            draft.replayPath = field.value;
        }
        // Unknown names are ignored on purpose: a config written by a newer build must not brick an
        // older one, and an unknown key cannot change behaviour here anyway.
    }

    // -- Cross-field rules. These run after every individual field has parsed, because each one is
    // -- a relationship between two fields rather than a property of either alone.

    if (draft.minAltitudeHaeM >= draft.maxAltitudeHaeM) {
        return fail("safety.minAltitudeHaeM", "must be < safety.maxAltitudeHaeM");
    }

    // The same rule the altitude pair has always carried, now that speed is a two-sided envelope
    // too (v1.8.27, C19). An envelope whose ends cross accepts nothing at all, and it would present
    // as "the model stopped producing usable orders" rather than as a configuration error.
    if (draft.minSpeedMps >= draft.maxSpeedMps) {
        return fail("safety.minSpeedMps", "must be < safety.maxSpeedMps");
    }

    // Fail-closed on the hosted backend, and deliberately NOT a silent fall back to local: a silent
    // fallback teaches operators that the authorization switch does not matter.
    if (draft.backend == Backend::Claude && !draft.claudeEnabled) {
        return fail("commander.backend", "'claude' requires claude.enabled = true (data egress gate)");
    }

    // Plain-HTTP misconfiguration would put the key and scenario state in cleartext on the wire.
    if (!startsWith(trim(draft.claudeBaseUrl), kHttpsPrefix)) {
        return fail("claude.baseUrl", "must begin with https://");
    }

    if (draft.backend == Backend::Replay && trim(draft.replayPath).empty()) {
        return fail("replay.path", "is required when commander.backend = replay");
    }

    // The retained order must be able to outlive its own validity window, and release must come
    // after the standing order rather than before it, or the ladder's steps cannot be observed
    // in order.
    if (draft.releaseAfterS < draft.orderValidityS) {
        return fail("commander.releaseAfterS", "must be >= commander.orderValidityS");
    }

    out = draft;
    return true;
}

std::vector<PluginConfigField> parseConfigFileText(const std::string& text) {
    std::vector<PluginConfigField> fields;

    std::size_t lineStart = 0;
    while (lineStart <= text.size()) {
        std::size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = text.size();
        }
        const std::string_view line =
            trim(std::string_view(text).substr(lineStart, lineEnd - lineStart));
        lineStart = lineEnd + 1;

        // Blank and comment lines. `#` only — that is what the shipped example config uses, and a
        // second comment character would be format invented rather than derived.
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string_view::npos) {
            continue;
        }

        const std::string_view name = trim(line.substr(0, separator));
        if (name.empty()) {
            continue;
        }

        // An empty value is an empty string, not an absent field: `replay.path=` and
        // `claude.effort=` are both meaningful in the shipped example, and both mean "empty".
        const std::string_view value = trim(line.substr(separator + 1));
        fields.push_back(PluginConfigField{
            std::string(name), PluginConfigFieldType::Text, std::string(value)});
    }

    return fields;
}

std::optional<std::vector<PluginConfigField>> readDeployedConfigFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return parseConfigFileText(buffer.str());
}

} // namespace arkheon::aicommander
