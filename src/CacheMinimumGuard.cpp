#include "CacheMinimumGuard.h"

#include <sstream>

namespace arkheon::aicommander {

const char* toString(CacheState state) {
    switch (state) {
        case CacheState::ColdWrite:        return "coldWrite";
        case CacheState::Hit:              return "hit";
        case CacheState::Shortfall:        return "shortfall";
        case CacheState::ReadBelowMinimum: return "readBelowMinimum";
        case CacheState::UnknownModel:     return "unknownModel";
    }
    return "unknown";
}

std::optional<int> cacheMinimumTokensFor(const std::string& model) {
    // Exact match, deliberately. A prefix or substring match ("starts with claude-haiku") would
    // quietly answer for a model this project has never measured, and the whole point of the
    // UnknownModel state is that the series is not monotonic and cannot be guessed at.
    if (model == "claude-haiku-4-5") { return kHaiku45CacheMinimumTokens; }
    if (model == "claude-sonnet-5")  { return kSonnet5CacheMinimumTokens; }
    if (model == "claude-opus-5")    { return kOpus5CacheMinimumTokens; }
    return std::nullopt;
}

CacheState classifyCacheState(
    int cacheReadTokens, int cacheCreationTokens, const std::string& model) {
    // The write is checked FIRST, and the order matters. A cold start has creation > 0 and read
    // == 0; a shortfall has both at zero. Testing the read field first would collapse them into
    // one branch, which is exactly the mistake C9-without-C4 would have shipped.
    if (cacheCreationTokens > 0) {
        return CacheState::ColdWrite;
    }
    if (cacheReadTokens <= 0) {
        // Nothing read and nothing written. On the hosted path this means the block never formed.
        // Note the caller is responsible for only asking about backends that report cache
        // accounting at all — a stub or replay adapter reports (0, 0) on every response and is not
        // suffering a cache regression, it simply has no cache.
        return CacheState::Shortfall;
    }

    const std::optional<int> minimum = cacheMinimumTokensFor(model);
    if (!minimum.has_value()) {
        // Something is caching. How much it needs to be caching is not knowable for this model,
        // so nothing is asserted about it.
        return CacheState::UnknownModel;
    }
    return (cacheReadTokens >= *minimum) ? CacheState::Hit : CacheState::ReadBelowMinimum;
}

std::optional<std::string> CacheMinimumGuard::observe(
    int cacheReadTokens, int cacheCreationTokens, const std::string& model) {
    const CacheState state = classifyCacheState(cacheReadTokens, cacheCreationTokens, model);

    if (state == CacheState::Hit || state == CacheState::ColdWrite
        || state == CacheState::UnknownModel) {
        return std::nullopt;
    }
    if (warned_) {
        return std::nullopt;   // Latched. See the header for why this is normative.
    }

    const std::optional<int> minimum = cacheMinimumTokensFor(model);
    std::ostringstream out;

    if (state == CacheState::Shortfall) {
        out << "ai-commander: the prompt prefix is NOT caching.";
        if (minimum.has_value()) {
            out << "\n    model '" << model << "' will not cache a block under " << *minimum
                << " tokens;";
        } else {
            out << "\n    model '" << model << "' has no minimum on record;";
        }
        out << "\n    this response read 0 and wrote 0, so no block formed at all."
            << "\n    Cost per order goes from ~$0.001220 to ~$0.005829 (~4.8x) while this holds -"
            << "\n    about $0.88 to $4.20 per four-ship-hour, against a <= $1.10 target."
            << "\n    Likely causes, in the order worth checking: data/doctrine.txt was edited in"
            << "\n    the RELEASE TREE (the build-time byte pin only sees this repository's copy);"
            << "\n    claude.model was changed to one with a higher minimum; or the prefix is no"
            << "\n    longer byte-stable across requests (AIC-BE-3)."
            << "\n    Reported once per run.";
        warned_ = true;
        return out.str();
    }

    // ReadBelowMinimum. The service served a block smaller than this table says it will cache, so
    // one of the two is wrong and it is more likely to be the table. Said plainly rather than
    // folded into the shortfall message, because the remedy is different: this one is a document
    // correction, not a deployment problem.
    out << "ai-commander: cached block is smaller than this build believes is possible."
        << "\n    model '" << model << "' read " << cacheReadTokens << " tokens, below the "
        << (minimum.has_value() ? *minimum : 0) << " minimum recorded for it."
        << "\n    Caching IS occurring, so the cost regression above does not apply. What is"
        << "\n    suspect is the constant: verify the model's published minimum and correct"
        << "\n    AIC-BE-3's table and CacheMinimumGuard.h together."
        << "\n    Reported once per run.";
    warned_ = true;
    return out.str();
}

} // namespace arkheon::aicommander
