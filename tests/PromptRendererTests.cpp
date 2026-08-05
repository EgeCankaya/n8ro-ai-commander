#include "TestSupport.h"

#include "CommanderConfig.h"
#include "PromptRenderer.h"
#include "Snapshot.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace arkheon::aicommander;

namespace {

// THE CACHE-MINIMUM GUARD, BUILD-TIME HALF (UAC-AIC-BE-3, PRD §Corrections item 33).
//
// WHAT THIS PROTECTS. On the hosted path the prompt prefix is cached, and Anthropic will only cache
// a block at or above a per-model minimum - 4,096 tokens for Haiku 4.5, which is the default model.
// Under that minimum the cache silently does not form: no error, no counter, no rejection. The only
// observable is `cache_read_input_tokens` falling to zero in an order log nobody diffs, and the bill
// going from $0.001220 to ≈$0.005829 per order (§Cost model). Wrong silently, in the direction that
// looks fine.
//
// WHY IT IS A SIZE PIN AND NOT A TOKEN CHECK. There is no offline Claude tokenizer - the only
// authority is a live `count_tokens` call, which is authorization-gated and costs money. So this
// asserts the quantity that CAN be checked offline, exactly: the prefix's byte count. The token
// count it stands in for was measured once, under the fifth grant, and is recorded here rather than
// re-derived.
//
//   prefix as shipped ........... 8,750 bytes   (§Corrections item 31(a), arm B, 120 orders)
//   bytes per token ............. 3.955         (measured on THIS corpus, v1.8.2 - not a constant)
//   prefix text alone ........... ≈2,212 tokens
//   what actually caches ........ 5,118 tokens  (the adapter ALSO sends the schema structurally in
//                                                output_config.format.schema, and it caches too -
//                                                §Corrections item 22; measured in-engine, item 32(b))
//   Haiku 4.5 cache minimum ..... 4,096 tokens
//   margin ...................... 1,022 tokens (24.9 %) ≈ 4,042 bytes of prefix
//
// So roughly 4,000 bytes of prefix can still be removed before the cache stops forming, and this
// test is what makes crossing that boundary a deliberate act rather than an editorial accident.
// PRD §Corrections item 22 told an editor "a page of doctrine can be deleted without consequence";
// item 31(e) records that this stopped being true when C3 spent the margin. A reader of the doctrine
// file has no way to know that. This test does.
//
// WHY THE FAILURE IS A PIN AND NOT A `>` BOUND. It used to be `size() > 800`, which passed at 8,750
// with ≈10x of slack and a message that named a schema the prefix no longer carries. A lower bound
// cannot catch what actually goes wrong here, which is a change nobody thought was a change: the
// one-byte divergence §Corrections item 31(f) records catching BY HAND - a deleted blank line that
// made the shipped prefix 8,749 against the 8,750 that measured 120/120 - passes any bound loose
// enough to be maintainable. An equality pin catches it, and a reviewer updating the constant has
// to say why.
//
// UPDATING THESE NUMBERS is a deliberate act with a precondition: the prefix that ships must be the
// prefix that was measured. If the prefix changes on purpose, the cache figures above are stale
// until a new arm measures them, and §Cost model's rows are computed from them.
constexpr std::size_t kMeasuredPrefixBytes = 8750;
constexpr std::size_t kShippedDoctrineBytes = 6932;

// Everything PromptRenderer::build contributes that is not the doctrine text: the system prompt, the
// posture/ROE vocabulary, the DOCTRINE: label, the cadence paragraph, and the newlines between them.
// 8,750 - 6,932. Pinned separately so a C++ edit and a doctrine edit fail with different messages -
// they have different fixes and different reviewers.
constexpr std::size_t kPrefixScaffoldBytes = kMeasuredPrefixBytes - kShippedDoctrineBytes;

// The cache arithmetic, appended to both failure messages. Written out rather than left to the
// reader because the number that matters (5,118 cached tokens) is NOT the number that changed
// (the byte count), and someone reading a red test at speed will otherwise reach for the wrong one.
const char* kCacheMinimumArithmetic =
    "\n    The prefix is cached on the hosted path and Haiku 4.5 will not cache a block under"
    "\n    4,096 tokens. What caches is 5,118 tokens - the prefix text (8,750 B / 3.955 B per"
    "\n    token = ~2,212) PLUS the structural schema copy the adapter sends in"
    "\n    output_config.format.schema (PRD Corrections item 22). The margin over the minimum is"
    "\n    1,022 tokens (24.9 percent), i.e. about 4,042 bytes of prefix. Below it the cache"
    "\n    silently stops forming and the cost per order goes from $0.001220 to ~$0.005829 - no"
    "\n    error, no counter, nothing red. If you MEANT to change the prefix, update the constants"
    "\n    in this file and say so in the PRD; the cost rows in Cost model are computed from them.";

[[nodiscard]] bool readShippedDoctrine(std::string& out, std::string& whereFrom) {
    const std::string& repo = arkheon::aicommander::testing::repoRoot();
    if (repo.empty()) {
        return false;
    }
    whereFrom = repo + "\\data\\doctrine.txt";
    std::ifstream stream(whereFrom, std::ios::binary);
    if (!stream) {
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

// Every string field in the snapshot gets a unique, searchable sentinel, so the allowlist test can
// assert exactly which ones reached the rendered bytes.
OrderSnapshot sentinelSnapshot() {
    OrderSnapshot snapshot;
    snapshot.entityId = "SENTINEL_ENTITY_ID";
    snapshot.team = "SENTINEL_TEAM";
    snapshot.situationNote = "SENTINEL_NOTE";
    snapshot.simTimeS = 412.5;
    snapshot.latitudeDeg = 13.5;
    snapshot.longitudeDeg = 144.8;
    snapshot.altitudeHaeM = 9000.0;
    snapshot.headingDeg = 270.0;
    snapshot.velNMps = -10.0;
    snapshot.velEMps = 210.0;
    snapshot.velDMps = 1.0;
    snapshot.tracks.push_back(TrackReport{"SENTINEL_TARGET_ID", 42000.0, 18.5});
    snapshot.loadout.push_back(LoadoutReport{"SENTINEL_HARDPOINT", "SENTINEL_WEAPON_PROFILE", 2, 4});
    return snapshot;
}

PromptRenderer builtRenderer() {
    PromptRenderer renderer;
    renderer.build(CommanderConfig{}, "Generic tactical doctrine for the test.");
    return renderer;
}

} // namespace

// UAC-AIC-BE-3: the prefix must be byte-identical across every render in a run, or both the local
// KV cache and the hosted prompt cache are forfeited - the single largest latency lever there is.
AIC_TEST(PromptPrefixIsByteStableAcrossRenders) {
    const PromptRenderer renderer = builtRenderer();
    const std::string reference = renderer.prefix();
    // The prefix's SIZE is pinned by PromptPrefixSizeIsPinnedToTheMeasuredArtifact below. What this
    // one needs is only that a prefix was built at all, so a renderer that silently produced nothing
    // cannot pass 100 byte-equality comparisons against its own emptiness.
    AIC_EXPECT_TRUE(!reference.empty(), "build() produced no prefix at all");

    // 100 renders from 100 different snapshots, per the acceptance criterion.
    for (int i = 0; i < 100; ++i) {
        OrderSnapshot snapshot = sentinelSnapshot();
        snapshot.entityId = "Entity_" + std::to_string(i);
        snapshot.simTimeS = 100.0 + i * 3.7;
        snapshot.latitudeDeg = 13.0 + i * 0.01;
        snapshot.tracks.push_back(TrackReport{"Track_" + std::to_string(i), 1000.0 * i, i * 0.5});

        const std::string full = renderer.render(snapshot);
        AIC_EXPECT_TRUE(full.compare(0, reference.size(), reference) == 0,
                        "render #" + std::to_string(i) + " did not begin with the identical prefix");
        AIC_EXPECT_EQ(renderer.prefix(), reference,
                      "the prefix itself changed at render #" + std::to_string(i));
    }
    return true;
}

// UAC-AIC-BE-3, the cache-minimum guard's build-time half. Pins the prefix that SHIPS - built from
// the repository's own data/doctrine.txt - to the 8,750 bytes that measured 120/120 in C3's arm B.
//
// This is the half of the guard that can exist without a network. The runtime half - comparing
// `cache_read_input_tokens` against the model's minimum on a live response - is NOT implemented and
// is not implementable without widening LlmResult, which carries tokensIn and tokensOut and no
// cache field at all; §Corrections item 33 records both facts and carries the runtime half as C9.
// Do not read this test as the whole of UAC-AIC-BE-3. It is the deliberate half.
AIC_TEST(PromptPrefixSizeIsPinnedToTheMeasuredArtifact) {
    std::string doctrine;
    std::string path;
    if (!readShippedDoctrine(doctrine, path)) {
        // FAILURE, not a skip. This test exists because the failure mode it guards is invisible;
        // a version of it that turns itself off when it cannot find its input has the same defect.
        AIC_FAIL("could not read the shipped doctrine (repo root resolved to '"
                 << arkheon::aicommander::testing::repoRoot() << "', looked for '" << path
                 << "'). The cache-minimum guard cannot run, which is a failure and not a pass.");
    }

    AIC_EXPECT_EQ(doctrine.size(), kShippedDoctrineBytes,
                  "data/doctrine.txt changed size, so the shipped prefix is no longer the artifact "
                  "that was measured."
                      << kCacheMinimumArithmetic);

    PromptRenderer renderer;
    renderer.build(CommanderConfig{}, doctrine);
    AIC_EXPECT_EQ(renderer.prefix().size(), kMeasuredPrefixBytes,
                  "the shipped prompt prefix is no longer "
                      << kMeasuredPrefixBytes << " bytes." << kCacheMinimumArithmetic);
    return true;
}

// The same pin, expressed so that it fails on a C++ edit even when the doctrine file is untouched -
// and so that it still fails on a machine where the doctrine file cannot be found at all.
//
// build() emits scaffold + doctrine and nothing else, so `prefix().size() - doctrine.size()` is a
// constant of the source and holds for ANY doctrine. That constant is what changes when someone
// edits kSystemPrompt, edits the posture vocabulary, or deletes one of the newlines joining them -
// including the bare '\n' at PromptRenderer.cpp:95, whose removal takes the scaffold from 1,818 to
// 1,817 and is exactly the one-byte divergence §Corrections item 31(f) records catching by hand.
// That edit is the reason this project's prefix arithmetic is worth a test at all.
AIC_TEST(PromptPrefixScaffoldIsPinnedIndependentlyOfTheDoctrine) {
    // Several sizes, because a scaffold that is constant across all of them is a scaffold and not a
    // coincidence at one length.
    const std::size_t doctrineSizes[] = {std::size_t{1}, std::size_t{100}, kShippedDoctrineBytes,
                                         std::size_t{20000}};
    for (const std::size_t doctrineSize : doctrineSizes) {
        const std::string doctrine(doctrineSize, 'D');
        PromptRenderer renderer;
        renderer.build(CommanderConfig{}, doctrine);
        AIC_EXPECT_EQ(renderer.prefix().size() - doctrineSize, kPrefixScaffoldBytes,
                      "the prefix scaffold (everything build() emits that is not the doctrine text) "
                      "changed at doctrine size "
                          << doctrineSize
                          << ". A newline, a word of the system prompt, or a line of the posture "
                             "vocabulary moved."
                          << kCacheMinimumArithmetic);
    }

    // The absent-doctrine case is real and deliberately does NOT follow the identity above: an empty
    // doctrine renders "(none provided)" rather than nothing, because a prefix that silently loses
    // its doctrine degrades order quality with no counter to show it (AiCommanderPlugin.cpp warns
    // for the same reason). Pinned here so the substitution cannot be dropped as dead code.
    {
        PromptRenderer renderer;
        renderer.build(CommanderConfig{}, "");
        AIC_EXPECT_EQ(renderer.prefix().size(),
                      kPrefixScaffoldBytes + std::string("(none provided)").size(),
                      "an absent doctrine must render the '(none provided)' placeholder"
                          << kCacheMinimumArithmetic);
    }

    // And the arithmetic stated as the one number a reader will look for: scaffold plus the shipped
    // doctrine is the measured artifact. Asserted rather than left implicit, so the two constants
    // cannot be updated independently into a pair that no longer adds up.
    AIC_EXPECT_EQ(kPrefixScaffoldBytes + kShippedDoctrineBytes, kMeasuredPrefixBytes,
                  "the pinned constants in this file no longer add up");
    return true;
}

// The prefix must carry nothing that varies per request, or it is not cacheable no matter how
// stable the surrounding code is.
AIC_TEST(PromptPrefixCarriesNoVolatileState) {
    const PromptRenderer renderer = builtRenderer();
    const std::string& prefix = renderer.prefix();

    const char* forbidden[] = {
        "SENTINEL_ENTITY_ID", "SENTINEL_TEAM", "SENTINEL_TARGET_ID", "SENTINEL_NOTE",
        "SENTINEL_HARDPOINT", "SENTINEL_WEAPON_PROFILE",
        "simTimeS", "412.5", "13.5", "144.8",
    };
    for (const char* needle : forbidden) {
        AIC_EXPECT_TRUE(prefix.find(needle) == std::string::npos,
                        std::string("the prefix must not contain '") + needle
                            + "' - it would make the prefix volatile");
    }
    return true;
}

// UAC-AIC-SEC-2, the transmitted-field allowlist. Renders from a snapshot whose every string field
// carries a unique sentinel, then asserts exactly which sentinels appear.
AIC_TEST(PromptTransmitsOnlyAllowlistedFields) {
    const PromptRenderer renderer = builtRenderer();
    const std::string suffix = renderer.renderSuffix(sentinelSnapshot());

    // Allowlisted - these MUST appear.
    const char* permitted[] = {
        "SENTINEL_ENTITY_ID", "SENTINEL_TEAM", "SENTINEL_TARGET_ID",
        "SENTINEL_HARDPOINT", "SENTINEL_WEAPON_PROFILE", "SENTINEL_NOTE",
    };
    for (const char* needle : permitted) {
        AIC_EXPECT_TRUE(suffix.find(needle) != std::string::npos,
                        std::string("allowlisted field '") + needle + "' is missing from the suffix");
    }

    // componentTrackIdentity free text. These are documented as ingested from an external feed
    // (live ADS-B), making them attacker-influenced, and the threat model excludes them from the
    // prompt entirely. The snapshot has no field to carry them at all - this asserts the property
    // survives anyone later adding one.
    const char* excluded[] = {
        "trackSource", "callsign", "originCountry",
        // Track attributes dropped in PRD v1.2: the ingress verb does not carry them and the
        // plugin will not infer them.
        "\"team\":\"blue\"", "\"kind\"", "\"domain\"",
        // Config values beyond model name and cadence.
        "apiKeyEnvVar", "ANTHROPIC_API_KEY", "baseUrl", "localhost", "api.anthropic.com",
        "geofenceRadiusM", "maxSpeedMps", "replay.path", "doctrinePath",
        // Paths and tree contents.
        "C:\\", "data/ai", "userPlugins", ".gguf",
    };
    for (const char* needle : excluded) {
        AIC_EXPECT_TRUE(suffix.find(needle) == std::string::npos,
                        std::string("field '") + needle + "' must NOT appear in the prompt suffix");
    }
    return true;
}

// The track row is scalars only. If anyone later widens it, this fails before the widened field
// can reach a third-party API.
AIC_TEST(PromptTrackRowsCarryOnlyThreeScalars) {
    const PromptRenderer renderer = builtRenderer();
    OrderSnapshot snapshot = sentinelSnapshot();
    snapshot.tracks.clear();
    snapshot.tracks.push_back(TrackReport{"Bandit_01", 42000.0, 18.5});

    const std::string suffix = renderer.renderSuffix(snapshot);
    AIC_EXPECT_TRUE(suffix.find("targetEntityId") != std::string::npos, "targetEntityId present");
    AIC_EXPECT_TRUE(suffix.find("rangeM") != std::string::npos, "rangeM present");
    AIC_EXPECT_TRUE(suffix.find("snrDb") != std::string::npos, "snrDb present");

    // The api key value must never appear anywhere, under any circumstances (ADR-5).
    AIC_EXPECT_TRUE(suffix.find("sk-ant-") == std::string::npos, "no credential shape in the prompt");
    return true;
}

// snapshotHash must be a function of the PICTURE, not of the order Lua happened to report in.
// Otherwise prompt-drift detection and replay comparison both become noise.
AIC_TEST(PromptHashIsIndependentOfReportOrder) {
    const PromptRenderer renderer = builtRenderer();

    OrderSnapshot a = sentinelSnapshot();
    a.tracks = {{"Charlie", 3000.0, 3.0}, {"Alpha", 1000.0, 1.0}, {"Bravo", 2000.0, 2.0}};
    a.loadout = {{"WingRight", "R77", 2, 4}, {"WingLeft", "R73", 1, 2}};

    OrderSnapshot b = sentinelSnapshot();
    b.tracks = {{"Bravo", 2000.0, 2.0}, {"Charlie", 3000.0, 3.0}, {"Alpha", 1000.0, 1.0}};
    b.loadout = {{"WingLeft", "R73", 1, 2}, {"WingRight", "R77", 2, 4}};

    canonicalizeSnapshot(a);
    canonicalizeSnapshot(b);

    const std::string suffixA = renderer.renderSuffix(a);
    const std::string suffixB = renderer.renderSuffix(b);
    AIC_EXPECT_EQ(suffixA, suffixB,
                  "two snapshots holding the same picture must render byte-identically");
    AIC_EXPECT_EQ(PromptRenderer::stableHash(suffixA), PromptRenderer::stableHash(suffixB),
                  "and therefore hash identically");

    // And a genuinely different picture must hash differently, or the hash detects nothing.
    OrderSnapshot c = a;
    c.tracks.push_back(TrackReport{"Delta", 4000.0, 4.0});
    AIC_EXPECT_TRUE(PromptRenderer::stableHash(renderer.renderSuffix(c))
                        != PromptRenderer::stableHash(suffixA),
                    "an added track must change the hash");
    return true;
}

// The doctrine block is read once at build time. A change mid-run must not take effect, because it
// would invalidate the prefix cache silently - AIC-BE-3 makes cache invalidation a deliberate act.
AIC_TEST(PromptPrefixChangesOnlyWhenRebuilt) {
    PromptRenderer renderer;
    renderer.build(CommanderConfig{}, "Doctrine A.");
    const std::string first = renderer.prefix();
    AIC_EXPECT_TRUE(first.find("Doctrine A.") != std::string::npos, "doctrine A is embedded");

    // Rendering many times does not disturb it.
    for (int i = 0; i < 10; ++i) {
        (void)renderer.render(sentinelSnapshot());
    }
    AIC_EXPECT_EQ(renderer.prefix(), first, "rendering must not mutate the prefix");

    // An explicit rebuild does change it - deliberately, invalidating the cache exactly once.
    renderer.build(CommanderConfig{}, "Doctrine B.");
    AIC_EXPECT_TRUE(renderer.prefix() != first, "an explicit rebuild changes the prefix");
    AIC_EXPECT_TRUE(renderer.prefix().find("Doctrine B.") != std::string::npos, "doctrine B embedded");
    return true;
}
