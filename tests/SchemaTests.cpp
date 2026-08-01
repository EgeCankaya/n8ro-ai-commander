#include "TestSupport.h"

#include "EnvVar.h"
#include "Order.h"
#include "OrderSchema.h"

#include <core/json/JsonValue.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using arkheon::aicommander::Posture;
using arkheon::aicommander::Roe;
using arkheon::aicommander::orderJsonSchema;
using arkheon::aicommander::orderJsonSchemaText;
using arkheon::aicommander::postureRequiresTarget;
using arkheon::aicommander::postureRequiresWaypoint;
using arkheon::aicommander::toString;
using arkheon::aicommander::tryParsePosture;
using arkheon::aicommander::tryParseRoe;
using n8ro::core::JsonValue;

namespace {

// The schema reference is a release-tree artifact and is deliberately NOT committed to this
// repository (it is proprietary SDK surface). The test resolves it through N8RO_RELEASE, which is
// the same variable the build uses.
[[nodiscard]] std::string schemaReferencePath() {
    const std::string& release = arkheon::aicommander::testing::releaseRoot();
    if (release.empty()) {
        return {};
    }
    return release + "\\dev\\ai-coding\\schema-reference\\schema-reference.json";
}

[[nodiscard]] bool readFile(const std::string& path, std::string& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

// Finds a record by its `path` key and returns its `unit`.
[[nodiscard]] bool findUnit(const JsonValue& records, const std::string& path, std::string& unit) {
    for (std::size_t i = 0; i < records.size(); ++i) {
        const JsonValue record = records.at(i);
        if (record.get("path").asString() == path) {
            unit = record.get("unit").asString();
            return true;
        }
    }
    return false;
}

} // namespace

// The test that keeps "derive, don't guess" true over time (AIC-ORD-1, Appendix A).
//
// It re-reads schema-reference.json - the file that actually owns units - and asserts every
// quantity this plugin's order schema carries still traces to the record it claims. If the SDK
// ever renames a path or changes a unit, this fails rather than the plugin quietly commanding
// altitudes in the wrong datum.
AIC_TEST(SchemaUnitsTraceToTheSchemaReference) {
    const std::string path = schemaReferencePath();
    if (path.empty()) {
        AIC_FAIL("N8RO_RELEASE is not set; cannot locate schema-reference.json. "
                 "Run this suite through setup.cmd.");
    }

    std::string text;
    if (!readFile(path, text)) {
        AIC_FAIL("could not read " + path);
    }

    const std::optional<JsonValue> parsed = JsonValue::parse(text);
    if (!parsed.has_value()) {
        AIC_FAIL("schema-reference.json did not parse as JSON");
    }
    AIC_EXPECT_TRUE(parsed->isArray(), "schema-reference.json must be a flat JSON array");

    // Every quantity the order schema carries, with the record it derives from. These pairs are
    // the contract; the assertion is that the file still agrees with them.
    struct Trace {
        const char* quantity;
        const char* schemaPath;
        const char* expectedUnit;
    };
    const Trace traces[] = {
        {"waypoint.latitudeDeg",  "/datablocks/positionGeodetic/latitudeDeg",  "Deg"},
        {"waypoint.longitudeDeg", "/datablocks/positionGeodetic/longitudeDeg", "Deg"},
        {"waypoint.altitudeHaeM", "/datablocks/positionGeodetic/altitudeHaeM", "M"},
        {"cruiseSpeedMps",        "/datablocks/waypoint/speed",                "Mps"},
        {"cruiseSpeedMps (alt)",  "/datablocks/componentTransform/speedMps",   "Mps"},
        {"own.headingDeg",        "/datablocks/componentTransform/headingDeg", "Deg"},
        {"orbitRadiusM",
         "/datablocks/componentNavigation/onWaypointReachedLoiterRadiusM",     "M"},
        {"sensor detection range", "/datablocks/componentSensor/detectionRangeM", "M"},
        {"mission tick period",   "/datablocks/componentMission/updateIntervalS", "S"},
    };

    for (const Trace& trace : traces) {
        std::string unit;
        if (!findUnit(*parsed, trace.schemaPath, unit)) {
            AIC_FAIL(std::string("schema-reference.json no longer contains '") + trace.schemaPath
                     + "' (cited for " + trace.quantity + "). The order schema's unit for that "
                       "quantity can no longer be derived - reconcile before shipping.");
        }
        AIC_EXPECT_EQ(unit, std::string(trace.expectedUnit),
                      std::string("unit drift for ") + trace.quantity + " at " + trace.schemaPath);
    }

    return true;
}

// The line that makes "the model is structurally forbidden to emit kinematics" a property of the
// contract rather than a claim about the prompt.
AIC_TEST(SchemaForbidsAdditionalProperties) {
    const JsonValue& schema = orderJsonSchema();
    AIC_EXPECT_TRUE(schema.has("additionalProperties"),
                    "the order schema must declare additionalProperties");
    AIC_EXPECT_FALSE(schema.get("additionalProperties").asBool(),
                     "additionalProperties must be false - an unknown property is a rejection, "
                     "not something quietly ignored");

    // And the same on the nested waypoint object, or a kinematic field could hide one level down.
    const JsonValue waypoint = schema.get("properties").get("waypoint");
    AIC_EXPECT_TRUE(waypoint.has("additionalProperties"),
                    "the waypoint object must declare additionalProperties");
    AIC_EXPECT_FALSE(waypoint.get("additionalProperties").asBool(),
                     "waypoint.additionalProperties must be false");
    return true;
}

// The negative space of the schema is the requirement. Enumerate the fields the model must not be
// able to express, and assert none of them is a property at either level.
AIC_TEST(SchemaHasNoKinematicProperties) {
    const JsonValue properties = orderJsonSchema().get("properties");
    const JsonValue waypointProperties = properties.get("waypoint").get("properties");

    const char* forbidden[] = {
        "headingDeg", "heading", "pitchDeg", "pitch", "rollDeg", "roll",
        "velN", "velE", "velD", "velocity", "velocityNed",
        "accelerationNed", "acceleration", "turnRateDegS", "loadFactorG",
        "hardpointName", "hardpoint", "fire", "requestFire", "weaponProfileName",
    };

    for (const char* name : forbidden) {
        AIC_EXPECT_FALSE(properties.has(name),
                         std::string("the order schema must not expose a '") + name
                             + "' property - kinematics belong to Tier 0/1");
        AIC_EXPECT_FALSE(waypointProperties.has(name),
                         std::string("waypoint must not expose a '") + name + "' property");
    }
    return true;
}

// AIC-BE-3 requires the prompt's stable prefix be byte-identical for the life of the run, and the
// schema document is embedded in that prefix. If rendering it twice produced different bytes -
// through map ordering, say - the prefix could never be stable.
AIC_TEST(SchemaTextIsByteStable) {
    const std::string first = orderJsonSchemaText();
    const std::string second = orderJsonSchemaText();
    AIC_EXPECT_EQ(first, second, "the schema text must be byte-identical across calls");
    AIC_EXPECT_TRUE(first.size() > 500,
                    "the schema text looks implausibly short; it should carry every field with "
                    "its description");
    return true;
}

// Round-trip the two enums. A posture that stringifies to something tryParsePosture cannot read
// back would break the order log and replay, which both go through the string form.
AIC_TEST(PostureAndRoeRoundTrip) {
    const Posture postures[] = {Posture::Ingress, Posture::Engage, Posture::Crank,
                                Posture::Defend,  Posture::Hold,   Posture::Rtb};
    for (const Posture posture : postures) {
        Posture parsed{};
        AIC_EXPECT_TRUE(tryParsePosture(toString(posture), parsed),
                        std::string("posture '") + toString(posture) + "' must parse back");
        AIC_EXPECT_TRUE(parsed == posture,
                        std::string("posture '") + toString(posture) + "' round-trip mismatch");
    }

    const Roe roes[] = {Roe::WeaponsFree, Roe::WeaponsTight, Roe::WeaponsHold};
    for (const Roe roe : roes) {
        Roe parsed{};
        AIC_EXPECT_TRUE(tryParseRoe(toString(roe), parsed),
                        std::string("roe '") + toString(roe) + "' must parse back");
        AIC_EXPECT_TRUE(parsed == roe, std::string("roe '") + toString(roe) + "' round-trip mismatch");
    }

    Posture unused{};
    AIC_EXPECT_FALSE(tryParsePosture("attack", unused), "'attack' is not a posture");
    AIC_EXPECT_FALSE(tryParsePosture("Ingress", unused), "posture parsing is case-sensitive");
    AIC_EXPECT_FALSE(tryParsePosture("", unused), "the empty string is not a posture");

    Roe unusedRoe{};
    AIC_EXPECT_FALSE(tryParseRoe("weaponsfree", unusedRoe), "roe parsing is case-sensitive");
    return true;
}

// The schema's enum lists and the C++ enums must agree, or the validator would accept a posture
// the code cannot represent (or reject one it can).
AIC_TEST(SchemaEnumsMatchTheCppEnums) {
    const JsonValue postureEnum = orderJsonSchema().get("properties").get("posture").get("enum");
    AIC_EXPECT_EQ(postureEnum.size(), static_cast<std::size_t>(6), "posture enum size");
    for (std::size_t i = 0; i < postureEnum.size(); ++i) {
        Posture parsed{};
        const std::string value = postureEnum.at(i).asString();
        AIC_EXPECT_TRUE(tryParsePosture(value, parsed),
                        "schema posture '" + value + "' has no C++ counterpart");
    }

    const JsonValue roeEnum = orderJsonSchema().get("properties").get("roe").get("enum");
    AIC_EXPECT_EQ(roeEnum.size(), static_cast<std::size_t>(3), "roe enum size");
    for (std::size_t i = 0; i < roeEnum.size(); ++i) {
        Roe parsed{};
        const std::string value = roeEnum.at(i).asString();
        AIC_EXPECT_TRUE(tryParseRoe(value, parsed), "schema roe '" + value + "' has no C++ counterpart");
    }
    return true;
}

// The conditional-presence rules are the part of the contract most likely to drift, because they
// live in code rather than in the JSON Schema (draft-07 cannot express "required when posture is
// one of...").
AIC_TEST(ConditionalPresenceRulesMatchTheContract) {
    AIC_EXPECT_TRUE(postureRequiresTarget(Posture::Engage), "engage requires a target");
    AIC_EXPECT_TRUE(postureRequiresTarget(Posture::Crank), "crank requires a target");
    AIC_EXPECT_FALSE(postureRequiresTarget(Posture::Defend),
                     "defend must NOT require a target - the script picks the munition track "
                     "itself; the model is not asked to name a missile");
    AIC_EXPECT_FALSE(postureRequiresTarget(Posture::Ingress), "ingress takes no target");
    AIC_EXPECT_FALSE(postureRequiresTarget(Posture::Hold), "hold takes no target");
    AIC_EXPECT_FALSE(postureRequiresTarget(Posture::Rtb), "rtb takes no target");

    AIC_EXPECT_TRUE(postureRequiresWaypoint(Posture::Ingress), "ingress carries a waypoint");
    AIC_EXPECT_TRUE(postureRequiresWaypoint(Posture::Hold), "hold carries a waypoint");
    AIC_EXPECT_TRUE(postureRequiresWaypoint(Posture::Rtb), "rtb carries a waypoint");
    AIC_EXPECT_FALSE(postureRequiresWaypoint(Posture::Engage),
                     "engage must NOT carry a waypoint - the script computes pursuit geometry");
    AIC_EXPECT_FALSE(postureRequiresWaypoint(Posture::Crank),
                     "crank must NOT carry a waypoint - the script computes the offset steer point");
    AIC_EXPECT_FALSE(postureRequiresWaypoint(Posture::Defend),
                     "defend must NOT carry a waypoint - the script computes the pump");

    // No posture both requires a target and carries a waypoint. If one ever did, AIC-ORD-2's
    // mapping table would have two sources of truth for where the entity is going.
    const Posture all[] = {Posture::Ingress, Posture::Engage, Posture::Crank,
                           Posture::Defend,  Posture::Hold,   Posture::Rtb};
    for (const Posture posture : all) {
        AIC_EXPECT_FALSE(postureRequiresTarget(posture) && postureRequiresWaypoint(posture),
                         std::string("posture '") + toString(posture)
                             + "' both requires a target and carries a waypoint");
    }
    return true;
}
