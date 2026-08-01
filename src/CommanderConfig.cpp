#include "CommanderConfig.h"

#include <algorithm>
#include <charconv>
#include <cmath>
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

    pushReal(fields, "safety.maxSpeedMps", config.maxSpeedMps);
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
            if (draft.claudeMaxTokens < 1) return fail(name, "must be >= 1");
        } else if (name == "claude.apiKeyEnvVar") {
            draft.claudeApiKeyEnvVar = field.value;
        } else if (name == "claude.effort") {
            const std::string_view value = trim(field.value);
            if (!value.empty() && value != "low" && value != "medium" && value != "high") {
                return fail(name, "expected empty, low, medium, or high");
            }
            draft.claudeEffort = std::string(value);
        } else if (name == "safety.maxSpeedMps") {
            if (!parseReal(field.value, draft.maxSpeedMps)) return fail(name, "expected a finite number");
            if (draft.maxSpeedMps <= 0.0) return fail(name, "must be > 0");
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

} // namespace arkheon::aicommander
