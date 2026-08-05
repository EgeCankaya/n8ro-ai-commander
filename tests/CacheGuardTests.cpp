#include "TestSupport.h"

#include "CacheMinimumGuard.h"

#include <string>

using namespace arkheon::aicommander;

// THE CACHE-MINIMUM GUARD, RUNTIME HALF (AIC-BE-3, PRD §Corrections item 35; C9 + C4).
//
// These run against no network, no key, and no engine - the guard is a pure function over two
// integers and a model string, which is deliberate. The failure it detects costs ~4.8x per order
// and produces no other symptom, so the check that proves it works must not itself be gated behind
// an egress grant. PRD item 35: "the seam is testable end to end against a fake transport through
// the injectable factory the adapter suite already uses."
//
// The three cases below are the three the PRD names, and the middle one is the reason C4 could not
// be deferred: cold start and shortfall BOTH read zero cached tokens, and only the creation field
// separates them.
//
// VERIFIED BY BREAKING IT ON PURPOSE, which is the standard tests/PromptRendererTests.cpp set for
// this project's guards after item 33(g) found a live probe whose headline assertion could not
// fail. Deleting the `cacheCreationTokens > 0` branch from classifyCacheState - i.e. simulating
// C9 landing WITHOUT C4, which is how these two rows were scheduled to arrive - takes the suite to
// 127/129 with both cold-start cases red, and the failure message is the predicted one:
//
//     [FAIL] CacheGuardIsSilentOnTheColdFirstRequestOfARun: a cold start (wrote 5,118, read 0)
//            is correct and expected once per run, got: ai-commander: the prompt prefix is NOT
//            caching.
//
// That is the muted-guard scenario from PRD item 35(b), reproduced: the guard crying wolf on the
// healthy first request of every single run. The PRD argues C4 is a precondition rather than a
// companion; this is that argument made executable.

// -- the derived table ---------------------------------------------------------------------------

AIC_TEST(CacheMinimumTableMatchesTheDerivedFigures) {
    // Pinned to the values AIC-BE-3 states. If a vendor figure moves, this test and the PRD table
    // move together - the constant is not the authority, the document is.
    AIC_EXPECT_TRUE(cacheMinimumTokensFor("claude-haiku-4-5").value_or(-1) == 4096,
        "Haiku 4.5's minimum is 4,096 tokens - it is the SHIPPED DEFAULT, so this row is the one "
        "the margin arithmetic in Cost model actually rests on");
    AIC_EXPECT_TRUE(cacheMinimumTokensFor("claude-sonnet-5").value_or(-1) == 1024,
        "Sonnet 5's minimum is 1,024 tokens");
    AIC_EXPECT_TRUE(cacheMinimumTokensFor("claude-opus-5").value_or(-1) == 512,
        "Opus 5's minimum is 512 tokens");
    return true;
}

AIC_TEST(CacheMinimumIsUnknownForAnUnlistedModelRatherThanGuessed) {
    // The property this protects is NOT MONOTONICITY. Neighbouring generations in the same tier
    // have different minimums, so "it is a Haiku, therefore 4,096" and "it is newer, therefore
    // smaller" are both wrong inferences. An unknown model must produce no bound at all.
    AIC_EXPECT_FALSE(cacheMinimumTokensFor("claude-haiku-9-9").has_value(),
        "an unlisted model must NOT inherit its tier's minimum - the series is not monotonic");
    AIC_EXPECT_FALSE(cacheMinimumTokensFor("claude-haiku-4-5-20251001").has_value(),
        "matching is exact: a dated snapshot this project has never measured is not the alias");
    AIC_EXPECT_FALSE(cacheMinimumTokensFor("").has_value(),
        "an empty model name is unknown, not a default");

    // And the classifier must not assert a shortfall on the strength of a guess.
    AIC_EXPECT_TRUE(classifyCacheState(300, 0, "claude-haiku-9-9") == CacheState::UnknownModel,
        "300 cached tokens on an unmeasured model asserts nothing - it is caching, and how much it "
        "needs to be caching is not knowable here");
    return true;
}

// -- the three states ----------------------------------------------------------------------------

AIC_TEST(CacheGuardIsSilentOnTheColdFirstRequestOfARun) {
    // THE CASE THAT MAKES C4 LOAD-BEARING. The first request of every run legitimately reads zero:
    // there was nothing cached yet. It WROTE the block, and cache_creation_input_tokens says so.
    // A guard that cannot see the write fires here, on every run, and gets muted - which is worse
    // than no guard, because the PRD would still describe it as one.
    CacheMinimumGuard guard;
    const std::optional<std::string> warning = guard.observe(0, 5118, "claude-haiku-4-5");
    AIC_EXPECT_FALSE(warning.has_value(),
        "a cold start (wrote 5,118, read 0) is correct and expected once per run, got: "
            << warning.value_or(""));
    AIC_EXPECT_FALSE(guard.hasWarned(), "the latch must not be spent on a healthy cold start");
    AIC_EXPECT_TRUE(classifyCacheState(0, 5118, "claude-haiku-4-5") == CacheState::ColdWrite,
        "creation > 0 with read == 0 is a cold write, not a shortfall");
    return true;
}

AIC_TEST(CacheGuardIsSilentOnAHealthyCachedRun) {
    // Steady state: the 119-of-120 case in C3's arms. 5,118 read against a 4,096 minimum - the
    // 1,022-token (24.9 %) margin Cost model spends.
    CacheMinimumGuard guard;
    for (int i = 0; i < 120; ++i) {
        const std::optional<std::string> warning = guard.observe(5118, 0, "claude-haiku-4-5");
        AIC_EXPECT_FALSE(warning.has_value(),
            "a hit at 5,118 against a 4,096 minimum must never warn (response " << i
                << "): " << warning.value_or(""));
    }
    AIC_EXPECT_TRUE(classifyCacheState(5118, 0, "claude-haiku-4-5") == CacheState::Hit,
        "5,118 >= 4,096 is a hit");

    // Exactly at the boundary is a hit, not a shortfall. An off-by-one here would warn on a run
    // that is caching correctly and costs nothing extra.
    AIC_EXPECT_TRUE(classifyCacheState(4096, 0, "claude-haiku-4-5") == CacheState::Hit,
        "a block exactly AT the minimum caches - the bound is inclusive");
    return true;
}

AIC_TEST(CacheGuardWarnsOnAGenuineShortfallAndCarriesTheArithmetic) {
    // The failure: both counters zero. No block was read and none was written, so nothing cached.
    // Silent on the wire - no error, no rejection, no counter - and ~4.8x the cost per order.
    CacheMinimumGuard guard;
    const std::optional<std::string> warning = guard.observe(0, 0, "claude-haiku-4-5");
    AIC_EXPECT_TRUE(warning.has_value(),
        "read 0 and wrote 0 means the block never cached - this is the whole point of the guard");
    AIC_EXPECT_TRUE(classifyCacheState(0, 0, "claude-haiku-4-5") == CacheState::Shortfall,
        "both zero is a shortfall");

    const std::string& text = *warning;
    // The message must carry the ARITHMETIC, not a verdict - same rule the build-time byte pin
    // follows, and for the same reason: the number that changed is not the number that matters.
    AIC_EXPECT_TRUE(text.find("4096") != std::string::npos,
        "the warning must name the minimum it compared against, got: " << text);
    AIC_EXPECT_TRUE(text.find("claude-haiku-4-5") != std::string::npos,
        "the warning must name the model, because the minimum is model-specific, got: " << text);
    AIC_EXPECT_TRUE(text.find("0.005829") != std::string::npos,
        "the warning must state the cost consequence - an operator who cannot see the price has no "
        "reason to act, got: " << text);
    AIC_EXPECT_TRUE(text.find("RELEASE TREE") != std::string::npos,
        "the warning must name the most likely cause: the build-time pin only sees THIS "
        "repository's doctrine file, got: " << text);
    return true;
}

AIC_TEST(CacheGuardWarnsExactlyOncePerRunAndRearmsOnReset) {
    // The latch is normative (AIC-BE-3(iii)). At the shipped 20 s cadence a ten-minute run issues
    // ~30 orders per entity; 30 identical warnings is the shape an operator silences, and a
    // silenced warning is the failure this criterion already had once.
    CacheMinimumGuard guard;
    int warnings = 0;
    for (int i = 0; i < 30; ++i) {
        if (guard.observe(0, 0, "claude-haiku-4-5").has_value()) { ++warnings; }
    }
    AIC_EXPECT_EQ(warnings, 1, "30 shortfall responses must produce exactly one warning");
    AIC_EXPECT_TRUE(guard.hasWarned(), "the latch must be set after warning");

    // reset() is scenario stop. A second run in the same process suffering the same regression has
    // to be told about it too, or the latch silently becomes once-per-process.
    guard.reset();
    AIC_EXPECT_FALSE(guard.hasWarned(), "reset must re-arm the guard");
    AIC_EXPECT_TRUE(guard.observe(0, 0, "claude-haiku-4-5").has_value(),
        "a new run must get its own warning - the latch is per RUN, not per process");
    return true;
}

AIC_TEST(CacheGuardReportsAReadBelowTheMinimumAsATableProblemNotACostProblem) {
    // Should not occur: the service would not serve a sub-minimum block. If it does, this build's
    // constant disagrees with the service, and the remedy is a document correction rather than a
    // deployment fix. Swallowing the impossible is how a wrong constant survives.
    CacheMinimumGuard guard;
    AIC_EXPECT_TRUE(classifyCacheState(2000, 0, "claude-haiku-4-5") == CacheState::ReadBelowMinimum,
        "a non-zero read below the recorded minimum is its own state, not a shortfall");

    const std::optional<std::string> warning = guard.observe(2000, 0, "claude-haiku-4-5");
    AIC_EXPECT_TRUE(warning.has_value(), "it must be reported rather than ignored");
    AIC_EXPECT_TRUE(warning->find("Caching IS occurring") != std::string::npos,
        "the message must NOT imply the 4.8x cost regression - caching is working, got: "
            << *warning);
    return true;
}

// -- Sonnet and Opus, so the table is exercised rather than merely declared ----------------------

AIC_TEST(CacheGuardAppliesThePerModelMinimumRatherThanTheDefaultOne) {
    // A 2,000-token block is a SHORTFALL-adjacent read on Haiku (4,096) and a healthy hit on both
    // Sonnet 5 (1,024) and Opus 5 (512). If the guard hard-coded the default model's minimum, this
    // is where it would show - and a run that switched models would be warned about wrongly, which
    // is exactly what item 24 established about reading one model's numbers as another's.
    AIC_EXPECT_TRUE(classifyCacheState(2000, 0, "claude-sonnet-5") == CacheState::Hit,
        "2,000 >= Sonnet 5's 1,024 minimum is a hit");
    AIC_EXPECT_TRUE(classifyCacheState(2000, 0, "claude-opus-5") == CacheState::Hit,
        "2,000 >= Opus 5's 512 minimum is a hit");
    AIC_EXPECT_TRUE(classifyCacheState(2000, 0, "claude-haiku-4-5") == CacheState::ReadBelowMinimum,
        "the SAME 2,000 tokens is below Haiku 4.5's 4,096 - the minimum is model-specific");

    // And the shortfall is model-independent: nothing read and nothing written is nothing cached,
    // whatever the bound would have been.
    AIC_EXPECT_TRUE(classifyCacheState(0, 0, "claude-opus-5") == CacheState::Shortfall,
        "both zero is a shortfall on every model in the table");
    return true;
}
