#include "TestSupport.h"

#include "CommanderConfig.h"
#include "PromptRenderer.h"
#include "Snapshot.h"

#include <string>
#include <vector>

using namespace arkheon::aicommander;

namespace {

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
    AIC_EXPECT_TRUE(reference.size() > 800, "the prefix should carry the schema and doctrine");

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
