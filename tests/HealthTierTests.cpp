#include "TestSupport.h"

#include "HealthTier.h"

#include <optional>
#include <string>

using namespace arkheon::aicommander;

namespace {

// A reader pair that resolves neither column - the shape of a schema rename, and the shape of an
// entity that carries no componentLifecycle at all.
const HealthOrdinalReader kNoOrdinal = [](const std::string&) -> std::optional<std::int64_t> {
    return std::nullopt;
};
const HealthNameReader kNoName = [](const std::string&) -> std::optional<std::string> {
    return std::nullopt;
};

HealthOrdinalReader ordinalOf(std::int64_t value) {
    return [value](const std::string&) -> std::optional<std::int64_t> { return value; };
}

HealthNameReader nameOf(std::string value) {
    return [value](const std::string&) -> std::optional<std::string> { return value; };
}

} // namespace

// The ordinal IS the tier, and the mapping is the schema's declaration order rather than ours. A
// transposition here would silently reclassify a healthy aircraft as a wreck, or the reverse.
AIC_TEST(HealthTierOrdinalsMatchTheSchemaDeclarationOrder) {
    const std::pair<std::int64_t, const char*> expected[] = {
        {0, "nominal"}, {1, "degraded"}, {2, "disabled"}, {3, "wrecked"}, {4, "destroyed"}};

    for (const auto& [ordinal, name] : expected) {
        const std::optional<HealthTier> tier =
            readHealthTierWith("RedSu35_01", ordinalOf(ordinal), kNoName);
        AIC_EXPECT_TRUE(tier.has_value(),
                        std::string("ordinal ") + std::to_string(ordinal) + " must resolve");
        AIC_EXPECT_EQ(std::string(toString(*tier)), std::string(name),
                      "the schema's spelling for ordinal " + std::to_string(ordinal));
    }
    return true;
}

// The boundary the guard turns on, asserted on every tier rather than at the edge only. `disabled`
// is a mission kill and MUST stay commandable: BlueF16_01 was disabled at t+64 s of the 2026-08-12
// run and went on flying and fighting.
AIC_TEST(HealthTierUncommandableStartsAtWreckedAndNotBefore) {
    AIC_EXPECT_FALSE(isUncommandable(HealthTier::Nominal), "nominal is commandable");
    AIC_EXPECT_FALSE(isUncommandable(HealthTier::Degraded), "degraded is commandable");
    AIC_EXPECT_FALSE(isUncommandable(HealthTier::Disabled),
                     "disabled is a MISSION kill, not a lost airframe - it must stay commandable");
    AIC_EXPECT_TRUE(isUncommandable(HealthTier::Wrecked), "wrecked is uncommandable");
    AIC_EXPECT_TRUE(isUncommandable(HealthTier::Destroyed), "destroyed is uncommandable");
    return true;
}

// The muted-guard branch, and the reason this unit has a seam at all. `componentLifecycle/health` is
// an ENUM leaf and nothing in ComponentFieldAccess.h or the schema reference specifies how an enum
// column is stored. If only the int accessor were tried and the column is text, the guard would
// return nullopt for the life of the process - installed, green, and never firing (AIC-BE-3,
// §Corrections item 35(b)). Both orders of resolution must produce the same tier.
AIC_TEST(HealthTierResolvesFromTheTextColumnWhenTheOrdinalColumnDoesNot) {
    const std::optional<HealthTier> fromName =
        readHealthTierWith("RedSu35_01", kNoOrdinal, nameOf("wrecked"));
    AIC_EXPECT_TRUE(fromName.has_value(), "a text-typed enum column must still resolve");
    AIC_EXPECT_TRUE(*fromName == HealthTier::Wrecked, "text 'wrecked' must be the wrecked tier");
    AIC_EXPECT_TRUE(isUncommandable(*fromName), "and it must trip the guard");

    const std::optional<HealthTier> fromOrdinal =
        readHealthTierWith("RedSu35_01", ordinalOf(3), kNoName);
    AIC_EXPECT_TRUE(fromOrdinal.has_value() && *fromOrdinal == *fromName,
                    "both columns must agree on the tier");
    return true;
}

// The TEXT column wins when both resolve, and this test was inverted deliberately on 2026-08-13.
//
// It previously asserted the opposite and said "a later refactor that flips it should fail here",
// which is exactly what it did — so the flip is recorded rather than slipped through. The reason is
// measured, not aesthetic: on the validated release the enum is a text column, and trying the int
// accessor first makes the SDK log `DynamicStore::getInt: handle is not an int field` at ERROR on
// every single read before the fallback succeeds. Which column is authoritative is still a decision;
// it is now the one that does not spend an ERROR line per cadence tick to reach the same answer.
AIC_TEST(HealthTierPrefersTheTextColumnWhenBothResolve) {
    int ordinalReads = 0;
    const HealthOrdinalReader countingOrdinal =
        [&ordinalReads](const std::string&) -> std::optional<std::int64_t> {
            ++ordinalReads;
            return 0;   // "nominal" - deliberately disagreeing with the name below
        };

    const std::optional<HealthTier> tier =
        readHealthTierWith("RedSu35_01", countingOrdinal, nameOf("destroyed"));
    AIC_EXPECT_TRUE(tier.has_value() && *tier == HealthTier::Destroyed,
                    "the text column is authoritative");
    AIC_EXPECT_EQ(ordinalReads, 0, "the int column must not be read once the text column resolved");
    return true;
}

// The fail-open direction, and it is the one that matters most. An unreadable tier must be nullopt
// so the caller treats the aircraft as commandable. A guard that grounds every aircraft on a tree
// where this leaf does not resolve would be a far worse failure than the one it prevents - it would
// silently disable the whole commander on an SDK the plugin was not built against.
AIC_TEST(HealthTierIsUnreadableRatherThanWreckedWhenNeitherColumnResolves) {
    AIC_EXPECT_FALSE(readHealthTierWith("RedSu35_01", kNoOrdinal, kNoName).has_value(),
                     "an unresolvable health leaf must be nullopt, never a tier");
    AIC_EXPECT_FALSE(readHealthTierWith("RedSu35_01", nullptr, nullptr).has_value(),
                     "absent seams must be nullopt rather than a crash");
    AIC_EXPECT_FALSE(readHealthTierWith("", ordinalOf(4), nameOf("destroyed")).has_value(),
                     "no entity means no tier");
    return true;
}

// A value outside the schema's declared set is UNKNOWN, and specifically not read as wrecked. A tier
// this code cannot interpret must never be the reason an intact aircraft goes uncommanded - which is
// the failure mode of clamping an out-of-range ordinal to the top of the enum.
AIC_TEST(HealthTierRejectsValuesOutsideTheSchemasDeclaredSet) {
    for (const std::int64_t rogue : {-1, 5, 99, 4096}) {
        AIC_EXPECT_FALSE(readHealthTierWith("RedSu35_01", ordinalOf(rogue), kNoName).has_value(),
                         "ordinal " + std::to_string(rogue) + " is not a declared tier");
    }
    for (const char* rogue : {"", "WRECKED", "unknown", "obliterated", "nominal "}) {
        AIC_EXPECT_FALSE(readHealthTierWith("RedSu35_01", kNoOrdinal, nameOf(rogue)).has_value(),
                         std::string("name '") + rogue + "' is not a declared tier");
    }
    return true;
}

// An undeclared NAME must not fall through to the ordinal column and resolve there. The text column
// resolving at all means it exists and this build cannot interpret its value; consulting the other
// column would report a tier on a value the schema does not declare. This is the mirror of the rule
// that used to guard the ordinal path, and it moved with the precedence.
AIC_TEST(HealthTierDoesNotFallBackToTheOrdinalAfterAnUndeclaredName) {
    int ordinalReads = 0;
    const HealthOrdinalReader countingOrdinal =
        [&ordinalReads](const std::string&) -> std::optional<std::int64_t> {
            ++ordinalReads;
            return 0;
        };

    AIC_EXPECT_FALSE(readHealthTierWith("RedSu35_01", countingOrdinal, nameOf("obliterated")).has_value(),
                     "an undeclared name is unknown, not an ordinal lookup");
    AIC_EXPECT_EQ(ordinalReads, 0, "the int column must not be consulted after an undeclared name");
    return true;
}

// And the surviving half of the old rule: an out-of-range ORDINAL is still unknown rather than a
// tier, on the path where the text column did not resolve at all.
AIC_TEST(HealthTierRejectsAnOutOfRangeOrdinalWhenTextDidNotResolve) {
    AIC_EXPECT_FALSE(readHealthTierWith("RedSu35_01", ordinalOf(7), kNoName).has_value(),
                     "an out-of-range ordinal is unknown, whichever column reached it");
    return true;
}

// toString must never read off the end of the name table, whatever a cast produces.
AIC_TEST(HealthTierToStringIsTotal) {
    AIC_EXPECT_EQ(std::string(toString(static_cast<HealthTier>(99))), std::string("unknown"),
                  "an undeclared tier stringifies as unknown rather than reading out of bounds");
    return true;
}
