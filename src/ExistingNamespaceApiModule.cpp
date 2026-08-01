// Arkheon Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Technologies. All rights reserved.

#include "ExistingNamespaceApiModule.h"

#include <scripting/LuaApiHelpers.h>
#include <scripting/MissionRegistrar.h>

#include <cmath>

namespace n8ro::sample::simscripting {

namespace {

// Pure great-circle initial-bearing helper. Returns degrees in [0, 360), or -1.0 on bad input
// (mirrors the navigation.* convention of returning a negative sentinel on invalid arguments).
// This helper needs no engine state; see the (void)context note below for the stateful case.
n8ro::core::LuaValue bearingDegrees(const n8ro::core::LuaVariadicArgs& args) {
    if (args.size() < 4) {
        return -1.0;
    }
    double lat1 = 0.0;
    double lon1 = 0.0;
    double lat2 = 0.0;
    double lon2 = 0.0;
    if (!n8ro::core::tryReadLuaNumber(args[0], lat1)
        || !n8ro::core::tryReadLuaNumber(args[1], lon1)
        || !n8ro::core::tryReadLuaNumber(args[2], lat2)
        || !n8ro::core::tryReadLuaNumber(args[3], lon2)) {
        return -1.0;
    }

    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    const double phi1 = lat1 * kDegToRad;
    const double phi2 = lat2 * kDegToRad;
    const double deltaLambda = (lon2 - lon1) * kDegToRad;
    const double y = std::sin(deltaLambda) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2)
        - std::sin(phi1) * std::cos(phi2) * std::cos(deltaLambda);
    const double bearing = std::atan2(y, x) / kDegToRad;
    return std::fmod(bearing + 360.0, 360.0);
}

} // namespace

const char* ExistingNamespaceApiModule::moduleId() const {
    return "sample.navigationExtensions";
}

bool ExistingNamespaceApiModule::registerWith(
    n8ro::sim::MissionRegistrar& registrar,
    const n8ro::sim::ScriptingApiContext& context) {
    // The context (entityManager, messageBusPacked, scenarioManager, simulationEngine) is captured
    // by value into the callback when a function needs runtime state. bearingDegrees is pure math,
    // so this module does not use it.
    (void)context;

    n8ro::sim::LuaApiFunctionMeta meta;
    meta.description =
        "Returns the great-circle initial bearing in degrees [0, 360) from point 1 to point 2.\n"
        "Input: bearingDegrees(lat1, lon1, lat2, lon2)\n"
        "- lat1, lon1: source latitude/longitude in degrees\n"
        "- lat2, lon2: destination latitude/longitude in degrees\n"
        "Returns: number (degrees, or -1 on invalid input).\n"
        "Registered by the ai-commander sample plugin into the existing 'navigation' namespace.";
    meta.signatures.push_back(n8ro::sim::LuaApiSignatureMeta{
        {
            n8ro::sim::LuaApiParamMeta{"lat1", n8ro::sim::LuaApiType::Number, false},
            n8ro::sim::LuaApiParamMeta{"lon1", n8ro::sim::LuaApiType::Number, false},
            n8ro::sim::LuaApiParamMeta{"lat2", n8ro::sim::LuaApiType::Number, false},
            n8ro::sim::LuaApiParamMeta{"lon2", n8ro::sim::LuaApiType::Number, false}
        },
        {n8ro::sim::LuaApiType::Number}
    });

    return registrar.registerFunctionWithMetadata(
        "navigation",
        "bearingDegrees",
        [](const n8ro::core::LuaVariadicArgs& args) -> n8ro::core::LuaValue {
            return bearingDegrees(args);
        },
        std::move(meta));
}

} // namespace n8ro::sample::simscripting
