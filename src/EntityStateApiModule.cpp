// Arkheon Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Technologies. All rights reserved.

#include "EntityStateApiModule.h"

#include <scripting/LuaApiHelpers.h>
#include <scripting/MissionRegistrar.h>

#include <component/ComponentFieldAccess.h>
#include <component/ComponentTypeNames.h>
#include <entity/IEntityManager.h>

#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace n8ro::sample::simscripting {

namespace {

constexpr std::string_view kEntityStateNamespace = "entityState";

// The load-bearing access idiom, in one place: read the transform component's geodetic-position
// leaves field-by-field through the generic, schema-driven column read seam
// (component/ComponentFieldAccess.h). Components are generic schema-driven records, reached by
// (componentType, fieldPath) — not by a typed component interface. `fieldPath` is the schema leaf
// path (see the SDK schema reference); each read returns nullopt if the manager, entity, component,
// or field is unavailable. Reads { latitude, longitude, altitude } (degrees, degrees, meters).
bool readPositionGeodetic(
    const n8ro::sim::ScriptingApiContext& context,
    std::string_view entityId,
    double& latitudeDeg,
    double& longitudeDeg,
    double& altitudeMeters) {
    if (!context.entityManager) {
        return false;
    }
    const std::optional<double> latitude = n8ro::sim::readComponentFieldReal(
        *context.entityManager, entityId, n8ro::sim::kComponentTransform,
        "positionGeodetic/latitudeDeg");
    const std::optional<double> longitude = n8ro::sim::readComponentFieldReal(
        *context.entityManager, entityId, n8ro::sim::kComponentTransform,
        "positionGeodetic/longitudeDeg");
    const std::optional<double> altitude = n8ro::sim::readComponentFieldReal(
        *context.entityManager, entityId, n8ro::sim::kComponentTransform,
        "positionGeodetic/altitudeHaeM");
    if (!latitude.has_value() || !longitude.has_value() || !altitude.has_value()) {
        return false;
    }
    latitudeDeg = *latitude;
    longitudeDeg = *longitude;
    altitudeMeters = *altitude;
    return true;
}

// entityState.getAltitude(entityId) -> number (altitude in meters), or nil on any failure.
n8ro::core::LuaValue getAltitude(
    const n8ro::sim::ScriptingApiContext& context,
    const n8ro::core::LuaVariadicArgs& args) {
    if (args.empty()) {
        return std::monostate{};
    }
    std::string entityId;
    if (!n8ro::core::tryReadLuaString(args[0], entityId) || entityId.empty()) {
        return std::monostate{};
    }
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeMeters = 0.0;
    if (!readPositionGeodetic(context, entityId, latitudeDeg, longitudeDeg, altitudeMeters)) {
        return std::monostate{};
    }
    return altitudeMeters;
}

// entityState.rangeMeters(entityIdA, entityIdB) -> number (3D slant range in meters), or nil on
// failure. Spherical-earth approximation: haversine ground range combined with the altitude delta.
n8ro::core::LuaValue rangeMeters(
    const n8ro::sim::ScriptingApiContext& context,
    const n8ro::core::LuaVariadicArgs& args) {
    if (args.size() < 2U) {
        return std::monostate{};
    }
    std::string entityIdA;
    std::string entityIdB;
    if (!n8ro::core::tryReadLuaString(args[0], entityIdA) || entityIdA.empty()
        || !n8ro::core::tryReadLuaString(args[1], entityIdB) || entityIdB.empty()) {
        return std::monostate{};
    }

    double latADeg = 0.0;
    double lonADeg = 0.0;
    double altAMeters = 0.0;
    double latBDeg = 0.0;
    double lonBDeg = 0.0;
    double altBMeters = 0.0;
    if (!readPositionGeodetic(context, entityIdA, latADeg, lonADeg, altAMeters)
        || !readPositionGeodetic(context, entityIdB, latBDeg, lonBDeg, altBMeters)) {
        return std::monostate{};
    }

    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    constexpr double kEarthRadiusMeters = 6371000.0;
    const double phi1 = latADeg * kDegToRad;
    const double phi2 = latBDeg * kDegToRad;
    const double deltaPhi = (latBDeg - latADeg) * kDegToRad;
    const double deltaLambda = (lonBDeg - lonADeg) * kDegToRad;
    const double a = std::sin(deltaPhi / 2.0) * std::sin(deltaPhi / 2.0)
        + std::cos(phi1) * std::cos(phi2) * std::sin(deltaLambda / 2.0) * std::sin(deltaLambda / 2.0);
    const double groundMeters =
        2.0 * kEarthRadiusMeters * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    const double deltaAltitude = altBMeters - altAMeters;
    return std::sqrt(groundMeters * groundMeters + deltaAltitude * deltaAltitude);
}

} // namespace

const char* EntityStateApiModule::moduleId() const {
    return "sample.entityState";
}

bool EntityStateApiModule::registerWith(
    n8ro::sim::MissionRegistrar& registrar,
    const n8ro::sim::ScriptingApiContext& context) {
    bool allOk = true;

    // -- entityState.getAltitude --
    {
        n8ro::sim::LuaApiFunctionMeta meta;
        meta.description =
            "Returns an entity's geodetic altitude in meters by reading its transform component.\n"
            "Input: getAltitude(entityId)\n"
            "- entityId: runtime entity id (string)\n"
            "Returns: number (altitude in meters), or nil if the entity / transform is unavailable.\n"
            "Registered by the ai-commander sample under the new 'entityState' namespace.";
        meta.signatures.push_back(n8ro::sim::LuaApiSignatureMeta{
            {n8ro::sim::LuaApiParamMeta{"entityId", n8ro::sim::LuaApiType::String, false}},
            {n8ro::sim::LuaApiType::Number}
        });
        // The context is captured BY VALUE into the callback — the stateful pattern: the Lua runtime
        // calls the closure with only the script args, so it must close over the context.
        allOk = registrar.registerFunctionWithMetadata(
                    kEntityStateNamespace,
                    "getAltitude",
                    [context](const n8ro::core::LuaVariadicArgs& args) -> n8ro::core::LuaValue {
                        return getAltitude(context, args);
                    },
                    std::move(meta))
            && allOk;
    }

    // -- entityState.rangeMeters --
    {
        n8ro::sim::LuaApiFunctionMeta meta;
        meta.description =
            "Returns the 3D slant range in meters between two entities (spherical-earth approximation).\n"
            "Input: rangeMeters(entityIdA, entityIdB)\n"
            "Returns: number (meters), or nil if either entity / transform is unavailable.\n"
            "Registered by the ai-commander sample under the new 'entityState' namespace.";
        meta.signatures.push_back(n8ro::sim::LuaApiSignatureMeta{
            {n8ro::sim::LuaApiParamMeta{"entityIdA", n8ro::sim::LuaApiType::String, false},
             n8ro::sim::LuaApiParamMeta{"entityIdB", n8ro::sim::LuaApiType::String, false}},
            {n8ro::sim::LuaApiType::Number}
        });
        allOk = registrar.registerFunctionWithMetadata(
                    kEntityStateNamespace,
                    "rangeMeters",
                    [context](const n8ro::core::LuaVariadicArgs& args) -> n8ro::core::LuaValue {
                        return rangeMeters(context, args);
                    },
                    std::move(meta))
            && allOk;
    }

    return allOk;
}

} // namespace n8ro::sample::simscripting
