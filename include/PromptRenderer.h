#pragma once

#include "CommanderConfig.h"
#include "Snapshot.h"

#include <string>

namespace arkheon::aicommander {

// Renders the prompt as a byte-stable prefix followed by a volatile per-request suffix
// (AIC-BE-3).
//
// The split is the single highest-leverage optimization in the design. Token generation is
// memory-bandwidth-bound, and both llama.cpp and Ollama cache the processed KV of an unchanged
// prefix; on the hosted backend an unstable prefix forfeits the cache discount entirely. So the
// prefix must be byte-identical for the life of a run — no timestamp, no entity id, no counter, no
// formatted live state.
//
// It is also the security boundary. §"Exactly what is transmitted" enumerates every field that may
// appear in the suffix, and this class is the only place that list is realized. Nothing reaches a
// prompt except through renderSuffix.
class PromptRenderer {
public:
    // Builds the prefix once from the configuration and the doctrine file. Called at initialize();
    // a later config change rebuilds it deliberately, invalidating the cache exactly once.
    void build(const CommanderConfig& config, const std::string& doctrineText);

    [[nodiscard]] const std::string& prefix() const { return prefix_; }
    [[nodiscard]] bool isBuilt() const { return built_; }

    // The per-entity, per-order suffix. Carries ONLY the allowlisted fields.
    //
    // Deliberately absent, and asserted absent by test: every componentTrackIdentity free-text
    // field (trackSource, callsign, originCountry - documented as ingested from an external feed
    // such as live ADS-B, and therefore attacker-influenced), file paths, scenario or mission
    // names, entity profile names, terrain or AI database contents, and any config value beyond
    // the model name and cadence.
    [[nodiscard]] std::string renderSuffix(const OrderSnapshot& snapshot) const;

    // Prefix + suffix, which is what an adapter sends.
    [[nodiscard]] std::string render(const OrderSnapshot& snapshot) const;

    // A stable hash over a canonical serialization, for the order record's snapshotHash /
    // promptHash (AIC-DET-1). Lets prompt drift between runs be detected without storing the
    // prompts themselves — which would put scenario state in the log twice.
    [[nodiscard]] static std::string stableHash(const std::string& text);

private:
    std::string prefix_;
    bool built_ = false;
};

} // namespace arkheon::aicommander
