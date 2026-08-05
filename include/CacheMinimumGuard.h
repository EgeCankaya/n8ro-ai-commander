#pragma once

#include <optional>
#include <string>

namespace arkheon::aicommander {

// THE CACHE-MINIMUM GUARD, RUNTIME HALF (AIC-BE-3, PRD §Corrections item 35; carried as C9 from
// v1.8.16 to v1.8.17).
//
// WHAT IT PROTECTS, and why the failure needs a guard at all. On the hosted path the prompt prefix
// is cached, and Anthropic will only cache a block at or above a per-model minimum. Below that
// minimum the cache silently does not form: no error, no rejection, no counter moves, and order
// quality is unchanged. The only observable is the bill going from $0.001220 to ~$0.005829 per
// order (§Cost model) — roughly 4.8x — and a per-order cost figure nobody is reading in real time.
// C3 spent 72 % of the margin this guards: the cached block is 5,118 tokens against Haiku 4.5's
// 4,096 minimum, so the room left is 1,022 tokens (24.9 %), about 4,000 bytes of prefix.
//
// WHY IT IS NOT THE BUILD-TIME PIN AGAIN. tests/PromptRendererTests.cpp already pins the shipped
// prefix to 8,750 bytes, and that catches a prefix edit made in THIS repository. It structurally
// cannot see three other ways the same failure arrives:
//
//   - data/doctrine.txt edited in the RELEASE TREE, which is where it is deployed and where
//     whoever tunes tactics actually edits it (§Corrections item 16 is the same shape: the
//     doctrine file was never deployed for an entire phase and nothing said so);
//   - claude.model changed to one with a HIGHER minimum, which moves the boundary without
//     touching a single byte of prefix;
//   - the block failing to cache for a reason unrelated to its size.
//
// Two guards, two failure modes. Neither subsumes the other.
//
// WHAT IT COMPARES, and the two things it must not do.
//
//   (1) It reads the response's `usage`, NEVER prefix(). PRD v1.8.4(a) established that reading the
//       prefix alone is wrong by 69 % about the quantity being guarded — the adapter also sends the
//       schema structurally in output_config.format.schema, and that caches too (item 22). After
//       C3 removed the prose schema it is wrong in the OTHER direction: the prefix text is ~2,212
//       tokens against a 4,096 minimum, so a prefix()-based check would report a shortfall on
//       EVERY run while the block was in fact caching perfectly at 5,118. A byte-to-token ratio may
//       predict a crossing; it may not report one.
//
//   (2) It is not a STARTUP check, which is what the PRD asserted from v1.3 to v1.8.15. The values
//       exist only after a response. There is nothing at initialize() to compare, which is why the
//       assertion survived twelve revisions without anyone noticing it described no code.
//
// THE COLD-START PROBLEM, which is what makes this a two-field guard. See the state table on
// LlmResult: a block under the minimum reports cacheCreationTokens == 0, and a cold-but-eligible
// first request reports it NON-ZERO with zero reads. Both report cacheReadTokens == 0. So a guard
// built on the read field alone must either warn on the first request of every run — and be muted,
// which is worse than absent, because the PRD would still describe it as a guard — or skip the
// first response and thereby be unable to see a shortfall that begins at the first response, which
// is the only way this failure ever begins. That is the whole argument for closing C4 with C9.
enum class CacheState {
    // The block was written this request: eligible, and there was nothing to read yet. Correct and
    // expected exactly once per run. NOT a warning.
    ColdWrite,
    // Steady state. The block was read back at or above the model's minimum.
    Hit,
    // Both counters zero: no block formed and none was written. THE failure this guard exists for.
    Shortfall,
    // A non-zero read below the model's stated minimum. Should not occur — the service would not
    // serve a sub-minimum block — so it means this table's number disagrees with the service's.
    // Reported rather than swallowed: a guard that silently ignores the impossible is how a wrong
    // constant survives.
    ReadBelowMinimum,
    // `claude.model` is not in the table. The guard asserts NOTHING. The per-model minimums are not
    // monotonic across generations, so an unlisted model's bound cannot be interpolated from its
    // tier or its neighbours (§Corrections item 35(d)) — the same refusal to scale that item 24
    // established for tokenization.
    UnknownModel,
};

[[nodiscard]] const char* toString(CacheState state);

// The minimums, derived against the vendor's published figures rather than carried from memory
// (v1.8.18), and matching what AIC-BE-3's acceptance criterion has stated since v1.3.
//
// DO NOT EXTEND THIS TABLE BY PATTERN. The series is not monotonic across model generations —
// neighbouring releases in the same tier differ, and both 2,048 and 4,096 appear elsewhere in the
// same family — so a new model's minimum is looked up, never inferred from the rows below.
inline constexpr int kHaiku45CacheMinimumTokens = 4096;
inline constexpr int kSonnet5CacheMinimumTokens = 1024;
inline constexpr int kOpus5CacheMinimumTokens = 512;

// std::nullopt when the model is not in the table — which the caller must treat as "do not assert",
// not as "assume the default". Returning the Haiku minimum for an unknown model would make the
// guard confidently wrong about a model nobody measured.
[[nodiscard]] std::optional<int> cacheMinimumTokensFor(const std::string& model);

[[nodiscard]] CacheState classifyCacheState(
    int cacheReadTokens, int cacheCreationTokens, const std::string& model);

// One latched warning per run.
//
// The latch is normative rather than cosmetic (AIC-BE-3(iii)). At the shipped 20-second cadence a
// ten-minute run issues ~30 orders per entity, so an unlatched warning emits ~30 identical lines
// per entity — which is the shape an operator silences, and a silenced warning is the failure mode
// this criterion already had once. Reported once, with the arithmetic, is the version someone acts
// on.
//
// Lives on the simulation thread with the rest of CommanderRuntime's state. Not thread-safe and
// does not need to be: the worker never reaches it.
class CacheMinimumGuard {
public:
    // Returns the line to log, or std::nullopt when there is nothing to say — which is the common
    // case and includes every healthy response, every cold start, and every unknown model.
    //
    // The message carries the ARITHMETIC rather than a verdict, for the same reason the build-time
    // pin's failure message does: the number that changed (a doctrine edit, a model swap) is not
    // the number that matters (what actually cached), and a reader meeting this line at speed will
    // otherwise reach for the wrong one.
    [[nodiscard]] std::optional<std::string> observe(
        int cacheReadTokens, int cacheCreationTokens, const std::string& model);

    // Scenario stop. A new run gets a fresh warning; the latch does not span runs, or a second run
    // in the same process would be silent about a regression it is also suffering.
    void reset() { warned_ = false; }

    [[nodiscard]] bool hasWarned() const { return warned_; }

private:
    bool warned_ = false;
};

} // namespace arkheon::aicommander
