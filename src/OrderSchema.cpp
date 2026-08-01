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

JsonValue buildOrderSchema() {
    // -- waypoint ------------------------------------------------------------------------------
    // Units and frames are read from dev/ai-coding/schema-reference/schema-reference.json, never
    // from memory. The `unit` key on each cited record is the authority, and SchemaUnitTests
    // re-reads that file to assert these descriptions have not drifted from it.
    JsonValue waypointProps = JsonValue::object();
    (void)waypointProps.set("latitudeDeg", makeNumber(kMinLatitudeDeg, kMaxLatitudeDeg,
        "Geodetic WGS-84 latitude in degrees (unit Deg; "
        "/datablocks/positionGeodetic/latitudeDeg)."));
    (void)waypointProps.set("longitudeDeg", makeNumber(kMinLongitudeDeg, kMaxLongitudeDeg,
        "Geodetic WGS-84 longitude in degrees (unit Deg; "
        "/datablocks/positionGeodetic/longitudeDeg)."));
    (void)waypointProps.set("altitudeHaeM", makeNumber(-1000.0, 100000.0,
        "Height above the WGS-84 ellipsoid in metres - NOT above ground level, NOT mean sea level "
        "(unit M; /datablocks/positionGeodetic/altitudeHaeM)."));

    JsonValue waypointRequired = JsonValue::array();
    (void)waypointRequired.pushBack(JsonValue::fromString("latitudeDeg"));
    (void)waypointRequired.pushBack(JsonValue::fromString("longitudeDeg"));
    (void)waypointRequired.pushBack(JsonValue::fromString("altitudeHaeM"));

    JsonValue waypoint = JsonValue::object();
    (void)waypoint.setString("type", "object");
    (void)waypoint.set("properties", waypointProps);
    (void)waypoint.set("required", waypointRequired);
    (void)waypoint.setBool("additionalProperties", false);
    (void)waypoint.setString("description",
        "Destination. Required for ingress, hold, and rtb; omitted otherwise, because for engage, "
        "crank, and defend the script computes the geometry itself.");

    // -- the order document --------------------------------------------------------------------
    JsonValue properties = JsonValue::object();

    JsonValue version = JsonValue::object();
    (void)version.setString("type", "integer");
    (void)version.setInt64("minimum", kOrderSchemaVersion);
    (void)version.setInt64("maximum", kOrderSchemaVersion);
    (void)version.setString("description", "Order schema version. Must be exactly 1.");
    (void)properties.set("schemaVersion", version);

    (void)properties.set("entityId", makeString(1, kMaxEntityIdChars,
        "The commanded entity's runtime id. Must equal the id in the request."));

    (void)properties.set("posture", makeEnum(
        {"ingress", "engage", "crank", "defend", "hold", "rtb"},
        "The tactical posture the entity should adopt."));

    (void)properties.set("targetEntityId", makeString(0, kMaxEntityIdChars,
        "The contact to act against. Required and non-empty for engage and crank; empty string "
        "otherwise."));

    (void)properties.set("waypoint", waypoint);

    (void)properties.set("cruiseSpeedMps", makeNumber(0.0, 1000.0,
        "Commanded ground speed in metres per second (unit Mps; /datablocks/waypoint/speed). "
        "Must be greater than zero; the upper bound the entity will actually accept is "
        "safety.maxSpeedMps."));

    (void)properties.set("orbitRadiusM", makeNumber(0.0, kMaxOrbitRadiusM,
        "Orbit radius in metres (unit M; "
        "/datablocks/componentNavigation/onWaypointReachedLoiterRadiusM). Greater than zero only "
        "when posture is hold; zero otherwise."));

    (void)properties.set("roe", makeEnum(
        {"weaponsFree", "weaponsTight", "weaponsHold"},
        "Rules of engagement. Orthogonal to posture: weaponsHold suppresses fire regardless of "
        "what the entity is flying toward."));

    (void)properties.set("reason", makeString(kMinReasonChars, kMaxReasonChars,
        "One sentence of rationale. Advisory only - it is recorded for humans reading the order "
        "log and is never parsed."));

    JsonValue required = JsonValue::array();
    for (const char* name : {"schemaVersion", "entityId", "posture", "cruiseSpeedMps", "roe", "reason"}) {
        (void)required.pushBack(JsonValue::fromString(name));
    }

    JsonValue schema = JsonValue::object();
    (void)schema.setString("$schema", "http://json-schema.org/draft-07/schema#");
    (void)schema.setString("title", "N8RO AI Entity Commander Order");
    (void)schema.setString("type", "object");
    (void)schema.set("properties", properties);
    (void)schema.set("required", required);
    // The load-bearing line. Without it, a model that emits "headingDeg": 270 would have that
    // field silently ignored rather than the order rejected - and "the model cannot express raw
    // kinematics" would be a claim about the prompt rather than a property of the contract.
    (void)schema.setBool("additionalProperties", false);
    (void)schema.setString("description",
        "Exactly one order for exactly one entity. There is deliberately no property for heading, "
        "pitch, roll, velocity, acceleration, turn rate, load factor, hardpoint selection, or a "
        "fire command: those are the deterministic tiers' job, not the model's.");

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

} // namespace arkheon::aicommander
