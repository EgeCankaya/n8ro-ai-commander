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
        "log and is never parsed."));

    return properties;
}

// One branch of the oneOf.
//
// `withWaypoint` is the whole point of the split: the waypoint branch requires the field, and the
// geometry branch does not declare it at all, so additionalProperties:false turns its absence into
// a prohibition rather than a silence. A constrained decoder can hold both of those; it cannot hold
// "required only when posture is one of these three", which is what a flat document would need
// if/then/else to say and which the decoding path does not honour.
JsonValue buildBranch(const std::vector<const char*>& postures, bool withWaypoint,
                      const std::string& postureDescription, const std::string& branchDescription) {
    JsonValue properties = buildSharedProperties();
    (void)properties.set("posture", makeEnum(postures, postureDescription));

    if (withWaypoint) {
        (void)properties.set("targetEntityId", makeString(0, 0,
            "Not applicable to these postures. Emit the empty string."));
        (void)properties.set("waypoint", buildWaypoint());
    } else {
        (void)properties.set("targetEntityId", makeString(1, kMaxEntityIdChars,
            "The contact to act against, taken from the reported track list. Never invent one."));
    }

    // orbitRadiusM is described differently per branch because its legal range differs, and a
    // decoder that is told the range can simply not emit an illegal value.
    if (withWaypoint) {
        (void)properties.set("orbitRadiusM", makeNumber(0.0, kMaxOrbitRadiusM,
            "Orbit radius in metres (unit M; "
            "/datablocks/componentNavigation/onWaypointReachedLoiterRadiusM). Greater than zero "
            "for hold, and exactly 0 for ingress and rtb."));
    } else {
        (void)properties.set("orbitRadiusM", makeNumber(0.0, 0.0,
            "Not applicable to these postures. Emit 0."));
    }

    std::vector<const char*> required{
        "schemaVersion", "entityId", "posture", "targetEntityId",
        "cruiseSpeedMps", "orbitRadiusM", "roe", "reason",
    };
    if (withWaypoint) {
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

const JsonValue& waypointBranch() {
    static const JsonValue branch = buildBranch(
        {"ingress", "hold", "rtb"}, true,
        "Postures that fly to an ordered point. ingress repositions to it, hold orbits it, rtb "
        "egresses to it.",
        "An order that sends the aircraft to a point. Carries a waypoint and no target.");
    return branch;
}

const JsonValue& geometryBranch() {
    static const JsonValue branch = buildBranch(
        {"engage", "crank", "defend"}, false,
        "Postures where the aircraft computes its own geometry. engage pursues the target, crank "
        "flies an offset while a shot is supported, defend turns cold from the nearest threat.",
        "An order that names an intent the aircraft resolves into geometry itself. Carries no "
        "waypoint - supplying one would be a field nothing reads.");
    return branch;
}

JsonValue buildOrderSchema() {
    JsonValue branches = JsonValue::array();
    (void)branches.pushBack(waypointBranch());
    (void)branches.pushBack(geometryBranch());

    JsonValue schema = JsonValue::object();
    (void)schema.setString("$schema", "http://json-schema.org/draft-07/schema#");
    (void)schema.setString("title", "N8RO AI Entity Commander Order");
    (void)schema.set("oneOf", branches);
    (void)schema.setString("description",
        "Exactly one order for exactly one entity, matching exactly one of the two branches below. "
        "Which branch applies is decided by the posture. There is deliberately no property for "
        "heading, pitch, roll, velocity, acceleration, turn rate, load factor, hardpoint selection, "
        "or a fire command: those are the deterministic tiers' job, not the model's.");

    return schema;
}

} // namespace

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
    return postureRequiresWaypoint(posture) ? waypointBranch() : geometryBranch();
}

} // namespace arkheon::aicommander
