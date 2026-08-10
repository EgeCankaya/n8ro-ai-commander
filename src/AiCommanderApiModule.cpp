#include "AiCommanderApiModule.h"

#include "CommanderRuntime.h"
#include "Order.h"
#include "OrderSlot.h"

#include <core/json/JsonValue.h>
#include <scripting/LuaApiHelpers.h>
#include <scripting/MissionRegistrar.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arkheon::aicommander {

namespace {

using n8ro::core::LuaMultiReturn;
using n8ro::core::LuaScalarValue;
using n8ro::core::LuaValue;
using n8ro::core::LuaVariadicArgs;
using n8ro::sim::LuaApiFunctionMeta;
using n8ro::sim::LuaApiParamMeta;
using n8ro::sim::LuaApiSignatureMeta;
using n8ro::sim::LuaApiType;

constexpr std::string_view kNamespace = "aiCommander";

// The failure shapes, named once. Every function returns its documented failure shape rather than
// raising — a script must never have to pcall an aiCommander read.
[[nodiscard]] LuaValue nilTriple() {
    return LuaMultiReturn{LuaScalarValue{std::monostate{}},
                          LuaScalarValue{std::monostate{}},
                          LuaScalarValue{std::monostate{}}};
}

[[nodiscard]] bool readEntityId(const LuaVariadicArgs& args, std::size_t index, std::string& out) {
    if (args.size() <= index) {
        return false;
    }
    return n8ro::core::tryReadLuaString(args[index], out) && !out.empty();
}

[[nodiscard]] bool readNumberArg(const LuaVariadicArgs& args, std::size_t index, double& out) {
    if (args.size() <= index) {
        return false;
    }
    return n8ro::core::tryReadLuaNumber(args[index], out);
}

[[nodiscard]] bool readStringArg(const LuaVariadicArgs& args, std::size_t index, std::string& out) {
    if (args.size() <= index) {
        return false;
    }
    return n8ro::core::tryReadLuaString(args[index], out);
}

LuaApiFunctionMeta meta(
    std::string description, std::vector<LuaApiParamMeta> params, std::vector<LuaApiType> returns) {
    LuaApiFunctionMeta value;
    value.description = std::move(description);
    value.signatures.push_back(LuaApiSignatureMeta{std::move(params), std::move(returns)});
    return value;
}

LuaApiParamMeta str(const char* name) {
    return LuaApiParamMeta{name, LuaApiType::String, false};
}
LuaApiParamMeta num(const char* name) {
    return LuaApiParamMeta{name, LuaApiType::Number, false};
}

} // namespace

const char* AiCommanderApiModule::moduleId() const {
    return "arkheon.aiCommander.api";
}

bool AiCommanderApiModule::registerWith(
    n8ro::sim::MissionRegistrar& registrar,
    const n8ro::sim::ScriptingApiContext& context) {
    bool allOk = true;
    CommanderRuntime* runtime = runtime_;

    // Every lambda below captures `context` BY VALUE, per the IScriptingApiModule contract: the
    // registrar keeps the callback and the Lua runtime invokes it with only the script arguments,
    // so a function needing engine state must own its context rather than be handed one.
    // `runtime` is a raw pointer to plugin-owned state that outlives the registrar, and is only
    // ever touched on the update thread — which is where the Lua runtime calls these.
    const auto reg = [&](const char* name, LuaApiFunctionMeta functionMeta, auto&& body) {
        allOk = registrar.registerFunctionWithMetadata(
                    kNamespace, name,
                    [context, runtime, body](const LuaVariadicArgs& args) -> LuaValue {
                        (void)context;
                        if (runtime == nullptr) {
                            return body(nullptr, args);
                        }
                        return body(runtime, args);
                    },
                    std::move(functionMeta))
            && allOk;
    };

    // -- isValid ---------------------------------------------------------------------------------
    reg("isValid",
        meta("Returns true when the entity is on the commander roster and holds a currently valid "
             "order.\nInput: isValid(entityId)\nReturns: boolean. False when the commander is "
             "disabled, the entity is not commanded, or the order has been released.",
             {str("entityId")}, {LuaApiType::Boolean}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return false;
            }
            return rt->publishedOrder(entityId) != nullptr;
        });

    // -- requestCommand --------------------------------------------------------------------------
    reg("requestCommand",
        meta("Enrolls an entity on the commander roster. Idempotent.\n"
             "Input: requestCommand(entityId)\nReturns: boolean - false when the roster is at "
             "commander.maxCommandedEntities or the id is invalid.",
             {str("entityId")}, {LuaApiType::Boolean}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return false;
            }
            return rt->requestCommand(entityId);
        });

    // -- releaseCommand --------------------------------------------------------------------------
    reg("releaseCommand",
        meta("Removes an entity from the commander roster.\nInput: releaseCommand(entityId)\n"
             "Returns: boolean - false when the entity was not commanded.",
             {str("entityId")}, {LuaApiType::Boolean}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return false;
            }
            return rt->releaseCommand(entityId);
        });

    // -- getPosture ------------------------------------------------------------------------------
    reg("getPosture",
        meta("Returns the ordered posture, target, and cruise speed.\nInput: getPosture(entityId)\n"
             "Returns: string posture, string targetEntityId (empty when the posture carries none), "
             "number cruiseSpeedMps - or nil, nil, nil when there is no valid order.\n"
             "Postures: ingress | engage | crank | defend | hold | rtb.",
             {str("entityId")},
             {LuaApiType::String, LuaApiType::String, LuaApiType::Number}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return nilTriple();
            }
            const PublishedOrder* published = rt->publishedOrder(entityId);
            if (published == nullptr) {
                return nilTriple();
            }
            return LuaMultiReturn{
                LuaScalarValue{std::string(toString(published->order.posture))},
                LuaScalarValue{published->order.targetEntityId},
                LuaScalarValue{published->order.cruiseSpeedMps}};
        });

    // -- getWaypoint -----------------------------------------------------------------------------
    reg("getWaypoint",
        meta("Returns the ordered waypoint.\nInput: getWaypoint(entityId)\n"
             "Returns: number latitudeDeg, number longitudeDeg, number altitudeHaeM - or "
             "nil, nil, nil when the posture carries no waypoint (engage, crank, defend compute "
             "their own geometry).\nAltitude is height above the WGS-84 ellipsoid, not AGL or MSL.",
             {str("entityId")},
             {LuaApiType::Number, LuaApiType::Number, LuaApiType::Number}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return nilTriple();
            }
            const PublishedOrder* published = rt->publishedOrder(entityId);
            if (published == nullptr || !published->order.hasWaypoint()) {
                return nilTriple();
            }
            return LuaMultiReturn{
                LuaScalarValue{published->order.latitudeDeg},
                LuaScalarValue{published->order.longitudeDeg},
                LuaScalarValue{published->order.altitudeHaeM}};
        });

    // -- getOrbitRadiusM -------------------------------------------------------------------------
    reg("getOrbitRadiusM",
        meta("Returns the ordered orbit radius in metres.\nInput: getOrbitRadiusM(entityId)\n"
             "Returns: number - or -1 when the posture is not 'hold'.",
             {str("entityId")}, {LuaApiType::Number}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return -1.0;
            }
            const PublishedOrder* published = rt->publishedOrder(entityId);
            if (published == nullptr || published->order.posture != Posture::Hold) {
                return -1.0;
            }
            return published->order.orbitRadiusM;
        });

    // -- getRoe ----------------------------------------------------------------------------------
    reg("getRoe",
        meta("Returns the ordered rules of engagement.\nInput: getRoe(entityId)\n"
             "Returns: string - weaponsFree | weaponsTight | weaponsHold, or an empty string when "
             "there is no valid order.\nROE is independent of posture: weaponsHold obliges the "
             "script to cease fire whatever the posture.",
             {str("entityId")}, {LuaApiType::String}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return std::string();
            }
            const PublishedOrder* published = rt->publishedOrder(entityId);
            if (published == nullptr) {
                return std::string();
            }
            return std::string(toString(published->order.roe));
        });

    // -- getOrderSerial --------------------------------------------------------------------------
    reg("getOrderSerial",
        meta("Returns the monotonic serial of the published order.\n"
             "Input: getOrderSerial(entityId)\nReturns: number - or -1 when there is none.\n"
             "Lets a script detect a new order without diffing every field.",
             {str("entityId")}, {LuaApiType::Number}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return static_cast<std::int64_t>(-1);
            }
            const PublishedOrder* published = rt->publishedOrder(entityId);
            return published == nullptr ? static_cast<std::int64_t>(-1) : published->serial;
        });

    // -- getOrderAgeS ----------------------------------------------------------------------------
    reg("getOrderAgeS",
        meta("Returns simulation seconds since the order was accepted.\n"
             "Input: getOrderAgeS(entityId)\nReturns: number - or -1 when there is no order.",
             {str("entityId")}, {LuaApiType::Number}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return -1.0;
            }
            const EntityCommandState* state = rt->find(entityId);
            if (state == nullptr || !state->ladder.published.valid
                || state->ladder.lastAcceptedSimTimeS < 0.0) {
                return -1.0;
            }
            const double ageS = rt->currentSimTimeS() - state->ladder.published.acceptedSimTimeS;
            // Clamped at zero rather than reported negative: a scenario reload can run the clock
            // backwards, and a negative "age" is a value no script has a sensible branch for.
            return ageS < 0.0 ? 0.0 : ageS;
        });

    // -- getOrder --------------------------------------------------------------------------------
    reg("getOrder",
        meta("Returns the full published order as a JSON string.\nInput: getOrder(entityId)\n"
             "Returns: string - the order document, or \"{}\" when there is none.\n"
             "For logging and debugging; prefer the typed getters in behaviour code.",
             {str("entityId")}, {LuaApiType::String}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            if (rt == nullptr || !readEntityId(args, 0, entityId)) {
                return std::string("{}");
            }
            const PublishedOrder* published = rt->publishedOrder(entityId);
            if (published == nullptr) {
                return std::string("{}");
            }
            const Order& order = published->order;
            n8ro::core::JsonValue doc = n8ro::core::JsonValue::object();
            (void)doc.setInt64("schemaVersion", order.schemaVersion);
            (void)doc.setString("entityId", order.entityId);
            (void)doc.setString("posture", toString(order.posture));
            (void)doc.setString("targetEntityId", order.targetEntityId);
            (void)doc.setDouble("cruiseSpeedMps", order.cruiseSpeedMps);
            (void)doc.setDouble("orbitRadiusM", order.orbitRadiusM);
            (void)doc.setString("roe", toString(order.roe));
            (void)doc.setString("reason", order.reason);
            (void)doc.setInt64("serial", published->serial);
            if (order.hasWaypoint()) {
                n8ro::core::JsonValue waypoint = n8ro::core::JsonValue::object();
                (void)waypoint.setDouble("latitudeDeg", order.latitudeDeg);
                (void)waypoint.setDouble("longitudeDeg", order.longitudeDeg);
                (void)waypoint.setDouble("altitudeHaeM", order.altitudeHaeM);
                (void)doc.set("waypoint", waypoint);
            }
            return doc.toString();
        });

    // -- setSituationNote ------------------------------------------------------------------------
    reg("setSituationNote",
        meta("Attaches up to 256 characters of deterministic Tier-1 context to the next prompt "
             "(e.g. \"winchester\", \"2 shots in air\").\n"
             "Input: setSituationNote(entityId, text)\nReturns: boolean.\n"
             "The text is truncated and charset-filtered on ingress.",
             {str("entityId"), str("text")}, {LuaApiType::Boolean}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            std::string text;
            if (rt == nullptr || !readEntityId(args, 0, entityId) || !readStringArg(args, 1, text)) {
                return false;
            }
            return rt->setSituationNote(entityId, text);
        });

    // -- reportTrack (v1.2; arity 4 -> 6 in v1.8.30) ----------------------------------------------
    reg("reportTrack",
        meta("Reports one detected track to the commander for this cadence window.\n"
             "Input: reportTrack(entityId, targetEntityId, rangeM, snrDb, kind, team)\n"
             "  rangeM: metres (sensor.getTrackById's range_m)\n"
             "  snrDb:  decibels (sensor.getTrackById's snr_DB)\n"
             "  kind:   \"air\" | \"ground\" | \"surface\" | \"munition\" | \"other\"\n"
             "  team:   \"hostile\" | \"friendly\" | \"unknown\", RELATIVE to the reporting entity\n"
             "Returns: boolean - false when the entity is not commanded, the arguments are "
             "invalid, or the list already holds commander.maxTracksInPrompt entries.\n"
             "kind and team are CLAMPED to their vocabularies rather than validated: an omitted or "
             "unrecognised value becomes \"other\" / \"unknown\" and the call still succeeds, so a "
             "script that cannot classify a contact still reports it rather than losing it.\n"
             "Do NOT report own-team contacts (AIC-ORD-2): filter them in Tier 1, where the team "
             "is already in hand from entityControl.getEntityInfo.\n"
             "Idempotent per targetEntityId. The commander cannot see sensor tracks itself, so an "
             "unreported contact cannot be targeted by an order: Stage-B validation rejects any "
             "order naming a target that was not reported.",
             {str("entityId"), str("targetEntityId"), num("rangeM"), num("snrDb"),
              str("kind"), str("team")},
             {LuaApiType::Boolean}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            std::string targetEntityId;
            double rangeM = 0.0;
            double snrDb = 0.0;
            if (rt == nullptr || !readEntityId(args, 0, entityId)
                || !readEntityId(args, 1, targetEntityId)
                || !readNumberArg(args, 2, rangeM) || !readNumberArg(args, 3, snrDb)) {
                return false;
            }
            // The two attribute arguments are read PERMISSIVELY, and the discarded returns are the
            // point rather than an oversight. A script still on the pre-v1.8.30 four-argument form
            // passes neither; a failed read leaves the local empty, which both parsers map to the
            // safe member of their vocabulary. So the omission degrades to the OLD picture rather
            // than to NO picture - and no picture would mean Stage-B B3 rejecting every targeted
            // order, which is an outage nobody would trace back to an arity change.
            std::string kindText;
            std::string teamText;
            (void)readStringArg(args, 4, kindText);
            (void)readStringArg(args, 5, teamText);
            return rt->reportTrack(entityId, targetEntityId, rangeM, snrDb,
                                   parseTrackKind(kindText), parseTrackTeam(teamText));
        });

    // -- reportLoadout (v1.2) --------------------------------------------------------------------
    reg("reportLoadout",
        meta("Reports one hardpoint's remaining stores for this cadence window.\n"
             "Input: reportLoadout(entityId, hardpointName, weaponProfileName, ammoCount, ammoMax)\n"
             "Returns: boolean. Idempotent per hardpointName.\n"
             "Feed it from weapon.getWeaponLoadout(entityId).",
             {str("entityId"), str("hardpointName"), str("weaponProfileName"),
              num("ammoCount"), num("ammoMax")},
             {LuaApiType::Boolean}),
        [](CommanderRuntime* rt, const LuaVariadicArgs& args) -> LuaValue {
            std::string entityId;
            std::string hardpointName;
            std::string weaponProfileName;
            double ammoCount = 0.0;
            double ammoMax = 0.0;
            if (rt == nullptr || !readEntityId(args, 0, entityId)
                || !readEntityId(args, 1, hardpointName)
                || !readStringArg(args, 2, weaponProfileName)
                || !readNumberArg(args, 3, ammoCount) || !readNumberArg(args, 4, ammoMax)) {
                return false;
            }
            return rt->reportLoadout(entityId, hardpointName, weaponProfileName,
                                     static_cast<int>(ammoCount), static_cast<int>(ammoMax));
        });

    // -- getStats --------------------------------------------------------------------------------
    reg("getStats",
        meta("Returns the commander's health and counters as a JSON string.\nInput: getStats()\n"
             "Returns: string (JSON): enabled, operational, backend, model, rosterSize, requested, "
             "accepted, rejected, timeouts, droppedSnapshots, lastLatencyMs, rejectByReason, "
             "runtimeColumnProbe, and per-entity fallback levels.",
             {}, {LuaApiType::String}),
        [](CommanderRuntime* rt, const LuaVariadicArgs&) -> LuaValue {
            if (rt == nullptr) {
                return std::string("{}");
            }
            return rt->statsJson();
        });

    return allOk;
}

} // namespace arkheon::aicommander
