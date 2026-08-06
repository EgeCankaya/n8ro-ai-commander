#include "OrderSchema.h"

#include "Order.h"

#include <string>
#include <vector>

namespace arkheon::aicommander {

namespace {

using n8ro::core::JsonValue;

JsonValue makeNumber(double minimum, double maximum, const std::string& description) {
    JsonValue node = JsonValue::object();
    (void)node.setString("type", "number");
    (void)node.setDouble("minimum", minimum);
    (void)node.setDouble("maximum", maximum);
    (void)node.setString("description", description);
    return node;
}

JsonValue makeString(std::size_t minChars, std::size_t maxChars, const std::string& description) {
    JsonValue node = JsonValue::object();
    (void)node.setString("type", "string");
    (void)node.setInt64("minLength", static_cast<std::int64_t>(minChars));
    (void)node.setInt64("maxLength", static_cast<std::int64_t>(maxChars));
    (void)node.setString("description", description);
    return node;
}

JsonValue makeEnum(const std::vector<const char*>& values, const std::string& description) {
    JsonValue node = JsonValue::object();
    (void)node.setString("type", "string");
    JsonValue allowed = JsonValue::array();
    for (const char* value : values) {
        (void)allowed.pushBack(JsonValue::fromString(value));
    }
    (void)node.set("enum", allowed);
    (void)node.setString("description", description);
    return node;
}

JsonValue makeStringArray(const std::vector<const char*>& values) {
    JsonValue array = JsonValue::array();
    for (const char* value : values) {
        (void)array.pushBack(JsonValue::fromString(value));
    }
    return array;
}

// -- waypoint ----------------------------------------------------------------------------------
// Units and frames are read from dev/ai-coding/schema-reference/schema-reference.json, never from
// memory. The `unit` key on each cited record is the authority, and SchemaUnitTests re-reads that
// file to assert these descriptions have not drifted from it.
JsonValue buildWaypoint() {
    JsonValue props = JsonValue::object();
    (void)props.set("latitudeDeg", makeNumber(kMinLatitudeDeg, kMaxLatitudeDeg,
        "Geodetic WGS-84 latitude in degrees (unit Deg; "
        "/datablocks/positionGeodetic/latitudeDeg)."));
    (void)props.set("longitudeDeg", makeNumber(kMinLongitudeDeg, kMaxLongitudeDeg,
        "Geodetic WGS-84 longitude in degrees (unit Deg; "
        "/datablocks/positionGeodetic/longitudeDeg)."));
    (void)props.set("altitudeHaeM", makeNumber(-1000.0, 100000.0,
        "Height above the WGS-84 ellipsoid in metres - NOT above ground level, NOT mean sea level "
        "(unit M; /datablocks/positionGeodetic/altitudeHaeM)."));

    JsonValue waypoint = JsonValue::object();
    (void)waypoint.setString("type", "object");
    (void)waypoint.set("properties", props);
    (void)waypoint.set("required", makeStringArray({"latitudeDeg", "longitudeDeg", "altitudeHaeM"}));
    (void)waypoint.setBool("additionalProperties", false);
    (void)waypoint.setString("description",
        "Destination for this posture. The aircraft flies to it; it does not interpret it.");
    return waypoint;
}

// The fields both branches carry, described identically in both. Built in one place so the two
// branches cannot end up disagreeing about a field they share.
JsonValue buildSharedProperties() {
    JsonValue properties = JsonValue::object();

    JsonValue version = JsonValue::object();
    (void)version.setString("type", "integer");
    (void)version.setInt64("minimum", kOrderSchemaVersion);
    (void)version.setInt64("maximum", kOrderSchemaVersion);
    (void)version.setString("description", "Order schema version. Must be exactly 1.");
    (void)properties.set("schemaVersion", version);

    (void)properties.set("entityId", makeString(1, kMaxEntityIdChars,
        "The commanded entity's runtime id. Must equal the id in the request."));

    (void)properties.set("cruiseSpeedMps", makeNumber(0.0, kMaxCruiseSpeedMps,
        "Commanded ground speed in metres per second (unit Mps; /datablocks/waypoint/speed). "
        "Must be greater than zero; the upper bound the entity will actually accept is "
        "safety.maxSpeedMps."));

    (void)properties.set("roe", makeEnum(
        {"weaponsFree", "weaponsTight", "weaponsHold"},
        "Rules of engagement. Orthogonal to posture: weaponsHold suppresses fire regardless of "
        "what the entity is flying toward."));

    (void)properties.set("reason", makeString(kMinReasonChars, kMaxReasonChars,
        "One sentence of rationale. Advisory only - it is recorded for humans reading the order "
        "log and is never parsed. An over-long reason is shortened rather than refused, so brevity "
        "costs nothing and length costs nothing but tokens."));

    return properties;
}

// The three things a branch fixes. Every posture in a branch must agree on all three, which is what
// makes the branch expressible to a constrained decoder at all: within a branch, no field's rule is
// conditional on anything.
struct BranchShape {
    bool waypoint = false;     // waypoint required (true) or not declared at all (false)
    bool target = false;       // targetEntityId non-empty (true) or forced empty (false)
    bool orbit = false;        // orbitRadiusM > 0 (true) or forced 0 (false)
};

// One branch of the oneOf.
//
// The split is the whole point. A branch that requires `waypoint` and one that does not declare it
// at all — combined with additionalProperties:false, which turns "not declared" into a prohibition
// rather than a silence — says what a flat document would need if/then/else to say, and the
// decoding path does not honour if/then/else.
//
// The same trick carries the other two rules: `targetEntityId` is bounded to exactly zero
// characters where the posture forbids one, and `orbitRadiusM` to exactly zero where the posture
// forbids an orbit. A decoder told the bound cannot emit outside it.
JsonValue buildBranch(const std::vector<const char*>& postures, BranchShape shape,
                      const std::string& postureDescription, const std::string& branchDescription) {
    JsonValue properties = buildSharedProperties();
    (void)properties.set("posture", makeEnum(postures, postureDescription));

    if (shape.target) {
        (void)properties.set("targetEntityId", makeString(1, kMaxEntityIdChars,
            "The contact to act against, taken from the reported track list. Never invent one."));
    } else {
        (void)properties.set("targetEntityId", makeString(0, 0,
            "Not applicable to these postures - the aircraft selects for itself. Emit the empty "
            "string."));
    }

    if (shape.waypoint) {
        (void)properties.set("waypoint", buildWaypoint());
    }

    if (shape.orbit) {
        (void)properties.set("orbitRadiusM", makeNumber(1.0, kMaxOrbitRadiusM,
            "Orbit radius in metres (unit M; "
            "/datablocks/componentNavigation/onWaypointReachedLoiterRadiusM). Required and greater "
            "than zero for this posture."));
    } else {
        (void)properties.set("orbitRadiusM", makeNumber(0.0, 0.0,
            "Not applicable to these postures. Emit 0."));
    }

    std::vector<const char*> required{
        "schemaVersion", "entityId", "posture", "targetEntityId",
        "cruiseSpeedMps", "orbitRadiusM", "roe", "reason",
    };
    if (shape.waypoint) {
        required.push_back("waypoint");
    }

    JsonValue branch = JsonValue::object();
    (void)branch.setString("type", "object");
    (void)branch.set("properties", properties);
    (void)branch.set("required", makeStringArray(required));
    (void)branch.setBool("additionalProperties", false);
    (void)branch.setString("description", branchDescription);
    return branch;
}

// -- the four branches ---------------------------------------------------------------------------
//
// One per distinct field-constraint profile, which is the partition the A6 rules actually induce.
// An earlier two-branch split grouped `defend` with `engage` and `crank` because none of them
// carries a waypoint — and then measured 2/12 rejections for "defend must not carry a
// targetEntityId", because that grouping is wrong about the OTHER rule. Postures group by their
// whole constraint profile or not at all.

const JsonValue& transitBranch() {
    static const JsonValue branch = buildBranch(
        {"ingress", "rtb"}, BranchShape{true, false, false},
        "Postures that fly to an ordered point without orbiting it. ingress repositions to the "
        "assigned area; rtb egresses.",
        "An order that sends the aircraft to a point. Carries a waypoint, no target, no orbit.");
    return branch;
}

const JsonValue& holdBranch() {
    static const JsonValue branch = buildBranch(
        {"hold"}, BranchShape{true, false, true},
        "Orbit the ordered point at the ordered radius, retaining options rather than committing.",
        "A hold order. Carries a waypoint AND a non-zero orbit radius; the two are meaningless "
        "apart.");
    return branch;
}

const JsonValue& targetedBranch() {
    static const JsonValue branch = buildBranch(
        {"engage", "crank"}, BranchShape{false, true, false},
        "Postures directed at a specific contact. engage pursues it; crank flies an offset while a "
        "shot is supported.",
        "An order against a named contact. Carries a target and no waypoint - the aircraft computes "
        "the geometry itself.");
    return branch;
}

const JsonValue& defendBranch() {
    static const JsonValue branch = buildBranch(
        {"defend"}, BranchShape{false, false, false},
        "Turn cold from the nearest threat and descend. The aircraft picks the threat and the "
        "maneuver.",
        "A defensive order. Carries neither a target nor a waypoint: naming either would be "
        "commanding geometry the aircraft is better placed to choose.");
    return branch;
}

JsonValue buildOrderSchema() {
    JsonValue branches = JsonValue::array();
    (void)branches.pushBack(transitBranch());
    (void)branches.pushBack(holdBranch());
    (void)branches.pushBack(targetedBranch());
    (void)branches.pushBack(defendBranch());

    JsonValue schema = JsonValue::object();
    (void)schema.setString("$schema", "http://json-schema.org/draft-07/schema#");
    (void)schema.setString("title", "N8RO AI Entity Commander Order");
    (void)schema.set("oneOf", branches);
    (void)schema.setString("description",
        "Exactly one order for exactly one entity, matching exactly one of the branches below. "
        "Which branch applies is decided by the posture, and each branch states every field's rule "
        "unconditionally. There is deliberately no property for heading, pitch, roll, velocity, "
        "acceleration, turn rate, load factor, hardpoint selection, or a fire command: those are "
        "the deterministic tiers' job, not the model's.");

    return schema;
}

// -- the hosted projection (AIC-BE-2, PRD v1.8 §Corrections item 19) -----------------------------
//
// Everything below computes the hosted document FROM the canonical one. Nothing below authors a
// schema. If a branch is added above, it appears here without anyone remembering to add it — which
// is the entire reason this is a projection and not a second document.

// The keywords the hosted structured-output path does not accept. Removed wherever they appear, at
// any depth.
constexpr const char* kUnsupportedKeywords[] = {
    "minimum", "maximum", "multipleOf", "minLength", "maxLength",
};

// Turns a PIN into the `const` that says the same thing.
//
// A pin is a bound whose two ends meet: `minimum == maximum` says "exactly this number",
// `minLength == maxLength == 0` says "exactly the empty string". Those three pins are not
// decoration — they are the mechanism the four-branch encoding uses to forbid a field rather than
// merely to bound it, and dropping them without replacement would give back the 10/12 shape
// rejections §Corrections item 13 measured.
//
// A length pin with a NON-zero length (`minLength == maxLength == 8`) pins a length, not a value,
// and has no `const` equivalent. None exists in the canonical document today; if one is ever added
// it is dropped like any other bound and A7 continues to enforce it.
void pinToConst(JsonValue& node) {
    if (node.has("minimum") && node.has("maximum")) {
        const double low = node.get("minimum").asDouble();
        const double high = node.get("maximum").asDouble();
        if (low == high) {
            if (node.get("type").asString() == "integer") {
                (void)node.setInt64("const", static_cast<std::int64_t>(low));
            } else {
                (void)node.setDouble("const", low);
            }
        }
    }
    if (node.has("minLength") && node.has("maxLength")) {
        if (node.get("minLength").asInt64() == 0 && node.get("maxLength").asInt64() == 0) {
            (void)node.setString("const", "");
        }
    }
}

void projectNode(JsonValue& node) {
    if (node.isArray()) {
        // JsonValue has pushBack and no element assignment, so an array is rebuilt rather than
        // edited in place. Order is preserved, which matters: the branch order is the order the
        // canonical document declares and a reader diffing the two documents should see them line
        // up.
        JsonValue rebuilt = JsonValue::array();
        for (std::size_t i = 0; i < node.size(); ++i) {
            JsonValue child = node.at(i);
            projectNode(child);
            (void)rebuilt.pushBack(child);
        }
        node = rebuilt;
        return;
    }
    if (!node.isObject()) {
        return;
    }

    // Pin BEFORE the bounds are removed — the pin is derived from them.
    pinToConst(node);
    for (const char* keyword : kUnsupportedKeywords) {
        node.remove(keyword);
    }

    // `oneOf` -> `anyOf`. The support list names `anyOf` and does not name `oneOf`, and the
    // substitution is sound HERE specifically because the four branches are mutually exclusive by
    // construction: every branch pins `posture` to a disjoint enum, so no document can satisfy two
    // of them and "exactly one" and "at least one" cannot disagree. This is a property of THIS
    // schema, not a general rewrite rule, and the round-trip test is what keeps it true.
    if (node.has("oneOf")) {
        JsonValue branches = node.get("oneOf");
        node.remove("oneOf");
        (void)node.set("anyOf", branches);
    }

    // A draft-07 declaration on a document that is no longer draft-07-complete would be a claim the
    // projection cannot honour. Dropped rather than left to be interpreted.
    node.remove("$schema");

    for (const std::string& key : node.keys()) {
        JsonValue child = node.get(key);
        projectNode(child);
        (void)node.set(key, child);
    }
}

} // namespace

const n8ro::core::JsonValue& orderJsonSchemaForStructuredOutputs() {
    static const JsonValue projected = [] {
        JsonValue copy = orderJsonSchema();  // by value: the canonical document is not touched
        projectNode(copy);
        return copy;
    }();
    return projected;
}

const n8ro::core::JsonValue& orderJsonSchema() {
    // Built once. The prompt's stable prefix embeds this document, and AIC-BE-3 requires the
    // prefix be byte-identical for the life of the run, so it cannot be rebuilt per render.
    static const JsonValue schema = buildOrderSchema();
    return schema;
}

const std::string& orderJsonSchemaText() {
    static const std::string text = orderJsonSchema().toString();
    return text;
}

const n8ro::core::JsonValue& orderSchemaBranch(Posture posture) {
    switch (posture) {
        case Posture::Hold:
            return holdBranch();
        case Posture::Engage:
        case Posture::Crank:
            return targetedBranch();
        case Posture::Defend:
            return defendBranch();
        case Posture::Ingress:
        case Posture::Rtb:
            break;
    }
    return transitBranch();
}

} // namespace arkheon::aicommander
